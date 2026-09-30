"""Render one observed bot action as a field-kit lesson with timed input cues.

The source is a real native eye plus process audio, before XR composition.
Input markers follow set_controller_input RPC completion acknowledgments; a
separate native input audit must confirm that the action's controls were held.
This is not a native sampled-time trace or a full feature acceptance video.
"""
import argparse
from collections import defaultdict, deque
import hashlib
import json
import math
import pathlib
import re
import shutil
import struct
import subprocess
import textwrap

ROOT = pathlib.Path(__file__).resolve().parents[1]

TOKEN_CHANNELS = {
    "a": ("right", "A", None, 1.),
    "b": ("right", "B", None, 1.),
    "x": ("left", "X", None, 1.),
    "y": ("left", "Y", None, 1.),
    "menu": ("left", "Menu", None, 1.),
    "left_stick_click": ("left", "ThumbstickClick", None, 1.),
    "right_stick_click": ("right", "ThumbstickClick", None, 1.),
    "left_grip": ("left", "Grip", None, 1.),
    "right_grip": ("right", "Grip", None, 1.),
    "left_trigger": ("left", "Trigger", None, 1.),
    "right_trigger": ("right", "Trigger", None, 1.),
    "left_stick_up": ("left", "Thumbstick", "Y", 1.),
    "left_stick_down": ("left", "Thumbstick", "Y", -1.),
    "left_stick_left": ("left", "Thumbstick", "X", -1.),
    "left_stick_right": ("left", "Thumbstick", "X", 1.),
    "right_stick_up": ("right", "Thumbstick", "Y", 1.),
    "right_stick_down": ("right", "Thumbstick", "Y", -1.),
    "right_stick_left": ("right", "Thumbstick", "X", -1.),
    "right_stick_right": ("right", "Thumbstick", "X", 1.),
}
BUTTON_INDEX = {
    "a": 0, "b": 1, "x": 2, "y": 3, "menu": 4,
    "left_stick_click": 5, "right_stick_click": 6,
    "left_grip": 7, "right_grip": 8, "left_trigger": 9, "right_trigger": 10,
}
STICK_AUDIT = {
    "left_stick_left": (0, -1.), "left_stick_right": (0, 1.),
    "left_stick_down": (1, -1.), "left_stick_up": (1, 1.),
    "right_stick_left": (2, -1.), "right_stick_right": (2, 1.),
    "right_stick_down": (3, -1.), "right_stick_up": (3, 1.),
}
TOKEN_LABELS = {
    "a": "A", "b": "B", "x": "X", "y": "Y", "menu": "MENU",
    "left_stick_click": "L STICK CLICK", "right_stick_click": "R STICK CLICK",
    "left_grip": "L GRIP", "right_grip": "R GRIP",
    "left_trigger": "L TRIGGER", "right_trigger": "R TRIGGER",
    "left_stick_up": "L STICK ↑", "left_stick_down": "L STICK ↓",
    "left_stick_left": "L STICK ←", "left_stick_right": "L STICK →",
    "right_stick_up": "R STICK ↑", "right_stick_down": "R STICK ↓",
    "right_stick_left": "R STICK ←", "right_stick_right": "R STICK →",
}
def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def stamp(seconds):
    centiseconds = max(0, round(seconds * 100))
    minutes, tail = divmod(centiseconds, 6000)
    hours, minutes = divmod(minutes, 60)
    whole, fraction = divmod(tail, 100)
    return f"{hours}:{minutes:02d}:{whole:02d}.{fraction:02d}"


def posture(state):
    native = state.get("native", {})
    for key, title in (("status_CRAWL", "PRONE"), ("status_SQUAT", "CROUCHED"), ("status_STAND", "STANDING")):
        if native.get(key) is True:
            return title
    return state.get("scene", "observed state").upper()


def context_title(state):
    labels = {
        "nativeButtons": "Gamepad controls",
        "gameplay": "VR gameplay controls",
        "equipment": "Equipment controls",
        "commands": "Buddy command controls",
        "binoculars": "Binocular controls",
        "menus": "Menu controls",
        "horse": "Horse controls",
        "vehicle": "Vehicle controls",
    }
    context = (state.get("controls") or {}).get("context")
    return labels.get(context, str(context or "Control state").replace("_", " ").title())


