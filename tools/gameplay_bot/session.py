"""Run arrival and a complete behavior queue on the same live connection."""
import math
from contextlib import contextmanager, ExitStack

from .core import BotFault


NORMAL_VIEW_HAND_POSES = (
    {"op": "pose", "hand": "left", "position": [-0.30, -0.30, -0.555],
     "orientation": [0.0, 0.0, 0.0, 1.0]},
    {"op": "pose", "hand": "right", "position": [0.30, -0.30, -0.555],
     "orientation": [0.0, 0.0, 0.0, 1.0]},
)


@contextmanager
def preserved_head_pose(adapter):
    """Restore the exact starting headset transform even after a failed path."""
    from .motion import _read
    saved = _read(adapter, "get_head_pose", {"base_space": "local"}, local_head=True)
    adapter.events.emit("head_pose_saved", pose=saved, source="operator_readback")
    try:
        yield saved
    finally:
        release_error = None
        try:
            adapter.release()
        except Exception as error:
            release_error = error
        try:
            adapter.call("set_head_pose", {"base_space": "local", **saved})
            verified = _read(adapter, "get_head_pose", {"base_space": "local"}, local_head=True)
            if any(abs(a-b) > 1e-5 for a, b in zip(saved["position"] + saved["orientation"],
                                                 verified["position"] + verified["orientation"])):
                raise BotFault("Restored head pose does not match its saved readback")
            adapter.events.emit("head_pose_restored", pose=saved, verified=True)
        except Exception as error:
            raise BotFault("Could not restore saved head pose: " + str(error)) from error
        if release_error is not None:
            raise BotFault("Could not neutralize input before restoring head pose: " + str(release_error)) from release_error


def _controller_pose(adapter, hand, pose_type, base_space):
    arguments = {"hand": hand, "pose_type": pose_type, "base_space": base_space}
    raw = adapter.call("get_controller_pose", arguments)
    text_result = getattr(adapter, "text_result", None)
    result = text_result(raw) if callable(text_result) else raw
    pose = result.get("pose") if isinstance(result, dict) else None
    flags = result.get("flags") if isinstance(result, dict) else None
    position = pose.get("position") if isinstance(pose, dict) else None
    orientation = pose.get("orientation") if isinstance(pose, dict) else None
    if (not isinstance(flags, dict) or flags.get("position_valid") is not True
            or flags.get("orientation_valid") is not True
            or not isinstance(position, list) or len(position) != 3
            or not isinstance(orientation, list) or len(orientation) != 4
            or any(type(value) not in (int, float) or not math.isfinite(value)
                   for value in position + orientation)):
        raise BotFault(f"Cannot safely preserve {hand} {pose_type} pose in {base_space} space")
    return {**arguments, "position": list(position), "orientation": list(orientation)}


def _restore_controller_pose(adapter, pose):
    adapter.call("set_controller_pose", dict(pose))
    verified = _controller_pose(adapter, pose["hand"], pose["pose_type"], pose["base_space"])
    if any(abs(a-b) > 1e-5 for a, b in zip(pose["position"] + pose["orientation"],
                                             verified["position"] + verified["orientation"])):
        raise BotFault("Restored controller pose does not match its saved readback")
    events = getattr(adapter, "events", None)
    if events is not None and callable(getattr(events, "emit", None)):
        events.emit("controller_pose_restored", **pose, verified=True)


@contextmanager
def preserved_controller_pose(adapter, hand="right", pose_type="grip", base_space="local"):
    """Temporarily change one pose and restore its validated value in cleanup.

    The adapter's RPC layer still owns process-generation and transport checks;
    a failure there is surfaced instead of sending a pose to a different game.
    """
    saved = _controller_pose(adapter, hand, pose_type, base_space)
    events = getattr(adapter, "events", None)
    if events is not None and callable(getattr(events, "emit", None)):
        events.emit("controller_pose_saved", **saved, source="operator_readback")
    try:
        yield saved
    finally:
        release_error = None
        try:
            adapter.release()
        except Exception as error:
            release_error = error
        restore_error = None
        try:
            _restore_controller_pose(adapter, saved)
        except Exception as error:
            restore_error = error
        if restore_error is not None:
            raise BotFault(f"Could not restore saved {hand} {pose_type} pose: {restore_error}") from restore_error
        if release_error is not None:
            raise BotFault(f"Could not neutralize inputs before restoring {hand} {pose_type} pose: {release_error}") from release_error


