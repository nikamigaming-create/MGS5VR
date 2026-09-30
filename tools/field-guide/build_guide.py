#!/usr/bin/env python3
"""Build a local field-guide prototype from the current controls export and evidence."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUTPUT = ROOT / "artifacts" / "field-guide-20260926"


def read_json(path: Path):
    with path.open("r", encoding="utf-8-sig") as stream:
        return json.load(stream)


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def relative(path: Path) -> str:
    resolved = path.resolve()
    try:
        return resolved.relative_to(ROOT.resolve()).as_posix()
    except ValueError:
        return str(resolved)


def config_display(path: Path) -> str:
    resolved = path.resolve()
    try:
        local = resolved.relative_to(ROOT.resolve()).as_posix()
        return "Repository file · " + local
    except ValueError:
        return "Selected file · " + resolved.name


BOT_CHANNELS = {
    "a": ("right", "A", None), "b": ("right", "B", None),
    "x": ("left", "X", None), "y": ("left", "Y", None),
    "menu": ("left", "Menu", None),
    "left_stick_click": ("left", "ThumbstickClick", None),
    "right_stick_click": ("right", "ThumbstickClick", None),
    "left_grip": ("left", "Grip", None), "right_grip": ("right", "Grip", None),
    "left_trigger": ("left", "Trigger", None), "right_trigger": ("right", "Trigger", None),
    "left_stick_up": ("left", "Thumbstick", "Y"), "left_stick_down": ("left", "Thumbstick", "Y"),
    "left_stick_left": ("left", "Thumbstick", "X"), "left_stick_right": ("left", "Thumbstick", "X"),
    "right_stick_up": ("right", "Thumbstick", "Y"), "right_stick_down": ("right", "Thumbstick", "Y"),
    "right_stick_left": ("right", "Thumbstick", "X"), "right_stick_right": ("right", "Thumbstick", "X"),
}


def read_events(path: Path) -> list[dict]:
    return [json.loads(line) for line in path.read_text(encoding="utf-8-sig").splitlines() if line.strip()]


def case_event_window(events: list[dict], case_id: str) -> tuple[int, int]:
    start = next((index for index, event in enumerate(events)
                  if event.get("event") == "case_started" and event.get("case_id") == case_id), None)
    if start is None:
        raise SystemExit(f"No case_started event for {case_id}.")
    for index in range(start + 1, len(events)):
        event = events[index]
        result_id = (event.get("result") or {}).get("id")
        if event.get("event") == "case_finished" and (
                event.get("case_id") == case_id or result_id == case_id):
            return start, index
        if event.get("event") == "case_started":
            raise SystemExit(f"Case {case_id} has no matching finish before the next case.")
    raise SystemExit(f"No case_finished event for {case_id}.")


def audit_holds_inputs(audit: dict, tokens: list[str]) -> bool:
    physical = audit.get("physical")
    buttons = physical.get("buttons") if isinstance(physical, dict) else None
    sticks = physical.get("sticks") if isinstance(physical, dict) else None
    if not isinstance(buttons, list) or not isinstance(sticks, list) or len(buttons) < 11 or len(sticks) != 4:
        return False
    button_index = {
        "a": 0, "b": 1, "x": 2, "y": 3, "menu": 4,
        "left_stick_click": 5, "right_stick_click": 6,
        "left_grip": 7, "right_grip": 8, "left_trigger": 9, "right_trigger": 10,
    }
    stick_axis = {
        "left_stick_left": (0, -1.), "left_stick_right": (0, 1.),
        "left_stick_down": (1, -1.), "left_stick_up": (1, 1.),
        "right_stick_left": (2, -1.), "right_stick_right": (2, 1.),
        "right_stick_down": (3, -1.), "right_stick_up": (3, 1.),
    }
    for token in tokens:
        if token in button_index:
            if not isinstance(buttons[button_index[token]], (int, float)) or buttons[button_index[token]] < .5:
                return False
        elif token in stick_axis:
            index, sign = stick_axis[token]
            if not isinstance(sticks[index], (int, float)) or sticks[index] * sign < .5:
                return False
        else:
            return False
    return set(audit.get("requested_inputs", [])) == set(tokens)


def case_input_event(events: list[dict], case_id: str, run_stance: dict) -> dict | None:
    start, finish = case_event_window(events, case_id)
    window = events[start + 1:finish]
    semantic = next((event for event in window if event.get("event") == "semantic_action"
                     and event.get("action") == "gameplay.stance"), None)
    if not semantic:
        return None
    tokens = (semantic.get("binding") or {}).get("inputs", [])
    if not tokens:
        tokens = [token for binding in (run_stance or {}).get("bindings", []) for token in binding.get("inputs", [])]
    if not tokens:
        return None
    audit = next((event for event in window if event.get("event") == "action_input_audit"
                  and event.get("action") == semantic.get("action")
                  and event.get("qpc_100ns", 0) >= semantic.get("qpc_100ns", 0)
                  and set(event.get("requested_inputs", [])) == set(tokens)
                  and audit_holds_inputs(event, tokens)), None)
    if not audit:
        return None
    components = {BOT_CHANNELS[token] for token in tokens if token in BOT_CHANNELS}
    press = next((event for event in window if event.get("event") == "rpc_started"
                  and event.get("tool") == "set_controller_input"
                  and event.get("qpc_100ns", 0) >= semantic.get("qpc_100ns", 0)
                  and float((event.get("arguments") or {}).get("value") or 0) > 0
                  and ((event.get("arguments") or {}).get("hand"),
                       (event.get("arguments") or {}).get("component"),
                       (event.get("arguments") or {}).get("sub_component")) in components), None)
    if not press:
        return None
    arguments = press["arguments"]
    return {
        "hand": arguments.get("hand"),
        "component": arguments.get("component"),
        "sub_component": arguments.get("sub_component"),
        "value": arguments.get("value"),
        "monotonic_seconds": press.get("monotonic_seconds"),
        "qpc_100ns": audit.get("qpc_100ns"),
        "sample_ms": audit.get("sample_ms"),
        "requested_inputs": tokens,
        "audit_confirmed_held": True,
    }


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--controls-exe", type=Path, default=ROOT / "build" / "Release" / "mgs5vr_controls.exe")
    parser.add_argument("--controls-config", type=Path, default=ROOT / "config" / "mgs5vr-controls.ini")
    parser.add_argument("--coverage", type=Path, default=ROOT / "docs" / "NATIVE_CONTEXT_COVERAGE.json")
    parser.add_argument("--action-catalog", type=Path, default=ROOT / "tools" / "gameplay_bot" / "catalogs" / "mgsv-action-coverage.json")
    parser.add_argument("--posture-result", type=Path, default=ROOT / "artifacts" / "bot-20260926" / "live-posture-run-01" / "result.json")
    parser.add_argument("--posture-identity", type=Path, default=ROOT / "artifacts" / "bot-20260926" / "live-posture-run-01" / "identity.json")
    parser.add_argument("--posture-events", type=Path, default=ROOT / "artifacts" / "bot-20260926" / "live-posture-run-01" / "events.jsonl")
    parser.add_argument("--posture-bindings", type=Path, default=ROOT / "artifacts" / "bot-20260926" / "live-posture-run-01" / "bindings.json")
    parser.add_argument("--posture-capture", type=Path, default=ROOT / "artifacts" / "bot-20260926" / "live-posture-finaleye-01" / "capture.json")
    parser.add_argument("--posture-video", type=Path, default=ROOT / "artifacts" / "bot-20260926" / "live-posture-finaleye-01" / "simulator.mp4")
    parser.add_argument("--field-lesson", type=Path, default=DEFAULT_OUTPUT / "lessons" / "binoculars-01")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    paths = (args.controls_exe, args.controls_config, args.coverage, args.action_catalog, args.posture_result,
             args.posture_identity, args.posture_events, args.posture_bindings, args.posture_capture)
    missing = [str(path) for path in paths if not path.is_file()]
    if missing:
        raise SystemExit("Required source file missing:\n  " + "\n  ".join(missing))

    result = subprocess.run(
        [str(args.controls_exe), "--bindings-json", str(args.controls_config)],
        cwd=ROOT, check=True, text=True, capture_output=True,
    )
    bindings = json.loads(result.stdout)
    if bindings.get("schema") != 1 or not bindings.get("actions"):
        raise SystemExit("Controls tool returned an unexpected bindings export.")

    coverage = read_json(args.coverage)
    action_catalog = read_json(args.action_catalog)
    slots = action_catalog.get("denominators", {}).get("loadout_positions", {})
    families = action_catalog.get("denominators", {}).get("state_families", {})
    if not slots.get("count") or not families.get("count"):
        raise SystemExit("Action catalog is missing its loadout or state-family denominator.")
    records = coverage.get("records", [])
    if not records:
        raise SystemExit("Native context coverage has no records.")
    posture_record = next((item for item in records if item.get("id") == "TPP.FOOT.POSTURE_LOCOMOTION"), None)
    if posture_record is None:
        raise SystemExit("Coverage inventory is missing TPP.FOOT.POSTURE_LOCOMOTION.")

    posture_result = read_json(args.posture_result)
    posture_identity = read_json(args.posture_identity)
    posture_bindings = read_json(args.posture_bindings)
    capture = read_json(args.posture_capture)
    lesson_manifest_path = args.field_lesson / "lesson.json"
    field_lesson = read_json(lesson_manifest_path) if lesson_manifest_path.is_file() else None
    optics_record = next((item for item in records if item.get("id") == "TPP.OPTICS.BINOCULAR_MARKING"), None)
    if optics_record is None:
        raise SystemExit("Coverage inventory is missing TPP.OPTICS.BINOCULAR_MARKING.")
    case_id = "standing-to-crouched-single"
    posture_case = next((item for item in posture_result.get("cases", []) if item.get("id") == case_id), None)
    if posture_case is None:
        raise SystemExit("Current posture result does not contain " + case_id)

    run_events = read_events(args.posture_events)
    output = args.output.resolve()
    evidence_dir = output / "evidence"
    evidence_dir.mkdir(parents=True, exist_ok=True)

    def evidence_copy(source: Path, name: str) -> str:
        if not source.is_file():
            return ""
        target = evidence_dir / name
        shutil.copy2(source, target)
        return target.relative_to(output).as_posix()

    evidence = {
        "result_url": evidence_copy(args.posture_result, "posture-result.json"),
        "identity_url": evidence_copy(args.posture_identity, "posture-identity.json"),
        "run_bindings_url": evidence_copy(args.posture_bindings, "posture-run-bindings.json"),
        "capture_url": evidence_copy(args.posture_capture, "posture-capture.json"),
        "video_url": evidence_copy(args.posture_video, "crouch-diagnostic.mp4"),
    }
    field_lesson_evidence = None
    if field_lesson is not None:
        if (field_lesson.get("case") != "vr-binocular-latch-enter"
                or field_lesson.get("case_status") != "observed_pass"
                or field_lesson.get("action") != "gameplay.equip_binoculars"):
            raise SystemExit("Binocular field lesson must match the passing vr-binocular-latch-enter equip action.")
        lesson_video = args.field_lesson / "lesson.mp4"
        if not lesson_video.is_file():
            raise SystemExit("Field-lesson manifest has no matching lesson.mp4.")
        lesson_output_dir = output / "lessons" / "binoculars-01"
        lesson_output_dir.mkdir(parents=True, exist_ok=True)
        if lesson_video.resolve() != (lesson_output_dir / "lesson.mp4").resolve():
            shutil.copy2(lesson_video, lesson_output_dir / "lesson.mp4")
        if lesson_manifest_path.resolve() != (lesson_output_dir / "lesson.json").resolve():
            shutil.copy2(lesson_manifest_path, lesson_output_dir / "lesson.json")
        inset_manifest = args.field_lesson / "controller-inset.json"
        inset_manifest_url = ""
        if inset_manifest.is_file():
            inset_output = lesson_output_dir / "controller-inset.json"
            if inset_manifest.resolve() != inset_output.resolve():
                shutil.copy2(inset_manifest, inset_output)
            inset_manifest_url = "lessons/binoculars-01/controller-inset.json"
        field_lesson_evidence = {
            "coverage_record_id": optics_record["id"],
            "title": field_lesson.get("title", "Equip binoculars"),
            "case_id": field_lesson.get("case"),
            "case_status": field_lesson.get("case_status"),
            "action": field_lesson.get("action"),
            "before_context": field_lesson.get("before_context"),
            "after_context": field_lesson.get("after_context"),
            "input_label": field_lesson.get("input_label"),
            "input_cues": field_lesson.get("input_cues", []),
            "acknowledgment_note": field_lesson.get("acknowledgment_note"),
            "source_kind": field_lesson.get("source_kind"),
            "source_eye": field_lesson.get("source_eye"),
            "source_fps": field_lesson.get("source_fps"),
            "source_seconds": field_lesson.get("source_seconds"),
            "source_dropped_frames": field_lesson.get("source_dropped_frames"),
            "visual_review": field_lesson.get("visual_review", "pending"),
            "controls_hash_matches_current": (field_lesson.get("run_identity") or {}).get("controls_sha256") == digest(args.controls_config),
            "scope": field_lesson.get("scope"),
            "video_url": "lessons/binoculars-01/lesson.mp4",
            "manifest_url": "lessons/binoculars-01/lesson.json",
            "controller_manifest_url": inset_manifest_url,
        }
    run_dir = args.posture_result.parent
    before_image = next(iter(sorted(run_dir.glob("*standing-to-crouched-single-before*left.png"))), None)
    after_image = next(iter(sorted(run_dir.glob("*standing-to-crouched-single-after*left.png"))), None)
    evidence["before_image_url"] = evidence_copy(before_image, "posture-before-left.png") if before_image else ""
    evidence["after_image_url"] = evidence_copy(after_image, "posture-after-left.png") if after_image else ""

    before_native = posture_case.get("before", {}).get("native", {})
    after_native = posture_case.get("after", {}).get("native", {})
    run_stance = next((item for item in posture_bindings.get("actions", [])
                       if item.get("name") == "gameplay.stance"), None)
    input_event = case_input_event(run_events, case_id, run_stance)
    if posture_case.get("status") == "observed_pass" and input_event is None:
        raise SystemExit("The passing posture case has no matching case-local held-input audit.")
    current_stance = next((item for item in bindings.get("actions", [])
                           if item.get("name") == "gameplay.stance"), None)
    posture = {
        "coverage_record_id": posture_record["id"],
        "case_id": posture_case["id"],
        "case_status": posture_case.get("status", "unknown"),
        "visual_acceptance": posture_case.get("visual_acceptance", posture_result.get("visual_acceptance", "pending")),
        "before": {name: before_native.get(name) for name in ("status_STAND", "status_SQUAT", "status_CRAWL")},
        "after": {name: after_native.get(name) for name in ("status_STAND", "status_SQUAT", "status_CRAWL")},
        "input_event": input_event,
        "run_stance_action": run_stance,
        "run_stance_matches_current": run_stance == current_stance,
        "capture_source": capture.get("source"),
        "capture_eye": capture.get("eye"),
        "capture_fps": capture.get("measured_capture_fps"),
        "capture_duration": capture.get("seconds"),
        "stereo_acceptance": capture.get("stereo_acceptance", False),
        "full_mod_acceptance": capture.get("full_mod_acceptance", False),
        "capture_frames": len(capture.get("frames", [])),
        "run_id": posture_identity.get("run_id"),
        "controls_hash_matches_current": posture_identity.get("controls_sha256") == digest(args.controls_config),
        "run_runtime_config_sha256": posture_identity.get("config_sha256"),
        **evidence,
    }
    metadata = {
        "built_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "controls_command": relative(args.controls_exe) + " --bindings-json " + relative(args.controls_config),
        "controls_executable_sha256": digest(args.controls_exe),
        "controls_config": relative(args.controls_config),
        "controls_config_display": config_display(args.controls_config),
        "controls_config_sha256": digest(args.controls_config),
        "coverage_inventory": relative(args.coverage),
        "coverage_updated": coverage.get("updated"),
        "coverage_record_count": len(records),
        "action_catalog": relative(args.action_catalog),
        "action_catalog_sha256": digest(args.action_catalog),
        "loadout_position_count": slots["count"],
        "tracked_state_family_count": families["count"],
        "direct_readback_family_count": families.get("currently_observable_families", 0),
        "posture_run": relative(args.posture_result),
        "posture_capture": relative(args.posture_capture),
        "posture_video": relative(args.posture_video),
        "guide_output": relative(output),
        "note": "Prototype only; the coverage inventory, bounded posture run, and any linked binocular equip pass have different scopes."
    }
    data = {
        "metadata": metadata,
        "bindings": bindings,
        "coverage": {
            "inventory_id": coverage.get("inventory_id"),
            "updated": coverage.get("updated"),
            "scope": coverage.get("scope"),
            "records": records,
        },
        "action_catalog": {
            "url": "action-coverage-catalog.json",
            "loadout_position_count": slots["count"],
            "tracked_state_family_count": families["count"],
            "direct_readback_family_count": families.get("currently_observable_families", 0),
            "loadout_definition": slots.get("definition"),
            "state_family_definition": families.get("definition"),
        },
        "posture": posture,
        "field_lesson": field_lesson_evidence,
    }

    output.mkdir(parents=True, exist_ok=True)
    tool_dir = Path(__file__).resolve().parent
    for filename in ("index.html", "guide.css", "guide.js"):
        shutil.copy2(tool_dir / filename, output / filename)
    shutil.copy2(args.action_catalog, output / "action-coverage-catalog.json")
    (output / "effective-bindings.json").write_text(
        json.dumps(bindings, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    (output / "guide-data.js").write_text(
        "window.FIELD_GUIDE_DATA = " + json.dumps(data, ensure_ascii=False, separators=(",", ":")).replace("</", "<\\/") + ";\n",
        encoding="utf-8",
    )
    (output / "provenance.json").write_text(
        json.dumps(metadata, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    lesson_readme = (
        "A bounded binocular equip pass is linked from the TPP optics situation. "
        "It shows the equip action only; it does not establish eye alignment, zoom, "
        "marking, or analysis. The clip manifest records input cue timing and sources.\n"
        if field_lesson_evidence else
        "No binocular video is attached in this build. The TPP optics record shows "
        "effective mapped inputs; marking and other optic behaviors remain unverified.\n"
    )
    readme = """# MGS5VR Field Guide — local prototype

