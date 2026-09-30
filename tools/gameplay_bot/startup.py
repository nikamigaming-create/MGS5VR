"""Advance observed native startup stages using the existing guarded inputs."""
import time

from .core import BlankCompositorFrame, BotFault


def capture_startup_baseline(live, timeout=15., clock=time.monotonic, sleep=time.sleep):
    """Wait through an observed native boot fade without accepting blank pixels."""
    deadline = clock() + timeout
    while True:
        try:
            return live.capture("baseline")
        except BlankCompositorFrame as error:
            state = error.state
            native = state.get("native", {})
            boot = (state.get("scene") in ("title", "loading") or
                    (state.get("scene") == "unknown" and native.get("mission") in (65535, 1)
                     and native.get("sequence") is None
                     and native.get("status_NORMAL_ACTION") is not True
                     and native.get("player_life", 0) == 0))
            if not boot or state.get("camera_active") is True:
                raise
            if clock() >= deadline:
                raise BotFault("Startup compositor remained blank before readiness deadline") from error
            live.events.emit("startup_fade_wait", state=state, error=str(error))
            sleep(min(.25, max(0., deadline-clock())))


def advance_startup(live, timeout=90., clock=time.monotonic, sleep=time.sleep):
    deadline = clock() + timeout
    acknowledged = set()
    previous = None
    while True:
        state = live.observe(native=True)
        native = state.get("native", {})
        phase = (state["scene"], native.get("sequence"), native.get("popup"), state.get("title_menu"))
        if phase != previous:
            live.events.emit("startup_stage", state=state)
            previous = phase
        if state["scene"] in ("loading", "gameplay", "cabin"):
            return state
        if state["scene"] == "title" and state.get("title_menu") is True:
            return state

        action = None
        if native.get("sequence") == "Seq_Demo_ConfirmAutoSave" and native.get("popup") is True:
            action = ("autosave-notice", {"sequence": "Seq_Demo_ConfirmAutoSave", "popup": True})
        elif (state["scene"] == "title" and not state.get("title_menu")
              and state.get("press_start_ready") is True
              and native.get("sequence") == "Seq_Demo_StartHasTitleMission"
              and native.get("title") is True and native.get("popup") is False):
            action = ("press-start", {"sequence": "Seq_Demo_StartHasTitleMission", "title": True, "popup": False})
        if action and action[0] not in acknowledged:
            label, guard = action
            live.capture(label)
            # execute() rechecks native ownership after capture. Never repeat
            # confirm into a menu just because a startup flag lingers.
            # The title splash consumes native START, while the autosave
            # notice consumes A. In normal VR controls the configured iDroid
            # tap supplies START on release; A can be consumed without opening
            # the title menu, leaving the cockpit idle until the deadline.
            step = {"op": "action", "name": "system.idroid" if label == "press-start" else "menus.confirm",
                    "native_before": guard}
            if label == "press-start":
                step["state_before"] = {"title_menu": False, "press_start_ready": True}
            live.execute(step)
            acknowledged.add(label)
        if clock() >= deadline:
            live.capture("startup-wait-failure")
            raise BotFault("Native startup deadline; last sequence=" + str(native.get("sequence")))
        # Re-read the native stage, not only the coarse title flag. Logos and
        # PRESS ENTER share that flag; a single early check misses the prompt.
        sleep(.1)


def wait_for_continue_rack(live, initial_state, timeout=5., clock=time.monotonic, sleep=time.sleep):
    """Wait briefly for physical Continue-rack telemetry after the title menu appears.

    This is observation-only: once menu ownership changes or an unrecognized
    popup appears, stop instead of sending input into a potentially different
    screen. Every sample is fresh and the wait is strictly bounded.
    """
    state = initial_state
    deadline = clock() + timeout
    while True:
        if state.get("scene") != "title" or state.get("title_menu") is not True:
            live.capture("continue-rack-readiness-interrupted")
            raise BotFault("Title menu ownership changed while waiting for Continue rack; no input sent")
        native = state.get("native", {})
        if native.get("popup") is True:
            live.capture("continue-rack-readiness-popup")
            raise BotFault("Unexpected title popup while waiting for Continue rack; no input sent")
        ready = (state.get("title_cabin") is True and state.get("opening_assets") is True
                 and state.get("opening_position") and state.get("opening_orientation"))
        if ready:
            return state
        if clock() >= deadline:
            live.capture("continue-rack-readiness-timeout")
            raise BotFault("Continue rack telemetry did not become ready before bounded deadline; no input sent")
        sleep(min(.1, max(0., deadline - clock())))
        state = live.observe(native=True)
