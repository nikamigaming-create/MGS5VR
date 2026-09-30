import importlib.util
import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "render_gameplay_lesson", ROOT / "tools" / "render-gameplay-lesson.py"
)
lesson = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(lesson)


def rpc(event, qpc, arguments=None):
    row = {"event": event, "tool": "set_controller_input", "qpc_100ns": qpc}
    if arguments is not None:
        row["arguments"] = arguments
    return row


def action_audit(qpc, requested, buttons=None, sticks=None, action="gameplay.stance"):
    return {
        "event": "action_input_audit",
        "action": action,
        "qpc_100ns": qpc,
        "sample_ms": qpc // 10000,
        "sampled_context": "gameplay",
        "requested_inputs": requested,
        "physical": {
            "buttons": buttons or [0] * 11,
            "sticks": sticks or [0, 0, 0, 0],
        },
    }


class AcknowledgedCueTests(unittest.TestCase):
    def test_cue_uses_delayed_press_and_release_acknowledgments(self):
        events = [
            {"event": "case_started", "case_id": "stance", "qpc_100ns": 100},
            {"event": "semantic_action", "action": "gameplay.stance", "label": "A",
             "binding": {"inputs": ["a"]}, "qpc_100ns": 110},
            rpc("rpc_started", 120, {"hand": "right", "component": "A", "value": 1}),
            rpc("rpc_completed", 200),
            action_audit(250, ["a"], buttons=[1] + [0] * 10),
            rpc("rpc_started", 500, {"hand": "right", "component": "A", "value": 0}),
            rpc("rpc_completed", 600),
            {"event": "case_finished", "result": {"id": "stance", "status": "observed_pass"},
             "qpc_100ns": 700},
        ]
        _, cues = lesson.compile_action_cues(events, "stance")
        cue = cues[0]
        self.assertEqual(cue["start_qpc_100ns"], 200)
        self.assertEqual(cue["end_qpc_100ns"], 600)
        self.assertEqual(cue["audit_event_qpc_100ns"], 250)
        self.assertEqual(cue["channel_intervals"][0]["press_request_qpc_100ns"], 120)
        self.assertEqual(cue["channel_intervals"][0]["release_request_qpc_100ns"], 500)

    def test_chord_starts_when_last_press_is_acknowledged(self):
        events = [
            {"event": "case_started", "case_id": "mode", "qpc_100ns": 100},
            {"event": "semantic_action", "action": "system.native_buttons", "label": "HOLD A + MENU",
             "binding": {"inputs": ["a", "menu"]}, "qpc_100ns": 110},
            rpc("rpc_started", 120, {"hand": "right", "component": "A", "value": 1}),
            rpc("rpc_completed", 200),
            rpc("rpc_started", 210, {"hand": "left", "component": "Menu", "value": 1}),
            rpc("rpc_completed", 320),
            action_audit(350, ["a", "menu"], buttons=[1, 0, 0, 0, 1] + [0] * 6,
                         action="system.native_buttons"),
            rpc("rpc_started", 500, {"hand": "right", "component": "A", "value": 0}),
            rpc("rpc_completed", 600),
            rpc("rpc_started", 610, {"hand": "left", "component": "Menu", "value": 0}),
            rpc("rpc_completed", 700),
            {"event": "case_finished", "result": {"id": "mode", "status": "observed_pass"},
             "qpc_100ns": 800},
        ]
        _, cues = lesson.compile_action_cues(events, "mode")
        cue = cues[0]
        self.assertEqual(cue["start_qpc_100ns"], 320)
        self.assertEqual(cue["end_qpc_100ns"], 600)
        self.assertEqual(
            [item["start_qpc_100ns"] for item in cue["channel_intervals"]],
            [200, 320],
        )

    def test_stick_direction_and_click_channels_are_supported(self):
        action = {
            "action": "gameplay.stance", "qpc_100ns": 10,
            "binding": {"inputs": ["right_stick_up", "left_stick_click"]},
        }
        audit = action_audit(350, ["right_stick_up", "left_stick_click"],
                             buttons=[0, 0, 0, 0, 0, 1] + [0] * 5,
                             sticks=[0, 0, 0, 1])
        events = [
            rpc("rpc_started", 20, {"hand": "right", "component": "Thumbstick",
                                    "sub_component": "Y", "value": 1}),
            rpc("rpc_completed", 100),
            rpc("rpc_started", 110, {"hand": "left", "component": "ThumbstickClick", "value": 1}),
            rpc("rpc_completed", 200),
            audit,
            rpc("rpc_started", 500, {"hand": "right", "component": "Thumbstick",
                                     "sub_component": "Y", "value": 0}),
            rpc("rpc_completed", 600),
            rpc("rpc_started", 610, {"hand": "left", "component": "ThumbstickClick", "value": 0}),
            rpc("rpc_completed", 700),
        ]
        cue = lesson.acknowledged_intervals(events, action, audit)
        self.assertEqual(cue["start_qpc_100ns"], 200)
        self.assertEqual(cue["channel_intervals"][0]["input"], "right_stick_up")

    def test_thumbrest_fails_with_a_specific_actionable_error(self):
        action = {
            "action": "gameplay.stance", "qpc_100ns": 10,
            "binding": {"inputs": ["left_thumbrest"]},
        }
        audit = action_audit(20, ["left_thumbrest"])
        with self.assertRaisesRegex(ValueError, "no thumb-rest controller channel"):
            lesson.acknowledged_intervals([], action, audit)

    def test_unrecognized_effective_input_fails_instead_of_disappearing(self):
        action = {
            "action": "gameplay.stance", "qpc_100ns": 10,
            "binding": {"inputs": ["mystery_button"]},
        }
        audit = action_audit(20, ["mystery_button"])
        with self.assertRaisesRegex(ValueError, "Unsupported effective input token 'mystery_button'"):
            lesson.acknowledged_intervals([], action, audit)

    def test_case_finish_must_match_case_id(self):
        events = [
            {"event": "case_started", "case_id": "wanted", "qpc_100ns": 10},
            {"event": "case_finished", "result": {"id": "unrelated"}, "qpc_100ns": 20},
            {"event": "case_finished", "result": {"id": "wanted"}, "qpc_100ns": 30},
        ]
        self.assertEqual(lesson.case_window(events, "wanted"), (0, 2))
        with self.assertRaisesRegex(ValueError, "whose case_id or result.id matches"):
            lesson.case_window(events[:2], "wanted")

    def test_source_is_cropped_to_render_fov_without_stretching(self):
        width, height, x, y = lesson.source_crop_rect(1920, 1080, 1.15)
        self.assertEqual((height, y), (1080, 0))
        self.assertAlmostEqual(width / height, 1.15, delta=.002)
        self.assertLess(abs((2 * x + width) - 1920), 3)


if __name__ == "__main__":
    unittest.main()
