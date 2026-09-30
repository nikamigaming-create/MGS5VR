#!/usr/bin/env python3
"""Tail gameplay-bot events and print compact supervision notifications."""
from __future__ import annotations

import argparse
import json
import pathlib
import sys
import time
from collections import deque


IMPORTANT_EVENTS = {
    "attention_required", "milestone_reached", "decision_rejected", "supervisor_waiting",
    "plan_interrupted", "plan_finished", "case_finished", "run_finished",
}


def event_identity(event: dict, line_number: int) -> tuple:
    """Prefer producer sequence IDs; fall back to stable event coordinates."""
    for key in ("event_id", "sequence", "event_sequence", "seq"):
        if key in event:
            return (event.get("run_id"), key, _freeze(event[key]))
    stamp = next((event[k] for k in ("qpc_100ns", "unix_ns", "monotonic_seconds") if k in event), None)
    if stamp is None:
        # The line offset fallback still deduplicates repeated scans of a file.
        stamp = ("line", line_number)
    discriminator = event.get("case_id", event.get("id", event.get("action")))
    return (event.get("run_id"), event.get("event"), _freeze(stamp), _freeze(discriminator))


def _freeze(value):
    if isinstance(value, dict):
        return tuple(sorted((key, _freeze(item)) for key, item in value.items()))
    if isinstance(value, (list, tuple)):
        return tuple(_freeze(item) for item in value)
    try:
        hash(value)
        return value
    except TypeError:
        return repr(value)


def _first(mapping, *keys):
    for key in keys:
        value = mapping.get(key)
        if value not in (None, ""):
            return value
    return None


def _case_result(event: dict) -> dict:
    result = event.get("result")
    return result if isinstance(result, dict) else {}


def is_important(event: dict) -> bool:
    name = event.get("event")
    if name in IMPORTANT_EVENTS - {"case_finished"}:
        return True
    if name != "case_finished":
        return False
    result = _case_result(event)
    status = _first(event, "status") or _first(result, "status")
    return status in ("failed", "observed_pass", "passed", "success", "succeeded")


def _capture_paths(*values) -> list[str]:
    paths = []

    def visit(value, key=""):
        if isinstance(value, dict):
            for child_key, child in value.items():
                if child_key in ("path", "capture_path", "image_path") and isinstance(child, str):
                    paths.append(child)
                elif child_key in ("captures", "capture_paths", "evidence_paths", "eyes"):
                    visit(child, child_key)
                elif child_key in ("left", "right"):
                    visit(child, child_key)
        elif isinstance(value, (list, tuple)):
            for child in value:
                visit(child, key)
        elif isinstance(value, str) and (key in ("captures", "capture_paths", "evidence_paths", "eyes")
                                           or value.lower().endswith((".png", ".jpg", ".jpeg"))):
            paths.append(value)

    for value in values:
        visit(value)
    return list(dict.fromkeys(paths))


def compact_notification(event: dict, *, active_cases=None, recent_captures=None) -> dict:
    """Normalize significant events without dropping their actionable context."""
    name = event.get("event", "unknown")
    result = _case_result(event)
    case_id = _first(event, "case_id", "id") or _first(result, "case_id", "id")
    metadata = (active_cases or {}).get(case_id, {}) if case_id is not None else {}
    status = _first(event, "status") or _first(result, "status")
    action = _first(event, "action", "semantic_action", "last_action")
    if action is None:
        action = _first(result, "action", "semantic_action", "last_action")
    if action is None:
        action = metadata.get("action")
    expectation = _first(event, "expectation", "expected", "predicate")
    if expectation is None:
        expectation = _first(result, "expectation", "expected", "predicate")
    if expectation is None:
        expectation = metadata.get("expectation")
    elapsed = _first(event, "elapsed_seconds", "elapsed", "monotonic_seconds")
    if elapsed is None:
        elapsed = _first(result, "elapsed_seconds", "elapsed")
    reason = _first(event, "reason", "error", "message")
    if reason is None:
        reason = _first(result, "reason", "error", "message")
    captures = _capture_paths(event, result)
    if not captures and case_id is not None:
        captures = list((recent_captures or {}).get(case_id, []))
    if not captures and case_id is None:
        captures = list((recent_captures or {}).get("_latest", []))

    notification = {
        "event": name,
        "run_id": event.get("run_id"),
        "sequence": _first(event, "sequence", "event_sequence", "seq"),
        "case_id": case_id,
        "action": action,
        "status": status,
        "reason": reason,
        "expectation": expectation,
        "elapsed_seconds": elapsed,
        "captures": captures,
    }
    # Keep event-specific transition data useful for root's decision logic.
    for key in ("milestone", "phase", "observed", "state", "previous_state", "state_delta", "blocking", "attention",
                "decision_id", "observation_id", "plan_id", "completed", "remaining", "results", "cases",
                "wait_seconds", "worker_wait_seconds", "rationale"):
        if key in event:
            notification[key] = event[key]
    for key in ("failure_phase", "entry_predicates_still_match", "last_state"):
        if key in result:
            notification[key] = result[key]
    return notification


