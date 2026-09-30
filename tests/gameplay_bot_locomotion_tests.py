"""Deterministic contracts for bounded, native-feedback locomotion."""
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
from gameplay_bot.core import BotFault
from gameplay_bot.locomotion import move_local


class Clock:
    def __init__(self):
        self.now = 0.0

    def __call__(self):
        return self.now

    def sleep(self, duration):
        self.now += duration


class Events:
    def __init__(self):
        self.values = []

    def emit(self, event, **fields):
        self.values.append((event, fields))


class Live:
    def __init__(self, clock, *, movement=True, step=0.1, bad_context_at=None,
                 bad_gamepad=False, malformed_physical=False, dynamics=False,
                 release_latency=0.0):
        self.bindings = {"axes": [{"name": "axes.move", "source": "left_stick"}]}
        self.clock = clock
        self.events = Events()
        self.movement = movement
        self.step = step
        self.bad_context_at = bad_context_at
        self.bad_gamepad = bad_gamepad
        self.malformed_physical = malformed_physical
        self.dynamics = dynamics
        self.release_latency = release_latency
        self.held = False
        self.current_value = 0.0
        self.velocity = 0.0
        self.last_motion_time = clock.now
        self.releases = 0
        self.observe_count = 0
        self.position = [10.0, 20.0, 30.0]
        self.activation = 4
        self.input_values = None
        self.input_duration = None
        self.input_history = []
        self.leases = []

    def _advance_motion(self):
        now = self.clock.now
        elapsed = max(0.0, now - self.last_motion_time)
        self.last_motion_time = now
        if not self.movement:
            return
        if not self.dynamics:
            if self.held:
                self.position[0] += self.step * self.current_value / .30
            return
        remaining = elapsed
        while remaining > 0:
            dt = min(.01, remaining)
            remaining -= dt
            if self.held:
                desired_speed = self.current_value * 3.0
                self.velocity += (desired_speed - self.velocity) * min(1.0, dt / .25)
            else:
                self.velocity = max(0.0, self.velocity - 4.0 * dt)
            self.position[0] += self.velocity * dt

    def release(self):
        self.releases += 1
        if self.held and self.release_latency:
            self.clock.now += self.release_latency
            self._advance_motion()
        self.held = False
        self.current_value = 0.0
        self.last_motion_time = self.clock.now

    def input(self, values, duration, *, lease_seconds=None):
        self.input_values = values
        self.input_duration = duration
        value = values[0]["value"]
        self.input_history.append(value)
        self.leases.append(lease_seconds)
        self.clock.now += .02
        self._advance_motion()
        self.held = True
        self.current_value = value
        self.last_motion_time = self.clock.now

    def observe(self, native=False):
        self.observe_count += 1
        self._advance_motion()
        now_ms = 10000 + round(self.clock() * 1000)
        buttons = [0.0] * 11
        physical = {"buttons": buttons} if self.malformed_physical else buttons
        stick_index = 1 if self.bindings["axes"][0]["source"] == "left_stick" else 3
        sticks = [0.0, 0.0, 0.0, 0.0]
        sticks[stick_index] = self.current_value if self.held else 0.0
        context = "menus" if self.bad_context_at == self.observe_count else "gameplay"
        return {
            "scene": "gameplay",
            "camera_active": True,
            "camera_available": True,
            "menu": False,
            "idroid": False,
            "gamepad": self.bad_gamepad,
            "activation": self.activation,
            "now_ms": now_ms,
            "controls": {
                "context": context,
                "age_ms": 0,
                "sample_ms": now_ms,
                "rig_input": True,
                "travel_mode": 1,
                "physical": physical,
                "sticks": sticks,
                "native_buttons": 0,
            },
            "native": {
                "mission": 30010,
                "location": 10,
                "sequence": "Seq_Game_FreePlay",
                "title": False,
                "status_NORMAL_ACTION": True,
                "saving": False,
                "popup": False,
                "game_over": 0,
                "player_vehicle_id": 65535,
                "player_x": self.position[0],
                "player_y": self.position[1],
                "player_z": self.position[2],
            },
        }


