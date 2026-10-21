"""Acceptance accounting must reject identity swaps and non-reloads."""
import copy
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
from gameplay_bot.showcase import reload_outcome, FAMILIES
from gameplay_bot.arsenal import discharge, SLOTS


class ShowcaseTests(unittest.TestCase):
    def setUp(self):
        self.before = {s: {'custom_or_equipped_id': i+1, 'ammo': 10, 'sub_ammo': 0}
                       for i, s in enumerate(SLOTS)}

    def test_reload_needs_increase_in_selected_magazine(self):
        self.assertEqual(reload_outcome(self.before, self.before, 'SECONDARY')['status'], 'unproven')
        after = copy.deepcopy(self.before); after['PRIMARY_HIP']['ammo'] += 2
        self.assertEqual(reload_outcome(self.before, after, 'SECONDARY')['status'], 'unproven')
        after['SECONDARY']['ammo'] += 3
        self.assertEqual(reload_outcome(self.before, after, 'SECONDARY')['magazine_increase'], 3)

    def test_changed_weapon_is_not_a_reload(self):
        after = copy.deepcopy(self.before)
        after['SECONDARY'].update(custom_or_equipped_id=42, ammo=30)
        self.assertEqual(reload_outcome(self.before, after, 'SECONDARY')['status'], 'fail')

    def test_fixture_ammo_reset_is_not_firing(self):
        after = copy.deepcopy(self.before); after['SECONDARY']['ammo'] += 1
        self.assertEqual(discharge(self.before, after, 'SECONDARY')['status'], 'unknown')

    def test_different_slot_cannot_pass(self):
        after = copy.deepcopy(self.before); after['PRIMARY_HIP']['ammo'] -= 1
        self.assertEqual(discharge(self.before, after, 'SECONDARY')['status'], 'fail')

    def test_eight_native_families_are_present(self):
        self.assertEqual(len(set(FAMILIES)), 8)

    def test_grenade_secondary_magazine_is_independent(self):
        after = copy.deepcopy(self.before)
        after['PRIMARY_HIP']['sub_ammo'] = 1
        self.assertEqual(reload_outcome(self.before, after, 'PRIMARY_HIP')['status'], 'unproven')
        self.assertEqual(reload_outcome(self.before, after, 'PRIMARY_HIP', 'sub_ammo')['status'], 'observed_pass')
        self.assertEqual(discharge(after, self.before, 'PRIMARY_HIP', 'sub_ammo')['rounds'], 1)


if __name__ == '__main__':
    unittest.main()
