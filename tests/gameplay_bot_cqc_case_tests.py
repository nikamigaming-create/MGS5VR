"""Current-camp identity, danger and action handoff contracts; no live operators."""
import copy
import pathlib
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from gameplay_bot.cqc_case import camp_roster, placement_node, run_camp_cqc_body, select_guard
from gameplay_bot.core import BotFault
from gameplay_bot.navigation import NativeMap

ANCHOR = (30010, 10, 2, 99, 4)
CAMP = 'fixture_cp'
ACTOR = {'name': 'sol_fixture_0000', 'id': 1166, 'life': '0',
         'position': [1., 0., 0.], 'camp': CAMP}
BODY = {'name': 'sol_fixture_0001', 'id': 1167, 'life': '3',
        'position': [.5, 0., 0.], 'camp': CAMP}


def snapshot(*rows):
    return {'mission': 30010, 'location': 10, 'normal_life': '0', 'truncated': False,
            'unavailable_loaded': 0, 'soldiers': copy.deepcopy(list(rows or (ACTOR, BODY)))}


def nav_map():
    return NativeMap([{'source': 'owned-synthetic-fixture', 'sha256': 'a'*64,
        'groups': [{'id': 0, 'nodes': [[0, 0, 0], [1, 0, 0], [4, 0, 0], [5, 0, 0]],
                    'edges': [{'from': a, 'to': b, 'flags': 0}
                              for a, b in ((0, 1), (1, 0), (1, 2), (2, 1), (2, 3), (3, 2))]}]}])


def state():
    return {'native': {'player_x': .5, 'player_y': 0., 'player_z': 0.,
                      'player_life': 7801, 'player_injury_count': 0,
                      'status_INJURY_LOWER': False, 'status_INJURY_UPPER': False,
                      'status_CURE': False, 'not_alert': True}}


