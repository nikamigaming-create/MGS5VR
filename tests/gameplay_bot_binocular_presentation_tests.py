"""Authored contact, real controller joins, continuity and interruption tests."""
import copy
import math
import pathlib
import re
import sys
import unittest
from unittest.mock import Mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from gameplay_bot.binocular_presentation import (
    BinocularPresentation, OCULAR, PALM, PRIMARY, SUPPORT, _angle, _compose,
    grip_rotation, inspection_poses, static_support_capture_allowed, support_poses)
from gameplay_bot.combat import controller_pair, mul, normalized
from gameplay_bot.core import BotFault
from gameplay_bot.live import channel, rotate


class Clock:
    def __init__(self):
        self.now = 0.
    def __call__(self):
        return self.now
    def sleep(self, seconds):
        self.now += seconds


def settings(pitch=-90, yaw=0, roll=0):
    return {'settings': [{'name': 'settings.binocular_'+axis+'_degrees', 'value': value}
                         for axis, value in zip(('pitch', 'yaw', 'roll'), (pitch, yaw, roll))],
            'actions': [
                {'name': 'binoculars.support_grip', 'modifier': True, 'contexts': ['binoculars'],
                 'bindings': [{'gesture': 'level', 'inputs': ['left_grip']}]},
                {'name': 'binoculars.mark', 'modifier': False, 'contexts': ['binoculars'],
                 'bindings': [{'gesture': 'press', 'inputs': ['right_trigger']}]},
                {'name': 'binoculars.zoom', 'modifier': False, 'contexts': ['binoculars'],
                 'bindings': [{'gesture': 'press', 'inputs': ['left_stick_click']}]}]}


class Live:
    """Pose/input transport fixture; no native game outcomes are fabricated."""
    def __init__(self, move_hands_with_head=False):
        self.clock = Clock(); self.events = Mock(); self.bindings = settings()
        self.held = {}; self.inputs = []; self.mutations = []; self.releases = []
        self.head = {'position': [0., 0., 0.], 'orientation': [0., 0., 0., 1.]}
        self.poses = {
            ('right', 'grip'): {'position': [.3, -.33, -.46], 'orientation': [.5, 0., 0., math.sqrt(.75)]},
            ('right', 'aim'): {'position': [.3, -.3, -.555], 'orientation': [0., 0., 0., 1.]},
            ('left', 'grip'): {'position': [-.04, -.16, -.55], 'orientation': [0., math.sqrt(.5), 0., math.sqrt(.5)]},
            ('left', 'aim'): {'position': [-.05, -.14, -.59], 'orientation': [0., .6, 0., .8]}}
        self.move_hands_with_head = move_hands_with_head
        self.owner = 123; self.focused = True; self.stamp = 100
        self.held_optic = False; self.frozen_publication = False; self.ignore_endpoint = False
        self.packet_clock_cost = 0.

    def text_result(self, value):
        return value

    def call(self, tool, args):
        if tool == 'get_head_pose':
            value = self.head
        elif tool == 'get_controller_pose':
            value = self.poses[args['hand'], args['pose_type']]
        elif tool in ('set_head_pose', 'set_controller_pose'):
            value = {k: copy.deepcopy(args[k]) for k in ('position', 'orientation')}
            self.mutations.append((tool, copy.deepcopy(args)))
            if tool == 'set_head_pose':
                if self.move_hands_with_head:
                    from gameplay_bot.binocular_presentation import _relative
                    self.poses = {k: _compose(value, _relative(self.head, p)) for k, p in self.poses.items()}
                self.head = value
            elif not self.ignore_endpoint:
                self.poses[args['hand'], args['pose_type']] = value
            self.clock.now += self.packet_clock_cost
        else:
            raise AssertionError(tool)
        return {'pose': copy.deepcopy(value), 'flags': {'position_valid': True, 'orientation_valid': True}}

    def observe(self):
        if not self.frozen_publication:
            self.stamp += 1
        grip, aim = self.poses['right', 'grip'], self.poses['right', 'aim']
        body_q = normalized(mul(aim['orientation'], grip_rotation(self.bindings)))
        body = {'orientation': body_q, 'position': [g-d for g, d in zip(grip['position'], rotate(body_q, PRIMARY))]}
        ocular = _compose(body, {'position': OCULAR, 'orientation': [0., 0., 0., 1.]})
        physical = [0.]*11
        indices = {('left', 'Grip', None): 7, ('right', 'Trigger', None): 10, ('left', 'ThumbstickClick', None): 5}
        for key, item in self.held.items():
            physical[indices[key]] = item['value']
        frame = {'head_pose': copy.deepcopy(self.head), 'binocular_ocular': ocular,
                 'left_grip': copy.deepcopy(self.poses['left', 'grip']),
                 'right_grip': copy.deepcopy(grip), 'binocular_held': self.held_optic,
                 'sample_ms': self.stamp, 'rig_sequence': self.stamp, 'presentation_epoch': 4,
                 'player_owner': self.owner, 'presentation_focused': self.focused,
                 'native_pose': copy.deepcopy(self.head),
                 **{'raw_'+h+'_'+k+'_tracked': True for h in ('left', 'right') for k in ('grip', 'aim')}}
        return {'scene': 'gameplay', 'menu': False, 'idroid': False, 'gamepad': False,
                'camera_active': True, 'now_ms': self.stamp, 'activation': 2,
                'native': {'mission': 30010, 'location': 10}, 'rendered': frame,
                'controls': {'context': 'binoculars' if self.held_optic else 'gameplay',
                             'rig_input': True, 'sample_ms': self.stamp, 'age_ms': 0,
                             'physical': physical, 'sticks': [0.]*4, 'native_axes': [0]*4,
                             'native_buttons': 0, 'native_triggers': [0, 0]}}

    def input(self, values, seconds, **options):
        self.inputs.append((copy.deepcopy(values), seconds, options))
        for item in values:
            self.held[item['hand'], item['component'], item.get('sub_component')] = item

    def release(self, *, keep=()):
        self.releases.append(keep)
        self.held = {key: value for key, value in self.held.items() if key in keep}


