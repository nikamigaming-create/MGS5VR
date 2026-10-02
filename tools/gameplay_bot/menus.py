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
    selector = owner in ("customization", "customization_popup")
    if owner not in ("idroid", "pause", "pause_popup", "customization", "customization_popup"):
        raise BotFault("Menu navigation requires an explicit supported owner")
    native_guard = step.get("native_before")
    if owner != "idroid" and (not isinstance(native_guard, dict) or not native_guard):
        raise BotFault("Spatial menu navigation requires native scene prerequisites")
    if owner in ("pause_popup", "customization_popup") and native_guard.get("popup") is not True:
        raise BotFault("Popup navigation requires an explicit reviewed popup prerequisite")
    if selector and (native_guard.get("mission") != 40010
                     or native_guard.get("helicopter_space") is not True
                     or native_guard.get("sequence") != "Seq_Game_WeaponCustomize"
                     or native_guard.get("customization_kind") not in ("weapon", "helicopter", "vehicle")):
        raise BotFault("Customization navigation requires the exact native ACC selector target")
    native_sample = owner != "idroid"
    mode = step.get("mode", "edge")
    if mode not in ("edge", "hold") or (native_sample and mode != "edge"):
        raise BotFault("Menu cursor steps require edge mode; only iDroid permits an explicit scroll hold")
    expected_popup = owner in ("pause_popup", "customization_popup")
    during = step.get("while_held")
    if during is not None and (not isinstance(during, dict) or not during
                               or any(key.startswith("native.") for key in during)):
        raise BotFault("Navigation held outcomes require fast control/ownership predicates")
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

    def sample(state, *, check_native=True):
        if owner == "idroid":
            if (state.get("idroid") is not True or state.get("idroid_menu_input_ready") is not True
                    or state.get("pause") is not False):
                raise BotFault("iDroid navigation lost its native input owner")
            if check_native and native_guard and not matches(state.get("native", {}), native_guard):
                raise BotFault("iDroid navigation lost its native scene prerequisite")
        else:
            native = state.get("native", {})
            if (state.get("scene") != "menu" or state.get("menu") is not True
                    or state.get("pause") is not (not selector) or state.get("idroid") is not False
                    or state.get("title") is not False or state.get("loading") is not False
                    or (check_native and (native.get("popup") is not expected_popup or not matches(native, native_guard)))):
                raise BotFault("Spatial menu navigation lost its native input owner or scene prerequisite")
        controls = authoritative_controls(state)
        if controls["context"] != "menus":
            raise BotFault("Navigation requires the current menu context")
        sticks = controls.get("sticks")
        if (not isinstance(sticks, list) or len(sticks) != 4
                or any(isinstance(v, bool) or not isinstance(v, (int, float))
                       or not math.isfinite(v) for v in sticks)):
            raise BotFault("Fresh native menu stick samples are unavailable")
        return controls

    initial, _ = fresh_control_observation(live.observe, native=native_sample or bool(native_guard))
    admitted = sample(initial)["sample_ms"]
    live.release()
    live.events.emit("semantic_menu_navigation", source=source, direction=direction, owner=owner,
                     mode="sampled_" + mode,
                     sampled_milliseconds=math.ceil(seconds * 1000))
    first = None
    held_state = None
    deadline = clock() + 2.
    try:
        live.input([payload], seconds, lease_seconds=2.)
        while clock() < deadline:
            # Native iDroid lists repeat too: two 160 ms sampled holds moved
            # Rewards -> Staff Management, skipping the intended Development
            # row. Ordinary cursor steps release on the first fresh held edge.
            # An explicit iDroid scroll hold retains bounded duration sampling.
            # Keep Lua reads outside the held phase and recheck guards on release.
            state, _ = fresh_control_observation(live.observe, clock=clock, sleep=sleep)
            current = sample(state, check_native=False)
            if current["sample_ms"] > admitted and abs(current["sticks"][axis] - payload["value"]) < .08:
                first = current["sample_ms"] if first is None else first
                elapsed = current["sample_ms"] - first
                if mode == "edge" or elapsed >= math.ceil(seconds * 1000):
                    if during and not matches(state, during):
                        raise BotFault("Requested navigation held outcome was not observed")
                    held_state = state
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
        state, _ = fresh_control_observation(live.observe, native=native_sample or bool(native_guard), clock=clock, sleep=sleep)
        current = sample(state)
        if all(abs(v) < .08 for v in current["sticks"]):
            neutral_since = current["sample_ms"] if neutral_since is None else neutral_since
            if current["sample_ms"] - neutral_since >= 100:
                live.events.emit("menu_navigation_released", sample_ms=current["sample_ms"])
                if during:
                    checkpoint = live.events.emit("held_visual_checkpoint", action="menu_navigate",
                                                  source="native_control_sample", final_compositor_capture=False)
                    return {"state":held_state, "captures":[], "visual_checkpoint":checkpoint}
                return state
        else:
            neutral_since = None
        sleep(.025)
    raise BotFault("Native menu stick stayed held after release")
