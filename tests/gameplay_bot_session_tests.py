"""Focused startup/session pose lifecycle contracts."""
import importlib.util
import json
import pathlib
import sys
import unittest
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
from gameplay_bot.core import BotFault
from gameplay_bot.session import NORMAL_VIEW_HAND_POSES, preserved_controller_pose, run_suite


class Adapter:
    def __init__(self):
        self.bindings = {}
        self.pose_recording = False
        self.calls = []

    def release(self):
        self.calls.append(("release",))

    def execute(self, step):
        self.calls.append(("execute", step))

    def pose_snapshot(self, label):
        self.calls.append(("pose_snapshot", label))


class Events:
    def __init__(self, calls):
        self.calls = calls

    def emit(self, event, **fields):
        self.calls.append(("event", event, fields))


class Behavior:
    def __init__(self):
        self.adapter = Adapter()
        self.events = Events(self.adapter.calls)
        self.case_calls = []

    def case(self, case):
        self.case_calls.append(case["id"])
        self.adapter.calls.append(("case", case["id"]))
        return {"id": case["id"], "status": "observed_pass"}


class PoseAdapter:
    def __init__(self):
        self.pose = {"position": [-.02, -.3, -.38],
                     "orientation": [-.7071, .7071, 0., 0.]}
        self.calls = []
        self.owned = True
        self.valid = True

    @staticmethod
    def text_result(result):
        return result

    def call(self, tool, arguments):
        if not self.owned:
            raise BotFault("original game generation no longer owned")
        self.calls.append((tool, dict(arguments)))
        if tool == "get_controller_pose":
            return {"pose": {"position": list(self.pose["position"]),
                              "orientation": list(self.pose["orientation"])},
                    "flags": {"position_valid": self.valid, "orientation_valid": self.valid}}
        if tool == "set_controller_pose":
            self.pose = {"position": list(arguments["position"]),
                         "orientation": list(arguments["orientation"])}

    def release(self):
        self.calls.append(("release",))


