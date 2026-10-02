"""Short, feedback-bounded native-player movement through configured XR input.

This measures one ordinary stick-up segment in the current field. It does not
edit player transforms, infer route clearance, or claim an A* path is playable.
"""
from __future__ import annotations

import math
import time

from .core import BotFault, ProgressGuard
from .live import authoritative_controls, finite_number


def _axis_source(bindings, axis_name):
    entries = bindings.get("axes") if isinstance(bindings, dict) else None
    if not isinstance(entries, list):
        raise BotFault("Effective axis bindings are unavailable")
    matches = [entry.get("source") for entry in entries
               if isinstance(entry, dict) and entry.get("name") == axis_name]
    if len(matches) != 1 or matches[0] not in ("left_stick", "right_stick"):
        raise BotFault(f"{axis_name} must resolve to one enabled physical stick")
    return matches[0]


def _position(state):
    native = state.get("native")
    if not isinstance(native, dict):
        raise BotFault("Native player position is unavailable")
    names = ("player_x", "player_y", "player_z")
    values = [native.get(name) for name in names]
    if any(not finite_number(value) for value in values):
        raise BotFault("Native player position is missing or nonfinite")
    return tuple(float(value) for value in values)


def _validate_xr_scene_state(state, *, activation=None):
    if not isinstance(state, dict) or state.get("scene") != "gameplay":
        raise BotFault("Locomotion requires an observed gameplay scene")
    if state.get("camera_active") is not True or state.get("camera_available") is not True:
        raise BotFault("Locomotion requires the active immersive player camera")
    if state.get("menu") is not False or state.get("idroid") is not False:
        raise BotFault("Locomotion is unavailable while a native menu is open")
    if state.get("gamepad") is not False:
        raise BotFault("Locomotion is unavailable while physical XInput owns gameplay")
    if activation is not None and state.get("activation") != activation:
        raise BotFault("Camera generation changed during locomotion")


def _validate_xr_field_state(state, *, activation=None):
    _validate_xr_scene_state(state, activation=activation)
    controls = authoritative_controls(state)
    if controls.get("context") != "gameplay" or controls.get("rig_input") is not True:
        raise BotFault("Native XR controls are not in the live gameplay rig")
    if type(controls.get("travel_mode")) is not int or controls["travel_mode"] != 1:
        raise BotFault("The observed native travel mode is not on foot")

    now_ms = state.get("now_ms")
    if type(now_ms) is not int or now_ms < 0:
        raise BotFault("Native observation time is invalid")
    sample, sticks = controls.get("physical"), controls.get("sticks")
    if not isinstance(sample, list) or len(sample) < 11:
        raise BotFault("Fresh physical input audit is unavailable")
    if not isinstance(sticks, list) or len(sticks) != 4:
        raise BotFault("Fresh physical stick audit is unavailable")
    if any(not finite_number(value) for value in sample[:11] + sticks):
        raise BotFault("Physical input audit contains a nonfinite value")
    return controls, now_ms


def _validate_field_state(state, anchor=None):
    controls, now_ms = _validate_xr_field_state(state)

    native = state.get("native")
    if not isinstance(native, dict):
        raise BotFault("Native mission identity is unavailable")
    mission, location, sequence = native.get("mission"), native.get("location"), native.get("sequence")
    if (type(mission) is not int or mission <= 0 or type(location) is not int
            or not isinstance(sequence, str) or not sequence.startswith("Seq_Game_")
            or native.get("title") is not False or native.get("status_NORMAL_ACTION") is not True
            or native.get("saving") is not False or native.get("popup") is not False
            or type(native.get("game_over")) is not int or native["game_over"] != 0
            or type(native.get("player_vehicle_id")) is not int
            or native["player_vehicle_id"] != 65535):
        raise BotFault("Native state is not a stable, ordinary on-foot field state")
    _position(state)

    if anchor is not None:
        before = anchor["identity"]
        current = (mission, location, sequence, state.get("activation"))
        if current != before:
            raise BotFault("Mission, location, sequence, or camera generation changed during locomotion")
    return controls, now_ms


