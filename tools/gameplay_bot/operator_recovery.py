"""One reconnect for an unheld, read-only capture; never replay an input.

The caller supplies the normal Operator constructor as ``make_operator``. A
successful result invalidates any partly captured stereo pair: retain its files
as partial evidence and obtain a fresh pair through the usual presentation guard.
This helper does not claim that reconnecting fixes the native capture backend.
"""
from __future__ import annotations

from dataclasses import dataclass
import json

from .core import BotFault


@dataclass(frozen=True)
class CaptureRecoveryResult:
    mcp_result: dict
    state: dict
    pair_restart_required: bool = True


class CapturePairInvalidated(BotFault):
    """Retained partial images cannot be submitted as a stereo observation."""

    def __init__(self, captures, state):
        super().__init__("Capture recovery invalidated the partial stereo pair")
        self.captures = list(captures)
        self.state = state


def _diagnostic(operator):
    diagnostic = getattr(operator, "diagnostic", None)
    if diagnostic is None:
        return {"diagnostic": "unavailable"}
    try:
        return diagnostic()
    except Exception as error:
        return {"diagnostic": "unavailable", "error": str(error)[:512]}


def _snapshot(live, after_tick):
    # This retained process handle, unlike a PID lookup, rejects PID reuse.
    if (not live.process_handle
            or live.kernel.WaitForSingleObject(live.process_handle, 0) != 258):
        raise BotFault("Original game generation ended during capture recovery")
    if live.held:
        raise BotFault("Capture recovery requires all input leases released")
    state = live.observe()
    tick = state.get("now_ms")
    if type(tick) is not int or type(after_tick) is not int or tick <= after_tick:
        raise BotFault("Native tick did not advance during capture recovery")
    return state


def recover_capture_once(live, suffix, arguments, error, *, make_operator):
    """Return a recovered capture plus a mandatory fresh-pair requirement.

    ``live`` owns both the retained game handle and the old proxy. Budget is
    stored on that Live instance and is never reset here. A failed new client
    stays poisoned and is closed; neither input nor another recovery is sent.
    No request deadline is enlarged. Constructor, tools/list, session read,
    native snapshots and the one capture retry all precede unpoisoning Live.
    """
    allowed = (isinstance(error, TimeoutError)
               and suffix == "capture_composited_image"
               and isinstance(arguments, dict)
               and set(arguments) == {"eye"}
               and arguments["eye"] in ("left", "right")
               and not live.held
               and not getattr(live, "_capture_recovery_used", False))
    if not allowed:
        raise error

    live._capture_recovery_used = True
    live.transport_error = str(error)
    previous = live.operator
    diagnostic = _diagnostic(previous)
    live.events.emit("operator_capture_recovery_started", eye=arguments["eye"],
                     error=str(error), previous_proxy=diagnostic,
                     pair_restart_required=True)
    replacement = None
    try:
        before = _snapshot(live, live.last_tick)
        previous.close()  # Only the proxy child owned by this Live instance.
        replacement = make_operator()  # Normal initialize/initialized handshake.
        if replacement is previous:
            raise BotFault("Capture recovery requires a fresh proxy instance")
        live.operator = replacement
        tools = replacement.request("tools/list", {}, timeout=15.)
        if sum(t.get("name") == "openxr_capture_composited_image"
               for t in tools.get("tools", [])) != 1:
            raise BotFault("Fresh proxy does not expose the expected capture tool")
        session_result = replacement.call("openxr_get_session_info", {}, timeout=15.)
        session = json.loads(next(b["text"] for b in session_result.get("content", [])
                                  if b.get("type") == "text"))
        if session.get("state", {}).get("name") != "XR_SESSION_STATE_FOCUSED":
            raise BotFault("Fresh proxy is not attached to a focused XR session")
        current = _snapshot(live, before["now_ms"])
        result = replacement.call("openxr_capture_composited_image", arguments, timeout=15.)
        after = _snapshot(live, current["now_ms"])
        if not any(b.get("type") == "image" for b in result.get("content", [])):
            raise BotFault("Recovered capture returned no compositor image")
        # Pixel validation and a fresh stereo pair are still the caller's job.
        live.transport_error = None
        live.events.emit("operator_capture_recovery_completed", eye=arguments["eye"],
                         previous_proxy=diagnostic, replacement_proxy=_diagnostic(replacement),
                         native_tick_before=before["now_ms"], native_tick_after=after["now_ms"],
                         pair_restart_required=True)
        return CaptureRecoveryResult(result, after)
    except BaseException as recovery_error:
        live.transport_error = str(recovery_error)
        live.events.emit("operator_capture_recovery_failed", eye=arguments["eye"],
                         error=str(recovery_error), previous_proxy=diagnostic,
                         replacement_proxy=_diagnostic(replacement) if replacement else None,
                         pair_restart_required=True)
        if replacement is not None and replacement is not previous:
            try:
                replacement.close()
            except Exception as close_error:
                live.events.emit("operator_capture_recovery_close_failed",
                                 error=str(close_error), replacement_proxy=_diagnostic(replacement))
        raise
