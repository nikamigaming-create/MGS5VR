"""Bounded, observed head/hand trajectories under the maintained input lease."""
import math
import time

from .core import BotFault, matches


def _pose(position, orientation, *, local_head=False):
    if (not isinstance(position, list) or len(position) != 3
            or not isinstance(orientation, list) or len(orientation) != 4
            or any(type(v) not in (int, float) or not math.isfinite(v)
                   for v in position + orientation)
            or max(abs(v) for v in position) > (3 if local_head else 2)
            or not .99 < math.sqrt(sum(v*v for v in orientation)) < 1.01):
        raise BotFault("Motion needs finite positions and normalized rotations within the local envelope")
    return {"position": list(position), "orientation": list(orientation)}


def _read(live, tool, arguments, *, local_head=False):
    result = live.text_result(live.call(tool, arguments))
    flags, pose = result.get("flags", {}), result.get("pose", {})
    if flags.get("position_valid") is not True or flags.get("orientation_valid") is not True:
        raise BotFault("Motion requires a fresh valid operator pose")
    return _pose(pose.get("position"), pose.get("orientation"), local_head=local_head)


def _blend(start, end, weight):
    a, b = start["orientation"], end["orientation"]
    if sum(x*y for x, y in zip(a, b)) < 0:
        b = [-v for v in b]
    q = [x + (y-x)*weight for x, y in zip(a, b)]
    length = math.sqrt(sum(v*v for v in q))
    return {"position": [x + (y-x)*weight for x, y in zip(start["position"], end["position"])],
            "orientation": [v/length for v in q]}


def _generation(state):
    frame = state.get("rendered") or {}
    key = tuple(frame.get(k) for k in ("player_owner", "activation", "presentation_epoch"))
    if (state.get("camera_active") is not True or state.get("camera_suspended") is not False
            or frame.get("presentation_focused") is not True
            or not frame.get("rig_sequence") or any(type(v) is not int or v <= 0 for v in key)
            or any(frame.get("raw_" + hand + "_" + kind + "_tracked") is not True
                   for hand in ("left", "right") for kind in ("grip", "aim"))):
        raise BotFault("Motion requires focused raw tracking and a current owned gameplay rig")
    tick, sampled = state.get("now_ms"), frame.get("sample_ms")
    if type(tick) is not int or type(sampled) is not int or not 0 <= tick-sampled <= 250:
        raise BotFault("Motion requires a fresh rig publication")
    return key


def move(live, step, *, clock=time.monotonic, sleep=time.sleep):
    """Interleave normalized poses, observing every packet rather than sleeping.

    The operator has separate grip/aim RPCs. Their serialization is recorded;
    this is simulated controller motion, not an atomic physical device sample.
    Independent head motion reapplies both hands in LOCAL space, keeping the
    device fixed in the world instead of dragging it with the headset.
    """
    seconds = step.get("seconds")
    hands, head = step.get("hands", []), step.get("head")
    if (live.held or type(seconds) not in (int, float) or not math.isfinite(seconds)
            or not .2 <= seconds <= 10 or not isinstance(hands, list) or len(hands) > 2
            or (not hands and head is None)):
        raise BotFault("Motion needs released input, targets, and a bounded 0.2..10 second duration")
    targets, seen = [], set()
    # Validate the entire request before any pose mutation or partial dispatch.
    for hand in hands:
        if not isinstance(hand, dict):
            raise BotFault("Each motion hand needs an explicit paired pose")
        side, space = hand.get("hand"), hand.get("base_space", "view")
        if side not in ("left", "right") or side in seen or space not in ("local", "view"):
            raise BotFault("Invalid or duplicate motion hand/reference space")
        seen.add(side)
        for kind, prefix in (("grip", ""), ("aim", "aim_")):
            target = _pose(hand.get(prefix + "position"), hand.get(prefix + "orientation"))
            targets.append(("set_controller_pose", {"hand": side, "pose_type": kind,
                                                    "base_space": space}, target))
    if head is not None:
        if (not isinstance(head, dict) or set(head) != {"offset", "orientation"}
                or not isinstance(head["offset"], list) or len(head["offset"]) != 3
                or any(type(v) not in (int, float) or not math.isfinite(v) for v in head["offset"])
                or math.sqrt(sum(v*v for v in head["offset"])) > .35):
            raise BotFault("Head motion requires a bounded local offset of at most 0.35 metres")
        _pose([0., 0., 0.], head["orientation"])
    state = live.observe(native=True)
    generation = _generation(state)
    if step.get("state_before") and not matches(state, step["state_before"]):
        raise BotFault("Motion entry predicate changed; no pose dispatched")
    if state.get("scene") not in ("gameplay", "cabin", "menu") or any(
            state.get(k) is not False for k in ("loading", "title", "demo")):
        raise BotFault("Motion requires a playable scene or its owned menu")
    paths = []
    if head is not None:
        arguments = {"base_space": "local"}
        start = _read(live, "get_head_pose", arguments, local_head=True)
        end = _pose([p+d for p, d in zip(start["position"], head["offset"])],
                    head["orientation"], local_head=True)
        paths.append(("set_head_pose", arguments, start, end))
        for side in ("left", "right"):
            if side not in seen:
                for kind in ("grip", "aim"):
                    arguments = {"hand": side, "pose_type": kind, "base_space": "local"}
                    pose = _read(live, "get_controller_pose", arguments)
                    paths.append(("set_controller_pose", arguments, pose, pose))
    for tool, arguments, end in targets:
        start = _read(live, "get_controller_pose", arguments)
        paths.append((tool, arguments, start, end))
    # Recheck after readbacks; a changed native owner cannot inherit the path.
    if _generation(live.observe()) != generation:
        raise BotFault("Motion owner changed before dispatch")
    begun = last_packet = clock()
    packets = 0
    largest_gap = 0.
    live.events.emit("continuous_motion_started", seconds=seconds, step=step,
                     generation=generation, pose_rpc_atomic=False, target_hz=20)
    while True:
        now = clock()
        weight = min(1., (now-begun)/seconds)
        gap = now-last_packet
        largest_gap = max(largest_gap, gap)
        if gap > .5:
            raise BotFault("Continuous motion stalled for more than 500 ms; preserve failure evidence")
        for tool, arguments, start, end in paths:
            live.call(tool, {**arguments, **_blend(start, end, weight)})
        sampled = live.observe()
        if _generation(sampled) != generation:
            raise BotFault("Motion generation changed or lost its rig during the trajectory")
        live.events.emit("continuous_motion_sample", fraction=weight, native_tick=sampled["now_ms"],
                         rig_sequence=sampled["rendered"]["rig_sequence"], packet_seconds=clock()-now)
        packets += 1
        last_packet = now
        if weight >= 1:
            break
        sleep(max(0., min(.05 - (clock()-now), seconds-(clock()-begun))))
    for tool, arguments, start, target in paths:
        end = _read(live, "get_head_pose" if tool == "set_head_pose" else "get_controller_pose",
                    arguments, local_head=tool == "set_head_pose")
        if any(abs(a-b) > 1e-4 for a, b in zip(end["position"], target["position"])) \
                or abs(sum(a*b for a, b in zip(end["orientation"], target["orientation"]))) < .9999:
            raise BotFault("Motion endpoint readback did not reach its authored target")
    live.events.emit("continuous_motion_completed", seconds=clock()-begun, packets=packets,
                     largest_packet_gap_seconds=largest_gap, generation=generation,
                     evidence_limit="Observed simulated motion; physical headset acceptance remains separate")