class BinocularTests(unittest.TestCase):
    def helper(self, live):
        return BinocularPresentation(live, live.observe, clock=live.clock, sleep=live.clock.sleep)

    def assert_pose_close(self, a, b):
        self.assertLess(math.dist(a['position'], b['position']), 1e-8)
        self.assertLess(_angle(a['orientation'], b['orientation']), 3e-6)

    def test_quantized_operator_quaternion_and_its_double_cover_have_zero_angle(self):
        rounded = [0., -.5153, 0., .857]
        expected = normalized(rounded)
        self.assertLess(_angle(rounded, expected), 3e-6)
        self.assertLess(_angle([-v for v in rounded], expected), 3e-6)
        self.assertGreater(_angle([0., -.52, 0., .85], expected), .1)

    def test_authored_sockets_still_match_the_production_housing_contract(self):
        source = (pathlib.Path(__file__).resolve().parents[1]/'include/mgs5vr/optic_rig.hpp').read_text()
        for name, expected in [('binocularOcularCenter', OCULAR), ('binocularPrimarySocket', PRIMARY),
                               ('binocularSupportSocket', SUPPORT), ('binocularPalmOrientation', PALM)]:
            raw = re.search(r'\b'+name+r'\{([^}]+)\}', source).group(1)
            self.assertEqual(tuple(float(x.strip().rstrip('f')) for x in raw.split(',')), tuple(expected))

    def test_different_real_controller_joins_and_effective_fit_reach_one_rigid_assembly(self):
        live = Live(); right_pair = controller_pair(live.poses['right', 'grip'], live.poses['right', 'aim'])
        left_pair = controller_pair(live.poses['left', 'grip'], live.poses['left', 'aim'])
        for fit_settings in (settings(), settings(-35, 17, -12), settings(60, -90, 90)):
            fit = grip_rotation(fit_settings)
            head = {'position': [.1, .05, -.02], 'orientation': normalized([.1, .4, -.2, .8])}
            poses = inspection_poses(head, right_pair, left_pair, fit)
            for hand, pair in [('right', right_pair), ('left', left_pair)]:
                self.assert_pose_close(controller_pair(*poses[hand]), pair)
                self.assertGreater(math.dist(poses[hand][0]['position'], poses[hand][1]['position']), .01)
            grip, aim = poses['right']; q = normalized(mul(aim['orientation'], fit))
            self.assertLess(_angle(q, head['orientation']), 3e-6)
            body = {'position': [p-d for p, d in zip(grip['position'], rotate(q, PRIMARY))], 'orientation': q}
            self.assert_pose_close(poses['left'][0], _compose(body, {'position': SUPPORT, 'orientation': PALM}))
            self.assert_pose_close(_compose(body, {'position': OCULAR, 'orientation': [0., 0., 0., 1.]}),
                                   _compose(head, {'position': [.032, 0., -.06], 'orientation': [0., 0., 0., 1.]}))

    def test_support_uses_the_actual_rendered_ocular_instead_of_a_head_or_gun_offset(self):
        live = Live(); pair = controller_pair(live.poses['left', 'grip'], live.poses['left', 'aim'])
        frame = {'binocular_held': True, 'binocular_ocular': {
            'position': [.0210382, .0127406, .0633926],
            'orientation': normalized([-.0426062, .911833, -.0980039, -.396411])}}
        grip, aim = support_poses(frame, pair)
        self.assert_pose_close(controller_pair(grip, aim), pair)
        self.assertAlmostEqual(math.dist(grip['position'], frame['binocular_ocular']['position']),
                               math.dist(SUPPORT, OCULAR))
        for bad in ({**frame, 'binocular_held': False}, {**frame, 'binocular_ocular': None}):
            with self.assertRaises(BotFault): support_poses(bad, pair)

    def test_large_head_turn_keeps_both_measured_pairs_and_continuous_contact(self):
        for carry in (False, True):
            live = Live(carry); helper = self.helper(live); helper.raise_to_eye()
            start_head = copy.deepcopy(live.head); start = {h: copy.deepcopy(live.poses[h, 'grip']) for h in ('left', 'right')}
            live.mutations.clear()
            angle = math.radians(162.260)/2
            end = {**live.head, 'orientation': [0., math.sin(angle), 0., math.cos(angle)]}
            helper._motion(end, start, follow_head=True)
            packets = [live.mutations[i:i+5] for i in range(0, len(live.mutations), 5)]
            self.assertGreaterEqual(len(packets), 28)
            prior_head = start_head; prior = start
            for packet in packets:
                self.assertEqual(packet[0][0], 'set_head_pose')
                head = packet[0][1]
                self.assertLessEqual(_angle(prior_head['orientation'], head['orientation']), 6.000001)
                for hand, rows in [('right', packet[1:3]), ('left', packet[3:5])]:
                    grip, aim = [{k: row[1][k] for k in ('position', 'orientation')} for row in rows]
                    self.assert_pose_close(controller_pair(grip, aim), helper.pairs[hand])
                    self.assertLessEqual(math.dist(prior[hand]['position'], grip['position']), .015001)
                    prior[hand] = grip
                prior_head = head
            self.assert_pose_close(live.head, end)

    def test_owner_change_frozen_publication_and_slow_rpc_abort_without_replay(self):
        for fault in ('owner', 'frozen', 'slow'):
            live = Live(); helper = self.helper(live)
            if fault == 'owner': live.owner = 999
            if fault == 'frozen': live.frozen_publication = True
            if fault == 'slow': live.packet_clock_cost = .2
            with self.assertRaises(BotFault): helper.raise_to_eye()
            self.assertEqual(live.held, {})
            self.assertLessEqual(len(live.mutations), 4)

    def test_long_raise_segments_healthy_rpc_latency_without_skipping_poses(self):
        live = Live(); helper = self.helper(live); live.packet_clock_cost = .04
        helper.raise_to_eye()
        completed = [c.kwargs for c in live.events.emit.call_args_list
                     if c.args == ('binocular_motion_completed',)]
        self.assertGreater(live.clock.now, 5.)
        self.assertGreater(len(completed), 1)
        self.assertTrue(all(c['packets'] <= 12 and c['seconds'] < 5. for c in completed))
        self.assertTrue(all(c['largest_packet_gap_seconds'] < .5 for c in completed))

    def test_segmented_motion_retains_one_total_deadline_and_releases_on_failure(self):
        live = Live(); helper = self.helper(live); live.packet_clock_cost = .04
        targets = inspection_poses(live.head, helper.pairs['right'], helper.pairs['left'], helper.fit)
        with self.assertRaisesRegex(BotFault, 'thirty seconds'):
            helper._motion(live.head, {h:p[0] for h,p in targets.items()}, follow_head=False,
                           _path_deadline=live.clock.now+.2)
        self.assertTrue(live.releases)
        self.assertLessEqual(len(live.mutations), 8)

    def test_pose_endpoint_failure_is_not_reported_as_completed_motion(self):
        live = Live(); helper = self.helper(live); live.ignore_endpoint = True
        with self.assertRaisesRegex(BotFault, 'endpoint'): helper.raise_to_eye()
        self.assertFalse(any(c.args == ('binocular_motion_completed',) for c in live.events.emit.call_args_list))

    def test_support_is_never_credited_from_grip_input_without_authored_proximity(self):
        live = Live(); helper = self.helper(live); live.held_optic = True
        with self.assertRaisesRegex(BotFault, 'side cup'): helper.hold_support()
        self.assertFalse(live.inputs)
        helper.raise_to_eye(); helper.hold_support(); evidence = helper.support_observation()
        self.assertTrue(evidence['sampled_left_grip']); self.assertLess(evidence['authored_contact_distance'], 1e-8)
        self.assertFalse(evidence['actual_contact_flag_published'])

    def test_explicit_mark_requires_acquisition_and_releases_trigger_while_keeping_support(self):
        live = Live(); helper = self.helper(live); helper.raise_to_eye(); live.held_optic = True
        with self.assertRaises(BotFault): helper.press('binoculars.mark')
        self.assertFalse(live.inputs)
        helper.press('binoculars.mark', acquired=True)
        self.assertEqual(set(live.held), {('left', 'Grip', None)})
        self.assertEqual(live.inputs[-1][0], [channel('right_trigger')])
        self.assertFalse(any(c.args[0] == 'binocular_supported_action' and c.kwargs['native_outcome_acceptance']
                             for c in live.events.emit.call_args_list))
        helper.press('binoculars.zoom'); self.assertEqual(set(live.held), {('left', 'Grip', None)})

    def test_static_capture_contract_rejects_native_leaks_missing_tracking_and_binding_collisions(self):
        live = Live(); helper = self.helper(live); helper.raise_to_eye(); live.held_optic = True; helper.hold_support()
        good = live.observe(); self.assertTrue(static_support_capture_allowed(live.held, good, live.bindings))
        shared_focus = copy.deepcopy(live.bindings)
        shared_focus['actions'].append({'name': 'system.idroid_focus', 'modifier': True,
            'contexts': ['gameplay','binoculars'], 'bindings': [{'gesture': 'level', 'inputs': ['left_grip']}]})
        self.assertTrue(static_support_capture_allowed(live.held, good, shared_focus))
        open_handset = copy.deepcopy(good); open_handset['idroid'] = True
        self.assertFalse(static_support_capture_allowed(live.held, open_handset, shared_focus))
        shared_focus['actions'][-1]['bindings'][0]['gesture'] = 'press'
        self.assertFalse(static_support_capture_allowed(live.held, good, shared_focus))
        for key, value in [('native_buttons', 1), ('native_axes', [0, 0, 10, 0]),
                           ('native_triggers', [0, 255]), ('sticks', [0, 1, 0, 0])]:
            bad = copy.deepcopy(good); bad['controls'][key] = value
            self.assertFalse(static_support_capture_allowed(live.held, bad, live.bindings))
        for key, value in [('raw_left_grip_tracked', False), ('presentation_focused', False), ('sample_ms', -1000)]:
            bad = copy.deepcopy(good); bad['rendered'][key] = value
            self.assertFalse(static_support_capture_allowed(live.held, bad, live.bindings))
        bad = copy.deepcopy(good); bad['controls']['physical'][10] = 1.
        self.assertFalse(static_support_capture_allowed(live.held, bad, live.bindings))
        bad_bindings = copy.deepcopy(live.bindings)
        bad_bindings['actions'].append({'name': 'binoculars.mark', 'contexts': ['binoculars'],
                                        'bindings': [{'gesture': 'press', 'inputs': ['left_grip']}]})
        self.assertFalse(static_support_capture_allowed(live.held, good, bad_bindings))

    def test_ocular_feedback_is_small_continuous_and_retains_both_controller_joins(self):
        live = Live(); helper = self.helper(live); helper.raise_to_eye(); live.held_optic = True
        before = copy.deepcopy(live.poses['right', 'grip']); live.poses['right', 'grip']['position'][0] += .02
        live.poses['right', 'aim']['position'][0] += .02
        helper.align_to_ocular(); self.assert_pose_close(live.poses['right', 'grip'], before)
        for hand in ('right', 'left'):
            self.assert_pose_close(controller_pair(live.poses[hand, 'grip'], live.poses[hand, 'aim']), helper.pairs[hand])
        live.poses['right', 'grip']['position'][0] += .2
        live.poses['right', 'aim']['position'][0] += .2
        count = len(live.mutations)
        with self.assertRaisesRegex(BotFault, 'six centimetres'): helper.align_to_ocular()
        self.assertEqual(len(live.mutations), count)

    def test_restore_requires_stow_and_returns_head_and_measured_hand_pairs(self):
        live = Live(); original = copy.deepcopy(live.poses); helper = self.helper(live); helper.raise_to_eye()
        live.held_optic = True
        with self.assertRaisesRegex(BotFault, 'Stow'): helper.restore()
        live.held_optic = False; helper.restore()
        self.assert_pose_close(live.head, helper.initial_head)
        for key, pose in original.items(): self.assert_pose_close(live.poses[key], pose)

    def test_missing_effective_fit_or_nonfinite_second_hand_cannot_mutate(self):
        live = Live(); live.bindings['settings'].pop()
        with self.assertRaises(BotFault): self.helper(live)
        self.assertFalse(live.mutations)
        live = Live(); helper = self.helper(live)
        grips = {h: copy.deepcopy(live.poses[h, 'grip']) for h in ('left', 'right')}
        grips['left']['orientation'] = [0., float('nan'), 0., 1.]
        with self.assertRaises(BotFault): helper._motion(live.head, grips, follow_head=False)
        self.assertFalse(live.mutations)


if __name__ == '__main__':
    unittest.main()
