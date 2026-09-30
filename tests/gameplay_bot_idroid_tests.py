"""Acceptance accounting for handheld navigation; no simulated visual pass."""
import math
import pathlib
import sys
import unittest
from unittest.mock import Mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
from gameplay_bot.core import BotFault
from gameplay_bot.idroid import inspect_idroid, navigate_stationary


class Clock:
    now = 0.
    def __call__(self):
        return self.now
    def sleep(self, duration):
        self.now += duration


class Live:
    def __init__(self, clock, *, source="left_stick", fault=None):
        self.clock, self.fault = clock, fault
        self.bindings = {"axes": [{"name": "axes.menu", "source": source}]}
        self.events = Mock()
        self.held = None
        self.ever_held = False
        self.menu = True
        self.actions, self.inputs, self.captures = [], [], []
        self.serial = 1000
        self.releases = 0
        self.palm = {"position": [0.,0.,0.], "orientation": [0.,0.,0.,1.]}

    def observe(self, native=False):
        self.serial += 1
        sample = 1001 if self.ever_held and self.fault == "duplicate" else self.serial
        sticks = [0., 0., 0., 0.]
        if self.held:
            index = (0 if self.held["hand"] == "left" else 2) + (self.held["sub_component"] == "Y")
            sticks[index] = self.held["value"]
        if self.fault == "no_input":
            sticks = [0., 0., 0., 0.]
        if self.fault == "neutral_held" and self.ever_held and not self.held:
            sticks[0] = .65
        pos = .01 if self.held and self.fault == "moves" else 0.
        yaw = 1. if self.held and self.fault == "turns" else 0.
        context = "gameplay" if self.held and self.fault == "ownership" else "menus"
        return {"scene": "menu" if self.menu else "gameplay", "idroid": self.menu, "activation": 1,
            "rendered": {"sample_ms": sample, "activation": 1, "rig_sequence": 0 if self.fault == "frozen" else self.serial,
                         "right_palm_tracked": True, "right_palm": self.palm},
            "idroid_menu_input_ready": self.menu, "now_ms": sample,
            "native": {"player_x": pos, "player_y": 2., "player_z": 3., "player_yaw": yaw},
            "controls": {"context": context, "sample_ms": sample, "age_ms": 0, "sticks": sticks}}

    def input(self, values, duration, *, lease_seconds):
        self.held = values[0]
        self.ever_held = True
        self.inputs.append(values[0])

    def release(self):
        self.held = None
        self.releases += 1

    def execute(self, step):
        self.actions.append(step)
        if step.get("op") == "pose":
            self.palm = {key: step[key] for key in ("position", "orientation")}
        if step.get("name") == "menus.back" and self.fault != "exit":
            self.menu = False

    def capture(self, label):
        if self.held:
            raise AssertionError("Blocking capture with held input")
        self.captures.append(label)
        return [label+"-left.png", label+"-right.png"]


class Behavior:
    def __init__(self, live):
        self.live = live
    def wait_for(self, expected, timeout, label):
        state = self.live.observe(native=True)
        if any(state.get(key) != value for key, value in expected.items()):
            raise BotFault("Observed state did not reach " + label)
        return state


