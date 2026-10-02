"""Advance observed native startup stages using the existing guarded inputs."""
import time
import math

from .core import ActionPrerequisiteChanged, BlankCompositorFrame, BotFault


def neutral_startup_transition(state):
    """Recognize a released retail boot fade, never a playable blank frame."""
    native, controls = state.get("native", {}), state.get("controls", {})
    if not isinstance(native, dict) or not isinstance(controls, dict):
        return False
    if (type(native.get("mission")) is not int or native["mission"] != 1
            or native.get("sequence") not in ("Seq_Demo_ConfirmAutoSave",
                                             "Seq_Demo_CreateOrLoadSaveData",
                                             "Seq_Demo_LogInKonamiServer")
            or native.get("title") is not False
            or native.get("status_NORMAL_ACTION") is not False
            or native.get("player_life") != 0
            or state.get("camera_active") is not False
            or any(state.get(name) is not False for name in ("title", "menu", "idroid", "pause"))
            or state.get("scene") not in ("cinematic", "loading", "unknown")
            or type(controls.get("native_buttons")) is not int or controls["native_buttons"] != 0
            or type(controls.get("age_ms")) is not int or not 0 <= controls["age_ms"] <= 250):
        return False
    for key, count in (("physical", 11), ("sticks", 4), ("native_triggers", 2)):
        values = controls.get(key)
        if (not isinstance(values, list) or len(values) != count
                or any(type(v) not in (int, float) or not math.isfinite(v) or abs(v) > .001
                       for v in values)):
            return False
    return True


def capture_released_startup_transition(live, label, timeout=15., clock=time.monotonic, sleep=time.sleep):
    """Wait for real pixels through a known boot fade without issuing input.

    Every invalid image is retained by Live.capture. Only typed blank-frame
    failures in a sampled neutral startup owner may be retried; process,
    transport, stale-frame and gameplay failures still propagate immediately.
    """
    deadline = clock() + timeout
    while True:
        try:
            return live.capture(label)
        except BlankCompositorFrame as error:
            if getattr(live, "held", False) or not neutral_startup_transition(error.state):
                raise
            live.events.emit("startup_transition_pixels_wait", label=label,
                             state=error.state, error=str(error), input_replayed=False)
            if clock() >= deadline:
                raise BotFault("Known startup transition remained blank before bounded pixel deadline; no input replayed") from error
            sleep(min(.25, max(0., deadline-clock())))


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

        # The retail login sequence owns progress and result dialogs. Its
        # popup bit alone cannot authorize confirmation. The one completed
        # login notice below was matched to real final-eye pixels and the
        # verified native popup reader; unknown results remain observation-only.

        action = None
        if native.get("sequence") == "Seq_Demo_ConfirmAutoSave" and native.get("popup") is True:
            action = ("autosave-notice", {"sequence": "Seq_Demo_ConfirmAutoSave", "popup": True})
        elif (native.get("sequence") == "Seq_Demo_LogInKonamiServer"
              and native.get("mission") == 1 and native.get("title") is False
              and native.get("popup") is True
              and logged_in_notice(state)):
            action = ("logged-in-notice", {"mission": 1, "sequence": "Seq_Demo_LogInKonamiServer",
                                          "title": False, "popup": True})
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
            elif label == "logged-in-notice":
                step["state_before"] = LOGGED_IN_NOTICE
            try:
                live.execute(step)
            except ActionPrerequisiteChanged as error:
                # A native popup can close during capture. Admission sent no
                # input, so return to fresh observation within the same deadline.
                # Transport, sampling and post-dispatch faults still stop the run.
                live.events.emit("startup_admission_changed", label=label, error=str(error))
            else:
                acknowledged.add(label)
        if clock() >= deadline:
            live.capture("startup-wait-failure")
            raise BotFault("Native startup deadline; last sequence=" + str(native.get("sequence")))
        # Re-read the native stage, not only the coarse title flag. Logos and
        # PRESS ENTER share that flag; a single early check misses the prompt.
        sleep(.1)


# Native StringId recovered from the completed login notice, retained in
# 20261002T055911070140Z. Numeric ID 1 alone is a shared popup template.
LOGGED_IN_NOTICE = {
    "popup_observer.reader_verified": True,
    "popup_observer.owner_verified": True,
    "popup_observer.active": True,
    "popup_observer.numeric_id": 1,
    "popup_observer.string_id": "0x4dc3cae5b486",
}


def logged_in_notice(state):
    from .core import matches
    popup = state.get("popup_observer") or {}
    return (type(popup.get("numeric_id")) is int
            and all(popup.get(key) is True for key in ("reader_verified", "owner_verified", "active"))
            and matches(state, LOGGED_IN_NOTICE))


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


def capture_continue_rack(live, timeout=5., clock=time.monotonic, sleep=time.sleep):
    """Wait for real rack pixels across the observed native title transition.

    The native rack can become queryable before its title fade completes.
    Retry only a retained blank frame in that exact owner; never send input,
    accept blank pixels, retry transport failures, or exempt field gameplay.
    """
    deadline = clock() + timeout
    while True:
        try:
            return live.capture("continue-hover")
        except BlankCompositorFrame as error:
            state = error.state
            native = state.get("native", {})
            rack = (state.get("scene") == "title" and state.get("title") is True
                    and state.get("title_menu") is True and state.get("title_cabin") is True
                    and state.get("opening_assets") is True
                    and native.get("sequence") == "Seq_Game_TitleMenu"
                    and native.get("title") is True and native.get("popup") is False)
            if not rack:
                raise
            if clock() >= deadline:
                raise BotFault("Continue rack remained blank before readiness deadline; no input sent") from error
            live.events.emit("continue_rack_pixels_wait", state=state, error=str(error))
            sleep(min(.1, max(0., deadline-clock())))
