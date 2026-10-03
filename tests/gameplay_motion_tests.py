"""Trajectory failure/cleanup tests; native visual acceptance is separate."""
import copy
import pathlib
import sys
import unittest
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
from gameplay_bot.core import BotFault
from gameplay_bot.motion import move
from gameplay_bot.session import preserved_head_pose, preserved_tracking_poses


class Clock:
    def __init__(self):
        self.now = 0.
    def __call__(self):
        return self.now
    def sleep(self, seconds):
        self.now += seconds


class Live:
    def __init__(self):
        self.held = {}
        self.events = mock.Mock()
        self.clock = Clock()
        self.pose = {"head": {"position": [0., 1.6, 0.], "orientation": [0., 0., 0., 1.]}}
        for side in ("left", "right"):
            for kind in ("grip", "aim"):
                self.pose[side, kind] = {"position": [-.3 if side == "left" else .3, .1, -.5],
                                         "orientation": [0., 0., 0., 1.]}
        self.state = {"scene": "gameplay", "loading": False, "title": False, "demo": False,
                      "camera_active": True, "camera_suspended": False, "now_ms": 100,
                      "rendered": {"sample_ms": 100, "rig_sequence": 42, "player_owner": 123,
                                   "activation": 2, "presentation_epoch": 3, "presentation_focused": True,
                                   **{"raw_"+side+"_"+kind+"_tracked": True
                                      for side in ("left", "right") for kind in ("grip", "aim")}}}
        self.mutations = []
        self.release = mock.Mock()
    def text_result(self, value):
        return value
    def observe(self, native=False):
        return copy.deepcopy(self.state)
    def call(self, tool, args):
        key = "head" if "head" in tool else (args["hand"], args["pose_type"])
        if tool.startswith("set_"):
            self.pose[key] = {k: args[k] for k in ("position", "orientation")}
            self.mutations.append((self.clock(), tool, copy.deepcopy(args)))
        return {"pose": copy.deepcopy(self.pose[key]),
                "flags": {"position_valid": True, "orientation_valid": True}}


def trajectory():
    return {"op": "motion", "seconds": 1., "hands": [
        {"hand": "right", "position": [.15, -.2, -.35], "orientation": [.5, 0, 0, .8660254],
         "aim_position": [.15, -.19, -.34], "aim_orientation": [0, 0, 0, 1]}]}


