"""Reject wrong stationary owners without requiring a vehicle ID for a mortar."""
import copy
import importlib.util
from pathlib import Path
import sys
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
spec = importlib.util.spec_from_file_location('mortar_case', Path(__file__).resolve().parents[1]/'tools/gameplay-mortar.py')
mortar = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mortar)


class MortarTests(unittest.TestCase):
    def test_downrange_aim_uses_one_axis_and_crosses_the_native_dead_zone(self):
        self.assertEqual(mortar.aim_adjustment(30., 8.)[:2], ('X', -.6))
        self.assertEqual(mortar.aim_adjustment(-30., 8.)[:2], ('X', .6))
        self.assertEqual(mortar.aim_adjustment(1., 8.)[:2], ('Y', -.6))
        self.assertEqual(mortar.aim_adjustment(1., -8.)[:2], ('Y', .6))
        self.assertIsNone(mortar.aim_adjustment(2.9, 1.9))
        for error in (3., 8., 180.):
            self.assertGreaterEqual(mortar.aim_adjustment(error, 0.)[2], .20)
            self.assertLessEqual(mortar.aim_adjustment(error, 0.)[2], .35)
        from gameplay_bot.core import BotFault
        for value in (float('nan'), float('inf'), True):
            with self.assertRaises(BotFault):mortar.aim_adjustment(value, 0.)

    def test_streamed_registry_id_binds_once_to_the_same_authored_fixture(self):
        from gameplay_bot.emplacement import bind_fixture, verify_fixture
        from gameplay_bot.core import BotFault
        policy = {'id': 41473, 'type': 81, 'registry_kind': 5, 'mission': 30010, 'location': 10}
        observed = {'id': 41475, 'type': 81, 'declared_type': 81,
                    'registry_kind': 5, 'lookup_status': True, 'broken': False}
        live = Mock(); live.identity = {'process': {'ProcessId': 47916}}
        anchor = (30010, 10, 3, 12345, 6)
        state = {'native': {'mission': 30010, 'location': 10}}
        with patch('gameplay_bot.emplacement.fresh_rig', return_value=(state, anchor)) as fresh, \
             patch('gameplay_bot.emplacement._observe_fixture', return_value=observed):
            bound = bind_fixture(live, policy)
        self.assertEqual(bound['id'], 41475)
        self.assertEqual(bound['registry_binding']['reviewed_id'], 41473)
        self.assertEqual(policy['id'], 41473)
        fresh.assert_called_with(live, anchor)
        self.assertEqual(verify_fixture(observed, bound), observed)
        with self.assertRaises(BotFault):
            verify_fixture({**observed, 'id': 41476}, bound)

    def test_rebinding_rejects_invalid_id_wrong_categories_or_broken_fixture(self):
        from gameplay_bot.emplacement import bind_fixture
        from gameplay_bot.core import BotFault
        policy = {'id': 41473, 'type': 81, 'registry_kind': 5, 'mission': 30010, 'location': 10}
        observed = {'id': 41475, 'type': 81, 'declared_type': 81,
                    'registry_kind': 5, 'lookup_status': True, 'broken': False}
        live = Mock(); live.identity = {'process': {'ProcessId': 47916}}
        state = {'native': {'mission': 30010, 'location': 10}}
        for key, value in (('id', True), ('id', 0), ('id', 65535), ('type', 82),
                           ('declared_type', 82), ('registry_kind', 6), ('lookup_status', False), ('broken', True)):
            with self.subTest(key=key, value=value), \
                 patch('gameplay_bot.emplacement.fresh_rig', return_value=(state, (30010,10,3,12345,6))), \
                 patch('gameplay_bot.emplacement._observe_fixture', return_value={**observed, key:value}), \
                 self.assertRaises(BotFault):
                bind_fixture(live, policy)

    def test_broken_fixture_cannot_admit_an_interaction_or_shot(self):
        from gameplay_bot.emplacement import verify_fixture
        from gameplay_bot.core import BotFault
        policy = {'id': 41473, 'type': 81, 'registry_kind': 5}
        intact = {'id': 41473, 'type': 81, 'declared_type': 81,
                  'registry_kind': 5, 'lookup_status': True, 'broken': False}
        self.assertEqual(verify_fixture(intact, policy), intact)
        for value in (True, None, 0):
            with self.subTest(broken=value), self.assertRaises(BotFault):
                verify_fixture({**intact, 'broken': value}, policy)

    def setUp(self):
        self.policy = {'mission': 30010, 'location': 10, 'goal': [88.75677, 333.17639, 98.45851]}
        self.state = {'scene': 'gameplay', 'menu': False,
            'controls': {'context': 'gameplay', 'rig_input': True},
            'native': {'mission': 30010, 'location': 10, 'player_x': 89.32298,
                'player_y': 334.01351, 'player_z': 98.25696,
                'status_NORMAL_ACTION': False, 'status_PARTS_ACTIVE': True, 'status_CARRY': False,
                'status_ON_VEHICLE': False, 'status_ON_HORSE': False, 'popup': False, 'saving': False,
                'player_vehicle_id': 65535, 'player_ridden_game_object_id': 65535}}

    def test_real_stationary_mortar_does_not_require_a_ridden_vehicle(self):
        self.assertTrue(mortar.stationary_mortar(self.state, self.policy))

    def test_ordinary_field_or_another_action_cannot_authorize_a_trigger(self):
        for key, value in (('status_NORMAL_ACTION', True), ('status_PARTS_ACTIVE', False),
                ('status_CARRY', True), ('status_ON_VEHICLE', True), ('status_ON_HORSE', True),
                ('mission', 10020), ('location', 20), ('popup', True), ('saving', True),
                ('player_x', 90_000), ('player_y', float('nan'))):
            state = copy.deepcopy(self.state)
            state['native'][key] = value
            with self.subTest(key=key):
                self.assertFalse(mortar.stationary_mortar(state, self.policy))
        for key, value in (('context', 'menus'), ('rig_input', False)):
            state = copy.deepcopy(self.state)
            state['controls'][key] = value
            with self.subTest(key=key):
                self.assertFalse(mortar.stationary_mortar(state, self.policy))
        state = copy.deepcopy(self.state)
        state['menu'] = True
        self.assertFalse(mortar.stationary_mortar(state, self.policy))


if __name__ == '__main__':
    unittest.main()
