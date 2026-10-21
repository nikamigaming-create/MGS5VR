"""CQC refuses ambiguous/armed input and never retries an uncertain VR pulse."""
import copy
import pathlib
import sys
import tempfile
import unittest
from contextlib import nullcontext
from unittest.mock import Mock, patch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from gameplay_bot.cqc import (
    attempt_cqc, classify, cqc_binding, cqc_packet, exact_close_target,
    native_cqc_script, validate_native, wait_released, _checkpoint, _close_position, _facing, run_named_cqc, cqc_state,
)
from gameplay_bot.core import BotFault


ANCHOR = (30010, 10, 2, 99, 4)
ACTOR = {'name': 'sol_fixture_0000', 'id': 1166}
BINDINGS = {'actions': [{'name': 'gameplay.fire_or_cqc', 'contexts': ['gameplay', 'commands'],
    'bindings': [{'gesture': 'level', 'inputs': ['right_trigger'], 'milliseconds': 0}]}]}


class NamedCqcTests(unittest.TestCase):
    def test_explicit_target_keeps_existing_executor_and_bounds(self):
        live=Mock();policy={'target':ACTOR['name'],'native':{'mission':30010},'pulses':5,'pulse_seconds':.18}
        with patch('gameplay_bot.cqc.fresh_rig',return_value=({'native':{'mission':30010}},ANCHOR)), \
             patch('gameplay_bot.cqc.read_enemy_snapshot',return_value={'soldiers':[ACTOR]}), \
             patch('gameplay_bot.cqc.attempt_cqc',return_value={'status':'observed'}) as execute:
            self.assertEqual(run_named_cqc(live,policy,enemy_script='roster',output='out'),{'status':'observed'})
        execute.assert_called_once_with(live,actor=ACTOR,anchor=ANCHOR,enemy_script='roster',
                                        output='out',pulses=5,pulse_seconds=.18,contextual=False)

    def test_changed_world_or_missing_exact_target_never_dispatches(self):
        for native,roster in (({'mission':10020},[ACTOR]),({'mission':30010},[]),
                              ({'mission':30010},[{'name':'sol_other','id':2}]),
                              ({'mission':30010},[ACTOR,ACTOR])):
            with self.subTest(native=native,roster=roster):
                with patch('gameplay_bot.cqc.fresh_rig',return_value=({'native':native},ANCHOR)), \
                     patch('gameplay_bot.cqc.read_enemy_snapshot',return_value={'soldiers':roster}), \
                     patch('gameplay_bot.cqc.attempt_cqc') as execute:
                    with self.assertRaises(BotFault):
                        run_named_cqc(Mock(),{'target':ACTOR['name'],'native':{'mission':30010}},
                                      enemy_script='roster',output='out')
                    execute.assert_not_called()

    def test_unreviewed_or_incomplete_policy_is_rejected_before_live_reads(self):
        for policy in ({},{'target':ACTOR['name']},{'target':'hos_child','native':{'mission':30010}},
                       {'target':ACTOR['name'],'native':{'mission':30010},'warp':True}):
            with patch('gameplay_bot.cqc.fresh_rig') as read:
                with self.assertRaises(BotFault):run_named_cqc(Mock(),policy,enemy_script='roster',output='out')
                read.assert_not_called()


def native(life=0, hold=False, *, action='Idle', status='Normal'):
    return {'mission': 30010, 'location': 10, 'player_index': 0,
            'cqc_hold': hold, 'cqc_continuous': False, 'id': 1166,
            'life': life, 'action': action, 'status': status, 'position': [0, 0, 1]}