class CampCqcTests(unittest.TestCase):
    def test_selects_exact_normal_guard_while_nearer_sleeping_body_is_not_cqc(self):
        self.assertEqual(select_guard(snapshot(), CAMP, ANCHOR, [0, 0, 0])['id'], 1166)
        self.assertIsNone(select_guard(snapshot(BODY), CAMP, ANCHOR, [0, 0, 0]))

    def test_wrong_world_incomplete_duplicate_and_untyped_life_fail(self):
        bad = []
        for key, value in [('mission', 9), ('truncated', True), ('unavailable_loaded', 1), ('normal_life', None)]:
            data = snapshot(); data[key] = value; bad.append(data)
        data = snapshot(); data['soldiers'].append(copy.deepcopy(ACTOR)); bad.append(data)
        data = snapshot(); data['soldiers'][0]['life'] = True; bad.append(data)
        data = snapshot(); data['soldiers'][0]['position'] = [float('nan'), 0, 0]; bad.append(data)
        for data in bad:
            with self.subTest(data=data), self.assertRaises(BotFault):
                camp_roster(data, CAMP, ANCHOR)

    def test_other_camp_normal_guard_still_disqualifies_close_isolation(self):
        nearby = {**ACTOR, 'name': 'sol_other_0000', 'id': 1196, 'camp': 'other_cp', 'position': [2, 0, 0]}
        self.assertIsNone(select_guard(snapshot(ACTOR, BODY, nearby), CAMP, ANCHOR, [0, 0, 0]))
        nearby['position'][1] = 2.
        self.assertEqual(select_guard(snapshot(ACTOR, BODY, nearby), CAMP, ANCHOR, [0, 0, 0])['id'], 1166)

    def test_far_guard_is_not_selected_and_order_is_deterministic(self):
        far = {**ACTOR, 'position': [46, 0, 0]}
        self.assertIsNone(select_guard(snapshot(far, BODY), CAMP, ANCHOR, [0, 0, 0]))
        other = {**ACTOR, 'name': 'sol_fixture_0002', 'id': 1168, 'position': [-1, 0, 3]}
        self.assertEqual(select_guard(snapshot(other, ACTOR), CAMP, ANCHOR, [0, 0, 0])['id'], 1166)

    def test_placement_is_real_directed_connected_node_and_never_claims_cover(self):
        nav = nav_map()
        node, goal = placement_node(nav, [1, 0, 0])
        self.assertEqual(node, 2); self.assertEqual(goal, [4, 0, 0])
        self.assertEqual(nav.path(1, node), [1, 2])
        nav.adj[1].pop(2)
        with self.assertRaisesRegex(BotFault, 'No connected'):
            placement_node(nav, [1, 0, 0])

    def run_case(self, snapshots, cqc=None, body=None, *, current=None, error=None):
        with tempfile.TemporaryDirectory() as tmp:
            live = Mock(); live.events.output = pathlib.Path(tmp)
            patches = [patch('gameplay_bot.cqc_case.admit', side_effect=error,
                             return_value=(current or state(), ANCHOR)),
                       patch('gameplay_bot.cqc_case.read_enemy_snapshot', side_effect=snapshots),
                       patch('gameplay_bot.cqc_case.set_posture'),
                       patch('gameplay_bot.cqc_case.attempt_cqc', return_value=cqc),
                       patch('gameplay_bot.cqc_case.handle_body', return_value=body),
                       patch('gameplay_bot.cqc_case.navigate', return_value={'status': 'observed_native_route'})]
            with patches[0] as admission, patches[1], patches[2] as posture, patches[3] as attempt, patches[4] as carry, patches[5] as route:
                result = run_camp_cqc_body(live, nav_map(), camp=CAMP, enemy_script='owned',
                                           output=pathlib.Path(tmp)/'case', anchor=ANCHOR)
                return result, live, attempt, carry, posture, route, admission

    def test_cqc_released_outcome_hands_exact_new_body_to_existing_handler(self):
        after = snapshot({**ACTOR, 'life': '4'}, BODY)
        action = {'status': 'observed_exact_actor_incapacitation_during_vr_cqc_input',
                  'ordinary_field_resumed': True, 'target_id': 1166, 'target_life_after': 4,
                  'pulses': [{'index': 1}]}
        result, live, attempt, carry, posture, route, _ = self.run_case(
            [snapshot(), snapshot(), after], action,
            {'status': 'observed_native_roster_candidate_pickup_carry_drop'})
        self.assertEqual(result['status'], 'observed_exact_actor_incapacitation_during_cqc_input_and_body_displacement')
        self.assertEqual(carry.call_args.kwargs['actor']['id'], 1166)
        self.assertEqual(attempt.call_args.kwargs['pulses'], 5)
        self.assertEqual(attempt.call_args.kwargs['pulse_seconds'], .20)
        posture.assert_called_once(); route.assert_not_called()
        self.assertFalse(result['enemy_ai_concealment_proven'])
        self.assertFalse(result['native_player_to_actor_link_verified'])
        live.release.assert_called()

    def test_unresolved_native_restraint_blocks_body_without_extra_input(self):
        result, _, attempt, carry, _, _, _ = self.run_case([snapshot(), snapshot()],
            {'status': 'native_cqc_release_pending', 'ordinary_field_resumed': False})
        self.assertEqual(result['status'], 'failed'); self.assertIn('release', result['error'])
        attempt.assert_called_once(); carry.assert_not_called()

    def test_cqc_miss_and_remaining_normal_guard_cannot_fall_through_to_nearer_sleeping_body(self):
        result, _, _, carry, _, _, _ = self.run_case([snapshot(), snapshot(), snapshot()],
            {'status': 'no_native_cqc_outcome_observed', 'ordinary_field_resumed': True})
        self.assertEqual(result['status'], 'failed'); self.assertIn('active native guard', result['error'])
        carry.assert_not_called()

    def test_all_sleeping_actual_camp_can_test_body_without_fabricated_cqc(self):
        data = snapshot({**ACTOR, 'life': '3'}, BODY)
        result, _, attempt, carry, posture, route, _ = self.run_case([data, data], body={
            'status': 'observed_native_roster_candidate_pickup_carry_drop'})
        self.assertEqual(result['status'], 'observed_current_camp_body_displacement')
        self.assertIn('cqc_unavailable', result)
        attempt.assert_not_called(); posture.assert_not_called(); route.assert_not_called()
        carry.assert_called_once()

    def test_pickup_drop_without_displacement_cannot_become_full_body_success(self):
        data = snapshot(BODY)
        result, _, _, _, _, _, _ = self.run_case([data, data], body={
            'status': 'observed_pickup_drop_without_proven_body_displacement'})
        self.assertEqual(result['status'], 'observed_pickup_drop_without_proven_body_displacement')

    def test_detection_or_owner_change_dispatches_no_cqc_or_body(self):
        current = state(); current['native']['not_alert'] = False
        for kwargs in ({'current': current}, {'error': BotFault('Current native owner changed')}):
            with self.subTest(kwargs=kwargs):
                result, live, attempt, carry, _, route, _ = self.run_case([], **kwargs)
                self.assertEqual(result['status'], 'failed')
                attempt.assert_not_called(); carry.assert_not_called(); route.assert_not_called()
                live.release.assert_called()


if __name__ == '__main__':
    unittest.main()
