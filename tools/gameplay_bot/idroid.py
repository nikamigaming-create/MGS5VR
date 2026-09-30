"""Handheld iDroid attachment and stationary-navigation acceptance sequence."""
import math
import time

from .core import BotFault
from .live import ControlSampleUnavailable, authoritative_controls, fresh_control_observation


def position(state):
    native = state.get("native", {})
    values = [native.get("player_" + axis) for axis in "xyz"]
    if any(isinstance(value, bool) or not isinstance(value, (int, float))
           or not math.isfinite(value) for value in values):
        raise BotFault("Fresh native player position is required for the iDroid movement check")
    return values


def distance(a, b):
    return math.sqrt(sum((x-y)**2 for x, y in zip(a, b)))


def menu_axis(bindings):
    source = next((row["source"] for row in bindings.get("axes", []) if row["name"] == "axes.menu"), None)
    if source not in ("left_stick", "right_stick"):
        raise BotFault("The configured menu axis must identify a usable controller stick")
    return source.split("_", 1)[0], 1 if source == "left_stick" else 3


def menu_sample(state):
    if state.get("idroid") is not True or state.get("idroid_menu_input_ready") is not True:
        raise BotFault("iDroid input ownership changed while navigating")
    sample = authoritative_controls(state)
    if sample["context"] != "menus":
        raise BotFault("The iDroid does not own the native menu input context")
    sticks = sample.get("sticks")
    if not isinstance(sticks, list) or len(sticks) != 4 or any(
            isinstance(v, bool) or not isinstance(v, (int, float)) or not math.isfinite(v) for v in sticks):
        raise BotFault("Native stick samples are unavailable")
    return sample


def device_orientation(yaw=0.):
    # Upright native connector. A normal controller grip now places the device
    # and its projection; do not compensate with the old sideways palm twist.
    angle = math.radians(yaw) / 2
    return [0., math.sin(angle), 0., math.cos(angle)]


def verify_palm_motion(observations):
    """A fresh operator pose cannot stand in for the native rendered skin."""
    palms = []
    for state in observations:
        frame = state.get("rendered") or {}
        age = state.get("now_ms", -1) - frame.get("sample_ms", -10000)
        if (not frame.get("rig_sequence") or not frame.get("right_palm_tracked")
                or not 0 <= age <= 150 or frame.get("activation") != state.get("activation")):
            raise BotFault("Native palm publication is frozen or unavailable while XR controller poses change")
        palm = frame.get("right_palm", {})
        if len(palm.get("position", [])) != 3 or len(palm.get("orientation", [])) != 4:
            raise BotFault("Native palm transform is unavailable")
        palms.append(palm)
    if len(palms) < 5:
        raise BotFault("Translation and rotation palm observations are incomplete")
    if max(distance(palms[0]["position"], p["position"]) for p in palms[1:3]) < .03:
        raise BotFault("Native palm did not translate with the declared hand movement")
    q1, q2 = palms[3]["orientation"], palms[4]["orientation"]
    alignment = abs(sum(a*b for a, b in zip(q1, q2)))
    if not math.isfinite(alignment) or alignment > math.cos(math.radians(5)):
        raise BotFault("Native palm did not rotate with the declared hand movement")


def navigate_stationary(live, result, *, clock, sleep):
    """Require distinct native samples and a neutral settling window."""
    state, _ = fresh_control_observation(live.observe, native=True, timeout=.25, clock=clock, sleep=sleep)
    anchor = position(state)
    sample = menu_sample(state)
    hand, axis_index = menu_axis(live.bindings)
    result.update(before=state, navigation_samples=[], max_displacement=0.)
    # Exercise the configured menu axis, then the other stick's horizontal
    # axis. The latter must never leak into gameplay camera turn/locomotion.
    probes = [(hand, "Y", axis_index), ("right" if hand == "left" else "left", "X", 2 if hand == "left" else 0)]
    origin_yaw = state.get("native", {}).get("player_yaw")
    if not isinstance(origin_yaw, (int, float)) or not math.isfinite(origin_yaw):
        raise BotFault("Fresh native player yaw is required for the iDroid turn check")
    for probe_hand, component, index in probes:
        seen = set()
        admission_ms = sample["sample_ms"]
        value = {"hand": probe_hand, "component": "Thumbstick", "sub_component": component, "value": .65}
        live.events.emit("semantic_axis", action="axes.menu", source=probe_hand+"_stick", component=component,
                         value=.65, label="NAVIGATE IDROID / SNAKE STAYS STILL")
        live.input([value], .65, lease_seconds=1.0)
        deadline = clock() + .65
        missing_since = None
        try:
            while clock() < deadline:
                observed = live.observe(native=True)
                drift = distance(anchor, position(observed))
                yaw = observed.get("native", {}).get("player_yaw")
                if not isinstance(yaw, (int, float)) or not math.isfinite(yaw):
                    raise BotFault("Native player yaw disappeared while navigating")
                # MGSV's native rotation getter returns degrees, wrapped at 360.
                yaw_delta = abs((yaw-origin_yaw+180.) % 360. - 180.)
                result["max_displacement"] = max(result["max_displacement"], drift)
                if drift > .005 or yaw_delta > .5:
                    raise BotFault("Menu navigation moved Snake more than 5 mm or turned him more than 0.5 degrees")
                try:
                    sample = menu_sample(observed)
                    missing_since = None
                except ControlSampleUnavailable:
                    # A nonblocking publication read can miss one sample. Keep
                    # checking native motion, never replay or extend the input,
                    # and require two actual distinct samples before passing.
                    missing_since = clock() if missing_since is None else missing_since
                    result.setdefault("unavailable_control_samples", []).append({
                        "now_ms": observed.get("now_ms"), "displacement": drift, "yaw_delta_degrees": yaw_delta})
                    if clock()-missing_since >= .25:
                        raise
                    sleep(min(.025, max(0., deadline-clock())))
                    continue
                result["navigation_samples"].append({"native_position": position(observed), "displacement": drift,
                    "yaw_delta_degrees": yaw_delta, "sample_ms": sample["sample_ms"],
                    "hand": probe_hand, "component": component, "stick": sample["sticks"][index]})
                if sample["sample_ms"] > admission_ms and abs(sample["sticks"][index] - .65) < .08:
                    seen.add(sample["sample_ms"])
                sleep(.04)
        finally:
            live.release()
        if len(seen) < 2:
            raise BotFault("The native XR sampler did not observe two distinct menu-stick samples")
        deadline = clock() + .3
        while True:
            after, _ = fresh_control_observation(live.observe, native=True, timeout=.25, clock=clock, sleep=sleep)
            sample = menu_sample(after)
            drift = distance(anchor, position(after))
            result["max_displacement"] = max(result["max_displacement"], drift)
            if drift > .005:
                raise BotFault("Snake drifted after releasing the menu stick")
            yaw = after.get("native", {}).get("player_yaw")
            if (not isinstance(yaw, (int, float)) or not math.isfinite(yaw)
                    or abs((yaw-origin_yaw+180.) % 360.-180.) > .5):
                raise BotFault("Snake turned after releasing the menu stick")
            if any(abs(v) > .08 for v in sample["sticks"]):
                raise BotFault("Native stick input remained held after release")
            if clock() >= deadline:
                break
            sleep(.04)
    result["after"] = after


