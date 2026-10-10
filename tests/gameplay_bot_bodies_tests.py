"""Native carry ambiguity, body/guard gates and B-only bounded-input contracts."""
import copy
import json
import pathlib
import sys
import tempfile
import unittest
from contextlib import nullcontext
from unittest.mock import Mock, patch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from gameplay_bot.bodies import (
    carry_candidate, guarded_enemies, handle_body, native_body_script,
    pickup_binding, pickup_packet, pulse,
)
from gameplay_bot.core import BotFault
from gameplay_bot.navigation import NativeMap

ANCHOR = (30010, 10, 2, 99, 4)
ACTOR = {'name': 'sol_fixture_0000', 'id': 1166, 'life': '3',
         'position': [0, 0, 0], 'camp': 'fixture_cp'}
ROSTER = [{'name': ACTOR['name'], 'id': 1166, 'life': 3},
          {'name': 'sol_fixture_0001', 'id': 1167, 'life': 4}]
SNAPSHOT = {'mission': 30010, 'location': 10, 'normal_life': '0',
            'truncated': False, 'unavailable_loaded': 0,
            'soldiers': [ACTOR, {**ROSTER[1], 'position': [10, 0, 0], 'camp': 'fixture_cp'}]}
BINDINGS = {'actions': [{'name': 'gameplay.pickup_carry', 'contexts': ['gameplay'],
                        'bindings': [{'gesture': 'level', 'inputs': ['b'], 'milliseconds': 300}]}]}


def native(carry=True):
    return {'mission': 30010, 'location': 10, 'player_index': 0,
            'player_carry': carry, 'carried_constant': 21,
            'soldiers': [{'id': 1166, 'life': 3, 'action': 4 if carry else 0,
                          'status': 21 if carry else 0, 'carried': carry},
                         {'id': 1167, 'life': 4, 'action': 0, 'status': 0, 'carried': False}]}


def sampled(held=True, stamp=1001):
    physical = [0]*11; physical[1] = 1 if held else 0
    return {'now_ms': stamp, 'scene': 'gameplay', 'camera_active': True,
            'camera_available': True, 'menu': False, 'idroid': False, 'gamepad': False,
            'activation': 2, 'rendered': {'player_owner': 99, 'presentation_epoch': 4},
            'controls': {'context': 'gameplay', 'age_ms': 0, 'sample_ms': stamp,
                         'rig_input': True, 'travel_mode': 1, 'physical': physical,
                         'sticks': [0]*4, 'native_axes': [0]*4,
                         'native_buttons': 8192 if held else 0, 'native_triggers': [0, 0]}}