def source_crop_rect(width, height, render_aspect):
    if width <= 0 or height <= 0 or not .3 < render_aspect < 4:
        raise ValueError("Source dimensions or render-FOV aspect are invalid.")
    source_aspect = width / height
    if source_aspect > render_aspect:
        crop_width = min(width, int(height * render_aspect))
        crop_width -= crop_width % 2
        crop_height = height - height % 2
        left = ((width - crop_width) // 2) & ~1
        top = 0
    else:
        crop_height = min(height, int(width / render_aspect))
        crop_height -= crop_height % 2
        crop_width = width - width % 2
        left = 0
        top = ((height - crop_height) // 2) & ~1
    if crop_width <= 0 or crop_height <= 0:
        raise ValueError("Render-FOV crop produced an empty source image.")
    return crop_width, crop_height, left, top


def png_details(path):
    with path.open("rb") as stream:
        header = stream.read(26)
    if len(header) < 26 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"Controller overlay frame is not a valid PNG: {path}")
    width, height = struct.unpack(">II", header[16:24])
    color_type = header[25]
    if color_type not in (4, 6):
        raise ValueError(f"Controller overlay PNG has no alpha channel: {path}")
    return width, height


def controller_overlay_input(path, fps):
    if not isinstance(fps, (int, float)) or not math.isfinite(fps) or fps <= 0:
        raise ValueError("Controller overlay frame rate must be a positive finite number.")
    path = path.resolve()
    if path.is_dir():
        frames = sorted(path.glob("*.png"))
        if len(frames) < 2:
            raise ValueError("Controller overlay sequence needs at least two alpha PNG frames.")
        match = re.fullmatch(r"(.*?)(\d+)", frames[0].stem)
        if not match:
            raise ValueError("Name overlay frames with a consistent trailing frame number, such as inset-00001.png.")
        prefix, digits = match.groups()
        first = int(digits)
        expected = [f"{prefix}{number:0{len(digits)}d}.png" for number in range(first, first + len(frames))]
        if [frame.name for frame in frames] != expected:
            raise ValueError("Controller overlay PNG frame numbers must be continuous with consistent zero padding.")
        width, height = png_details(frames[0])
        if any(png_details(frame) != (width, height) for frame in frames):
            raise ValueError("Controller overlay PNG frames must all have the same dimensions.")
        pattern = path / f"{prefix}%0{len(digits)}d.png"
        sequence_hash = hashlib.sha256()
        for frame in frames:
            sequence_hash.update(frame.name.encode("utf-8"))
            sequence_hash.update(frame.read_bytes())
        return {
            "input_args": ["-framerate", str(fps), "-start_number", str(first), "-i", str(pattern)],
            "duration": len(frames) / fps,
            "width": width,
            "height": height,
            "frame_count": len(frames),
            "format": "alpha PNG sequence",
            "sha256": sequence_hash.hexdigest(),
        }
    if not path.is_file():
        raise ValueError(f"Controller overlay file does not exist: {path}")
    ffprobe = shutil.which("ffprobe")
    if not ffprobe:
        raise ValueError("ffprobe is required to verify alpha and duration in the 3D controller overlay.")
    probed = subprocess.run(
        [ffprobe, "-v", "error", "-select_streams", "v:0", "-show_entries",
         "stream=width,height,pix_fmt:format=duration", "-of", "json", str(path)],
        check=True, capture_output=True, text=True,
    )
    data = json.loads(probed.stdout)
    streams = data.get("streams", [])
    if not streams:
        raise ValueError("The controller overlay has no video stream.")
    stream = streams[0]
    pix_fmt = str(stream.get("pix_fmt", "")).lower()
    alpha = ("yuva" in pix_fmt or "rgba" in pix_fmt or "bgra" in pix_fmt
             or "argb" in pix_fmt or "abgr" in pix_fmt or "gbrap" in pix_fmt
             or pix_fmt == "pal8")
    if not alpha:
        raise ValueError(f"The controller overlay must retain transparency; decoded pixel format is {pix_fmt!r}.")
    try:
        duration = float(data["format"]["duration"])
    except (KeyError, TypeError, ValueError) as error:
        raise ValueError("The controller overlay duration could not be read.") from error
    return {
        "input_args": ["-i", str(path)],
        "duration": duration,
        "width": int(stream["width"]),
        "height": int(stream["height"]),
        "frame_count": None,
        "format": pix_fmt,
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
    }


def case_window(events, case_id):
    begin = next((i for i, event in enumerate(events)
                  if event.get("event") == "case_started" and event.get("case_id") == case_id), None)
    if begin is None:
        raise ValueError(f"No case_started event for case {case_id!r}.")
    for i in range(begin + 1, len(events)):
        event = events[i]
        if event.get("event") == "case_started":
            raise ValueError(f"Case {case_id!r} has no matching finish before the next case.")
        finished_case = event.get("case_id") or (event.get("result") or {}).get("id")
        if event.get("event") == "case_finished" and finished_case == case_id:
            return begin, i
    raise ValueError(f"No case_finished event whose case_id or result.id matches {case_id!r}.")


def pair_controller_rpcs(events):
    pending = defaultdict(deque)
    paired = []
    for event in events:
        tool = event.get("tool")
        if tool != "set_controller_input":
            continue
        if event.get("event") == "rpc_started":
            pending[tool].append(event)
        elif event.get("event") == "rpc_completed" and pending[tool]:
            start_event = pending[tool].popleft()
            started_qpc = start_event.get("qpc_100ns")
            completed_qpc = event.get("qpc_100ns")
            if not isinstance(started_qpc, int) or not isinstance(completed_qpc, int) or completed_qpc < started_qpc:
                continue
            paired.append({
                "arguments": start_event.get("arguments") or {},
                "request_qpc_100ns": started_qpc,
                "ack_qpc_100ns": completed_qpc,
            })
    return paired


def audit_holds_inputs(audit, tokens):
    physical = audit.get("physical")
    if not isinstance(physical, dict):
        raise ValueError("The matching action_input_audit has no physical control sample.")
    buttons, sticks = physical.get("buttons"), physical.get("sticks")
    if not isinstance(buttons, list) or len(buttons) < 11 or not isinstance(sticks, list) or len(sticks) != 4:
        raise ValueError("The matching action_input_audit has an incomplete buttons/sticks sample.")
    requested = audit.get("requested_inputs")
    if not isinstance(requested, list) or set(requested) != set(tokens):
        raise ValueError("The input audit's requested_inputs do not match the effective action binding.")
    for token in tokens:
        if token in BUTTON_INDEX:
            value = buttons[BUTTON_INDEX[token]]
            if isinstance(value, bool) or not isinstance(value, (int, float)) or value < .5:
                raise ValueError(f"The action audit did not sample {token} held.")
        elif token in STICK_AUDIT:
            index, sign = STICK_AUDIT[token]
            value = sticks[index]
            if isinstance(value, bool) or not isinstance(value, (int, float)) or value * sign < .5:
                raise ValueError(f"The action audit did not sample {token} held.")
        elif token in ("left_thumbrest", "right_thumbrest"):
            thumbrests = physical.get("thumbrests")
            index = 0 if token == "left_thumbrest" else 1
            if not isinstance(thumbrests, list) or len(thumbrests) < 2:
                raise ValueError(
                    f"Cannot render {token}: this run's native input audit does not publish thumb-rest samples."
                )
            value = thumbrests[index]
            if isinstance(value, bool) or not isinstance(value, (int, float)) or value < .5:
                raise ValueError(f"The action audit did not sample {token} held.")
        else:
            raise ValueError(
                f"Unsupported effective input token {token!r}; teach the renderer its audit and controller channel."
            )
    return True


def channel_matches(token, arguments):
    channel = TOKEN_CHANNELS.get(token)
    if channel is None:
        if token in ("left_thumbrest", "right_thumbrest"):
            raise ValueError(
                f"Cannot render {token}: the recorded bot RPC schema has no thumb-rest controller channel yet."
            )
        raise ValueError(
            f"Unsupported effective input token {token!r}; teach the renderer its audit and controller channel."
        )
    hand, component, sub_component, _ = channel
    return (arguments.get("hand") == hand
            and arguments.get("component") == component
            and arguments.get("sub_component") == sub_component)


def channel_is_held(token, arguments):
    value = arguments.get("value")
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return False
    expected_sign = TOKEN_CHANNELS[token][3]
    return value * expected_sign >= .5


def acknowledged_intervals(events, semantic, audit):
    tokens = (semantic.get("binding") or {}).get("inputs")
    if not isinstance(tokens, list) or not tokens:
        raise ValueError(f"Action {semantic.get('action')} has no effective physical inputs.")
    if len(set(tokens)) != len(tokens):
        raise ValueError(f"Action {semantic.get('action')} repeats an input token in its binding.")
    for token in tokens:
        channel_matches(token, {})
    audit_holds_inputs(audit, tokens)
    semantic_qpc, audit_qpc = semantic.get("qpc_100ns"), audit.get("qpc_100ns")
    if not isinstance(semantic_qpc, int) or not isinstance(audit_qpc, int):
        raise ValueError("Semantic action and held-input audit need QPC event timestamps.")
    if audit_qpc < semantic_qpc:
        raise ValueError("The held-input audit predates its semantic action.")
    calls = pair_controller_rpcs(events)
    intervals = []
    for token in tokens:
        press = next((call for call in calls
                      if call["request_qpc_100ns"] >= semantic_qpc
                      and channel_matches(token, call["arguments"])
                      and channel_is_held(token, call["arguments"])), None)
        if press is None:
            raise ValueError(
                f"No acknowledged press RPC for {token} follows action {semantic.get('action')}."
            )
        release = next((call for call in calls
                        if call["request_qpc_100ns"] > press["request_qpc_100ns"]
                        and call["request_qpc_100ns"] >= audit_qpc
                        and channel_matches(token, call["arguments"])
                        and not channel_is_held(token, call["arguments"])), None)
        if release is None:
            raise ValueError(
                f"No acknowledged release RPC for {token} follows its held-input audit."
            )
        start_qpc, end_qpc = press["ack_qpc_100ns"], release["ack_qpc_100ns"]
        if end_qpc <= start_qpc:
            raise ValueError(f"Recorded RPC acknowledgment interval for {token} is empty or reversed.")
        if not start_qpc <= audit_qpc <= end_qpc:
            raise ValueError(
                f"The held-input audit for {token} is outside its acknowledged controller interval."
            )
        intervals.append({
            "input": token,
            "label": TOKEN_LABELS[token],
            "start_qpc_100ns": start_qpc,
            "end_qpc_100ns": end_qpc,
            "press_request_qpc_100ns": press["request_qpc_100ns"],
            "release_request_qpc_100ns": release["request_qpc_100ns"],
        })
    chord_start = max(item["start_qpc_100ns"] for item in intervals)
    chord_end = min(item["end_qpc_100ns"] for item in intervals)
    if chord_end <= chord_start or not chord_start <= audit_qpc <= chord_end:
        raise ValueError("The held-input audit did not confirm the complete chord in its acknowledged interval.")
    return {
        "action": semantic.get("action"),
        "label": semantic.get("label") or semantic.get("action") or "Controller action",
        "inputs": tokens,
        "start_qpc_100ns": chord_start,
        "end_qpc_100ns": chord_end,
        "audit_event_qpc_100ns": audit_qpc,
        "audit_sample_ms": audit.get("sample_ms"),
        "audit_sampled_context": audit.get("sampled_context"),
        "audit_xr_gamepad_buttons": (audit.get("xr_published_gamepad") or {}).get("buttons"),
        "interval_basis": "set_controller_input RPC completion acknowledgments; not native sampled time",
        "channel_intervals": intervals,
    }


def compile_action_cues(case_events, case_id):
    start, finish = case_window(case_events, case_id)
    window = case_events[start:finish + 1]
    actions = [event for event in window if event.get("event") == "semantic_action"]
    if not actions:
        raise ValueError(f"No semantic input was recorded in case {case_id!r}.")
    cues = []
    for action in actions:
        tokens = (action.get("binding") or {}).get("inputs", [])
        audit = next((event for event in window
                      if event.get("event") == "action_input_audit"
                      and event.get("action") == action.get("action")
                      and event.get("qpc_100ns", 0) >= action.get("qpc_100ns", 0)
                      and set(event.get("requested_inputs") or []) == set(tokens)), None)
        if audit is None:
            raise ValueError(
                f"Action {action.get('action')} has no matching held-input audit; refusing to render its control cue."
            )
        cues.append(acknowledged_intervals(window, action, audit))
    return window, cues


def acknowledgment_note(cues):
    if not cues:
        return "No controller acknowledgment timing is available."
    cue = cues[0]
    intervals = cue["channel_intervals"]
    ordered = sorted(intervals, key=lambda item: item["start_qpc_100ns"])
    order = " then ".join(item["label"] for item in ordered)
    note = f"Input channel acknowledgments arrived {order}."
    if len(ordered) > 1:
        separation = (ordered[-1]["start_qpc_100ns"] - ordered[0]["start_qpc_100ns"]) / 1e7
        note += f" The first and last press acknowledgments were {separation * 1000:.1f} ms apart."
    if cue["action"] == "system.native_buttons" and [item["input"] for item in ordered] == ["a", "menu"]:
        note += " This historical take sends A before Menu; it does not validate the later Menu-first dispatch correction."
    note += " Highlight timing follows command acknowledgments. The held-input audit is separate."
    return note


def render(run, case_id, output, title=None, controller_overlay=None, overlay_fps=90):
    run, output = run.resolve(), output.resolve()
    if not output.is_relative_to(ROOT / "artifacts"):
        raise ValueError("Use a new lesson directory under artifacts.")
    result = read(run / "result.json")
    cases = result.get("cases", []) + [case for suite in result.get("suites", []) for case in suite.get("cases", [])]
    case = next((case for case in cases if case.get("id") == case_id), None)
    if case is None:
        raise ValueError(f"Case {case_id!r} does not appear in result.json.")
    if case.get("status") != "observed_pass":
        raise ValueError("A teaching lesson requires an observed action outcome.")
    events = [json.loads(line) for line in (run / "events.jsonl").read_text(encoding="utf-8-sig").splitlines() if line.strip()]
    window, cues = compile_action_cues(events, case_id)
    begin_event = next(event for event in window if event.get("event") == "case_started")
    finish_event = next(event for event in reversed(window) if event.get("event") == "case_finished")
    finished_result = finish_event.get("result") or {}
    if finished_result.get("id") not in (None, case_id):
        raise ValueError(f"case_finished belongs to {finished_result.get('id')!r}, not {case_id!r}.")
    if finished_result.get("status") not in (None, "observed_pass"):
        raise ValueError("The matching case_finished event does not report an observed pass.")
    start_qpc, finish_qpc = begin_event["qpc_100ns"], finish_event["qpc_100ns"]
    if finish_qpc <= start_qpc:
        raise ValueError("Case QPC range is empty or reversed.")
    if controller_overlay is None:
        raise ValueError(
            "A transparent 3D controller inset is required. Pass --controller-overlay with an alpha MOV or numbered alpha PNG sequence."
        )
    overlay_info = controller_overlay_input(controller_overlay, overlay_fps)
    controller_asset_manifest_path = controller_overlay.resolve().parent / "3d-preview-anchors.json"
    controller_asset_manifest = (
        read(controller_asset_manifest_path) if controller_asset_manifest_path.is_file() else None
    )
    index = read(run / "recording" / "index.json")
    action_qpc = min(cue["start_qpc_100ns"] for cue in cues)
    segment = next((item for item in index.get("segments", [])
                    if item.get("status") == "captured"
                    and item.get("video_stats", {}).get("first_qpc_100ns", finish_qpc + 1) <= start_qpc
                    and item.get("video_stats", {}).get("last_qpc_100ns", 0) >= finish_qpc
                    and item.get("video_stats", {}).get("first_qpc_100ns", finish_qpc + 1) <= action_qpc), None)
    if segment is None:
        raise ValueError("No one complete native-eye recording segment covers the case and its acknowledged input.")
    stats = segment["video_stats"]
    if not stats.get("complete") or not stats.get("frames"):
        raise ValueError("Incomplete native-eye recording.")
    source = pathlib.Path(segment.get("output", "")).resolve()
    if not source.is_relative_to(run) or not source.is_file():
        raise ValueError("Recording source is missing or outside this run.")
    render_fov = stats.get("render_fov")
    if not isinstance(render_fov, list) or len(render_fov) != 4:
        raise ValueError("Source recording does not include its render FOV.")
    left, right, up, down = render_fov
    aspect = (math.tan(right) - math.tan(left)) / (math.tan(up) - math.tan(down))
    if not .3 < aspect < 4:
        raise ValueError("Invalid recorded projection aspect.")
    source_width, source_height = int(stats.get("width", 0)), int(stats.get("height", 0))
    crop_width, crop_height, crop_x, crop_y = source_crop_rect(source_width, source_height, aspect)
    origin = stats["first_qpc_100ns"]
    clip_start_qpc = start_qpc - 2_000_000
    clip_end_qpc = finish_qpc + 6_000_000
    start = max(0., (clip_start_qpc - origin) / 1e7)
    end = min((stats["last_qpc_100ns"] - origin) / 1e7, (clip_end_qpc - origin) / 1e7)
    duration = end - start
    if duration <= 0:
        raise ValueError("Empty action excerpt.")
    if overlay_info["duration"] + 1e-3 < duration:
        raise ValueError(
            f"3D overlay is {overlay_info['duration']:.3f}s, shorter than the {duration:.3f}s lesson excerpt."
        )
    output.mkdir(parents=True, exist_ok=False)
    if controller_asset_manifest is not None:
        shutil.copy2(controller_asset_manifest_path, output / "controller-inset.json")
    paper, ink, red, edge = "0xf2efe5", "0x252625", "0xaf2624", "0x66655e"
    font = "C\\:/Windows/Fonts/bahnschrift.ttf"
    filters, texts = [], []

    def caption(text, x, y, size, color=ink):
        name = f"caption-{len(texts):02d}.txt"
        (output / name).write_text(text, encoding="utf-8")
        texts.append(name)
        return f"drawtext=fontfile='{font}':textfile='{name}':expansion=none:x={x}:y={y}:fontsize={size}:fontcolor={color}:line_spacing=12"

    canvas_width, canvas_height = 1920, 1080
    game_height = 920
    game_width = int(game_height * aspect) // 2 * 2
    game_x, game_y = 44, 86
    inset_size, inset_x, inset_y = 720, 1160, 190
    if game_x + game_width + 24 > inset_x:
        game_width = inset_x - game_x - 24
        game_height = int(game_width / aspect) // 2 * 2
    graph = [
        f"[0:v]trim=start={start:.7f}:end={end:.7f},setpts=PTS-STARTPTS,"
        f"crop={crop_width}:{crop_height}:{crop_x}:{crop_y},"
        f"scale={game_width}:{game_height}:flags=lanczos,setsar=1,fps={overlay_fps:g},"
        f"pad={canvas_width}:{canvas_height}:{game_x}:{game_y}:color={paper}[field]",
        f"[1:v]trim=duration={duration:.7f},setpts=PTS-STARTPTS,"
        f"scale={inset_size}:{inset_size}:force_original_aspect_ratio=decrease:flags=lanczos,"
        f"pad={inset_size}:{inset_size}:(ow-iw)/2:(oh-ih)/2:color=0x00000000,format=rgba[controller]",
        f"[field][controller]overlay=x={inset_x}:y={inset_y}:shortest=0:format=auto[composed]",
    ]
    cues_in_clip = []
    for cue in cues:
        action_start = (cue["start_qpc_100ns"] - origin) / 1e7 - start
        action_end = (cue["end_qpc_100ns"] - origin) / 1e7 - start
        if not 0 <= action_start < action_end <= duration:
            raise ValueError("Acknowledged controller interval lies outside the recorded excerpt.")
        channel_times = []
        for channel in cue["channel_intervals"]:
            channel_start = (channel["start_qpc_100ns"] - origin) / 1e7 - start
            channel_end = (channel["end_qpc_100ns"] - origin) / 1e7 - start
            if not 0 <= channel_start < channel_end <= duration:
                raise ValueError(f"Acknowledged {channel['input']} interval lies outside the recorded excerpt.")
            channel_times.append({
                **channel,
                "start_seconds": channel_start,
                "end_seconds": channel_end,
            })
        cues_in_clip.append({
            **cue,
            "start_seconds": action_start,
            "end_seconds": action_end,
            "channel_intervals": channel_times,
        })
    action_label = cues[0]["label"]
    before_label = context_title(case.get("before") or {})
    after_label = context_title(case.get("after") or {})
    case_title = title or ("Equip binoculars" if cues[0]["action"] == "gameplay.equip_binoculars"
                           else f"{before_label} to {after_label}")
    controller_profile_id = ((controller_asset_manifest or {}).get("controller_profile") or {}).get("profile_id")
    controller_title = ("QUEST 3 TOUCH PLUS · ILLUSTRATIVE VIEW"
                        if controller_profile_id == "meta-quest-touch-plus"
                        else "CONTROLLER LAYOUT · ILLUSTRATIVE")
    filters += [
        f"drawbox=x={game_x - 4}:y={game_y - 4}:w={game_width + 8}:h={game_height + 8}:color={edge}:t=2",
        f"drawbox=x=36:y=23:w=5:h=47:color={red}:t=fill",
        caption("MGS5 / FIELD GUIDE", 52, 18, 17),
        caption(case_title.upper(), 52, 43, 23),
        caption("TPP · " + cues[0]["label"], 52, 69, 12, red),
        caption(controller_title, inset_x + 30, inset_y - 20, 16),
        caption("Native source-eye game view · command-acknowledgment cues", 52, 1020, 12),
        caption("Equip pass only · eye alignment, zoom, marking, and analysis unverified", 52, 1041, 11),
    ]
    ass = ["[Script Info]", "ScriptType: v4.00+", f"PlayResX: {canvas_width}", f"PlayResY: {canvas_height}",
           "[V4+ Styles]", "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding",
           "Style: Cue,Arial,32,&H00252625,&H00252625,&H00E5EFF2,&H00000000,0,0,0,0,100,100,0,0,1,0,0,7,0,0,0,1",
           "[Events]", "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text"]

    def overlay(start_time, end_time, body, layer=0):
        ass.append(f"Dialogue: {layer},{stamp(start_time)},{stamp(end_time)},Cue,,0,0,0,,{body}")

    cues_manifest = []
    for cue in cues_in_clip:
        label = "HOLD " + " + ".join(TOKEN_LABELS[token] for token in cue["inputs"])
        overlay(cue["start_seconds"], cue["end_seconds"],
                r"{\fad(250,250)\move(52,952,190,952,0,350)\fs28\c&H002426AF&}" + label, 2)
        cues_manifest.append(cue)
    (output / "input-cues.ass").write_text("\n".join(ass) + "\n", encoding="utf-8")
    graph.append("[composed]" + ",".join(filters) +
                 f",subtitles=input-cues.ass,fps={overlay_fps:g},format=yuv420p[v]")
    graph.append(f"[0:a]atrim=start={start:.7f}:end={end:.7f},asetpts=PTS-STARTPTS[a]")
    graph_text = ";".join(graph)
    (output / "lesson-filter.txt").write_text(graph_text, encoding="utf-8")
    destination = output / "lesson.mp4"
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        raise ValueError("ffmpeg is required to render the native-eye lesson.")
    subprocess.run([ffmpeg, "-hide_banner", "-loglevel", "error", "-n", "-filter_complex_threads", "2",
                    "-threads", "2", "-i", str(source), "-threads", "2", *overlay_info["input_args"], "-filter_complex_script", "lesson-filter.txt",
                    "-map", "[v]", "-map", "[a]", "-t", f"{duration:.7f}",
                    "-c:v", "libx264", "-preset", "fast", "-crf", "18", "-threads", "2", "-fps_mode", "vfr",
                    "-c:a", "aac", "-b:a", "192k", "-movflags", "+faststart", str(destination)],
                   cwd=output, check=True)
    source_duration = (stats["last_qpc_100ns"] - stats["first_qpc_100ns"]) / 1e7
    source_fps = stats["frames"] / source_duration if source_duration > 0 else None
    note = acknowledgment_note(cues)
    manifest = {
        "case": case_id,
        "title": case_title,
        "action": cues[0]["action"],
        "case_status": case.get("status"),
        "before_context": before_label,
        "after_context": after_label,
        "input_label": action_label,
        "source": str(source),
        "source_kind": segment.get("source"),
        "source_eye": {0: "left", 1: "right"}.get(stats.get("eye"), "unknown"),
        "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "source_fps": source_fps,
        "source_frames": stats["frames"],
        "source_dropped_frames": stats.get("dropped"),
        "source_seconds": source_duration,
        "run_identity": read(run / "identity.json"),
        "excerpt_start": start,
        "excerpt_end": end,
        "input_cues": cues_manifest,
        "acknowledgment_note": note,
        "timeline_basis": "set_controller_input RPC completion acknowledgments; not native sampled time",
        "held_input_audit_required": True,
        "native_held_input_audit": [
            {"action": cue["action"], "event_qpc_100ns": cue["audit_event_qpc_100ns"],
             "sample_ms": cue["audit_sample_ms"], "sampled_context": cue["audit_sampled_context"],
             "requested_inputs": cue["inputs"], "xr_gamepad_buttons": cue["audit_xr_gamepad_buttons"]}
            for cue in cues
        ],
        "projection_aspect": aspect,
        "source_crop_rect": {"width": crop_width, "height": crop_height, "x": crop_x, "y": crop_y},
        "composite_fps": overlay_fps,
        "game_frame_treatment": "source frames repeated at composite cadence for control-cue timing; no interpolation",
        "controller_overlay": {
            "source": str(controller_overlay.resolve()),
            "format": overlay_info["format"],
            "width": overlay_info["width"],
            "height": overlay_info["height"],
            "duration_seconds": overlay_info["duration"],
            "sha256": overlay_info["sha256"],
            "pose_provenance": (controller_asset_manifest or {}).get(
                "pose_provenance",
                "illustrative controller layout; tracked_pose readbacks are not synchronized to video-frame times or linked to this inset",
            ),
            "asset_manifest": "controller-inset.json" if controller_asset_manifest is not None else None,
            "controller_profile": (controller_asset_manifest or {}).get("controller_profile"),
        },
        "interpolation": False,
        "scope": ("One observed binocular equip/latch action. It does not verify optic view, eye alignment, zoom, target marking, or intel analysis."
                  if cues[0]["action"] == "gameplay.equip_binoculars"
                  else "One observed action, not the complete showcase."),
        "visual_review": "pending",
    }
    (output / "lesson.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(destination)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", required=True, type=pathlib.Path)
    parser.add_argument("--case", required=True)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--title", help="Player-facing title for the observed before/during/after change.")
    parser.add_argument(
        "--controller-overlay", required=True, type=pathlib.Path,
        help="Transparent ProRes/RGBA video or directory of consecutively numbered alpha PNG frames from render-controller-inset.py.",
    )
    parser.add_argument("--overlay-fps", type=float, default=90,
                        help="Composite/inset frame rate; source gameplay frames are repeated, never interpolated (default: 90).")
    args = parser.parse_args()
    render(args.run, args.case, args.output, args.title, args.controller_overlay, args.overlay_fps)