def _neutral_audit(controls):
    physical = controls["physical"]
    buttons, sticks = physical[:11], controls["sticks"]
    if any(value > .09 for value in buttons) or any(abs(value) >= .18 for value in sticks):
        raise BotFault("Center the sticks and release controls before the movement probe")


def _distance(a, b):
    return math.sqrt(sum((right - left) ** 2 for left, right in zip(a, b)))


def _stick_y_index(source):
    if source == "left_stick":
        return 1
    if source == "right_stick":
        return 3
    raise BotFault("Effective movement axis is not a supported controller stick")


def _sampled_move_input(state, source, requested, *, after_sample_ms):
    """Verify the fresh XR stick sample matches this movement phase.

    This is a control-resolver sample, not proof that the retail XInput hook or
    player movement code consumed the value.
    """
    try:
        controls = authoritative_controls(state)
    except BotFault:
        return None
    sample_ms = controls["sample_ms"]
    if (type(after_sample_ms) is not int or sample_ms <= after_sample_ms
            or controls.get("context") != "gameplay" or controls.get("rig_input") is not True):
        return None
    sticks = controls.get("sticks")
    if not isinstance(sticks, list) or len(sticks) != 4 or any(not finite_number(v) for v in sticks):
        return None
    sampled = float(sticks[_stick_y_index(source)])
    tolerance = max(.025, requested * .20)
    if sampled < .05 or abs(sampled - requested) > tolerance:
        return None
    return {
        "sample_ms": sample_ms,
        "age_ms": controls["age_ms"],
        "context": controls["context"],
        "requested_value": requested,
        "sampled_value": sampled,
        "xr_sampled_sticks": list(sticks),
        "xr_published_gamepad_buttons": controls.get("native_buttons"),
    }


