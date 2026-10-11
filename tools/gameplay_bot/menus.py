"""Bounded, owner-checked stick navigation through the effective VR mapping."""
import math
import time

from .core import BotFault


def focus_sequence(live, step, *, clock=None, sleep=None):
    """A finite ordinary-iDroid sequence under one physically continuous grip.

    Only one Confirm is admitted: opening a reviewed upgrade dialog can be
    followed by Back, but this operation cannot blindly accept a second dialog.
    Native scene admission happens once before grip; the held interval is
    strictly fast-only, including owner-loss and cleanup observations.
    """
    from .live import authoritative_controls, channel, fresh_control_observation, held_input_evidence, sampled_release_keys
    from .core import ActionPrerequisiteChanged, matches
    clock, sleep = clock or time.monotonic, sleep or time.sleep
    if getattr(live, '_menu_focus_sequence_active', False) or getattr(live, '_menu_focus_keep', ()):
        raise BotFault('A continuous menu-focus sequence cannot be nested')
    seconds = step.get('seconds', 5.5)
    if type(seconds) not in (int, float) or not math.isfinite(seconds) or not 5 <= seconds <= 8:
        raise BotFault('Continuous menu focus requires a sampled 5..8 second span and a ten-second deadline')
    children = step.get('steps')
    if not isinstance(children, list) or not 3 <= len(children) <= 5:
        raise BotFault('Continuous menu focus requires three to five explicit semantic steps')
    native_guard = step.get('native_before')
    if (not isinstance(native_guard, dict) or type(native_guard.get('mission')) is not int
            or native_guard['mission'] <= 0 or not isinstance(native_guard.get('sequence'), str)
            or not native_guard['sequence'].startswith('Seq_Game_')
            or type(native_guard.get('helicopter_space')) is not bool
            or native_guard.get('popup') is not False or native_guard.get('saving') is not False):
        raise BotFault('Continuous focus requires an exact native mission/sequence/seat and no popup or saving')
    focus = next((a for a in live.bindings['actions'] if a['name'] == 'system.idroid_focus'), None)
    if (not focus or not focus.get('modifier') or len(focus.get('bindings', [])) != 1
            or focus['bindings'][0]['gesture'] != 'level'
            or focus['bindings'][0]['inputs'] not in (['left_grip'], ['right_grip'])):
        raise BotFault('Continuous menu focus requires one effective held grip binding')
    focus_tokens = focus['bindings'][0]['inputs']
    focus_channels = [channel(t) for t in focus_tokens]
    key = lambda c: (c['hand'], c['component'], c.get('sub_component'))
    keep = tuple(key(c) for c in focus_channels)
    plans, confirms, requested_time = [], 0, 0.
    for child in children:
        if not isinstance(child, dict):
            raise BotFault('Continuous focus steps must be semantic objects')
        allowed = {'op', 'name', 'seconds', 'state_before', 'settle_seconds'} if child.get('op') == 'action' else {
            'op', 'owner', 'direction', 'mode', 'seconds', 'state_before', 'settle_seconds'}
        if set(child) - allowed:
            raise BotFault('Continuous focus steps cannot contain Lua guards, captures or nested operations')
        guard = child.get('state_before')
        if guard is not None and (not isinstance(guard, dict) or not guard or any(k.startswith('native.') for k in guard)):
            raise BotFault('Continuous focus child guards must be nonempty fast-state predicates')
        duration = child.get('seconds', .12 if child.get('op') == 'action' else .16)
        settle = child.get('settle_seconds', .15)
        if type(settle) not in (int, float) or not math.isfinite(settle) or not 0 <= settle <= 1.5:
            raise BotFault('Menu step settling must be 0..1.5 seconds')
        if child.get('op') == 'action':
            name = child.get('name')
            action = next((a for a in live.bindings['actions'] if a['name'] == name), None)
            if (name not in ('menus.confirm', 'menus.back') or not action
                    or 'menus' not in action.get('contexts', []) or len(action.get('bindings', [])) != 1):
                raise BotFault('Continuous focus permits only effective Confirm/Back and menu navigation')
            binding = action['bindings'][0]
            if (binding['gesture'] != 'level' or len(binding['inputs']) != 1
                    or binding['inputs'][0] not in ('a', 'b', 'x', 'y')):
                raise BotFault('Continuous focus menu buttons require one nonoverlapping effective face input')
            tokens = binding['inputs']
            maximum = .25
            confirms += name == 'menus.confirm'
        elif child.get('op') == 'menu_navigate':
            source = next((a['source'] for a in live.bindings.get('axes', []) if a['name'] == 'axes.menu'), None)
            mode = child.get('mode', 'edge')
            if (child.get('owner', 'idroid') != 'idroid' or mode not in ('edge', 'hold')
                    or source not in ('left_stick', 'right_stick') or child.get('direction') not in ('up', 'down', 'left', 'right')):
                raise BotFault('Continuous focus navigation requires the effective iDroid menu stick')
            tokens = [source + '_' + child['direction']]
            maximum = 1.25 if mode == 'hold' else .25
        else:
            raise BotFault('Unsupported step in continuous menu focus')
        if type(duration) not in (int, float) or not math.isfinite(duration) or not .08 <= duration <= maximum:
            raise BotFault('Continuous focus child duration exceeds its menu input bound')
        requested_time += duration + settle
        plans.append(({k:v for k,v in child.items() if k != 'settle_seconds'}, tokens, settle))
    if confirms > 1 or requested_time > seconds:
        raise BotFault('Continuous focus allows one Confirm and steps must fit the requested span')
    during = step.get('while_held')
    if during is not None and (not isinstance(during, dict) or not during or any(k.startswith('native.') for k in during)):
        raise BotFault('Continuous focus outcome must use fast-state predicates')
    initial, _ = fresh_control_observation(live.observe, native=True, clock=clock, sleep=sleep)
    if not matches(initial.get('native', {}), native_guard) or (step.get('state_before') and not matches(initial, step['state_before'])):
        raise ActionPrerequisiteChanged('Continuous focus native prerequisite changed before grip')
    frame = initial.get('rendered') or {}
    owner = (frame.get('player_owner'), frame.get('activation'))
    if not all(type(v) is int and v > 0 for v in owner):
        raise BotFault('Continuous focus requires an identified current player/activation')
    original_observe = live.observe
    def fast():
        return original_observe(native=False, fast_only=True)
    def route(state):
        frame = state.get('rendered') or {}
        if (any(state.get(k) is not False for k in ('pause', 'title', 'loading', 'demo', 'gamepad'))
                or any(state.get(k) is not True for k in ('idroid', 'idroid_handheld', 'idroid_menu_routing_available'))
                or state.get('idroid_tutorial_mode') != 0 or state.get('idroid_closing') is not False
                or (frame.get('player_owner'), frame.get('activation')) != owner
                or state.get('popup_observer', {}).get('active') is not False):
            raise BotFault('Continuous menu focus lost its ordinary native owner')
    route(initial)
    deadline = None
    lease_until = 0.
    first_sample = last_sample = None
    last_state = None
    active_keys = set(keep)
    child_index = None
    def observe(native=False, **kwargs):
        nonlocal lease_until, last_sample, last_state
        if native:
            raise BotFault('Lua observation is forbidden while continuous menu focus is held')
        now = clock()
        if now >= deadline or now >= lease_until:
            raise BotFault('Continuous focus deadline or independent grip lease expired')
        state = fast()
        controls = authoritative_controls(state)
        route(state)
        if (controls['context'] != 'menus' or state.get('idroid_menu_control') != 2
                or state.get('idroid_menu_input_ready') is not True or not held_input_evidence(state, focus_tokens)
                or sampled_release_keys(state) is None or not sampled_release_keys(state).issubset(active_keys)
                or controls['sample_ms'] < last_sample):
            raise BotFault('Continuous focus, readiness or exclusive physical input was lost')
        if controls['sample_ms'] != last_sample:
            live.events.emit('menu_focus_sequence_sample', sample_ms=controls['sample_ms'],
                             first_sample_ms=first_sample, child_index=child_index, control=2, ready=True)
        last_sample, last_state = controls['sample_ms'], state
        # Renew the same nonzero grip, never synthesize a release/repress edge.
        # Every lease remains <=5 seconds if the runner or transport stops.
        if clock() >= lease_until - 2.5:
            if clock() >= lease_until or clock() >= deadline:
                raise BotFault('Continuous focus expired before safe lease renewal')
            lease_until = clock() + min(5., deadline-clock())
            live.input(focus_channels, .2, lease_seconds=lease_until-clock())
        return state
    released = None
    try:
        live.release()
        neutral = fast()
        route(neutral)
        if sampled_release_keys(neutral) != set():
            raise BotFault('Continuous focus requires sampled neutral input before grip')
        deadline = clock() + min(10., seconds + 2.)
        lease_until = clock() + 5.
        live.input(focus_channels, .2, lease_seconds=5.)
        acquire_until = min(deadline, clock() + .8)
        while True:
            focused = fast()
            current = authoritative_controls(focused)
            route(focused)
            if (current['context'] == 'menus' and focused.get('idroid_menu_control') == 2
                    and focused.get('idroid_menu_input_ready') is True
                    and held_input_evidence(focused, focus_tokens, after_sample_ms=neutral['controls']['sample_ms'])):
                first_sample = last_sample = current['sample_ms']
                last_state = focused
                break
            if clock() >= acquire_until:
                raise BotFault('Continuous handheld focus was not acknowledged; no child action sent')
            sleep(.025)
        live._menu_focus_keep = keep
        live._menu_focus_sequence_active = True
        live.observe = observe
        live.events.emit('menu_focus_sequence_started', state=focused, steps=children, sampled_seconds=seconds)
        for child_index, (recipe, tokens, settle) in enumerate(plans):
            active_keys = set(keep)
            ready = observe()
            if recipe.get('state_before') and not matches(ready, recipe['state_before']):
                raise ActionPrerequisiteChanged('Continuous menu step prerequisite changed')
            active_keys.update(key(channel(t)) for t in tokens)
            live._execute(recipe)
            active_keys = set(keep)
            until = clock() + settle
            while True:
                observe()
                if clock() >= until:
                    break
                sleep(min(.025, until-clock()))
            live.events.emit('menu_focus_sequence_step_completed', child_index=child_index, state=last_state)
        child_index = None
        while last_sample-first_sample < math.ceil(seconds*1000):
            sleep(.025)
            observe()
        if during and not matches(last_state, during):
            raise BotFault('Continuous menu focus held outcome was not observed')
        checkpoint = live.events.emit('held_visual_checkpoint', action='menu_focus_sequence',
            source='native_control_samples', final_compositor_capture=False,
            first_sample_ms=first_sample, last_sample_ms=last_sample,
            sampled_milliseconds=last_sample-first_sample)
    finally:
        live._menu_focus_sequence_active = False
        live._menu_focus_keep = ()
        # release() observes before and after clearing its leases. It must
        # remain fast-only even if the owner changed to nativeButtons, where
        # an ordinary observe(False) deliberately falls back to native Lua.
        def cleanup_observe(native=False, **kwargs):
            if native:
                raise BotFault('Continuous focus cleanup cannot request Lua')
            return fast()
        live.observe = cleanup_observe
        try:
            live.release()
            neutral_until, neutral_since = clock()+2., None
            while clock() < neutral_until:
                released = fast()
                controls = authoritative_controls(released)
                if (sampled_release_keys(released) == set() and controls.get('native_buttons') == 0
                        and controls.get('native_axes') == [0, 0, 0, 0] and controls.get('native_triggers') == [0, 0]):
                    neutral_since = controls['sample_ms'] if neutral_since is None else neutral_since
                    if controls['sample_ms']-neutral_since >= 100:
                        live.events.emit('menu_focus_sequence_neutral', state=released,
                                         sampled_milliseconds=controls['sample_ms']-neutral_since)
                        break
                else:
                    neutral_since = None
                sleep(.025)
            else:
                raise BotFault('Continuous focus cleanup did not establish sampled neutral input')
        finally:
            live.observe = original_observe
    return {'state':last_state, 'released_state':released, 'captures':[], 'visual_checkpoint':checkpoint,
            'sampled_focus_milliseconds':last_sample-first_sample, 'steps_completed':len(plans),
            'page_outcome_proven':False}


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
    maximum = 1.25 if mode == "hold" else .25
    if (isinstance(seconds, bool) or not isinstance(seconds, (int, float))
            or not math.isfinite(seconds) or not .08 <= seconds <= maximum):
        raise BotFault(f"Menu navigation {mode} must last 80..{int(maximum * 1000)} sampled milliseconds")
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
