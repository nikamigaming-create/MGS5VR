"""Stale final-eye failures reproduced with actual PNG bytes and native state changes."""
import pathlib
import sys
import tempfile
import unittest
from unittest.mock import Mock

from PIL import Image
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
from gameplay_bot.core import BotFault
from gameplay_bot.live import Live
from gameplay_bot.presentation import PresentationGuard


class PresentationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = pathlib.Path(self.temporary.name)
        self.now = 0.
        self.guard = PresentationGuard(clock=lambda: self.now)
        self.state = dict(title=False, loading=False, demo=False, menu=False,
                          idroid=False, camera_active=True, activation=1,
                          rendered={"binocular_held": False})
        self.before = self.pair("before", (40, 40))
        self.guard.admit(self.before, self.state)

    def pair(self, label, values):
        paths = []
        for eye, value in zip(("left", "right"), values):
            path = self.root / (label + "-" + eye + ".png")
            Image.new("RGB", (8, 8), (value, value, value)).save(path)
            paths.append(path)
        return paths

    def test_unchanged_stationary_scene_can_repeat(self):
        self.assertTrue(self.guard.admit(self.before, self.state))

    def test_stationary_gameplay_cannot_freeze_indefinitely(self):
        self.now = 3.
        self.assertFalse(self.guard.admit(self.before, self.state))

    def test_one_eye_freezing_during_gameplay_is_rejected(self):
        self.now = 3.
        self.assertFalse(self.guard.admit(self.pair("one-frozen", (100, 40)), self.state))

    def test_static_pause_can_remain_visible(self):
        paused = {**self.state, "menu": True}
        pictures = self.pair("pause", (100, 100))
        self.assertTrue(self.guard.admit(pictures, paused))
        self.now = 20.
        self.assertTrue(self.guard.admit(pictures, paused))

    def test_left_forearm_motion_requires_new_pixels(self):
        palm = {"position": [0., 0., 0.], "orientation": [0., 0., 0., 1.]}
        state = {**self.state, "rendered": {**self.state["rendered"], "left_palm": palm}}
        self.assertTrue(self.guard.admit(self.before, state))
        moved = {**state, "rendered": {"left_palm": {**palm, "position": [.1, 0., 0.]}}}
        self.assertFalse(self.guard.admit(self.before, moved))

    def test_native_menu_open_cannot_pass_on_frozen_pixels(self):
        changed = {**self.state, "menu": True, "idroid": True}
        self.assertFalse(self.guard.admit(self.before, changed))
        self.assertTrue(self.guard.admit(self.pair("menu", (100, 100)), changed))

    def test_one_stale_eye_stops_stereo_acceptance(self):
        self.assertFalse(self.guard.admit(self.pair("partial", (100, 40)), {**self.state, "menu": True}))

    def test_movement_cannot_pass_on_a_frozen_world(self):
        state = {**self.state, "native": {"player_x": 1., "player_y": 2., "player_z": 3.}}
        self.assertTrue(self.guard.admit(self.before, state))
        moved = {**state, "native": {**state["native"], "player_x": 2.}}
        self.assertFalse(self.guard.admit(self.before, moved))

    def test_equipped_binoculars_cannot_pass_on_stale_pixels(self):
        state = {**self.state, "rendered": {"binocular_held": False}}
        self.assertTrue(self.guard.admit(self.before, state))
        self.assertFalse(self.guard.admit(self.before, {**state, "rendered": {"binocular_held": True}}))

    def test_png_reencoding_does_not_make_stale_pixels_fresh(self):
        from PIL.PngImagePlugin import PngInfo
        metadata = PngInfo();metadata.add_text("frame_counter", "99999")
        reencoded = self.root / "metadata.png"
        Image.open(self.before[0]).save(reencoded, pnginfo=metadata)
        self.assertFalse(self.guard.admit([reencoded, self.before[1]], {**self.state, "menu": True}))

    def test_idroid_hand_motion_requires_visible_motion(self):
        palm = {"position": [0., 0., 0.], "orientation": [0., 0., 0., 1.]}
        state = {**self.state, "menu": True, "idroid": True, "rendered": {"right_palm": palm}}
        pictures = self.pair("idroid", (100, 100))
        self.assertTrue(self.guard.admit(pictures, state))
        moved = {**state, "rendered": {"right_palm": {**palm, "position": [.1, 0., 0.]}}}
        self.assertFalse(self.guard.admit(pictures, moved))

    def test_cleanup_does_not_close_preexisting_menu(self):
        live = Live.__new__(Live);live.opened_menu = None;live.execute = Mock();live.observe = Mock()
        live.cleanup_menus()
        live.execute.assert_not_called();live.observe.assert_not_called()

    def test_cleanup_closes_owned_menu_with_back_only(self):
        live = Live.__new__(Live);live.opened_menu = "pause";live.release = Mock();live.events = Mock()
        live.observe = Mock(side_effect=[{"menu": True, "idroid": False,
              "native": {"popup": False, "tutorial_pause": False}}, {"menu": False}])
        live.execute = Mock()
        live.cleanup_menus()
        self.assertEqual(live.execute.call_args.args[0]["name"], "menus.back")
        self.assertIsNone(live.opened_menu)

    def test_cleanup_does_not_confirm_unknown_popup(self):
        live = Live.__new__(Live);live.opened_menu = "idroid";live.release = Mock();live.execute = Mock()
        live.observe = Mock(return_value={"menu": True, "idroid": True,
                                         "native": {"popup": True, "tutorial_pause": True}})
        with self.assertRaises(BotFault):
            live.cleanup_menus()
        live.execute.assert_not_called()


if __name__ == "__main__":
    unittest.main()
