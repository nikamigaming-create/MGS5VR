"""Capture the simulator's real composited eye through the installed Meta MCP proxy.

No desktop/window capture, generated frames, or OS input. Timestamps are wall-clock
capture intervals; recording metadata reports the measured cadence. This does not
make incomplete VR gameplay into a passing acceptance demonstration.
"""
import argparse
import base64
import datetime
import json
import math
import pathlib
import queue
import subprocess
import threading
import time
import shutil


class Operator:
    def __init__(self, executable):
        self.process = subprocess.Popen(
            [str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL, text=True, encoding="utf-8",
            creationflags=subprocess.CREATE_NO_WINDOW,
        )
        self.messages = queue.Queue()
        self.sequence = 0
        threading.Thread(target=self._read, daemon=True).start()
        self.request("initialize", {
            "protocolVersion": "2024-11-05", "capabilities": {},
            "clientInfo": {"name": "mgs5vr-final-eye-capture", "version": "0.1"},
        })
        self.send({"jsonrpc": "2.0", "method": "notifications/initialized"})

    def _read(self):
        for line in self.process.stdout:
            try:
                self.messages.put(json.loads(line))
            except json.JSONDecodeError:
                pass
        self.messages.put({"error": {"message": "Operator proxy exited"}})

    def send(self, message):
        self.process.stdin.write(json.dumps(message, separators=(",", ":")) + "\n")
        self.process.stdin.flush()

    def request(self, method, params):
        self.sequence += 1
        request_id = self.sequence
        self.send({"jsonrpc": "2.0", "id": request_id, "method": method, "params": params})
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            response = self.messages.get(timeout=max(.01, deadline - time.monotonic()))
            if "error" in response:
                raise RuntimeError(response["error"])
            if response.get("id") == request_id:
                return response["result"]
        raise TimeoutError(method)

    def call(self, name, arguments):
        result = self.request("tools/call", {"name": name, "arguments": arguments})
        if result.get("isError"):
            raise RuntimeError(result)
        return result

    def close(self):
        self.process.stdin.close()
        try:
            self.process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            self.process.terminate()  # Only this script's own proxy child.
            self.process.wait(timeout=3)


def native_camera_demo(proxy, output, started, status, locomotion=False):
    """Known-state simulator test: native FPS and stereo must already be active.

    Every action goes through OpenXR. This demonstrates the current native camera
    and gamepad weapon path, not controller-driven hands or an implemented HUD.
    """
    actions = []
    client = None
    try:
        client = Operator(proxy)
        available = client.request("tools/list", {})["tools"]
        def name(suffix):
            matches = [t["name"] for t in available if t["name"].endswith(suffix)]
            if len(matches) != 1:
                raise RuntimeError(f"Ambiguous or unavailable OpenXR tool: {suffix}")
            return matches[0]
        get_head = name("openxr_get_head_pose")
        set_head = name("openxr_set_head_pose")
        set_input = name("openxr_set_controller_input")
        def call(tool, args, label):
            row = {"seconds": time.monotonic() - started, "label": label,
                   "tool": tool, "args": args}
            if tool == set_head:
                row["head_before"] = client.call(get_head, {"base_space": "local"})
            actions.append(row)
            row["result"] = client.call(tool, args)
            row["completed_seconds"] = time.monotonic() - started
            print(json.dumps({"demo_step": label, "seconds": row["seconds"]}), flush=True)
        time.sleep(2)
        call(set_input, {"hand": "left", "component": "Trigger", "value": 1}, "native aim held")
        for label, position in [
                ("lean right", [.1, 0, 0]), ("lean left", [-.1, 0, 0]),
                ("head up", [0, .1, 0]), ("head down", [0, -.05, 0]),
                ("lean forward", [0, 0, -.1]), ("lean backward", [0, 0, .08]),
                ("translation neutral", [0, 0, 0])]:
            call(set_head, {"base_space": "local", "position": position,
                           "orientation": [0, 0, 0, 1], "duration_seconds": 1.4}, label)
            time.sleep(.35)
        for label, axis, degrees in [
                ("yaw left", 1, 20), ("yaw right", 1, -20),
                ("look down at native arm", 0, -25), ("look up", 0, 15),
                ("roll left", 2, 12), ("roll right", 2, -12),
                ("rotation neutral", 0, 0)]:
            q = [0, 0, 0, math.cos(math.radians(degrees) / 2)]
            q[axis] = math.sin(math.radians(degrees) / 2)
            call(set_head, {"base_space": "local", "position": [0, 0, 0],
                           "orientation": q, "duration_seconds": 1.4}, label)
            time.sleep(.35)
        if locomotion:
            for label, hand, axis, value, duration in [
                    ("walk forward while aiming", "left", "Y", .65, 2.2),
                    ("strafe left while aiming", "left", "X", -.6, 1.3),
                    ("walk backward while aiming", "left", "Y", -.65, 2.2),
                    ("native smooth turn", "right", "X", .25, .4)]:
                call(set_input, {"hand": hand, "component": "Thumbstick", "sub_component": axis,
                                 "value": value, "auto_release": True, "hold_duration": duration}, label)
                time.sleep(duration + .5)
            call(set_input, {"hand": "right", "component": "A", "value": 1,
                             "auto_release": True, "hold_duration": .15}, "first stance tap (native outcome to review)")
            time.sleep(1.2)
            call(set_input, {"hand": "left", "component": "Thumbstick", "sub_component": "Y",
                             "value": .65, "auto_release": True, "hold_duration": 1.5}, "walk after stance tap")
            time.sleep(2)
            call(set_input, {"hand": "right", "component": "A", "value": 1,
                             "auto_release": True, "hold_duration": .15}, "second stance tap (native outcome to review)")
            time.sleep(1.2)
        call(set_input, {"hand": "right", "component": "Thumbstick", "sub_component": "Y",
                         "value": -.3, "auto_release": True, "hold_duration": .25}, "native stick aim adjustment")
        time.sleep(1)
        call(set_input, {"hand": "right", "component": "Trigger", "value": 1,
                         "auto_release": True, "hold_duration": .2}, "native rifle fire")
        time.sleep(2)
        call(set_input, {"hand": "right", "component": "B", "value": 1,
                         "auto_release": True, "hold_duration": .15}, "native reload")
        time.sleep(5)
        if locomotion:
            call(set_input, {"hand": "left", "component": "Trigger", "value": 0}, "lower weapon: first-person persistence test")
            time.sleep(1.5)
            call(set_input, {"hand": "left", "component": "Thumbstick", "sub_component": "Y",
                             "value": .6, "auto_release": True, "hold_duration": 1.5}, "walk with weapon lowered")
            time.sleep(2)
        status["completed"] = True
    except Exception as error:
        status["error"] = repr(error)
        print(json.dumps({"demo_error": repr(error)}), flush=True)
    finally:
        if client:
            client.close()
        (output / "actions.json").write_text(json.dumps({
            "schema": 2, "source": "Meta OpenXR API; no OS input",
            "clock": "Same Python monotonic origin as capture.json",
            "actions": actions, "status": status}, indent=2), encoding="utf-8")
        status["finished_seconds"] = time.monotonic() - started


def controller_rig_demo(proxy, output, started, status):
    """Requires the native controller experiment already calibrated and active."""
    actions = []
    client = None
    try:
        client = Operator(proxy)
        available = client.request("tools/list", {})["tools"]
        def tool(suffix):
            matches = [t["name"] for t in available if t["name"].endswith(suffix)]
            if len(matches) != 1:
                raise RuntimeError(suffix)
            return matches[0]
        def call(suffix, args, label):
            row = {"seconds": time.monotonic()-started, "label": label, "args": args}
            if suffix == "openxr_set_controller_pose":
                row["before"] = client.call(tool("openxr_get_controller_pose"),
                    {"hand": args["hand"], "base_space": "local", "pose_type": "grip"})
            row["head"] = client.call(tool("openxr_get_head_pose"), {"base_space": "local"})
            actions.append(row)
            row["result"] = client.call(tool(suffix), args)
            print(json.dumps({"demo_step": label, "seconds": row["seconds"]}), flush=True)
        def grip(position, orientation, label):
            call("openxr_set_controller_pose", {"hand": "right", "base_space": "local",
                "pose_type": "grip", "position": position, "orientation": orientation,
                "duration_seconds": 0}, label)
        def button(component, label):
            call("openxr_set_controller_input", {"hand": "right", "component": component,
                "value": 1, "auto_release": True, "hold_duration": .12}, label)
        call("openxr_set_controller_input", {"hand": "right", "component": "Grip", "value": 1}, "right grip holds weapon ready")
        call("openxr_set_controller_input", {"hand": "left", "component": "Grip", "value": 1}, "left grip holds support hand")
        call("openxr_set_controller_input", {"hand": "left", "component": "Trigger", "value": 0}, "release legacy aim trigger")
        time.sleep(2)
        grip([.20,-.26,-.34], [0,-.173648,0,.984808], "controller right and yaw right; head fixed")
        time.sleep(2.5)
        button("Trigger", "fire along right barrel")
        time.sleep(2)
        grip([-.10,-.24,-.32], [0,.258819,0,.965926], "controller left and yaw left; head fixed")
        time.sleep(2.5)
        button("Trigger", "fire along left barrel")
        time.sleep(2)
        grip([.08,-.16,-.25], [.130526,0,0,.991445], "raise and pitch controller")
        time.sleep(2.5)
        grip([.10,-.26,-.30], [0,0,.130526,.991445], "roll controller")
        time.sleep(2.5)
        grip([.13,-.24,-.32], [0,0,0,1], "return controller to neutral")
        time.sleep(2)
        button("B", "native reload")
        time.sleep(3)
        call("openxr_set_head_pose", {"base_space": "local", "position": [.07,.02,0],
            "orientation": [0,.130526,0,.991445], "duration_seconds": 1}, "lean and turn head with controller fixed")
        actions[-1]["right_after"] = client.call(tool("openxr_get_controller_pose"),
            {"hand": "right", "base_space": "local", "pose_type": "grip"})
        time.sleep(2)
        call("openxr_set_head_pose", {"base_space": "local", "position": [0,0,0],
            "orientation": [0,0,0,1], "duration_seconds": 1}, "return head to neutral")
        time.sleep(2)
        status["completed"] = True
    except Exception as error:
        status["error"] = str(error)
    finally:
        if client:
            client.close()
        status["finished_seconds"] = time.monotonic()-started
        (output/"demo-actions.json").write_text(json.dumps({"actions": actions, "status": status}, indent=2), encoding="utf-8")


def stream_capture(client, capture, args):
    """Encode incoming real eye images immediately; retain no raw frame files."""
    encoder = shutil.which("ffmpeg")
    if not encoder:
        raise RuntimeError("ffmpeg is required for bounded MP4 capture")
    if args.seconds > 30 or args.demo_controller_rig or args.demo_native_camera or args.demo_locomotion:
        raise ValueError("MP4 capture is limited to 30 seconds; drive OpenXR actions separately")
    args.output.mkdir(parents=True, exist_ok=False)
    movie = args.output / "simulator.mp4"
    # Arrival timestamps preserve the measured capture cadence. No interpolation
    # or frame-rate conversion is requested. PNGs travel only through the pipe.
    process = subprocess.Popen([
        encoder, "-hide_banner", "-loglevel", "error", "-n",
        "-f", "image2pipe", "-framerate", "1000", "-vcodec", "png",
        "-use_wallclock_as_timestamps", "1", "-probesize", "32", "-analyzeduration", "0",
        "-i", "pipe:0", "-an", "-vf", "scale=-2:720", "-fps_mode", "vfr",
        "-enc_time_base", "1:1000", "-c:v", "libx264", "-preset", "fast",
        "-crf", "23", "-pix_fmt", "yuv420p", "-t", str(args.seconds),
        "-fs", str(12 * 1024 * 1024), "-movflags", "+faststart", str(movie),
    ], stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
        creationflags=subprocess.CREATE_NO_WINDOW)
    started = time.monotonic()
    started_utc = datetime.datetime.now(datetime.timezone.utc).isoformat()
    frames, error = [], None
    try:
        while time.monotonic() - started < args.seconds:
            before = time.monotonic()
            result = client.call(capture, {"eye": args.eye})
            after = time.monotonic()
            images = [c for c in result.get("content", []) if c.get("type") == "image"]
            if len(images) != 1 or images[0].get("mimeType") != "image/png":
                raise RuntimeError("Capture did not return one real PNG")
            data = base64.b64decode(images[0]["data"], validate=True)
            if len(data) > 8 * 1024 * 1024:
                raise RuntimeError("Eye image exceeds the bounded capture budget")
            process.stdin.write(data)
            process.stdin.flush()
            frames.append({"request_seconds": before - started, "response_seconds": after - started})
            if movie.exists() and movie.stat().st_size >= 12 * 1024 * 1024:
                raise RuntimeError("Capture reached its 12 MiB media budget")
            time.sleep(max(0, min(1 / args.max_fps - (time.monotonic() - before),
                                  args.seconds - (time.monotonic() - started))))
    except Exception as exc:
        error = repr(exc)
        raise
    finally:
        elapsed = time.monotonic() - started
        process.stdin.close()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.terminate()  # Only the encoder created by this capture.
            process.wait(timeout=3)
            error = error or "Encoder did not finalize within ten seconds"
        encoder_error = process.stderr.read().decode("utf-8", errors="replace")[-4000:]
        metadata = {"schema": 2, "eye": args.eye, "source": "Meta OpenXR composited eye",
                    "started_utc": started_utc, "seconds": elapsed, "requested_seconds": args.seconds,
                    "frames": frames, "measured_capture_fps": len(frames) / max(elapsed, .001),
                    "raw_frame_files": 0, "media_budget_bytes": 12 * 1024 * 1024,
                    "timestamp_clock": "PNG arrival at encoder; Python request/response bounds recorded",
                    "stereo_acceptance": False, "full_mod_acceptance": False,
                    "error": error, "encoder_exit": process.returncode, "encoder_error": encoder_error}
        (args.output / "capture.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    if process.returncode or error:
        raise RuntimeError(error or encoder_error or "Encoder failed")
    print(json.dumps({"video": str(movie.resolve()), "frames": len(frames),
                      "seconds": elapsed, "bytes": movie.stat().st_size}), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--proxy", required=True, type=pathlib.Path)
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--seconds", type=float, default=10)
    parser.add_argument("--eye", choices=["left", "right"], default="left")
    parser.add_argument("--output", type=pathlib.Path)
    parser.add_argument("--max-fps", type=float, default=30)
    parser.add_argument("--mp4", action="store_true",
                        help="Stream to a 720p MP4 (12 MiB/30 s maximum); no raw frame files")
    parser.add_argument("--demo-native-camera", action="store_true",
                        help="Record scripted head motion, native aiming/fire/reload; requires already active native FPS/stereo")
    parser.add_argument("--demo-locomotion", action="store_true",
                        help="Add native walking, strafe, turn, stance and lowered-weapon camera checks to the demo")
    parser.add_argument("--demo-controller-rig", action="store_true",
                        help="Move the calibrated right grip, fire and reload with head fixed; requires active controller experiment")
    args = parser.parse_args()
    if args.demo_controller_rig and (args.demo_native_camera or args.demo_locomotion):
        parser.error("controller and camera demos must run separately")
    client = Operator(args.proxy)
    try:
        tools = client.request("tools/list", {})["tools"]
        if args.list:
            print(json.dumps([t for t in tools if any(s in t["name"] for s in
                  ("composited", "head_pose", "controller_input"))], indent=2))
            return
        if not args.output or not (0 < args.seconds <= 300) or not (0 < args.max_fps <= 120):
            parser.error("provide --output, 0 < seconds <= 300, and 0 < max-fps <= 120")
        capture = [t["name"] for t in tools if "capture_composited_image" in t["name"]]
        if len(capture) != 1:
            raise RuntimeError(f"Expected one composited capture tool, got {capture}")
        if args.mp4:
            stream_capture(client, capture[0], args)
            return
        args.output.mkdir(parents=True, exist_ok=False)
        started = time.monotonic()
        started_utc = datetime.datetime.now(datetime.timezone.utc).isoformat()
        frames = []
        demo_status = {}
        demo_thread = None
        if args.demo_controller_rig:
            demo_thread = threading.Thread(target=controller_rig_demo,
                args=(args.proxy, args.output, started, demo_status), daemon=True)
            demo_thread.start()
        elif args.demo_native_camera or args.demo_locomotion:
            demo_thread = threading.Thread(target=native_camera_demo,
                args=(args.proxy, args.output, started, demo_status, args.demo_locomotion), daemon=True)
            demo_thread.start()
        while (time.monotonic() - started < args.seconds or
               (demo_thread and (demo_thread.is_alive() or
                time.monotonic() - started < demo_status.get("finished_seconds", 0) + 3))):
            before = time.monotonic()
            result = client.call(capture[0], {"eye": args.eye})
            after = time.monotonic()
            images = [c for c in result.get("content", []) if c.get("type") == "image"]
            if len(images) != 1 or images[0].get("mimeType") != "image/png":
                raise RuntimeError("Capture did not return one PNG; refusing to fabricate a frame")
            name = f"frame-{len(frames):06}.png"
            (args.output / name).write_bytes(base64.b64decode(images[0]["data"], validate=True))
            frames.append({"file": name, "request_seconds": before - started,
                           "response_seconds": after - started})
            remaining = 1 / args.max_fps - (time.monotonic() - before)
            if remaining > 0:
                time.sleep(remaining)
        elapsed = time.monotonic() - started
        metadata = {"schema": 1, "eye": args.eye, "source": "Meta OpenXR composited eye",
                    "started_utc": started_utc,
                    "seconds": elapsed, "frames": frames,
                    "measured_capture_fps": len(frames) / elapsed,
                    "stereo_acceptance": False, "full_mod_acceptance": False,
                    "demo": demo_status,
                    "timestamp_clock": "Python monotonic seconds; request and response bounds"}
        (args.output / "capture.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")
        concat = ["ffconcat version 1.0"]
        for i, frame in enumerate(frames):
            next_time = frames[i+1]["response_seconds"] if i+1 < len(frames) else elapsed
            duration = max(.001, next_time - frame["response_seconds"])
            concat.extend([f"file '{frame['file']}'", f"duration {duration:.6f}"])
        if frames:
            concat.append(f"file '{frames[-1]['file']}'")
        (args.output / "frames.ffconcat").write_text("\n".join(concat) + "\n", encoding="utf-8")
        print(json.dumps({"output": str(args.output.resolve()), "frames": len(frames),
                          "seconds": elapsed, "measured_capture_fps": len(frames) / elapsed}))
    finally:
        client.close()


if __name__ == "__main__":
    main()