class LocomotionTests(unittest.TestCase):
    def run_move(self, live, clock, **kwargs):
        return move_local(live, clock=clock, sleep=clock.sleep, **kwargs)

    def test_short_move_uses_effective_axis_and_measures_neutral_endpoint(self):
        clock = Clock()
        live = Live(clock, dynamics=True, release_latency=.05)
        result = self.run_move(live, clock, target_distance=.35)

        self.assertEqual(result["status"], "observed_displacement")
        self.assertEqual(result["input"]["axis"], "axes.move")
        self.assertEqual(result["input"]["source"], "left_stick")
        self.assertEqual(result["input"]["hand"], "left")
        self.assertEqual(result["input"]["direction"], "stick_up")
        self.assertEqual(result["input"]["cruise_value"], .30)
        self.assertEqual(result["input"]["brake_value"], .15)
        self.assertEqual(result["origin"], [10.0, 20.0, 30.0])
        self.assertGreaterEqual(result["distance_native_units"], 0.35)
        self.assertLessEqual(result["distance_native_units"], 0.75)
        self.assertTrue(result["target_reached"])
        self.assertGreaterEqual(result["input_samples_by_phase"]["cruise"], 1)
        self.assertGreaterEqual(result["input_samples_by_phase"]["braking"], 1)
        self.assertEqual(live.input_history, [.30, .15])
        self.assertTrue(all(.03 <= lease <= 5.0 for lease in live.leases))
        self.assertTrue(result["settled_after_release"])
        self.assertGreaterEqual(result["xr_sampled_input_count"], 2)
        self.assertTrue(result["held_audit_timeline"])
        self.assertIs(result["xinput_consumption_proven"], False)
        self.assertIs(result["route_or_collision_proof"], False)
        self.assertFalse(live.held)
        self.assertEqual(live.releases, 2)

    def test_right_stick_source_is_sampled_in_its_own_axis(self):
        clock = Clock()
        live = Live(clock, dynamics=True)
        live.bindings["axes"][0]["source"] = "right_stick"
        result = self.run_move(live, clock)

        self.assertEqual(result["input"]["source"], "right_stick")
        self.assertEqual(result["input"]["hand"], "right")
        samples = [sample for sample in result["held_audit_timeline"]
                   if sample.get("phase") in ("cruise", "braking")]
        self.assertTrue(samples)
        self.assertTrue(all(sample["xr_sampled_sticks"][3] > 0 for sample in samples))
        self.assertTrue(all(sample["xr_sampled_sticks"][1] == 0 for sample in samples))
        self.assertFalse(live.held)

    def test_braking_keeps_larger_short_target_inside_hard_limit(self):
        clock = Clock()
        live = Live(clock, dynamics=True, release_latency=.05)
        result = self.run_move(live, clock, target_distance=.55)

        self.assertGreaterEqual(result["distance_native_units"], .55)
        self.assertLessEqual(result["distance_native_units"], .75)
        self.assertTrue(result["settled_after_release"])
        self.assertEqual(result["input"]["brake_value"], .15)
        self.assertFalse(live.held)

    def test_position_crossing_cannot_credit_an_unobserved_braking_input(self):
        clock = Clock()
        live = Live(clock, step=.20)
        original_observe = live.observe
        def observe(native=False):
            state = original_observe(native=native)
            if live.current_value == .15:
                state["controls"]["sticks"][1] = .30
            return state
        live.observe = observe
        with self.assertRaisesRegex(BotFault, "each movement phase"):
            self.run_move(live, clock, target_distance=.25)
        self.assertFalse(live.held)

    def test_no_native_progress_releases_before_failing(self):
        clock = Clock()
        live = Live(clock, movement=False)

        with self.assertRaisesRegex(BotFault, "No native movement progress"):
            self.run_move(live, clock, target_distance=0.25, max_displacement=0.75,
                          max_seconds=1.0, progress_timeout=0.2)

        self.assertFalse(live.held)
        self.assertEqual(live.releases, 2)

    def test_field_identity_change_releases_input(self):
        clock = Clock()
        live = Live(clock, bad_context_at=3)

        with self.assertRaisesRegex(BotFault, "gameplay rig"):
            self.run_move(live, clock)

        self.assertFalse(live.held)
        self.assertEqual(live.releases, 2)

    def test_non_neutral_physical_gamepad_rejects_before_stick_input(self):
        clock = Clock()
        live = Live(clock, bad_gamepad=True)

        with self.assertRaisesRegex(BotFault, "physical XInput"):
            self.run_move(live, clock)

        self.assertIsNone(live.input_values)
        self.assertFalse(live.held)

    def test_malformed_physical_audit_rejects_before_input(self):
        clock = Clock()
        live = Live(clock, malformed_physical=True)

        with self.assertRaisesRegex(BotFault, "physical input audit"):
            self.run_move(live, clock)

        self.assertIsNone(live.input_values)
        self.assertFalse(live.held)

    def test_hard_displacement_limit_releases_input(self):
        clock = Clock()
        live = Live(clock, step=0.8)

        with self.assertRaisesRegex(BotFault, "exceeded the bounded movement envelope"):
            self.run_move(live, clock)

        self.assertFalse(live.held)
        self.assertEqual(live.releases, 2)


if __name__ == "__main__":
    unittest.main()