class BodiesTests(unittest.TestCase):
    def test_exact_native_roster_candidate_has_no_direct_carrier_link(self):
        result = carry_candidate(native(), ROSTER, ACTOR, ANCHOR)
        self.assertTrue(result['chosen_candidate_carried'])
        self.assertFalse(result['native_player_to_actor_link_verified'])
        self.assertEqual(result['matching_carried_ids'], [1166])
        data = native(); data['soldiers'][1].update(status=21, carried=True)
        self.assertFalse(carry_candidate(data, ROSTER, ACTOR, ANCHOR)['chosen_candidate_carried'])
        data = native(); data['player_carry'] = False
        self.assertFalse(carry_candidate(data, ROSTER, ACTOR, ANCHOR)['chosen_candidate_carried'])
        data = native(False); data['player_carry'] = True
        self.assertFalse(carry_candidate(data, ROSTER, ACTOR, ANCHOR)['chosen_candidate_carried'])

    def test_changed_world_life_id_or_incomplete_native_roster_cannot_authorize_carry(self):
        fixtures = []
        for key, value in [('mission', 1), ('player_index', True), ('player_carry', 1), ('carried_constant', None)]:
            data = native(); data[key] = value; fixtures.append(data)
        data = native(); data['soldiers'][0]['id'] = 123; fixtures.append(data)
        data = native(); data['soldiers'][0]['life'] = 0; fixtures.append(data)
        data = native(); data['soldiers'][0]['carried'] = 1; fixtures.append(data)
        data = native(); data['soldiers'][0]['status'] = []; fixtures.append(data)
        data = native(); data['soldiers'].pop(); fixtures.append(data)
        data = native(); data['soldiers'][1] = data['soldiers'][0]; fixtures.append(data)
        for data in fixtures:
            with self.subTest(data=data), self.assertRaises(BotFault):
                carry_candidate(data, ROSTER, ACTOR, ANCHOR)

    def test_guard_gate_covers_camp_and_45m_from_actual_player_or_body(self):
        chosen, roster = guarded_enemies(SNAPSHOT, ACTOR, 'fixture_cp', ANCHOR, [.5, 0, 0])
        self.assertEqual(chosen['id'], 1166); self.assertEqual(roster, ROSTER)
        for position, camp in [([46, 0, 0], 'fixture_cp'), ([45.2, 0, 0], 'elsewhere'),
                               ([-44.9, 0, 0], 'elsewhere')]:
            data = copy.deepcopy(SNAPSHOT)
            data['soldiers'].append({'name': 'sol_other_0002', 'id': 1214, 'life': '0',
                                     'position': position, 'camp': camp})
            with self.subTest(position=position), self.assertRaisesRegex(BotFault, 'active native guard'):
                guarded_enemies(data, ACTOR, 'fixture_cp', ANCHOR, [.5, 0, 0])
        data = copy.deepcopy(SNAPSHOT); data['soldiers'][0]['position'] = [60, 0, 0]
        data['soldiers'].append({'name': 'sol_other_0002', 'id': 1214, 'life': '0',
                                'position': [105, 0, 0], 'camp': 'elsewhere'})
        with self.assertRaisesRegex(BotFault, 'active native guard'):
            guarded_enemies(data, ACTOR, 'fixture_cp', ANCHOR, [0, 0, 0])

    def test_changed_exact_body_and_unknown_loaded_state_stop_before_action(self):
        for key, value in [('life', '0'), ('id', 1168), ('name', 'sol_changed'), ('camp', 'elsewhere')]:
            data = copy.deepcopy(SNAPSHOT); data['soldiers'][0][key] = value
            with self.subTest(key=key), self.assertRaises(BotFault):
                guarded_enemies(data, ACTOR, 'fixture_cp', ANCHOR, [.5, 0, 0])
        for key, value in [('truncated', True), ('unavailable_loaded', 1), ('mission', 1)]:
            data = {**SNAPSHOT, key: value}
            with self.subTest(key=key), self.assertRaises(BotFault):
                guarded_enemies(data, ACTOR, 'fixture_cp', ANCHOR, [.5, 0, 0])
        data = copy.deepcopy(SNAPSHOT); data['soldiers'][1]['life'] = '3'
        with self.assertRaisesRegex(BotFault, 'roster changed'):
            guarded_enemies(data, ACTOR, 'fixture_cp', ANCHOR, [.5, 0, 0], ROSTER)

    def test_only_plain_level_b_binding_is_admitted(self):
        self.assertEqual(pickup_binding(BINDINGS), ['b'])
        for change in ({'inputs': ['b', 'left_grip']}, {'gesture': 'tap'}, {'inputs': ['y']}):
            data = copy.deepcopy(BINDINGS); data['actions'][0]['bindings'][0].update(change)
            with self.assertRaises(BotFault): pickup_binding(data)

    def test_b_native_packet_has_no_grips_triggers_axes_or_context_leak(self):
        self.assertIsNotNone(pickup_packet(sampled(), after_sample_ms=1000))
        self.assertIsNone(pickup_packet(sampled(), after_sample_ms=1001))
        for key, value in [('native_buttons', 0x2001), ('native_triggers', [0, 255]),
                           ('native_axes', [1, 0, 0, 0]), ('context', 'binoculars')]:
            data = sampled(); data['controls'][key] = value
            with self.subTest(key=key), self.assertRaises(BotFault):
                pickup_packet(data, after_sample_ms=1000)
        data = sampled(); data['controls']['physical'][7] = 1
        with self.assertRaises(BotFault): pickup_packet(data, after_sample_ms=1000)
        data = sampled(); data['gamepad'] = True
        with self.assertRaises(BotFault): pickup_packet(data, after_sample_ms=1000)

    def test_native_script_is_bounded_exact_read_commands_only(self):
        source = native_body_script(ROSTER, ANCHOR)
        for command in ('GetLifeStatus', 'GetActionStatus', 'GetStatus'):
            self.assertIn("id='"+command+"'", source)
        for action in ('RequestCarryOff', 'SetPosition', 'SetStatus', 'Fulton', 'Warp', 'RequestCqc'):
            self.assertNotIn(action, source)
        for roster, anchor in [(ROSTER*9, ANCHOR), (ROSTER*2, ANCHOR),
                               (ROSTER, ('bad', 10, 2, 99, 4)), (ROSTER, (30010, 10))]:
            with self.assertRaises(BotFault): native_body_script(roster, anchor)

    def test_one_expiring_b_pulse_is_sampled_released_and_never_retried(self):
        live = Mock(); now = [0.]; stamp = [1000]
        def observe(**unused):
            stamp[0] += 1
            return sampled(stamp=stamp[0])
        live.observe.side_effect = observe
        with patch('gameplay_bot.bodies.body_state', return_value=(sampled(False, 1000), ANCHOR)):
            result = pulse(live, ANCHOR, .12, 'offline', clock=lambda: now[0],
                           sleep=lambda delay: now.__setitem__(0, now[0]+delay))
        live.input.assert_called_once_with([{'hand': 'right', 'component': 'B', 'value': 1.}],
                                           .12, lease_seconds=.12)
        self.assertTrue(result['sampled_packets']); self.assertEqual(live.release.call_count, 2)
        live = Mock(); live.observe.side_effect = BotFault('operator completion unknown')
        with patch('gameplay_bot.bodies.body_state', return_value=(sampled(False, 1000), ANCHOR)):
            with self.assertRaisesRegex(BotFault, 'unknown'):
                pulse(live, ANCHOR, .12, 'offline')
        self.assertEqual(live.input.call_count, 1); self.assertEqual(live.release.call_count, 2)

    def exercise(self, *, ambiguous_pickup=False, lose_carry=False, unsafe=False,
                 placement_root_offset=(0, 0, 0)):
        with tempfile.TemporaryDirectory() as directory:
            base = pathlib.Path(directory)
            live = Mock(); live.bindings = BINDINGS; live.events.output = base
            live.carry = False; live.player = [.5, 0, 0]; live.body = [0, 0, 0]
            live.text_result.return_value = {'pose': {'position': [0, 0, 0], 'orientation': [0, 0, 0, 1]}}
            live.capture.side_effect = lambda label: [str(base/(label+'-left.png')), str(base/(label+'-right.png'))]
            def state(*unused, **kwargs):
                return {'native': {'player_life': 7801, 'status_CARRY': live.carry,
                                   **{'player_'+axis: v for axis, v in zip('xyz', live.player)}},
                        'rendered': {'native_pose': {'position': [.5, 1.7, 0], 'orientation': [0, 0, 0, 1]}}}, ANCHOR
            def enemies(*unused):
                data = copy.deepcopy(SNAPSHOT); data['soldiers'][0]['position'] = live.body[:]
                if unsafe: data['soldiers'][1]['life'] = '0'
                return data
            def read(*unused):
                return native(live.carry), {'seconds': .01}
            live.native.read.side_effect = read
            def fake_pulse(adapter, anchor, seconds, label):
                if ambiguous_pickup and label == 'body_pickup_pulse':
                    raise BotFault('pickup completion unknown')
                live.carry = label == 'body_pickup_pulse'
                return {'label': label, 'seconds': seconds, 'experimental_duration': True}
            def walk(adapter, nav, goal, *, carry_reader=None, **kwargs):
                self.assertEqual(kwargs['mode'], 'walk')
                self.assertIsNotNone(carry_reader); self.assertTrue(carry_reader())
                if lose_carry:
                    live.carry = False
                    self.assertFalse(carry_reader())
                    return {'status': 'failed', 'error': 'carry lost'}
                live.player = [v+d for v, d in zip(goal, placement_root_offset)]
                live.body = live.player[:]
                self.assertTrue(carry_reader())
                return {'status': 'observed_native_route'}
            nav = NativeMap([{'source': 'owned-fixture', 'sha256': 'a'*64,
                              'groups': [{'id': 0, 'nodes': [[0, 0, 0], [3, 0, 0]],
                                          'edges': [{'from': 0, 'to': 1, 'flags': 0}]}]}])
            with patch('gameplay_bot.bodies.body_state', side_effect=state), \
                 patch('gameplay_bot.bodies.read_enemy_snapshot', side_effect=enemies), \
                 patch('gameplay_bot.bodies.preserved_tracking_poses', return_value=nullcontext()), \
                 patch('gameplay_bot.bodies.pulse', side_effect=fake_pulse) as input_pulse, \
                 patch('gameplay_bot.bodies.navigate', new=walk):
                result = handle_body(live, nav, actor=copy.deepcopy(ACTOR), camp='fixture_cp',
                                     enemy_script='owned_observer', output=base/'case',
                                     cover_goal=[3, 0, 0], anchor=ANCHOR)
                saved = json.loads((base/'case/result.json').read_text())
                self.assertEqual(saved['status'], result['status'])
                return result, input_pulse.call_count, live

    def test_full_qualified_case_uses_one_pickup_one_drop_and_measured_body_displacement(self):
        result, calls, live = self.exercise()
        self.assertEqual(result['status'], 'observed_native_roster_candidate_pickup_carry_drop')
        self.assertEqual(calls, 2); self.assertEqual(result['pickup_attempts'], 1)
        self.assertEqual(result['drop_attempts'], 1); self.assertEqual(result['body_position_displacement_m'], 3)
        self.assertFalse(result['native_player_to_actor_link_verified'])
        self.assertFalse(result['enemy_ai_concealment_proven']); self.assertEqual(len(result['captures']), 8)
        live.input.assert_not_called(); live.execute.assert_not_called()

    def test_ambiguous_pickup_and_lost_carry_never_replay_or_drop(self):
        for options in ({'ambiguous_pickup': True}, {'lose_carry': True}):
            result, calls, _ = self.exercise(**options)
            self.assertEqual(result['status'], 'failed'); self.assertEqual(calls, 1)
            self.assertEqual(result['pickup_attempts'], 1); self.assertEqual(result['drop_attempts'], 0)

    def test_floor_goal_uses_nav_xz_and_height_contract_not_body_pickup_3d_reach(self):
        result, calls, _ = self.exercise(placement_root_offset=(.95, 1., 0))
        self.assertEqual(result['status'], 'observed_native_roster_candidate_pickup_carry_drop')
        self.assertEqual(calls, 2)
        arrival = result['placement_arrival']
        self.assertGreater(arrival['distance_3d_m'], 1.25)
        self.assertAlmostEqual(arrival['xz_distance_m'], .95)
        self.assertEqual(arrival['root_minus_floor_y_m'], 1.)
        for offset in ((1.26, 0, 0), (0, 2.501, 0)):
            with self.subTest(offset=offset):
                result, calls, _ = self.exercise(placement_root_offset=offset)
                self.assertEqual(result['status'], 'failed')
                self.assertEqual(calls, 1); self.assertEqual(result['drop_attempts'], 0)

    def test_guard_appearing_before_pickup_dispatches_no_pulse(self):
        result, calls, _ = self.exercise(unsafe=True)
        self.assertEqual(result['status'], 'failed'); self.assertEqual(calls, 0)
        self.assertEqual(result['pickup_attempts'], 0)


if __name__ == '__main__':
    unittest.main()
