"""Bounded native Pause hand-motion regression, with both final eyes retained."""
import argparse
import json
import math
from pathlib import Path
import subprocess
import time

from gameplay_bot.core import Behaviors, BotFault, Events, atomic_json
from gameplay_bot.idroid import verify_palm_motion, device_orientation, position, distance
from gameplay_bot.live import InputLease, Live, ROOT, game_identity, digest, authoritative_controls, ControlSampleUnavailable
from gameplay_bot.recording import Recording


def probe(live, behavior):
    result = {"id": "paused-native-palm-motion", "status": "failed",
              "visual_acceptance": "pending", "headset_acceptance": "not_run",
              "captures": [], "palm_observations": [], "left_palm_observations": []}
    prior = []
    prior_head = None
    try:
        live.release()
        behavior.wait_for({"scene": "gameplay", "menu": False}, 3, "pause-probe-entry")
        if live.observe(native=True).get("native", {}).get("tutorial_pause") is True:
            raise BotFault("Tutorial Resume/Skip is not the ordinary field Pause menu")
        head = live.text_result(live.call("get_head_pose", {"base_space": "local"}))
        if not all(head.get("flags", {}).get(k) for k in ("position_valid", "orientation_valid")):
            raise BotFault("Cannot preserve the existing head pose")
        prior_head = {"base_space": "local", **head["pose"]}
        for hand in ("left", "right"):
            for kind in ("grip", "aim"):
                query = {"hand": hand, "pose_type": kind, "base_space": "view"}
                state = live.text_result(live.call("get_controller_pose", query))
                if not all(state.get("flags", {}).get(k) for k in ("position_valid", "orientation_valid")):
                    raise BotFault("Cannot preserve the existing controller pose")
                prior.append({**query, **state["pose"]})
        result["prior_poses"] = prior
        live.execute({"op": "pose", "hand": "left", "position": [-.25, -.15, -.55], "orientation": [0., 0., 0., 1.]})
        live.execute({"op": "pose", "hand": "right", "position": [.12, -.18, -.5], "orientation": device_orientation()})
        result["captures"] += live.capture("gameplay-neutral")
        live.execute({"op": "action", "name": "system.pause", "state_before": {"scene": "gameplay"}})
        result["opened"] = behavior.wait_for({"menu": True, "idroid": False}, 3, "pause-open")
        # Exceed the original rig's 150 ms freshness window and the report's
        # five-second pause duration before testing any hand transform.
        deadline = time.monotonic() + 5.2
        while time.monotonic() < deadline:
            state = live.observe()
            if state.get("menu") is not True or state.get("idroid") is not False:
                raise BotFault("Pause ownership changed during settling")
            time.sleep(.1)
        for index, (point, yaw) in enumerate((([.12, -.18, -.5], 0), ([.22, -.12, -.48], 0),
                                             ([.02, -.23, -.55], 0), ([.12, -.18, -.5], -20),
                                             ([.12, -.18, -.5], 20), ([.12, -.18, -.5], 0))):
            live.execute({"op": "pose", "hand": "right", "position": point, "orientation": device_orientation(yaw)})
            result["captures"] += live.capture(f"paused-palm-{index}")
            state = live.observe()
            if state.get("menu") is not True or state.get("idroid") is not False:
                raise BotFault("Pause ownership changed during motion")
            result["palm_observations"].append(state)
        verify_palm_motion(result["palm_observations"])
        # Moving/lowering the left hand must not move the Pause panel with it.
        for index, (point, yaw) in enumerate((([-.4, -.5, -.3], 0), ([.2, -.1, -.6], 0), ([.2, -.1, -.6], 25))):
            half = math.radians(yaw) / 2
            live.execute({"op": "pose", "hand": "left", "position": point, "orientation": [0., math.sin(half), 0., math.cos(half)]})
            result["captures"] += live.capture(f"paused-left-wrist-{index}")
            state = live.observe()
            if state.get("menu") is not True or state.get("idroid") is not False:
                raise BotFault("Pause ownership changed during left-hand motion")
            if state.get("rendered", {}).get("left_palm_tracked") is not True:
                raise BotFault("Left rendered palm publication is unavailable")
            result["left_palm_observations"].append(state)
        left = [s["rendered"]["left_palm"] for s in result["left_palm_observations"]]
        if distance(left[0]["position"], left[1]["position"]) < .02:
            raise BotFault("Left native palm did not translate during Pause")
        if abs(sum(a*b for a, b in zip(left[1]["orientation"], left[2]["orientation"]))) > .995:
            raise BotFault("Left native palm did not rotate during Pause")
        result["left_native_palm_motion"] = "observed_pass"
        moved_head = {**prior_head, "position": list(prior_head["position"])}
        moved_head["position"][0] += .08
        live.call("set_head_pose", moved_head)
        result["captures"] += live.capture("paused-head-translation")
        result["head_moved"] = live.observe()
        live.call("set_head_pose", prior_head)
        # Navigate rows and exercise the other stick without confirming any item.
        baseline = live.observe(native=True)
        anchor, yaw = position(baseline), baseline["native"]["player_yaw"]
        result.update(navigation_samples=[], skipped_control_samples=0, max_displacement=0., max_yaw_delta=0.)
        for hand, component, index in (("left", "Y", 1), ("right", "X", 2)):
            seen = set()
            admitted = baseline["now_ms"]
            live.input([{"hand": hand, "component": "Thumbstick", "sub_component": component, "value": .65}], .65, lease_seconds=1.)
            deadline = time.monotonic() + .65
            try:
                while time.monotonic() < deadline:
                    state = live.observe(native=True)
                    if not state.get("menu") or state.get("idroid"):
                        raise BotFault("Pause lost menu ownership during stick navigation")
                    try:
                        controls = authoritative_controls(state)
                    except ControlSampleUnavailable:
                        # A non-atomic diagnostic read can miss one input
                        # publication. Require distinct fresh held samples
                        # within the existing deadline; never count this one.
                        result["skipped_control_samples"] += 1
                        time.sleep(.01)
                        continue
                    if controls["context"] != "menus":
                        raise BotFault("Pause lost native menu input context")
                    if controls["sample_ms"] > admitted and abs(controls["sticks"][index]-.65)<.08:
                        seen.add(controls["sample_ms"])
                    drift = distance(anchor, position(state))
                    turn = abs((state["native"]["player_yaw"]-yaw+180.)%360.-180.)
                    result["max_displacement"] = max(result["max_displacement"], drift)
                    result["max_yaw_delta"] = max(result["max_yaw_delta"], turn)
                    result["navigation_samples"].append(state)
                    if drift > .005 or turn > .5:
                        raise BotFault("Pause navigation moved Snake")
                    time.sleep(.04)
            finally:
                live.release()
            if len(seen)<2:
                raise BotFault("Native sampler did not observe distinct held Pause stick samples")
        result["stationary_navigation"] = "observed_pass"
        result["captures"] += live.capture("paused-navigation-released")
        result.update(status="observed_pass", native_palm_motion="observed_pass")
    except Exception as error:
        result["error"] = str(error)
    finally:
        try:
            live.release()
            state = live.observe()
            if state.get("menu") is True and state.get("idroid") is False:
                live.execute({"op": "action", "name": "menus.back", "state_before": {"menu": True, "idroid": False}})
                result["exit"] = behavior.wait_for({"menu": False, "scene": "gameplay"}, 3, "pause-exit")
            for pose in prior:
                live.call("set_controller_pose", pose)
            if prior_head:
                live.call("set_head_pose", prior_head)
            result["captures"] += live.capture("restored-gameplay")
        except Exception as error:
            result.update(status="failed", cleanup_error=str(error))
        live.events.emit("case_finished", result=result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-dir", required=True, type=Path)
    parser.add_argument("--proxy", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--record", action="store_true")
    args = parser.parse_args()
    with InputLease():
        identity = game_identity(args.game_dir)
        checker = ROOT / "build/Release/mgs5vr_controls.exe"
        bindings = json.loads(subprocess.check_output([str(checker), "--bindings-json", str(args.game_dir / "mgs5vr-controls.ini")], text=True))
        identity["bindings_tool_sha256"] = digest(checker)
        events = Events(args.output, identity)
        atomic_json(args.output / "bindings.json", bindings)
        live = Live(args.proxy, args.game_dir, bindings, events)
        recording = None
        result = {"status": "failed", "cases": []}
        try:
            live.ready()
            if args.record:
                recording = Recording(args.game_dir, events, identity["process"]["ProcessId"])
                recording.start()
                live.background_check = recording.check
                live.pose_recording = True
            case = probe(live, Behaviors(live, events))
            result.update(status=case["status"], cases=[case])
            if recording:
                recording.capture_tail()
        except Exception as error:
            result["error"] = str(error)
        finally:
            if recording:
                try:
                    result["recording"] = recording.close()
                    error = Recording.failure_reason(result["recording"])
                    if error:
                        result.update(status="failed", recording_error=error)
                except Exception as error:
                    result.update(status="failed", recording_error=str(error))
            try:
                live.close()
            except Exception as error:
                result.update(status="failed", cleanup_error=str(error))
            atomic_json(args.output / "result.json", result)
            events.emit("run_finished", result=result)
        print(json.dumps({"status": result["status"], "result": str(args.output / "result.json"),
                          "errors": [c.get("error") for c in result["cases"]], "recording_error": result.get("recording_error")}))
        return int(result["status"] == "failed")


if __name__ == "__main__":
    raise SystemExit(main())
