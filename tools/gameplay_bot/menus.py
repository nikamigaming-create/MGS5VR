"""Bounded, owner-checked stick navigation through the effective VR mapping."""
import math
import time

from .core import BotFault


def observe(live, step, *, clock=time.monotonic, sleep=time.sleep):
    """Keep a test-opened menu idle while its native child block streams."""
    from .live import authoritative_controls, fresh_control_observation
    seconds = step.get("seconds", 5.)
    if (isinstance(seconds, bool) or not isinstance(seconds, (int, float))
            or not math.isfinite(seconds) or not .1 <= seconds <= 15.):
        raise BotFault("Menu observation must last 0.1..15 seconds")
    live.release()
    deadline = clock() + seconds
    try:
        while True:
            state, _ = fresh_control_observation(live.observe, native=True, clock=clock, sleep=sleep)
            controls = authoritative_controls(state)
            if state.get("idroid") is not True or controls["context"] != "menus":
                raise BotFault("Native iDroid ownership changed during menu observation")
            sticks, physical = controls.get("sticks"), controls.get("physical")
            if (not isinstance(sticks, list) or len(sticks) != 4
                    or not isinstance(physical, list) or len(physical) < 11
                    or any(isinstance(v, bool) or not isinstance(v, (int, float))
                           or not math.isfinite(v) or abs(v) > .08
                           for v in sticks + physical)):
                raise BotFault("Menu observation requires sampled neutral input")
            live.events.emit("menu_idle_observation", state=state)
            if clock() >= deadline:
                return state
            sleep(min(.25, deadline-clock()))
    finally:
        live.release()


def navigate(live, step, *, clock=time.monotonic, sleep=time.sleep):
    # Resolve and validate everything before dispatch. This operation only
    # moves the menu cursor; selection stays a separate semantic action.
    from .live import authoritative_controls, channel, fresh_control_observation
    from .core import matches
    owner = step.get("owner", "idroid")
    if owner not in ("idroid", "pause"):
        raise BotFault("Menu navigation requires an explicit supported owner")
    native_guard = step.get("native_before")
    if owner == "pause" and (not isinstance(native_guard, dict) or not native_guard):
        raise BotFault("Pause navigation requires native scene prerequisites")
    native_sample = owner == "pause"
    direction = step.get("direction")
    seconds = step.get("seconds", .16)
    if direction not in ("up", "down", "left", "right"):
        raise BotFault("Menu navigation requires a cardinal direction")
    if (isinstance(seconds, bool) or not isinstance(seconds, (int, float))
            or not math.isfinite(seconds) or not .08 <= seconds <= .25):
        raise BotFault("Menu navigation must last 80..250 sampled milliseconds")
    source = next((row["source"] for row in live.bindings.get("axes", [])
                   if row["name"] == "axes.menu"), None)
    if source not in ("left_stick", "right_stick"):
        raise BotFault("Effective menu stick mapping is unavailable")
    token = source + "_" + direction
    payload = channel(token)
    axis = (0 if source == "left_stick" else 2) + (1 if direction in ("up", "down") else 0)

    def sample(state):
        if owner == "idroid":
            if state.get("idroid") is not True or state.get("idroid_menu_input_ready") is not True:
                raise BotFault("iDroid navigation lost its native input owner")
        else:
            native = state.get("native", {})
            if (state.get("scene") != "menu" or state.get("menu") is not True
                    or state.get("pause") is not True or state.get("idroid") is not False
                    or state.get("title") is not False or state.get("loading") is not False
                    or native.get("popup") is not False or not matches(native, native_guard)):
                raise BotFault("Pause navigation lost its native input owner or scene prerequisite")
        controls = authoritative_controls(state)
        if controls["context"] != "menus":
            raise BotFault("Navigation requires the current menu context")
        sticks = controls.get("sticks")
        if (not isinstance(sticks, list) or len(sticks) != 4
                or any(isinstance(v, bool) or not isinstance(v, (int, float))
                       or not math.isfinite(v) for v in sticks)):
            raise BotFault("Fresh native menu stick samples are unavailable")
        return controls

    initial, _ = fresh_control_observation(live.observe, native=native_sample)
    admitted = sample(initial)["sample_ms"]
    live.release()
    live.events.emit("semantic_menu_navigation", source=source, direction=direction, owner=owner,
                     sampled_milliseconds=math.ceil(seconds * 1000))
    first = None
    deadline = clock() + 2.
    try:
        live.input([payload], seconds, lease_seconds=2.)
        while clock() < deadline:
            state, _ = fresh_control_observation(live.observe, native=native_sample, clock=clock, sleep=sleep)
            current = sample(state)
            if current["sample_ms"] > admitted and abs(current["sticks"][axis] - payload["value"]) < .08:
                first = current["sample_ms"] if first is None else first
                elapsed = current["sample_ms"] - first
                if elapsed >= math.ceil(seconds * 1000):
                    live.events.emit("menu_navigation_sampled", source=source, direction=direction,
                                     first_sample_ms=first, last_sample_ms=current["sample_ms"],
                                     sampled_milliseconds=elapsed)
                    break
            else:
                first = None
            sleep(.025)
        else:
            raise BotFault("Native menu navigation was not sampled before expiry")
    finally:
        live.release()
    # An RPC release is not evidence that the game sampled neutral input.
    deadline = clock() + 1.
    neutral_since = None
    while clock() < deadline:
        state, _ = fresh_control_observation(live.observe, native=native_sample, clock=clock, sleep=sleep)
        current = sample(state)
        if all(abs(v) < .08 for v in current["sticks"]):
            neutral_since = current["sample_ms"] if neutral_since is None else neutral_since
            if current["sample_ms"] - neutral_since >= 100:
                live.events.emit("menu_navigation_released", sample_ms=current["sample_ms"])
                return state
        else:
            neutral_since = None
        sleep(.025)
    raise BotFault("Native menu stick stayed held after release")
