"""Transport recovery boundary tests; no operator or game is launched."""
import base64
import io
import pathlib
import sys
import tempfile
import unittest
from unittest import mock

from PIL import Image

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
from gameplay_bot.operator_recovery import recover_capture_once
from gameplay_bot.core import BotFault
from gameplay_bot.live import Live as LiveAdapter
from gameplay_bot.presentation import PresentationGuard


class Events:
    def __init__(self):
        self.rows = []

    def emit(self, event, **fields):
        self.rows.append((event, fields))


class Operator:
    def __init__(self, *, fail_capture=False, focused=True):
        self.closed = False
        self.calls = []
        self.fail_capture = fail_capture
        self.focused = focused

    def diagnostic(self):
        return {"proxy_pid": 123, "exit_code": 0 if self.closed else None,
                "stderr_tail": ["fake diagnostic"]}

    def close(self):
        self.calls.append(("close",))
        self.closed = True

    def request(self, method, arguments, timeout):
        self.calls.append((method, arguments, timeout))
        return {"tools": [{"name": "openxr_capture_composited_image"}]}

    def call(self, tool, arguments, timeout):
        self.calls.append((tool, arguments, timeout))
        if tool == "openxr_get_session_info":
            state = "XR_SESSION_STATE_FOCUSED" if self.focused else "XR_SESSION_STATE_IDLE"
            return {"content": [{"type": "text", "text": '{"state":{"name":"'+state+'"}}'}]}
        if self.fail_capture:
            raise TimeoutError("second capture timeout")
        return {"content": [{"type": "image", "data": "fake image"}]}


class Kernel:
    result = 258

    def WaitForSingleObject(self, handle, timeout):
        return self.result


class Live:
    def __init__(self):
        self.operator = Operator()
        self.events = Events()
        self.held = {}
        self.transport_error = "original capture timeout"
        self.process_handle = 77
        self.kernel = Kernel()
        self.last_tick = 10
        self.ticks = iter((11, 12, 13))

    def observe(self):
        tick = next(self.ticks)
        self.last_tick = tick
        return {"now_ms": tick}


class RecoveryTests(unittest.TestCase):
    def invoke(self, live, factory, *, suffix="capture_composited_image", args=None):
        return recover_capture_once(live, suffix, args or {"eye": "right"},
                                    TimeoutError("original capture timeout"),
                                    make_operator=factory)

    def test_success_is_one_read_only_retry_and_requires_fresh_pair(self):
        live, replacement = Live(), Operator()
        old = live.operator
        result = self.invoke(live, lambda: replacement)
        self.assertTrue(old.closed)
        self.assertIs(live.operator, replacement)
        self.assertIsNone(live.transport_error)
        self.assertTrue(result.pair_restart_required)
        self.assertEqual(result.state["now_ms"], 13)
        self.assertEqual([x[0] for x in replacement.calls],
                         ["tools/list", "openxr_get_session_info", "openxr_capture_composited_image"])
        self.assertTrue(all(x[2] == 15. for x in replacement.calls))
        self.assertEqual(live.events.rows[0][1]["previous_proxy"]["stderr_tail"], ["fake diagnostic"])
        self.assertIsNone(live.events.rows[0][1]["previous_proxy"]["exit_code"])

    def test_action_timeout_is_never_replayed(self):
        live = Live()
        with self.assertRaisesRegex(TimeoutError, "original"):
            self.invoke(live, lambda: self.fail("must not create client"),
                        suffix="set_controller_input", args={"value": 1})
        self.assertFalse(live.operator.closed)
        self.assertFalse(getattr(live, "_capture_recovery_used", False))

    def test_held_capture_is_refused(self):
        live = Live()
        live.held = {"right_grip": 1}
        with self.assertRaises(TimeoutError):
            self.invoke(live, lambda: self.fail("must not create client"))
        self.assertFalse(live.operator.closed)

    def test_budget_is_not_reset_after_success(self):
        live = Live()
        self.invoke(live, Operator)
        with self.assertRaises(TimeoutError):
            self.invoke(live, lambda: self.fail("second reconnect forbidden"))

    def test_second_timeout_closes_replacement_and_stops(self):
        live, replacement = Live(), Operator(fail_capture=True)
        with self.assertRaisesRegex(TimeoutError, "second"):
            self.invoke(live, lambda: replacement)
        self.assertTrue(replacement.closed)
        self.assertEqual(live.transport_error, "second capture timeout")
        self.assertEqual(sum(x[0] == "openxr_capture_composited_image"
                             for x in replacement.calls), 1)
        with self.assertRaises(TimeoutError):
            self.invoke(live, lambda: self.fail("no second reconnect"))

    def test_original_game_exit_refuses_proxy_replacement(self):
        live = Live()
        live.kernel.result = 0
        with self.assertRaisesRegex(BotFault, "generation ended"):
            self.invoke(live, lambda: self.fail("no replacement in another game"))
        self.assertFalse(live.operator.closed)

    def test_native_tick_must_advance_after_handshake(self):
        live, replacement = Live(), Operator()
        live.ticks = iter((11, 11))
        with self.assertRaisesRegex(BotFault, "tick did not advance"):
            self.invoke(live, lambda: replacement)
        self.assertTrue(replacement.closed)
        self.assertFalse(any(x[0] == "openxr_capture_composited_image" for x in replacement.calls))

    def test_wrong_xr_owner_state_refuses_capture(self):
        live, replacement = Live(), Operator(focused=False)
        with self.assertRaisesRegex(BotFault, "focused XR session"):
            self.invoke(live, lambda: replacement)
        self.assertTrue(replacement.closed)
        self.assertFalse(any(x[0] == "openxr_capture_composited_image" for x in replacement.calls))

    def test_factory_failure_does_not_unpoison_live(self):
        live = Live()
        old = live.operator

        def fail():
            raise TimeoutError("fresh handshake failed")

        with self.assertRaisesRegex(TimeoutError, "handshake failed"):
            self.invoke(live, fail)
        self.assertTrue(old.closed)
        self.assertEqual(live.transport_error, "fresh handshake failed")


