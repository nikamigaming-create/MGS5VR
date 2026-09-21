"""Summarize bounded MGS5VR log windows without controlling the game or Windows.

Timings overlap: never add CPU, GPU and mailbox measurements together.
Percentiles here summarize reported windows, not unrecorded per-frame samples.
"""
import argparse
import json
import math
import pathlib
import re
import statistics


def summarize(text, start_ms=None, end_ms=None):
    patterns = {
        "native_stereo": re.compile(r"Native performance pairs_fps=([\d.]+) interval_ms_mean_p95_max=([\d.,]+).*?scene_gpu_elapsed_ms_mean_p95_max=([\d.,]+).*?scene_cpu_ms_mean_p95_max=([\d.,]+)"),
        "present": re.compile(r"Native present ms capture_mean_max=([\d.,]+) pacing_mean_max=([\d.,]+) present_mean_max=([\d.,]+)"),
        "mailbox": re.compile(r"Mailbox (producer|consumer) ms acquire_queue_flush_release=([\d.,;]+)"),
        "xr": re.compile(r"XR performance runtime_hz=([\d.]+) submissions_fps=([\d.]+) new_pairs_fps=([\d.]+) repeated_or_empty=(\d+) empty=(\d+)"),
    }
    rows = []
    for line in text.splitlines():
        stamp = re.match(r"^(\d+) ", line)
        if not stamp:
            continue
        when = int(stamp[1])
        if (start_ms is not None and when < start_ms) or (end_ms is not None and when > end_ms):
            continue
        for kind, pattern in patterns.items():
            match = pattern.search(line)
            if not match:
                continue
            values = {}
            if kind == "native_stereo":
                values["pairs_fps"] = float(match[1])
                for prefix, group in zip(("frame_interval", "scene_gpu_elapsed", "scene_cpu"), match.groups()[1:]):
                    for label, value in zip(("mean", "p95", "max"), group.split(",")):
                        values[f"{prefix}_{label}_ms"] = float(value)
            elif kind == "present":
                for prefix, group in zip(("capture", "pacing", "present"), match.groups()):
                    for label, value in zip(("mean", "max"), group.split(",")):
                        values[f"{prefix}_{label}_ms"] = float(value)
            elif kind == "mailbox":
                kind += "_" + match[1]
                for prefix, group in zip(("acquire", "queue", "flush", "release"), match[2].split(";")):
                    for label, value in zip(("mean", "max"), group.split(",")):
                        values[f"{prefix}_{label}_ms"] = float(value)
            else:
                for label, value in zip(("runtime_hz", "submissions_fps", "new_pairs_fps", "repeated_or_empty", "empty"), match.groups()):
                    values[label] = float(value)
            if all(math.isfinite(value) for value in values.values()):
                rows.append({"timestamp_ms": when, "kind": kind, **values})
            break
    metrics = {}
    for row in rows:
        for key, value in row.items():
            if key in ("timestamp_ms", "kind"):
                continue
            metrics.setdefault(row["kind"] + "." + key, []).append(value)
    return {
        "start_ms": start_ms, "end_ms": end_ms,
        "window_statistics": {
            key: {"windows": len(values), "minimum": min(values), "median": statistics.median(values), "maximum": max(values)}
            for key, values in sorted(metrics.items())
        },
        "windows": rows,
        "limits": [
            "CPU, GPU timeline, capture and mailbox timings overlap; do not sum them.",
            "Medians are across logged windows. Frame percentiles cannot be reconstructed from window percentiles.",
            "Runtime submissions and fresh stereo frames are different metrics.",
            "This log does not measure compositor encoding cost or physical headset display timing.",
        ],
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("--start-ms", type=int)
    parser.add_argument("--end-ms", type=int)
    parser.add_argument("--output", type=pathlib.Path)
    args = parser.parse_args()
    report = summarize(args.log.read_text(encoding="utf-8-sig", errors="replace"), args.start_ms, args.end_ms)
    report["log"] = str(args.log.resolve())
    result = json.dumps(report, indent=2)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(result + "\n", encoding="utf-8")
    else:
        print(result)