def move_local(live, *, target_distance=.25, max_displacement=.75, max_seconds=5.,
               progress_timeout=1.0, cruise_magnitude=.30, brake_magnitude=.15,
               brake_fraction=.45, pulse_seconds=.25, max_unsampled_retries=1,
               clock=time.monotonic, sleep=time.sleep):
    """Measure a short walk with expiring pulses and neutral native reads.

    Distances are native game-coordinate units: no meter conversion is assumed.
    Only the fast XR publication is read while a pulse is held. Input expires
    independently after at most .25 seconds and is explicitly released before
    any potentially blocking Lua position query. Each pulse must settle before
    another is admitted. An unsampled axis pulse can be retried only after
    neutral publication resumes and its native endpoint remains unchanged.
    Unknown RPC completion is never replayed. No-progress time counts sampled
    bounded input commands; attempted command time is retained separately,
    while all observation/transport time still counts against the wall deadline.
    The hard limit remains at most one native unit; this
    is an observed safety envelope, not a collision/route guarantee.
    """
    if (not finite_number(target_distance) or not finite_number(max_displacement)
            or not .05 <= target_distance < max_displacement <= 1.0
            or not finite_number(max_seconds) or not .5 <= max_seconds <= 5.0
            or not finite_number(progress_timeout) or not .1 <= progress_timeout < max_seconds
            or not finite_number(cruise_magnitude) or not .08 <= cruise_magnitude <= .45
            or not finite_number(brake_magnitude) or not .05 <= brake_magnitude < cruise_magnitude
            or not finite_number(brake_fraction) or not .25 <= brake_fraction <= .75
            or not finite_number(pulse_seconds) or not .03 <= pulse_seconds <= .25
            or type(max_unsampled_retries) is not int or not 0 <= max_unsampled_retries <= 2):
        raise BotFault("Locomotion probe bounds require target < hard limit <= 1 unit, time <= 5 seconds, and pulse 0.03..0.25 seconds")

    source = _axis_source(live.bindings, "axes.move")
    hand = source.split("_", 1)[0]
    _stick_y_index(source)
    value = {"hand": hand, "component": "Thumbstick", "sub_component": "Y",
             "value": cruise_magnitude}
    timeline = []
    pulses = []
    started = clock()
    deadline = started + max_seconds
    held = False
    reached_target = False
    settle_stable = False
    final_state = None
    current_phase = "cruise"
    phase_floor_ms = None
    phase_counts = {"cruise": 0, "braking": 0}
    speed_native_units_per_second = 0.0
    braking_started_distance = None

    # This uses only the process-private OpenXR lease. Release first so a
    # forgotten prior channel cannot turn the probe into a compound action.
    live.release()
    admission = live.observe(native=True)
    controls, admission_ms = _validate_field_state(admission)
    _neutral_audit(controls)
    origin = _position(admission)
    native = admission["native"]
    phase_floor_ms = controls["sample_ms"]
    identity = (native["mission"], native["location"], native["sequence"], admission.get("activation"))
    anchor = {"identity": identity}
    live.events.emit("locomotion_started", input={"axis": "axes.move", "source": source,
                      "hand": hand, "direction": "stick_up", "cruise_value": cruise_magnitude,
                      "brake_value": brake_magnitude},
                     admission_sample_ms=admission_ms, origin=list(origin),
                     identity={"mission": identity[0], "location": identity[1],
                               "sequence": identity[2], "activation": identity[3]},
                     target_distance=target_distance, max_displacement=max_displacement,
                     maximum_seconds=max_seconds, maximum_pulse_seconds=pulse_seconds,
                     maximum_unsampled_retries=max_unsampled_retries,
                     progress_clock="sampled_expiring_input_seconds",
                     input_evidence="fresh XR resolver stick samples required; retail consumption is not inferred")

    guard = ProgressGuard(timeout=progress_timeout, minimum_progress=.015)
    # A released Lua read took 1.422 seconds in approach-05. That latency is
    # part of the unchanged wall deadline, not evidence that the actor ignored
    # another second of movement: only one .10 second pulse had been sent.
    commanded_input_seconds = 0.0
    sampled_input_seconds = 0.0
    unsampled_attempts = 0
    guard.observe(target_distance, commanded_input_seconds)
    samples_with_input = 0
    sample_count = 0
    travelled = 0.0

    def neutral_cadence():
        nonlocal phase_floor_ms
        if held:
            raise BotFault("Release movement before waiting for neutral XR cadence")
        # xrEndFrame blocked 654ms in approach-06, swallowing the whole axis
        # lease. Wait for three advancing neutral publications spanning 60ms,
        # with no gap above100ms. Neither time nor freshness is relabeled.
        cadence_deadline = min(deadline, clock()+1.0)
        first_ms = last_ms = None
        advancing = 0
        while clock() < cadence_deadline:
            read_started = clock()
            state = live.observe(native=False)
            read_finished = clock()
            if read_finished >= deadline:
                raise BotFault("Neutral XR recovery exceeded the bounded segment deadline")
            if read_finished >= cadence_deadline:
                break
            _validate_xr_scene_state(state, activation=identity[3])
            if state.get("controls") is None:
                first_ms = last_ms = None
                advancing = 0
            else:
                controls, _ = _validate_xr_field_state(state, activation=identity[3])
                _neutral_audit(controls)
                sample_ms = controls["sample_ms"]
                fresh = controls["age_ms"]+(read_finished-read_started)*1000 <= 250
                if not fresh:
                    first_ms = last_ms = None
                    advancing = 0
                elif sample_ms > phase_floor_ms and (last_ms is None or sample_ms > last_ms):
                    if last_ms is None or sample_ms-last_ms > 100:
                        first_ms, advancing = sample_ms, 0
                    last_ms = sample_ms
                    advancing += 1
                    live.events.emit("locomotion_neutral_cadence", sample_ms=sample_ms,
                                     age_ms=controls["age_ms"], advancing_samples=advancing,
                                     span_ms=sample_ms-first_ms)
                    if advancing >= 3 and sample_ms-first_ms >= 60:
                        phase_floor_ms = sample_ms
                        return
            sleep(min(.02, max(0., cadence_deadline-clock())))
        raise BotFault("Neutral XR publication did not resume bounded fresh cadence")

    def neutral_position(phase):
        nonlocal sample_count, final_state, phase_floor_ms
        # A slow Lua queue cannot extend movement: every call is after release.
        if held:
            raise BotFault("Release movement before reading native player position")
        if clock() >= deadline:
            raise BotFault("Native movement observation exceeded the bounded segment deadline")
        read_started = clock()
        state = live.observe(native=True)
        read_finished = clock()
        controls, now_ms = _validate_field_state(state, anchor)
        _neutral_audit(controls)
        position = _position(state)
        distance = _distance(origin, position)
        sample_count += 1
        final_state = state
        phase_floor_ms = controls["sample_ms"]
        timeline.append({"sample_ms": now_ms, "phase": "neutral_settle",
                         "movement_phase": phase, "position": list(position),
                         "observation_seconds": read_finished-read_started,
                         "displacement": distance})
        live.events.emit("locomotion_sample", held=False, phase=phase,
                         observation_seconds=read_finished-read_started,
                         position=list(position), displacement=distance)
        if distance > max_displacement:
            raise BotFault("Native displacement exceeded the bounded movement envelope after release")
        if clock() >= deadline:
            raise BotFault("Native movement observation exceeded the bounded segment deadline")
        return position, distance

    try:
        while clock() < deadline:
            # Never renew a pulse across a native query. Lowering magnitude
            # provides the braking phase; the expiry cap also bounds dispatch
            # and observation failures independently of the runner thread.
            duration = pulse_seconds
            if deadline - clock() < duration:
                break
            before_distance = travelled
            before_position = _position(final_state) if final_state else origin
            pulse_started = clock()
            pulse_deadline = pulse_started + duration
            pulse_samples = 0
            try:
                # Mark held before dispatch so a partial RPC failure is released.
                held = True
                live.input([value], duration, lease_seconds=duration)
                while clock() < pulse_deadline:
                    read_started = clock()
                    state = live.observe(native=False)
                    read_finished = clock()
                    _validate_xr_scene_state(state, activation=identity[3])
                    # Missing controls are the explicit stale-publication
                    # representation, not permission to accept older input.
                    if state.get("controls") is None:
                        sleep(min(.01, max(0., pulse_deadline-clock())))
                        continue
                    controls, _ = _validate_xr_field_state(state, activation=identity[3])
                    audit = _sampled_move_input(state, source, value["value"],
                                                after_sample_ms=phase_floor_ms)
                    # The publication may have been sampled before a blocked
                    # read. Credit it only while this exact pulse is still live.
                    if (audit and read_finished < pulse_deadline
                            and controls["age_ms"] + (read_finished-read_started)*1000 <= 250):
                        pulse_samples += 1
                        samples_with_input += 1
                        phase_counts[current_phase] += 1
                        phase_floor_ms = controls["sample_ms"]
                        timeline.append({**audit, "phase": current_phase,
                                         "pulse_index": len(pulses), "position": None,
                                         "displacement": None})
                        live.events.emit("locomotion_sample", held=True,
                                         phase=current_phase, audit=audit,
                                         position=None, displacement=None)
                    sleep(min(.01, max(0., pulse_deadline-clock())))
            finally:
                if held:
                    live.release()
                    held = False
                    live.events.emit("locomotion_released", target_reached=False,
                                     pulse_index=len(pulses), expiry_seconds=duration)

            commanded_input_seconds += duration
            pulse = {"phase": current_phase, "duration_seconds": duration,
                     "sampled_input_count": pulse_samples,
                     "attempted_input_seconds": commanded_input_seconds}
            pulses.append(pulse)
            if not pulse_samples:
                unsampled_attempts += 1
                live.events.emit("locomotion_unsampled_attempt", pulse_index=len(pulses)-1,
                                 attempts=unsampled_attempts,
                                 attempted_input_seconds=commanded_input_seconds,
                                 sampled_input_seconds=sampled_input_seconds)
                neutral_cadence()
            position, travelled = neutral_position(current_phase)
            pulse.update(position_after_release=list(position), distance_after_release=travelled)
            live.events.emit("locomotion_pulse", **pulse)
            pulse["commanded_input_seconds"] = commanded_input_seconds

            # Inertia is measured with neutral controls. No new input is allowed
            # while position reads are delayed or the previous pulse is moving.
            settle_deadline = min(deadline, clock() + min(.9, max_seconds * .25))
            stable_samples = 0
            previous = position
            settle_stable = False
            while clock() < settle_deadline:
                sleep(min(.03, max(0., settle_deadline-clock())))
                position, travelled = neutral_position(current_phase)
                delta = _distance(previous, position)
                timeline[-1]["step_distance"] = delta
                stable_samples = stable_samples + 1 if delta <= .015 else 0
                previous = position
                if stable_samples >= 2:
                    settle_stable = True
                    break
            if not settle_stable:
                raise BotFault("Native player position did not settle promptly after neutral input")
            pulse["settled_position"] = list(position)
            pulse["settled_distance"] = travelled
            if not pulse_samples:
                # A native displacement without sampled intent is not replay
                # permission, even if it happens to cross the short target.
                if _distance(before_position, position) > .015:
                    raise BotFault("Movement pulse lacks a fresh XR sample and changed native position; do not retry")
                if unsampled_attempts > max_unsampled_retries:
                    raise BotFault("Movement pulse has no fresh XR sample for each movement phase/pulse; retry budget exhausted")
                pulse["outcome"] = "unsampled_unchanged_endpoint_retry_admitted"
                live.events.emit("locomotion_unsampled_retry_admitted", pulse_index=len(pulses)-1,
                                 position=list(position), sampled_input_seconds=sampled_input_seconds,
                                 attempted_input_seconds=commanded_input_seconds)
                continue
            sampled_input_seconds += duration
            pulse["sampled_input_seconds"] = sampled_input_seconds
            pulse["outcome"] = "sampled_neutral_endpoint"
            speed_native_units_per_second = max(speed_native_units_per_second,
                                                max(0., travelled-before_distance)/duration)
            guard.observe(max(0., target_distance-travelled), sampled_input_seconds)
            if travelled >= target_distance:
                reached_target = True
                break
            if current_phase == "cruise" and travelled >= target_distance * brake_fraction:
                current_phase = "braking"
                braking_started_distance = travelled
                value = {**value, "value": brake_magnitude}
                live.events.emit("locomotion_braking", position=list(position),
                                 displacement=travelled,
                                 estimated_speed_native_units_per_second=speed_native_units_per_second,
                                 requested_stick_value=brake_magnitude,
                                 xr_sample_floor_ms=phase_floor_ms)
        if not reached_target:
            raise BotFault("The native player did not reach the short displacement target before the deadline")
    finally:
        if held:
            live.release()
            held = False
            live.events.emit("locomotion_released", target_reached=reached_target,
                             last_position=list(_position(final_state)) if final_state else None)

    endpoint = _position(final_state)
    displacement = [endpoint[index] - origin[index] for index in range(3)]
    distance = _distance(origin, endpoint)
    result = {
        "status": "observed_displacement",
        "input": {"axis": "axes.move", "source": source, "hand": hand,
                  "direction": "stick_up", "cruise_value": cruise_magnitude,
                  "brake_value": brake_magnitude, "brake_started_at_native_distance": braking_started_distance},
        "origin": list(origin),
        "endpoint": list(endpoint),
        "displacement": displacement,
        "distance_native_units": distance,
        "target_distance_native_units": target_distance,
        "target_reached": distance >= target_distance,
        "native_identity": {"mission": identity[0], "location": identity[1],
                            "sequence": identity[2], "activation": identity[3]},
        "xr_sampled_input_count": samples_with_input,
        "input_samples_by_phase": phase_counts,
        "sample_count": sample_count,
        "held_audit_timeline": timeline,
        "input_strategy": "expiring_pulses_with_neutral_native_reads",
        "maximum_pulse_seconds": pulse_seconds,
        "commanded_input_seconds": commanded_input_seconds,
        "attempted_input_seconds": commanded_input_seconds,
        "sampled_input_seconds": sampled_input_seconds,
        "unsampled_attempts": unsampled_attempts,
        "maximum_unsampled_retries": max_unsampled_retries,
        "progress_clock": "sampled_expiring_input_seconds",
        "pulses": pulses,
        "settled_after_release": settle_stable,
        "route_or_collision_proof": False,
        "xinput_consumption_proven": False,
    }
    live.events.emit("locomotion_finished", result=result)
    return result