@contextmanager
def preserved_tracking_poses(adapter):
    # set_head_pose also moves simulated controllers. Restore the head FIRST,
    # then the exact saved LOCAL grip/aim transforms; the reverse order would
    # drag already-restored hands away after a failed headset trajectory.
    with ExitStack() as saved:
        for hand in ("left", "right"):
            for kind in ("grip", "aim"):
                saved.enter_context(preserved_controller_pose(adapter, hand, kind, "local"))
        saved.enter_context(preserved_head_pose(adapter))
        yield


def reset_post_continue_hand_poses(behavior, arrival):
    """Clear the title-rack pose after a real Continue, preserving warm poses."""
    if arrival.get("continued_from_title") is not True:
        return None

    if arrival.get("controller_pose_restored") is True:
        reset = {"status": "preserved", "pose": arrival.get("controller_pose_restore")}
        events = getattr(behavior, "events", None)
        if events is not None and callable(getattr(events, "emit", None)):
            events.emit("post_continue_hand_pose_preserved", **reset)
        return reset

    adapter = behavior.adapter
    adapter.release()
    for pose in NORMAL_VIEW_HAND_POSES:
        adapter.execute(dict(pose))

    # Non-recorded sessions still retain a readback proving the final operator
    # pose. Recorded sessions already read poses after each setup operation.
    if not getattr(adapter, "pose_recording", False):
        snapshot = getattr(adapter, "pose_snapshot", None)
        if callable(snapshot):
            snapshot("post-continue-normal-hands")

    reset = {
        "status": "applied",
        "space": "view",
        "poses": [dict(pose) for pose in NORMAL_VIEW_HAND_POSES],
    }
    events = getattr(behavior, "events", None)
    if events is not None and callable(getattr(events, "emit", None)):
        events.emit("post_continue_hand_pose_reset", **reset)
    return reset


def run_suite(behavior, suite, checkpoint, enter_game=None):
    required_settings = suite.get("required_settings", {})
    if not isinstance(required_settings, dict):
        raise BotFault("Suite required_settings must be a mapping")
    if required_settings:
        bindings = getattr(behavior.adapter, "bindings", {})
        configured = {entry["name"]: entry["value"] for entry in bindings.get("settings", [])}
        for name, value in required_settings.items():
            if name not in configured or type(configured[name]) is not type(value) or configured[name] != value:
                raise BotFault(f"Suite requires {name}={value}; effective setting is {configured.get(name, 'unavailable')}")
    cases = suite.get("cases")
    if not isinstance(cases, list) or not cases:
        raise BotFault("A suite needs nonempty, uniquely identified cases")
    identifiers = [case.get("id") if isinstance(case, dict) else None for case in cases]
    if (any(not isinstance(value, str) or not value for value in identifiers)
            or len(set(identifiers)) != len(identifiers)):
        raise BotFault("A suite needs nonempty, uniquely identified cases")
    # Reject an incomplete queue before Continue or any other gameplay input.
    if any(not case.get("before") or not case.get("after") or not case.get("steps") for case in cases):
        raise BotFault("Every queued case needs before, steps and after")
    for index, case in enumerate(cases):
        dependencies = case.get("depends_on", [])
        if (not isinstance(dependencies, list)
                or any(not isinstance(item, str) or item not in identifiers[:index] for item in dependencies)):
            raise BotFault("Case dependencies must name earlier cases in this queue")

    arrival = enter_game() if enter_game else None
    if arrival is not None and arrival.get("status") != "observed_arrival":
        raise BotFault("Session did not observe arrival; queue was not started")
    hand_pose_reset = reset_post_continue_hand_poses(behavior, arrival) if arrival else None
    records = []
    passed = set()
    for case in cases:
        if any(item not in passed for item in case.get("depends_on", [])):
            record = {"id": case["id"], "status": "skipped", "reason": "required earlier case did not pass"}
        else:
            record = behavior.case(case)
        records.append(record)
        checkpoint(records)
        if record["status"] == "observed_pass":
            passed.add(case["id"])
        elif record["status"] == "skipped":
            continue
        # An ambiguous failure cannot authorize the next dependent action.
        # Successful cases immediately advance; there is no per-clip stop.
        elif not (suite.get("continue_after_outcome_failure") is True
                  and record.get("failure_phase") == "outcome"
                  and record.get("entry_predicates_still_match") is True
                  and not record.get("release_error") and not record.get("capture_error")):
            break
    result = {
        "status": "observed_pass" if len(records) == len(cases)
                  and all(record["status"] == "observed_pass" for record in records) else "failed",
        "cases": records,
        "not_run": identifiers[len(records):],
        "visual_acceptance": "pending",
    }
    if arrival is not None:
        result["arrival"] = arrival
    if hand_pose_reset is not None:
        result["hand_pose_reset"] = hand_pose_reset
    return result