class ImageOperator(Operator):
    """Distinct synthetic compositor images, never a native acceptance fixture."""

    def __init__(self, *, timeout_eye=None, fail_capture_number=None, blank=False):
        super().__init__()
        self.timeout_eye = timeout_eye
        self.fail_capture_number = fail_capture_number
        self.capture_count = 0
        self.blank = blank

    def call(self, tool, arguments, timeout):
        if tool != "openxr_capture_composited_image":
            return super().call(tool, arguments, timeout)
        self.calls.append((tool, arguments, timeout))
        self.capture_count += 1
        if arguments["eye"] == self.timeout_eye or self.capture_count == self.fail_capture_number:
            raise TimeoutError("synthetic capture timeout")
        values = bytes([0] * 64 if self.blank else [32 + self.capture_count, 200] * 32)
        stream = io.BytesIO()
        Image.frombytes("L", (8, 8), values).save(stream, format="PNG")
        return {"content": [{"type": "image", "data": base64.b64encode(stream.getvalue()).decode()}]}


class LiveRecoveryIntegrationTests(unittest.TestCase):
    def fixture(self, *, timeout_eye="right", replacement=None):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        live = LiveAdapter.__new__(LiveAdapter)
        live.events = Events()
        live.events.output = pathlib.Path(temporary.name)
        live.operator = ImageOperator(timeout_eye=timeout_eye)
        live.held = {}
        live.bindings = {}
        live.transport_error = None
        live.process_handle = 77
        live.kernel = Kernel()
        live.last_tick = 10
        live.pose_recording = False
        live._capture_recovery_used = False
        replacement = replacement or ImageOperator()
        live._operator_factory = mock.Mock(return_value=replacement)

        def observe(native=False):
            live.last_tick += 1
            return {"now_ms": live.last_tick, "scene": "gameplay", "menu": False,
                    "camera_active": True, "native": {"player_x": 1., "player_y": 2., "player_z": 3.}}

        live.observe = mock.Mock(side_effect=observe)
        live.presentation_guard = mock.Mock(wraps=PresentationGuard())
        return live, replacement

    def test_right_recovery_retains_partial_metrics_and_admits_only_fresh_pair(self):
        live, replacement = self.fixture()
        old = live.operator
        accepted = live.capture("recovery")
        captures = [fields for event, fields in live.events.rows if event == "capture"]
        invalidated = next(fields for event, fields in live.events.rows if event == "capture_pair_invalidated")
        self.assertTrue(old.closed)
        self.assertEqual(len(captures), 4)
        self.assertEqual(len(invalidated["partial_captures"]), 2)
        self.assertTrue(all(pathlib.Path(path).is_file() for path in invalidated["partial_captures"]))
        self.assertTrue(captures[1]["recovered_partial"])
        self.assertGreater(captures[1]["metrics"]["luma_stddev"], .5)
        self.assertEqual(accepted, [capture["path"] for capture in captures[2:]])
        live.presentation_guard.admit.assert_called_once()
        self.assertEqual(live.presentation_guard.admit.call_args.args[0], accepted)
        self.assertTrue(set(accepted).isdisjoint(invalidated["partial_captures"]))
        self.assertEqual([args["eye"] for tool, args, _ in replacement.calls
                          if tool == "openxr_capture_composited_image"], ["right", "left", "right"])
        self.assertTrue(all(call[2] == 15. for call in replacement.calls))
        live._operator_factory.assert_called_once_with()

    def test_left_recovery_starts_a_fresh_pair_before_admission(self):
        live, replacement = self.fixture(timeout_eye="left")
        accepted = live.capture("left-recovery")
        captures = [fields for event, fields in live.events.rows if event == "capture"]
        self.assertEqual(len(captures), 3)
        self.assertTrue(captures[0]["recovered_partial"])
        self.assertEqual(accepted, [capture["path"] for capture in captures[1:]])
        self.assertEqual([args["eye"] for tool, args, _ in replacement.calls
                          if tool == "openxr_capture_composited_image"], ["left", "left", "right"])

    def test_timeout_in_required_fresh_pair_stops_without_second_reconnect(self):
        live, replacement = self.fixture(replacement=ImageOperator(fail_capture_number=2))
        with self.assertRaisesRegex(TimeoutError, "synthetic"):
            live.capture("second-fault")
        live._operator_factory.assert_called_once_with()
        live.presentation_guard.admit.assert_not_called()
        self.assertEqual(replacement.capture_count, 2)
        self.assertEqual(live.transport_error, "synthetic capture timeout")

    def test_recovery_does_not_bypass_presentation_guard_or_restart_its_deadline(self):
        live, _ = self.fixture()
        live.presentation_guard = mock.Mock()

        def reject(captures, state):
            self.clock = 3.
            return False

        self.clock = 0.
        live.presentation_guard.admit.side_effect = reject
        with mock.patch("gameplay_bot.live.time.monotonic", side_effect=lambda: self.clock):
            with self.assertRaisesRegex(BotFault, "Stale compositor"):
                live.capture("stale-fresh-pair")
        live.presentation_guard.admit.assert_called_once()
        self.assertEqual(sum(event == "capture_pair_restart" for event, _ in live.events.rows), 1)

    def test_blank_recovered_image_is_retained_and_rejected_before_pair_admission(self):
        from gameplay_bot.core import BlankCompositorFrame
        live, _ = self.fixture(replacement=ImageOperator(blank=True))
        with self.assertRaises(BlankCompositorFrame):
            live.capture("blank-recovery")
        recovered = next(fields for event, fields in live.events.rows
                         if event == "capture" and fields["recovered_partial"])
        self.assertTrue(pathlib.Path(recovered["path"]).is_file())
        self.assertEqual(recovered["metrics"]["maximum"], 0)
        live.presentation_guard.admit.assert_not_called()

    def test_old_new_fixture_without_factory_is_ineligible(self):
        live, _ = self.fixture()
        del live._operator_factory
        old = live.operator
        with self.assertRaises(TimeoutError):
            live.call("capture_composited_image", {"eye": "right"})
        self.assertFalse(old.closed)
        self.assertFalse(live._capture_recovery_used)

    def test_write_timeout_never_replays_or_reconnects_and_keeps_two_second_budget(self):
        live, _ = self.fixture()
        live.operator = mock.Mock()
        live.operator.call.side_effect = TimeoutError("input completion unknown")
        with self.assertRaises(TimeoutError):
            live.call("set_controller_input", {"value": 1})
        live.operator.call.assert_called_once_with("openxr_set_controller_input", {"value": 1}, timeout=2.)
        live._operator_factory.assert_not_called()
        self.assertFalse(live._capture_recovery_used)

    def test_held_timeout_does_not_close_or_reconnect_proxy(self):
        live, _ = self.fixture()
        live.held = {"grip": 1}
        old = live.operator
        with self.assertRaises(TimeoutError):
            live.call("capture_composited_image", {"eye": "right"})
        self.assertFalse(old.closed)
        live._operator_factory.assert_not_called()

    def test_injected_even_falsey_operator_never_receives_default_factory(self):
        class FalseyOperator(Operator):
            def __bool__(self):
                return False

        operator = FalseyOperator()
        kernel = mock.Mock()
        kernel.OpenProcess.return_value = 77
        with mock.patch("gameplay_bot.live.NativeReader"), \
                mock.patch("gameplay_bot.live.game_identity", return_value={"process": {"ProcessId": 77, "CreationDate": "fake"}}), \
                mock.patch("gameplay_bot.live.StartupEvidence"), \
                mock.patch("gameplay_bot.live.load_tool") as load, \
                mock.patch("ctypes.WinDLL", return_value=kernel, create=True):
            live = LiveAdapter("unused-proxy", ".", {}, Events(), operator=operator)
        self.assertIs(live.operator, operator)
        self.assertIsNone(live._operator_factory)
        load.assert_not_called()


if __name__ == "__main__":
    unittest.main()
