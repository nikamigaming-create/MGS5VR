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


def _validate_field_state(state, anchor=None):
    if not isinstance(state, dict) or state.get("scene") != "gameplay":
        raise BotFault("Locomotion requires an observed gameplay scene")
    if state.get("camera_active") is not True or state.get("camera_available") is not True:
        raise BotFault("Locomotion requires the active immersive player camera")
    if state.get("menu") is not False or state.get("idroid") is not False:
        raise BotFault("Locomotion is unavailable while a native menu is open")
    if state.get("gamepad") is not False:
        raise BotFault("Locomotion is unavailable while physical XInput owns gameplay")
    controls = authoritative_controls(state)
    if controls.get("context") != "gameplay" or controls.get("rig_input") is not True:
        raise BotFault("Native XR controls are not in the live gameplay rig")
    if type(controls.get("travel_mode")) is not int or controls["travel_mode"] != 1:
        raise BotFault("The observed native travel mode is not on foot")

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

    now_ms = state.get("now_ms")
    if type(now_ms) is not int or now_ms < 0:
        raise BotFault("Native observation time is invalid")
    sample = controls.get("physical")
    sticks = controls.get("sticks")
    if not isinstance(sample, list) or len(sample) < 11:
        raise BotFault("Fresh physical input audit is unavailable")
    if not isinstance(sticks, list) or len(sticks) != 4:
        raise BotFault("Fresh physical stick audit is unavailable")
    if any(not finite_number(value) for value in sample[:11] + sticks):
        raise BotFault("Physical input audit contains a nonfinite value")
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
               brake_fraction=.45, clock=time.monotonic, sleep=time.sleep):
    """Walk a short configured-stick segment with a lower-speed braking phase.

    Distances are native game-coordinate units: no meter conversion is assumed.
    The routine lowers forward input before the target, releases on the measured
    target crossing, and verifies drift after release. The hard displacement
    limit cannot exceed one native unit and the complete segment is bounded by
    five seconds. This is not a route or collision-clearance test.
    """
    if (not finite_number(target_distance) or not finite_number(max_displacement)
            or not .05 <= target_distance < max_displacement <= 1.0
            or not finite_number(max_seconds) or not .5 <= max_seconds <= 5.0
            or not finite_number(progress_timeout) or not .1 <= progress_timeout < max_seconds
            or not finite_number(cruise_magnitude) or not .08 <= cruise_magnitude <= .45
            or not finite_number(brake_magnitude) or not .05 <= brake_magnitude < cruise_magnitude
            or not finite_number(brake_fraction) or not .25 <= brake_fraction <= .75):
        raise BotFault("Locomotion probe bounds must be target < hard limit <= 1 unit and time <= 5 seconds")

    source = _axis_source(live.bindings, "axes.move")
    hand = source.split("_", 1)[0]
    _stick_y_index(source)
    value = {"hand": hand, "component": "Thumbstick", "sub_component": "Y",
             "value": cruise_magnitude}
    anchor = None
    timeline = []
    started = clock()
    deadline = started + max_seconds
    held = False
    reached_target = False
    settle_stable = False
    last_position = None
    final_state = None
    current_phase = "cruise"
    phase_floor_ms = None
    phase_started = None
    phase_counts = {"cruise": 0, "braking": 0}
    speed_native_units_per_second = 0.0
    previous_progress = 0.0
    previous_sample_ms = None
    braking_started_distance = None

    # This uses only the process-private OpenXR lease. Release first so a
    # forgotten prior channel cannot turn the probe into a compound action.
    live.release()
    admission = live.observe(native=True)
    controls, admission_ms = _validate_field_state(admission)
    _neutral_audit(controls)
    origin = _position(admission)
    native = admission["native"]
    phase_floor_ms = admission_ms
    previous_sample_ms = admission_ms
    identity = (native["mission"], native["location"], native["sequence"], admission.get("activation"))
    anchor = {"identity": identity}
    live.events.emit("locomotion_started", input={"axis": "axes.move", "source": source,
                      "hand": hand, "direction": "stick_up", "cruise_value": cruise_magnitude,
                      "brake_value": brake_magnitude},
                     admission_sample_ms=admission_ms, origin=list(origin),
                     identity={"mission": identity[0], "location": identity[1],
                               "sequence": identity[2], "activation": identity[3]},
                     target_distance=target_distance, max_displacement=max_displacement,
                     maximum_seconds=max_seconds,
                     input_evidence="fresh XR resolver stick samples required; retail consumption is not inferred")

    guard = ProgressGuard(timeout=progress_timeout, minimum_progress=.015)
    guard.observe(target_distance, clock())
    samples_with_input = 0
    sample_count = 0
    try:
        # Mark held before dispatch so a partial RPC failure still releases all
        # channels in finally.
        held = True
        phase_started = clock()
        lease = max(.03, min(5.0, deadline - clock()))
        live.input([value], max_seconds, lease_seconds=lease)
        while clock() < deadline:
            state = live.observe(native=True)
            _, now_ms = _validate_field_state(state, anchor)
            position = _position(state)
            travelled = _distance(origin, position)
            if travelled > max_displacement:
                raise BotFault("Native displacement exceeded the bounded movement envelope")

            if previous_sample_ms is not None and now_ms > previous_sample_ms:
                sample_seconds = (now_ms - previous_sample_ms) / 1000.0
                sampled_speed = max(0.0, (travelled - previous_progress) / sample_seconds)
                speed_native_units_per_second = max(sampled_speed, speed_native_units_per_second * .65)
            audit = _sampled_move_input(state, source, value["value"],
                                        after_sample_ms=phase_floor_ms)
            sample_count += 1
            if audit:
                samples_with_input += 1
                phase_counts[current_phase] += 1
                timeline.append({**audit, "phase": current_phase,
                                 "position": list(position), "displacement": travelled,
                                 "estimated_speed_native_units_per_second": speed_native_units_per_second})
                live.events.emit("locomotion_sample", held=True, phase=current_phase, audit=audit,
                                 position=list(position), displacement=travelled)
            else:
                live.events.emit("locomotion_sample", held=True, phase=current_phase, audit=None,
                                 position=list(position), displacement=travelled,
                                 requested_stick_value=value["value"])

            guard.observe(max(0.0, target_distance - travelled), clock())
            if travelled >= target_distance:
                reached_target = True
                final_state = state
                break
            if phase_counts[current_phase] == 0 and clock() - phase_started >= min(.6, progress_timeout):
                raise BotFault(f"The {current_phase} stick value was not present in a fresh XR sample")

            # Full analog drive and the old post-target polling interval both
            # carried Snake past this short waypoint. Taper the same effective
            # axis while there is still room, then stop only on native position.
            if current_phase == "cruise" and travelled >= target_distance * brake_fraction:
                current_phase = "braking"
                braking_started_distance = travelled
                phase_floor_ms = now_ms
                phase_started = clock()
                value = {**value, "value": brake_magnitude}
                lease = max(.03, min(5.0, deadline - clock()))
                live.input([value], max_seconds, lease_seconds=lease)
                live.events.emit("locomotion_braking", position=list(position),
                                 displacement=travelled,
                                 estimated_speed_native_units_per_second=speed_native_units_per_second,
                                 requested_stick_value=brake_magnitude,
                                 xr_sample_floor_ms=phase_floor_ms)

            previous_progress = travelled
            previous_sample_ms = now_ms
            sleep(min(.05, max(0.0, deadline - clock())))
        if not reached_target:
            raise BotFault("The native player did not reach the short displacement target before the deadline")
    finally:
        if held:
            live.release()
            held = False
            live.events.emit("locomotion_released", target_reached=reached_target,
                             last_position=list(_position(final_state)) if final_state else None)

    # Crossing the target on the first post-dispatch observation must not
    # bypass evidence for a phase whose new stick value was never sampled.
    required_phases = ("cruise", "braking") if braking_started_distance is not None else ("cruise",)
    if any(phase_counts[phase] == 0 for phase in required_phases):
        raise BotFault("Target reached without a fresh XR sample for each movement phase")

    # After release, wait for the native position to settle. This detects
    # inertia/overshoot rather than reporting only the position at button-up.
    settle_deadline = min(deadline, clock() + min(.9, max_seconds * .25))
    stable_samples = 0
    previous = _position(final_state)
    while clock() < settle_deadline:
        state = live.observe(native=True)
        _validate_field_state(state, anchor)
        position = _position(state)
        travelled = _distance(origin, position)
        if travelled > max_displacement:
            raise BotFault("Native displacement exceeded the bounded movement envelope after release")
        delta = _distance(previous, position)
        stable_samples = stable_samples + 1 if delta <= .015 else 0
        timeline.append({"sample_ms": state["now_ms"], "phase": "neutral_settle",
                         "position": list(position), "displacement": travelled,
                         "step_distance": delta})
        live.events.emit("locomotion_sample", held=False, position=list(position),
                         displacement=travelled, step_distance=delta)
        final_state = state
        if stable_samples >= 2:
            settle_stable = True
            break
        previous = position
        sleep(min(.05, max(0.0, settle_deadline - clock())))
    if not settle_stable:
        raise BotFault("Native player position did not settle promptly after neutral input")

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
        "settled_after_release": settle_stable,
        "route_or_collision_proof": False,
        "xinput_consumption_proven": False,
    }
    live.events.emit("locomotion_finished", result=result)
    return result