def inspect_idroid(live, behavior, *, clock=time.monotonic, sleep=time.sleep):
    """Use normal menu controls; never acknowledge an unknown native popup."""
    case_id = "idroid-palm-motion-stationary-navigation"
    live.events.emit("case_started", case_id=case_id,
                     claim="Palm-relative hologram motion and stationary Snake during mapped menu-stick input")
    result = {"id": case_id, "status": "failed", "visual_acceptance": "pending",
              "headset_acceptance": "not_run", "captures": []}
    try:
        live.release()
        state = live.observe(native=True)
        if not state.get("idroid"):
            if state.get("scene") != "gameplay":
                raise BotFault("iDroid check requires normal field gameplay or an already open iDroid")
            live.execute({"op": "pose", "hand": "right", "position": [.12, -.18, -.50],
                          "orientation": device_orientation()})
            live.execute({"op": "action", "name": "system.idroid", "state_before": {"scene": "gameplay"}})
        behavior.wait_for({"idroid": True, "idroid_menu_input_ready": True}, 8, "idroid-input-owner")
        # Native device upright, projection toward the wearer. Declared operator
        # poses for deterministic visual inspection, not a measured human pose.
        poses = (([.12, -.18, -.50], 0), ([.22, -.12, -.48], 0),
                 ([.02, -.23, -.55], 0), ([.12, -.18, -.50], -20),
                 ([.12, -.18, -.50], 20), ([.12, -.18, -.50], 0))
        result["declared_poses"] = []
        result["palm_observations"] = []
        for index, (point, yaw) in enumerate(poses):
            orientation = device_orientation(yaw)
            pose = {"op": "pose", "hand": "right", "position": point, "orientation": orientation}
            result["declared_poses"].append(pose)
            live.execute(pose)
            result["captures"] += live.capture(f"idroid-palm-{index}")
            result["palm_observations"].append(live.observe())
        # Preserve this independent result even if a later menu-input sample is
        # missing. A stationary player cannot conceal frozen rendered hands.
        try:
            verify_palm_motion(result["palm_observations"])
            result["native_palm_motion"] = "observed_pass"
        except BotFault as error:
            result.update(status="failed", native_palm_motion="failed", palm_motion_error=str(error))
        navigate_stationary(live, result, clock=clock, sleep=sleep)
        result["stationary_navigation"] = "observed_pass"
        result.update(status="observed_pass" if result["native_palm_motion"] == "observed_pass" else "failed",
                      interpretation="Native stationary-navigation and palm-motion gates; hologram alignment/readability require image review")
        result["captures"] += live.capture("idroid-navigation-released")
        # Back is escapable and user-directed; no guesses about confirm/options.
        live.execute({"op": "action", "name": "menus.back"})
        try:
            result["exit"] = behavior.wait_for({"idroid": False, "scene": "gameplay"}, 5, "idroid-normal-exit")
            result["captures"] += live.capture("idroid-closed")
        except BotFault as error:
            result.update(status="failed", exit_error=str(error),
                          reason="Menu requires a separately identified prompt or exit path; no blind confirm sent")
    except Exception as error:
        result.update(status="failed", error=str(error))
    finally:
        try:
            live.release()
        except Exception as error:
            result.update(status="failed", release_error=str(error))
        live.events.emit("case_finished", result=result)
    return {"status": result["status"], "cases": [result], "visual_acceptance": "pending"}