class MotionTests(unittest.TestCase):
    def test_complete_path_contains_observed_intermediate_paired_poses(self):
        live = Live()
        move(live, trajectory(), clock=live.clock, sleep=live.clock.sleep)
        samples = [c.kwargs for c in live.events.emit.call_args_list if c.args == ("continuous_motion_sample",)]
        self.assertGreaterEqual(len(samples), 20)
        self.assertEqual(samples[0]["fraction"], 0)
        self.assertEqual(samples[-1]["fraction"], 1)
        self.assertTrue(any(.25 < s["fraction"] < .75 for s in samples))
        for grip, aim in zip(live.mutations[::2], live.mutations[1::2]):
            self.assertEqual(grip[2]["pose_type"], "grip")
            self.assertEqual(aim[2]["pose_type"], "aim")
            self.assertEqual(grip[0], aim[0])
        self.assertNotEqual(live.pose["right", "grip"]["orientation"], live.pose["right", "aim"]["orientation"])

    def test_invalid_second_pose_never_dispatches_a_partial_path(self):
        for key, value in (("aim_orientation", [0, 0, 0, 0]), ("aim_position", [0, float("nan"), 0])):
            live, step = Live(), trajectory()
            step["hands"][0][key] = value
            with self.assertRaises(BotFault):
                move(live, step, clock=live.clock, sleep=live.clock.sleep)
            self.assertFalse(live.mutations)

    def test_unknown_raw_tracking_focus_stale_or_owner_cannot_move(self):
        for key, value in (("presentation_focused", False), ("raw_right_aim_tracked", False),
                           ("rig_sequence", 0), ("player_owner", None), ("sample_ms", -500)):
            live = Live()
            live.state["rendered"][key] = value
            with self.assertRaises(BotFault):
                move(live, trajectory(), clock=live.clock, sleep=live.clock.sleep)
            self.assertFalse(live.mutations)

    def test_generation_change_aborts_without_replaying(self):
        live = Live()
        call = live.call
        def changed(tool, args):
            result = call(tool, args)
            if tool.startswith("set_"):
                live.state["rendered"]["presentation_epoch"] += 1
            return result
        live.call = changed
        with self.assertRaisesRegex(BotFault, "generation changed"):
            move(live, trajectory(), clock=live.clock, sleep=live.clock.sleep)
        self.assertEqual(len(live.mutations), 2)

    def test_blocked_rpc_is_not_credited_as_continuous_motion(self):
        live = Live()
        call = live.call
        def slow(tool, args):
            result = call(tool, args)
            if tool.startswith("set_"):
                live.clock.sleep(.6)
            return result
        live.call = slow
        with self.assertRaisesRegex(BotFault, "stalled"):
            move(live, trajectory(), clock=live.clock, sleep=live.clock.sleep)
        self.assertEqual(len(live.mutations), 2)

    def test_independent_head_motion_keeps_both_hands_in_local_space(self):
        live = Live()
        saved = copy.deepcopy(live.pose)
        move(live, {"op": "motion", "seconds": .3, "head": {
            "offset": [.08, 0, 0], "orientation": [0, .13052619, 0, .99144486]}},
             clock=live.clock, sleep=live.clock.sleep)
        for key in saved:
            if key != "head":
                self.assertEqual(saved[key], live.pose[key])
        self.assertEqual(live.pose["head"]["position"], [.08, 1.6, 0])
        self.assertTrue(all(c[2]["base_space"] == "local" for c in live.mutations))

    def test_duration_offset_and_held_input_are_rejected_before_mutation(self):
        for seconds in (0, 11, True, float("inf")):
            live, step = Live(), trajectory()
            step["seconds"] = seconds
            with self.assertRaises(BotFault):
                move(live, step)
            self.assertFalse(live.mutations)
        live = Live()
        live.held = {("right", "Grip", None): 1}
        with self.assertRaises(BotFault):
            move(live, trajectory())
        self.assertFalse(live.mutations)

    def test_head_pose_is_verified_after_failed_trajectory_cleanup(self):
        live = Live()
        saved = copy.deepcopy(live.pose["head"])
        with self.assertRaisesRegex(RuntimeError, "trajectory failed"):
            with preserved_head_pose(live):
                live.call("set_head_pose", {"base_space": "local", "position": [.1, 1.6, 0],
                                            "orientation": [0, 0, 0, 1]})
                raise RuntimeError("trajectory failed")
        self.assertEqual(live.pose["head"], saved)
        live.release.assert_called_once()

    def test_cleanup_restores_head_before_local_hands_when_operator_drags_them(self):
        live = Live()
        starting = copy.deepcopy(live.pose)
        call = live.call
        def head_moves_hands(tool, args):
            if tool == "set_head_pose":
                shift = [a-b for a,b in zip(args["position"], live.pose["head"]["position"])]
                for key, pose in live.pose.items():
                    if key != "head":
                        pose["position"] = [a+b for a,b in zip(pose["position"], shift)]
            return call(tool, args)
        live.call = head_moves_hands
        with self.assertRaises(RuntimeError):
            with preserved_tracking_poses(live):
                live.call("set_head_pose", {"base_space": "local", "position": [.2, 1.6, 0],
                                            "orientation": [0, 0, 0, 1]})
                raise RuntimeError("failed halfway through head motion")
        self.assertEqual(starting, live.pose)


if __name__ == "__main__":
    unittest.main()
