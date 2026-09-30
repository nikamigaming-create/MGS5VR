"""Extract controller overlay timing from the actual recorded action and audit."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("lesson", ROOT / "tools/render-gameplay-lesson.py")
lesson = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lesson)


def prepare(run, case_id):
    run = Path(run).resolve()
    result = lesson.read(run / "result.json")
    cases = result.get("cases", []) + [case for suite in result.get("suites", []) for case in suite.get("cases", [])]
    case = next((row for row in cases if row["id"] == case_id), None)
    if not case or case["status"] != "observed_pass":
        raise ValueError("A recorded observed outcome is required")
    events = [json.loads(line) for line in (run / "events.jsonl").read_text().splitlines() if line]
    window, cues = lesson.compile_action_cues(events, case_id)
    begin = next(row for row in window if row["event"] == "case_started")["qpc_100ns"]
    end = next(row for row in reversed(window) if row["event"] == "case_finished")["qpc_100ns"]
    segments = lesson.read(run / "recording/index.json")["segments"]
    segment = next((row for row in segments if row.get("status") == "captured" and row["video_stats"].get("complete")
                    and row["video_stats"]["first_qpc_100ns"] <= begin < end <= row["video_stats"]["last_qpc_100ns"]), None)
    if not segment:
        raise ValueError("No complete recording covers this action")
    begin = max(begin - 2000000, segment["video_stats"]["first_qpc_100ns"])
    end = min(end + 6000000, segment["video_stats"]["last_qpc_100ns"])
    inputs = {}
    for cue in cues:
        for channel in cue["channel_intervals"]:
            token = channel["input"]
            if token in inputs:
                raise ValueError("Repeated input intervals need a multi-interval controller renderer")
            inputs[token] = {"press_s": (channel["start_qpc_100ns"] - begin) / 1e7,
                             "release_s": (channel["end_qpc_100ns"] - begin) / 1e7}
    return {"schema": 1, "source_take": str(run), "case": case_id, "duration_seconds": (end-begin)/1e7,
            "inputs": inputs, "events_sha256": hashlib.sha256((run / "events.jsonl").read_bytes()).hexdigest(),
            "timing_source": "Acknowledged controller RPCs corroborated by native input audit",
            "orientation": "Illustrative model orientation; not atomic runtime controller pose"}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", type=Path, required=True)
    parser.add_argument("--case", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(prepare(args.run, args.case), indent=2), encoding="utf-8")
    print(args.output)