Open index.html in a browser. The guide is a local static page with no network
requests or package dependencies.

Rebuild from the repository root with:

    python tools/field-guide/build_guide.py

The builder exports the selected controls file with
build/Release/mgs5vr_controls.exe --bindings-json, reads the situation index
and the current bounded crouch run, then regenerates this folder. To reflect
the installed game settings, pass --controls-config with the installed
mgs5vr-controls.ini path. Pass --posture-video PATH and --posture-capture PATH
when newer video evidence is available.

Mapped controls, simulator observations, and video notes are shown separately.
The 35 situations are an incomplete catalog, not a claim that every weapon or
mode has been tried. A separate TPP catalog lists 19 loadout positions and
tracks 13 state families; listed slots and readbacks do not prove actions work.
""" + lesson_readme + """The crouch clip remains a one-eye 3 FPS diagnostic with visual review pending.
"""
    (output / "README.md").write_text(readme, encoding="utf-8")

    print("Built " + str(output / "index.html"))
    print(f"Exported {len(bindings['actions'])} actions, {len(bindings.get('axes', []))} axes, {len(records)} situations.")
    print("Controls file: " + metadata["controls_config_display"])
    print("Crouch case: " + posture["case_status"] + "; video FPS: " + str(posture["capture_fps"]) +
          "; controls file matches run: " + str(posture["controls_hash_matches_current"]) + ".")
    if field_lesson_evidence:
        print("Field lesson: " + str(field_lesson_evidence["case_status"]) + "; source FPS: " +
              str(field_lesson_evidence["source_fps"]) + "; output: " + field_lesson_evidence["video_url"] + ".")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