class SessionPoseTests(unittest.TestCase):
    @staticmethod
    def continue_module():
        path = pathlib.Path(__file__).resolve().parents[1] / "tools/gameplay-bot.py"
        spec = importlib.util.spec_from_file_location("gameplay_bot_cli_test", path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    def suite(self):
        return {"cases": [{"id": "first", "before": {"scene": "gameplay"},
                           "steps": [{"op": "action", "name": "test.action"}],
                           "after": {"scene": "gameplay"}}]}

    def arrival(self, continued):
        return {"status": "observed_arrival", "continued_from_title": continued}

    def test_title_continue_resets_both_hands_before_first_case(self):
        behavior = Behavior()
        result = run_suite(behavior, self.suite(), lambda _: None,
                           lambda: self.arrival(True))

        self.assertEqual(result["status"], "observed_pass")
        self.assertEqual(result["hand_pose_reset"]["status"], "applied")
        calls = behavior.adapter.calls
        self.assertEqual(calls[0], ("release",))
        self.assertEqual(calls[1:3], [("execute", pose) for pose in NORMAL_VIEW_HAND_POSES])
        self.assertEqual(calls[3], ("pose_snapshot", "post-continue-normal-hands"))
        self.assertEqual(calls[4][0:2], ("event", "post_continue_hand_pose_reset"))
        self.assertEqual(calls[5], ("case", "first"))

    def test_warm_session_preserves_existing_hand_poses(self):
        behavior = Behavior()
        run_suite(behavior, self.suite(), lambda _: None,
                  lambda: self.arrival(False))

        self.assertEqual(behavior.adapter.calls, [("case", "first")])

    def test_missing_title_continue_metadata_preserves_existing_hand_poses(self):
        behavior = Behavior()
        arrival = {"status": "observed_arrival"}
        run_suite(behavior, self.suite(), lambda _: None, lambda: arrival)

        self.assertEqual(behavior.adapter.calls, [("case", "first")])

    def test_restored_continue_grip_is_not_overwritten_by_post_continue_defaults(self):
        behavior = Behavior()
        saved = {"hand": "right", "pose_type": "grip", "base_space": "local",
                 "position": [-.02, -.3, -.38], "orientation": [-.7071, .7071, 0., 0.]}
        result = run_suite(behavior, self.suite(), lambda _: None,
                           lambda: {**self.arrival(True), "controller_pose_restored": True,
                                    "controller_pose_restore": saved})
        self.assertEqual(result["hand_pose_reset"], {"status": "preserved", "pose": saved})
        self.assertEqual(behavior.adapter.calls, [("event", "post_continue_hand_pose_preserved",
                                                   {"status": "preserved", "pose": saved}),
                                                  ("case", "first")])

    def test_temporary_controller_pose_restores_exact_readback_on_success(self):
        adapter = PoseAdapter()
        saved = dict(hand="right", pose_type="grip", base_space="local",
                     position=[-.02, -.3, -.38], orientation=[-.7071, .7071, 0., 0.])
        with preserved_controller_pose(adapter) as pose:
            self.assertEqual(pose, saved)
            adapter.call("set_controller_pose", {**saved, "position": [-.12, -.58, -1.35],
                                                   "orientation": [0., 0., 0., 1.]})
        self.assertEqual(adapter.pose, {"position": saved["position"], "orientation": saved["orientation"]})
        self.assertEqual(adapter.calls[-2][0], "set_controller_pose")
        self.assertEqual(adapter.calls[-1][0], "get_controller_pose")
        self.assertEqual(adapter.calls[-2][1], saved)
        self.assertEqual(adapter.calls[-1][1], {"hand": "right", "pose_type": "grip", "base_space": "local"})
        self.assertEqual(adapter.calls[0][0], "get_controller_pose")
        self.assertEqual(adapter.calls[1], ("set_controller_pose", {**saved, "position": [-.12, -.58, -1.35],
                                                                   "orientation": [0., 0., 0., 1.]}))
        self.assertIn(("release",), adapter.calls)

    def test_temporary_controller_pose_restores_after_handoff_failure(self):
        adapter = PoseAdapter()
        original = {"position": list(adapter.pose["position"]), "orientation": list(adapter.pose["orientation"])}
        with self.assertRaisesRegex(BotFault, "title ownership changed"):
            with preserved_controller_pose(adapter):
                adapter.call("set_controller_pose", {"hand": "right", "pose_type": "grip", "base_space": "local",
                                                     "position": [-.12, -.58, -1.35],
                                                     "orientation": [0., 0., 0., 1.]})
                raise BotFault("title ownership changed before Continue")
        self.assertEqual(adapter.pose, original)

    def test_temporary_controller_pose_fails_closed_without_valid_prior_pose(self):
        adapter = PoseAdapter()
        adapter.valid = False
        entered = []
        with self.assertRaisesRegex(BotFault, "Cannot safely preserve"):
            with preserved_controller_pose(adapter):
                entered.append(True)
        self.assertEqual(entered, [])
        self.assertEqual(adapter.calls, [("get_controller_pose", {"hand": "right", "pose_type": "grip",
                                                                   "base_space": "local"})])

    def test_pose_restore_respects_original_process_ownership(self):
        adapter = PoseAdapter()
        with self.assertRaisesRegex(BotFault, "Could not restore saved.*original game generation"):
            with preserved_controller_pose(adapter):
                adapter.pose = {"position": [-.12, -.58, -1.35], "orientation": [0., 0., 0., 1.]}
                adapter.owned = False
        self.assertEqual(adapter.calls[-1], ("release",))

    def test_continue_game_restores_the_saved_grip_before_arrival_capture(self):
        cli = self.continue_module()
        live = PoseAdapter()
        live.scene = "title"
        live.captures = []
        live.inputs = []
        live.events = Events(live.calls)
        original_capture = live.call

        def call(tool, arguments):
            if tool == "get_controller_pose":
                return original_capture(tool, arguments)
            return original_capture(tool, arguments)
        live.call = call
        def observe(native=False):
            state = {"scene": live.scene, "title_menu": live.scene == "title",
                     "loading": False, "cabin": False}
            if native:
                state["native"] = {"title": live.scene == "title", "mission": 30010}
            return state
        live.observe = observe
        def capture(label):
            live.captures.append((label, live.pose["position"][:]))
            return [label + "-left.png", label + "-right.png"]
        live.capture = capture
        live.input = lambda *args: live.inputs.append(args)

        class BehaviorStub:
            def wait_for(self, predicate, *_args):
                if predicate == {"title": False}:
                    live.scene = "gameplay"

        with mock.patch.object(cli, "advance_startup", return_value={"scene": "title"}), \
             mock.patch.object(cli, "wait_for_continue_rack", return_value={
                 "scene": "title", "title_menu": True, "opening_position": [0., 0., 0.],
                 "opening_orientation": [0., 0., 0., 1.]}):
            result = cli.continue_game(live, BehaviorStub())

        self.assertEqual(result["status"], "observed_arrival")
        self.assertTrue(result["controller_pose_restored"])
        self.assertEqual(result["controller_pose_restore"]["position"], [-.02, -.3, -.38])
        self.assertEqual(live.pose["position"], [-.02, -.3, -.38])
        arrival_capture = next(pose for label, pose in live.captures if label == "gameplay-arrival")
        self.assertEqual(arrival_capture, [-.02, -.3, -.38])
        self.assertEqual(len(live.inputs), 1)

    def test_continue_game_restores_grip_when_title_capture_fails(self):
        cli = self.continue_module()
        live = PoseAdapter()
        live.scene = "title"
        live.events = Events(live.calls)
        def capture(label):
            if label == "continue-hover":
                raise BotFault("capture failed")
            return []
        live.capture = capture
        with mock.patch.object(cli, "advance_startup", return_value={"scene": "title"}), \
             mock.patch.object(cli, "wait_for_continue_rack", return_value={
                 "scene": "title", "title_menu": True, "opening_position": [0., 0., 0.],
                 "opening_orientation": [0., 0., 0., 1.]}):
            with self.assertRaisesRegex(BotFault, "capture failed"):
                cli.continue_game(live, mock.Mock())
        self.assertEqual(live.pose["position"], [-.02, -.3, -.38])
        self.assertEqual(live.pose["orientation"], [-.7071, .7071, 0., 0.])

    def test_non_session_suite_does_not_reset_warm_poses(self):
        behavior = Behavior()
        run_suite(behavior, self.suite(), lambda _: None)

        self.assertEqual(behavior.adapter.calls, [("case", "first")])


if __name__ == "__main__":
    unittest.main()