def state(held=False, stamp=1000):
    physical = [0]*11; physical[10] = int(held)
    return {'now_ms': stamp, 'scene': 'gameplay', 'camera_active': True,
            'camera_available': True, 'menu': False, 'idroid': False, 'gamepad': False,
            'activation': 2, 'rendered': {'player_owner': 99, 'presentation_epoch': 4,
                'head_pose': {'position': [0, 0, 0], 'orientation': [0, 0, 0, 1]},
                'sample_ms': stamp, 'raw_right_grip_tracked': True, 'raw_right_aim_tracked': True,
                'native_pose': {'position': [0, 1.5, 0], 'orientation': [0, 0, 0, 1]}},
            'native': {'player_x': 0, 'player_y': 0, 'player_z': 0,
                'status_NORMAL_ACTION': True, 'status_CQC_HOLD': False, 'status_CQC_CONTINUOUS': False,
                **{'ammo_in_weapon_'+k: 10 for k in ('PRIMARY_HIP', 'PRIMARY_BACK', 'SECONDARY')}},
            'controls': {'context': 'gameplay', 'age_ms': 0, 'sample_ms': stamp,
                'rig_input': True, 'travel_mode': 1, 'physical': physical,
                'sticks': [0]*4, 'native_axes': [0]*4, 'native_buttons': 0,
                'native_triggers': [0, 255 if held else 0]}}


def enemies():
    return {'mission': 30010, 'location': 10, 'normal_life': '0', 'truncated': False,
            'unavailable_loaded': 0, 'soldiers': [{**ACTOR, 'life': '0', 'position': [0, 0, 1]}]}


