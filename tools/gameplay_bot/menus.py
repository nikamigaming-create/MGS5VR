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
    from .core import ActionPrerequisiteChanged, matches
    owner = step.get("owner", "idroid")
    focus_keep = getattr(live, '_menu_focus_keep', ()) if owner == 'idroid' else ()
    if not isinstance(focus_keep, tuple):
        focus_keep = ()
    def release_navigation():
        if focus_keep:
            live.release(keep=focus_keep)
        else:
            live.release()
    selector = owner in ("customization", "customization_popup")
    sortie = owner == "sortie"
    help_owner = owner == "mother_base_help"
    terms_owner = owner == "terms"
    if owner not in ("idroid", "pause", "pause_popup", "customization", "customization_popup", "sortie", "mother_base_help", "terms"):
        raise BotFault("Menu navigation requires an explicit supported owner")
    native_guard = step.get("native_before")
    state_guard = step.get("state_before")
    if state_guard is not None and (not isinstance(state_guard, dict) or not state_guard):
        raise BotFault("Navigation state prerequisite must be a nonempty predicate")
    help_state = {"scene":"menu", "menu":True, "idroid":True, "pause":True,
                  "idroid_menu_input_ready":True, "title":False, "loading":False,
                  "controls.context":"menus", "controls.native_buttons":0,
                  "popup_observer.reader_verified":True, "popup_observer.owner_verified":True,
                  "popup_observer.active":False}
    help_native = {"mission":40010, "sequence":"Seq_Game_MainGame", "helicopter_space":True,
                   "title":False, "popup":False, "tutorial_pause":False, "saving":False}
    if help_owner:
        # These flags describe the observed ACC Help ownership intersection,
        # not a native page/cursor identity. Admit only a fresh supervised
        # decision after both final eyes were reviewed; platform acceptance
        # still requires an actual diagram/header change in the new eyes.
        def contains_exact(candidate, required):
            return isinstance(candidate, dict) and all(
                type(candidate.get(key)) is type(value) and candidate[key] == value
                for key, value in required.items())
        if (getattr(live, "supervised", False) is not True
                or not contains_exact(state_guard, help_state)
                or not contains_exact(native_guard, help_native)):
            raise BotFault("Mother Base Help navigation requires supervised reviewed Help and complete ACC prerequisites")
    # A row may intentionally change on this edge. Only the explicitly
    # requested popup identity survives navigation; completed results and
    # sample timestamps are not current focus or a stable dialog identity.
    popup_fields = {"reader_verified", "owner_verified", "active", "numeric_id", "string_id"}
    popup_guard = {key:value for key,value in (state_guard or {}).items()
                   if key in {"popup_observer." + field for field in popup_fields}}
    if owner != "idroid" and (not isinstance(native_guard, dict) or not native_guard):
        raise BotFault("Spatial menu navigation requires native scene prerequisites")
    if terms_owner and (native_guard.get('mission') != 1
            or native_guard.get('sequence') != 'Seq_Demo_LogInKonamiServer'
            or native_guard.get('popup') is not False or native_guard.get('saving') is not False):
        raise BotFault('Fresh Terms navigation requires the exact native first-run owner')
    if owner in ("pause_popup", "customization_popup") and native_guard.get("popup") is not True:
        raise BotFault("Popup navigation requires an explicit reviewed popup prerequisite")
    if selector and (native_guard.get("mission") != 40010
                     or native_guard.get("helicopter_space") is not True
                     or native_guard.get("sequence") != "Seq_Game_WeaponCustomize"
                     or native_guard.get("customization_kind") not in ("weapon", "helicopter", "vehicle")):
        raise BotFault("Customization navigation requires the exact native ACC selector target")
    if sortie and (native_guard.get("mission") not in (40010, 40020)
                   or native_guard.get("helicopter_space") is not True
                   or native_guard.get("sequence") not in (
                       "Seq_Game_MissionPreparationTop", "Seq_Game_MissionPreparation_SelectItem",
                       "Seq_Game_MissionPreparation_SelectSlot", "Seq_Game_MissionPreparation_SelectDetail")
                   or native_guard.get("popup") is not False or native_guard.get("saving") is not False):
        raise BotFault("Sortie navigation requires the exact current ACC preparation owner")
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
    admitted_idroid_mode = None
    admitted_terms_owner = None

    def sample(state, *, check_native=True):
        if help_owner:
            if (not matches(state, help_state)
                    or (check_native and not matches(state.get("native", {}), help_native))):
                raise BotFault("Mother Base Help navigation lost its reviewed owner or native scene prerequisite")
        elif terms_owner:
            terms = state.get('terms_menu', {})
            now, sampled = state.get('now_ms'), terms.get('sample_ms')
            if (terms.get('verified') is not True or terms.get('active') is not True
                    or terms.get('ready') is not True or terms.get('phase') != 5
                    or type(terms.get('owner')) is not int or not terms['owner']
                    or (admitted_terms_owner is not None and terms['owner'] != admitted_terms_owner)
                    or type(now) is not int or type(sampled) is not int or not 0 <= now - sampled <= 250
                    or state.get('idroid') is not False or state.get('pause') is not False
                    or state.get('title') is not False or state.get('loading') is not False
                    or state.get('gamepad') is not False
                    or (check_native and not matches(state.get('native', {}), native_guard))):
                raise BotFault('Terms navigation lost its typed, ready native owner')
        elif owner == "idroid":
            handheld, ready = state.get("idroid_handheld"), state.get("idroid_menu_input_ready")
            # Readiness describes the live handheld pad lease. A native
            # spatial iDroid intentionally has no such lease; require its
            # explicit observed mode rather than treating every false/missing
            # readiness value as permission to navigate.
            input_owner = ((handheld is True and ready is True)
                           or (handheld is False and ready is False))
            if (state.get("idroid") is not True or not input_owner
                    or (admitted_idroid_mode is not None and handheld is not admitted_idroid_mode)
                    or state.get("pause") is not False):
                raise BotFault("iDroid navigation lost its native input owner")
            if check_native and native_guard and not matches(state.get("native", {}), native_guard):
                raise BotFault("iDroid navigation lost its native scene prerequisite")
        else:
            native = state.get("native", {})
            if (state.get("scene") != "menu" or state.get("menu") is not True
                    or state.get("pause") is not (not (selector or sortie)) or state.get("idroid") is not False
                    or state.get("title") is not False or state.get("loading") is not False
                    or (check_native and (native.get("popup") is not expected_popup
                                          or (selector and native.get("saving") is not False)
                                          or not matches(native, native_guard)))):
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

    def check_popup(state, *, admission=False):
        if not popup_guard:
            return
        observer = state.get("popup_observer")
        now, sampled = state.get("now_ms"), observer.get("sample_ms") if isinstance(observer, dict) else None
        if (not isinstance(observer, dict) or observer.get("reader_verified") is not True
                or observer.get("owner_verified") is not True
                or observer.get("active") is not expected_popup
                or type(now) is not int or type(sampled) is not int
                or now < 0 or sampled < 0 or not 0 <= now - sampled <= 250
                or not matches(state, popup_guard)):
            error = ActionPrerequisiteChanged if admission else BotFault
            raise error("Navigation popup identity or fresh owner changed")

    initial, _ = fresh_control_observation(live.observe, native=native_sample or bool(native_guard), clock=clock, sleep=sleep)
    if state_guard is not None and not matches(initial, state_guard):
        raise ActionPrerequisiteChanged("Observed navigation prerequisite changed")
    check_popup(initial, admission=True)
    admitted = sample(initial)["sample_ms"]
    if owner == "idroid":
        admitted_idroid_mode = initial["idroid_handheld"]
    if terms_owner:
        admitted_terms_owner = initial['terms_menu']['owner']
    release_navigation()
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
                if (mode == "edge" and not terms_owner) or elapsed >= math.ceil(seconds * 1000):
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
        release_navigation()
    # An RPC release is not evidence that the game sampled neutral input.
    deadline = clock() + 1.
    neutral_since = None
    while clock() < deadline:
        state, _ = fresh_control_observation(live.observe, native=native_sample or bool(native_guard), clock=clock, sleep=sleep)
        current = sample(state)
        check_popup(state)
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