class EventWatcher:
    """Incremental JSONL reader, safe for appenders that split a write mid-line."""

    def __init__(self, path, *, poll_seconds=.2, start_at_end=False):
        self.path = pathlib.Path(path)
        self.poll_seconds = poll_seconds
        try:
            self.offset = self.path.stat().st_size if start_at_end else 0
        except FileNotFoundError:
            self.offset = 0
        self.pending = b""
        self.line_number = 0
        self.seen = set()
        self.active_cases = {}
        self.recent_captures = {}

    def scan(self):
        """Return newly important normalized records from currently complete lines."""
        try:
            with self.path.open("rb") as stream:
                size = stream.seek(0, 2)
                if size < self.offset:
                    # Handle truncation/replacement without re-emitting identities.
                    self.offset = 0
                    self.pending = b""
                    self.line_number = 0
                stream.seek(self.offset)
                chunk = stream.read()
                self.offset = stream.tell()
        except FileNotFoundError:
            return []

        if not chunk:
            return []
        data = self.pending + chunk
        pieces = data.split(b"\n")
        self.pending = pieces.pop()
        output = []
        for raw in pieces:
            self.line_number += 1
            if not raw.strip():
                continue
            try:
                event = json.loads(raw.decode("utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError):
                # Treat malformed completed records as noise; the next line can still be read.
                continue
            if not isinstance(event, dict):
                continue
            name = event.get("event")
            payload = event.get("result") if isinstance(event.get("result"), dict) else {}
            case_id = event.get("case_id", event.get("id", payload.get("case_id", payload.get("id"))))
            if name == "case_started" and isinstance(case_id, str):
                case = event.get("case") if isinstance(event.get("case"), dict) else {}
                actions = case.get("steps", [])
                action = event.get("action") or (actions[0].get("name") if actions and isinstance(actions[0], dict) else None)
                self.active_cases[case_id] = {
                    "action": action,
                    "expectation": case.get("after") or event.get("expectation"),
                }
                self.recent_captures[case_id] = []
            elif name == "capture":
                path = event.get("path")
                if isinstance(path, str):
                    candidates = [case_id] if isinstance(case_id, str) else []
                    if not candidates:
                        label = event.get("label", "")
                        basename = event.get("path", "").replace("\\", "/").rsplit("/", 1)[-1]
                        candidates = [key for key in self.active_cases
                                      if (isinstance(label, str) and label.startswith(key))
                                      or basename.startswith(key)]
                    target = candidates[-1] if candidates else "_latest"
                    bucket = self.recent_captures.setdefault(target, [])
                    bucket.append(path)
                    del bucket[:-8]
            identity = event_identity(event, self.line_number)
            if identity in self.seen:
                continue
            self.seen.add(identity)
            if is_important(event):
                output.append(compact_notification(event, active_cases=self.active_cases,
                                                   recent_captures=self.recent_captures))
            if name == "case_finished" and isinstance(case_id, str):
                self.active_cases.pop(case_id, None)
        return output


def watch(path, *, seconds=60., poll_seconds=.2, once=False, start_at_end=False, output=None,
          clock=time.monotonic, sleep=time.sleep):
    watcher = EventWatcher(path, poll_seconds=poll_seconds, start_at_end=start_at_end)
    emit = output or (lambda record: print(json.dumps(record, separators=(",", ":"), ensure_ascii=False), flush=True))
    deadline = clock() + (0 if once else seconds)
    while True:
        for record in watcher.scan():
            emit(record)
        if once or clock() >= deadline:
            return
        sleep(min(poll_seconds, max(0., deadline - clock())))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", type=pathlib.Path, help="run directory or its events.jsonl file")
    parser.add_argument("--seconds", type=float, default=60., help="bounded watch duration (default: 60 seconds)")
    parser.add_argument("--poll", type=float, default=.2, help="poll interval in seconds (default: 0.2)")
    parser.add_argument("--once", action="store_true", help="process one current snapshot, then exit")
    parser.add_argument("--tail", action="store_true", help="begin at the current end of an existing event file")
    args = parser.parse_args(argv)
    if args.seconds < 0 or args.poll <= 0:
        parser.error("--seconds must be nonnegative and --poll must be positive")
    # A worker should attach before the runner creates its output directory.
    # Testing is_dir() alone mistakes that not-yet-created directory for a file.
    path = args.run if args.run.suffix.lower() == ".jsonl" else args.run / "events.jsonl"
    watch(path, seconds=args.seconds, poll_seconds=args.poll, once=args.once, start_at_end=args.tail)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