class CqcTests(unittest.TestCase):
    def test_native_action_is_observable_but_never_admits_trigger_packet(self):
        fixture = state()
        fixture['native'].update(player_vehicle_id=65535, popup=False, saving=False,
            demo=False, demo_nonplayable=False, game_over=0, title=False,
            status_CARRY=False, status_CURE=False, status_NORMAL_ACTION=False)
        fixture['rendered']['binocular_held'] = False
        fixture['controls']['travel_mode'] = 4
        with patch('gameplay_bot.cqc.fresh_rig', return_value=(fixture, ANCHOR)):
            self.assertEqual(cqc_state(Mock(), ANCHOR, transition=True)[0], fixture)
            with self.assertRaises(BotFault): cqc_state(Mock(), ANCHOR)
            for mode in (0, 2, 3):
                fixture['controls']['travel_mode'] = mode
                with self.assertRaises(BotFault): cqc_state(Mock(), ANCHOR, transition=True)
        held = state(True, 1001)
        held['controls']['travel_mode'] = 4
        with self.assertRaises(BotFault): cqc_packet(held, after_sample_ms=1000)

    def test_effective_binding_rejects_ready_or_command_chords(self):
        self.assertEqual(cqc_binding(BINDINGS), ['right_trigger'])
        for change in ({'inputs': ['right_grip', 'right_trigger']}, {'gesture': 'hold'}, {'inputs': ['x']}):
            fixture = copy.deepcopy(BINDINGS); fixture['actions'][0]['bindings'][0].update(change)
            with self.assertRaises(BotFault): cqc_binding(fixture)

    def test_packet_refuses_firearm_ready_grips_axes_and_other_inputs(self):
        self.assertIsNotNone(cqc_packet(state(True, 1001), after_sample_ms=1000))
        self.assertIsNone(cqc_packet(state(True, 1001), after_sample_ms=1001))
        for key, value in [('native_triggers', [255, 255]), ('native_buttons', 0x8000),
                           ('native_axes', [0, 1, 0, 0]), ('context', 'commands')]:
            fixture = state(True, 1001); fixture['controls'][key] = value
            with self.subTest(key=key), self.assertRaises(BotFault):
                cqc_packet(fixture, after_sample_ms=1000)
        fixture = state(True, 1001); fixture['controls']['physical'][8] = 1
        with self.assertRaises(BotFault): cqc_packet(fixture, after_sample_ms=1000)

    def test_admission_requires_actual_close_unique_live_guard(self):
        self.assertEqual(exact_close_target(enemies(), ACTOR, ANCHOR, [0, 0, 0])['id'], 1166)
        for change in ({'life': '3'}, {'position': [0, 0, 1.751]}, {'position': [0, 1.501, 1]}):
            fixture = enemies(); fixture['soldiers'][0].update(change)
            with self.subTest(change=change), self.assertRaises(BotFault):
                exact_close_target(fixture, ACTOR, ANCHOR, [0, 0, 0])
        fixture = enemies(); fixture['soldiers'].append({'name': 'sol_fixture_0001', 'id': 1167,
            'life': '0', 'position': [0, 0, 2.5]})
        with self.assertRaisesRegex(BotFault, 'ambiguous'):
            exact_close_target(fixture, ACTOR, ANCHOR, [0, 0, 0])
        fixture['soldiers'][1]['life'] = '3'
        self.assertEqual(exact_close_target(fixture, ACTOR, ANCHOR, [0, 0, 0])['id'], 1166)

    def test_incomplete_duplicate_changed_world_rosters_refused(self):
        for change in ({'truncated': True}, {'unavailable_loaded': 1}, {'mission': 10020}):
            fixture = enemies(); fixture.update(change)
            with self.assertRaises(BotFault): exact_close_target(fixture, ACTOR, ANCHOR, [0, 0, 0])
        fixture = enemies(); fixture['soldiers'] *= 2
        with self.assertRaises(BotFault): exact_close_target(fixture, ACTOR, ANCHOR, [0, 0, 0])

    def test_script_is_one_typed_actor_read_only_with_exact_registered_cqc(self):
        source = native_cqc_script(ACTOR, ANCHOR)
        for command in ('GetPosition', 'GetLifeStatus', 'GetActionStatus', 'GetStatus'):
            self.assertIn("id='"+command+"'", source)
        self.assertIn('PlayerStatus.CQC_HOLD==87', source)
        self.assertIn('PlayerStatus.CQC_CONTINUOUS==88', source)
        for mutation in ('RequestCqc', 'SetPosition', 'SetStatus', 'Warp', 'RequestCarryOff'):
            self.assertNotIn(mutation, source)
        with self.assertRaises(BotFault): native_cqc_script({'name': "sol_foo';Warp()", 'id': 1166}, ANCHOR)

    def test_native_actor_player_status_and_finite_point_are_required(self):
        self.assertEqual(validate_native(native(), ACTOR, ANCHOR)['id'], 1166)
        for key, value in [('mission', 1), ('id', 1), ('player_index', True),
                           ('cqc_hold', 1), ('position', [0, float('nan'), 1])]:
            fixture = native(); fixture[key] = value
            with self.subTest(key=key), self.assertRaises(BotFault): validate_native(fixture, ACTOR, ANCHOR)
        with self.assertRaises(BotFault): validate_native(native(), ACTOR, ANCHOR, player_index=1)

    def test_outcome_does_not_invent_target_link_cause_or_choke(self):
        held = native(hold=True, action='UnknownNativeAction', status=29)
        result = classify(native(), [held], native(), ammo_unchanged=True)
        self.assertEqual(result['status'], 'observed_native_cqc_and_exact_actor_state_change')
        self.assertTrue(result['restraint_released_observed'])
        for key in ('native_player_to_actor_link_verified', 'buddy_attribution_excluded',
                    'choke_throw_knife_or_physical_hand_contact_proven'):
            self.assertFalse(result[key])
        result = classify(native(), [native()], native(life=4), ammo_unchanged=True)
        self.assertEqual(result['status'], 'observed_exact_actor_incapacitation_during_vr_cqc_input')
        self.assertFalse(result['native_cqc_state_observed'])
        self.assertEqual(classify(native(), [], native(life=3), ammo_unchanged=True)['status'],
                         'exact_actor_changed_before_cqc_dispatch')
        with self.assertRaisesRegex(BotFault, 'ammunition'):
            classify(native(), [held], native(life=4), ammo_unchanged=False)

    def test_fresh_facing_and_actor_range_checked_before_dispatch(self):
        self.assertEqual(_facing(state()['rendered']['native_pose'], [0, 0, 1]), 0)
        for point in ([0, 0, -1], [3, 0, 1]):
            with self.assertRaises(BotFault): _facing(state()['rendered']['native_pose'], point)
        with self.assertRaises(BotFault): _close_position([0, 0, 1.8], [0, 0, 0])

    def test_recording_checkpoint_never_claims_final_compositor(self):
        live = Mock(); live.held_action_recording = Mock()
        self.assertEqual(_checkpoint(live, 'cqc', state()), [])
        live.capture.assert_not_called()
        self.assertFalse(live.events.emit.call_args.kwargs['final_compositor_captured'])

    def test_release_waits_for_native_restraint_and_ordinary_animation_without_input(self):
        live = Mock(); now = [0.]; count = [0]
        def transient(*unused, **kwargs):
            count[0] += 1
            current = state()
            current['native']['status_NORMAL_ACTION'] = count[0] >= 6
            return current, ANCHOR
        live.native.read.return_value = (native(), {})
        with patch('gameplay_bot.cqc.cqc_state', side_effect=transient):
            result = wait_released(live, 'read-only', ACTOR, ANCHOR, 0,
                clock=lambda: now[0], sleep=lambda delay: now.__setitem__(0, now[0]+delay))
        self.assertTrue(result['released']); self.assertEqual(result['observations'], 3)
        live.input.assert_not_called(); live.execute.assert_not_called()

    def test_persistent_native_hold_stays_pending_without_guessed_release_or_replay(self):
        live = Mock(); now = [0.]; busy = state()
        busy['native'].update(status_NORMAL_ACTION=False, status_CQC_HOLD=True)
        live.native.read.return_value = (native(hold=True), {})
        with patch('gameplay_bot.cqc.cqc_state', return_value=(busy, ANCHOR)):
            result = wait_released(live, 'read-only', ACTOR, ANCHOR, 0,
                clock=lambda: now[0], sleep=lambda delay: now.__setitem__(0, now[0]+delay))
        self.assertFalse(result['released']); self.assertEqual(result['seconds'], 1.5)
        live.input.assert_not_called(); live.execute.assert_not_called()

    def exercise(self, *, unknown=False, miss_packet=False, actor_moved=False):
        with tempfile.TemporaryDirectory() as directory:
            live = Mock(); live.bindings = BINDINGS; live.held_action_recording = Mock()
            live.text_result.return_value = {'pose': {'position': [0, 0, 0], 'orientation': [0, 0, 0, 1]}}
            live.observe.side_effect = BotFault('operator completion unknown') if unknown else None
            live.observe.return_value = state(not miss_packet, 1001)
            before_dispatch = native(); before_dispatch['position'] = [0, 0, 1.8] if actor_moved else [0, 0, 1]
            live.native.read.side_effect = [(native(), {}), (before_dispatch, {}), (before_dispatch, {}),
                (native(hold=True, action='CqcRaw'), {}), (native(life=4), {})]
            now = [0.]
            with patch('gameplay_bot.cqc.cqc_state', return_value=(state(), ANCHOR)), \
                 patch('gameplay_bot.cqc.read_enemy_snapshot', return_value=enemies()), \
                 patch('gameplay_bot.cqc.preserved_tracking_poses', return_value=nullcontext()):
                result = attempt_cqc(live, actor=ACTOR, anchor=ANCHOR, enemy_script='owned',
                    output=directory, clock=lambda: now[0],
                    sleep=lambda delay: now.__setitem__(0, now[0]+delay))
            return result, live

    def test_single_expiring_vr_hold_observes_qualified_native_outcome(self):
        result, live = self.exercise()
        self.assertEqual(result['status'], 'observed_exact_actor_incapacitation_during_vr_cqc_input')
        self.assertEqual(len(result['pulses']), 1)
        live.input.assert_called_once_with([{'hand': 'right', 'component': 'Trigger', 'value': 1.}],
                                           .65, lease_seconds=.65)
        self.assertTrue(result['pulses'][0]['experimental_duration'])
        self.assertGreaterEqual(live.release.call_count, 3)
        self.assertFalse(result['native_player_to_actor_link_verified'])

    def test_unknown_unsampled_or_moved_target_never_retry(self):
        for options in ({'unknown': True}, {'miss_packet': True}, {'actor_moved': True}):
            result, live = self.exercise(**options)
            self.assertEqual(result['status'], 'failed')
            self.assertEqual(live.input.call_count, 0 if options.get('actor_moved') else 1)
            self.assertGreaterEqual(live.release.call_count, 2)


if __name__ == '__main__': unittest.main()