class IdroidAcceptanceTests(unittest.TestCase):
    def test_distinct_native_samples_from_both_sticks_with_neutral_exit(self):
        clock = Clock(); live = Live(clock)
        result = inspect_idroid(live, Behavior(live), clock=clock, sleep=clock.sleep)
        case = result["cases"][0]
        self.assertEqual(case["status"], "observed_pass")
        self.assertEqual(case["visual_acceptance"], "pending")
        self.assertEqual(case["headset_acceptance"], "not_run")
        self.assertFalse(live.held)
        self.assertFalse(live.menu)
        self.assertEqual({v["hand"] for v in live.inputs}, {"left", "right"})
        self.assertGreater(clock.now, 1.8)
        self.assertEqual(case["max_displacement"], 0.)
        self.assertGreater(len({tuple(p["orientation"]) for p in case["declared_poses"]}), 1)

    def test_remapped_menu_axis_is_respected(self):
        clock = Clock(); live = Live(clock, source="right_stick")
        navigate_stationary(live, {}, clock=clock, sleep=clock.sleep)
        self.assertEqual(live.inputs[0]["hand"], "right")
        self.assertEqual(live.inputs[1]["hand"], "left")

    def test_duplicate_input_sample_is_not_two_observations(self):
        clock = Clock(); live = Live(clock, fault="duplicate")
        with self.assertRaisesRegex(BotFault, "distinct"):
            navigate_stationary(live, {}, clock=clock, sleep=clock.sleep)
        self.assertFalse(live.held)

    def test_motion_turn_and_lost_ownership_fail_and_release(self):
        for fault in ("moves", "turns", "ownership", "no_input", "neutral_held"):
            with self.subTest(fault=fault):
                clock = Clock(); live = Live(clock, fault=fault)
                result = inspect_idroid(live, Behavior(live), clock=clock, sleep=clock.sleep)
                self.assertEqual(result["status"], "failed")
                self.assertFalse(live.held)
                self.assertGreater(live.releases, 1)

    def test_unknown_exit_never_guesses_confirm(self):
        clock = Clock(); live = Live(clock, fault="exit")
        result = inspect_idroid(live, Behavior(live), clock=clock, sleep=clock.sleep)
        self.assertEqual(result["status"], "failed")
        self.assertIn("exit_error", result["cases"][0])
        self.assertFalse(any(a.get("name") == "menus.confirm" for a in live.actions))

    def test_stationary_player_cannot_pass_a_frozen_native_hand(self):
        clock = Clock(); live = Live(clock, fault="frozen")
        result = inspect_idroid(live, Behavior(live), clock=clock, sleep=clock.sleep)
        case = result["cases"][0]
        self.assertEqual(case["stationary_navigation"], "observed_pass")
        self.assertEqual(case["native_palm_motion"], "failed")
        self.assertEqual(result["status"], "failed")
        self.assertFalse(live.held)

    def test_missing_position_is_not_stationary_proof(self):
        clock = Clock(); live = Live(clock)
        observe = live.observe
        def broken(native=False):
            state = observe(native=native)
            state["native"]["player_x"] = math.nan
            return state
        live.observe = broken
        result = inspect_idroid(live, Behavior(live), clock=clock, sleep=clock.sleep)
        self.assertEqual(result["status"], "failed")
        self.assertFalse(live.inputs)

    def test_one_missing_publication_reobserves_without_replaying_input(self):
        clock=Clock();live=Live(clock);observe=live.observe;missed=[]
        def intermittent(native=False):
            state=observe(native=native)
            if live.held and live.held['hand']=='right' and not missed:
                state['controls']=None;missed.append(True)
            return state
        live.observe=intermittent
        result={}
        navigate_stationary(live,result,clock=clock,sleep=clock.sleep)
        self.assertEqual(len(live.inputs),2)
        self.assertEqual(len(result['unavailable_control_samples']),1)
        self.assertFalse(live.held)
        self.assertEqual(result['max_displacement'],0.)

    def test_missing_control_sample_still_checks_native_player_motion(self):
        clock=Clock();live=Live(clock);observe=live.observe
        def missing_and_moving(native=False):
            state=observe(native=native)
            if live.held:
                state['controls']=None
                state['native']['player_x']=.01
            return state
        live.observe=missing_and_moving
        with self.assertRaisesRegex(BotFault,'moved Snake'):
            navigate_stationary(live,{},clock=clock,sleep=clock.sleep)
        self.assertFalse(live.held)

    def test_continuously_missing_held_samples_fail_in_bounded_time(self):
        clock=Clock();live=Live(clock);observe=live.observe
        def missing(native=False):
            state=observe(native=native)
            if live.held:state['controls']=None
            return state
        live.observe=missing
        with self.assertRaises(BotFault):
            navigate_stationary(live,{},clock=clock,sleep=clock.sleep)
        self.assertLess(clock.now,.4)
        self.assertEqual(len(live.inputs),1)
        self.assertFalse(live.held)


if __name__ == "__main__":
    unittest.main()
