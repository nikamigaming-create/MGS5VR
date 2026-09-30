"""Portable tests for the read-only gameplay event watcher."""
import importlib.util
import json
import pathlib
import tempfile
import unittest
from unittest import mock


ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("gameplay_watch", ROOT / "tools" / "gameplay-watch.py")
watch_module = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(watch_module)


class GameplayWatchTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = pathlib.Path(self.temp.name) / "events.jsonl"

    def append(self, value):
        with self.path.open("ab") as stream:
            stream.write((json.dumps(value) + "\n").encode("utf-8"))

    def test_cli_can_attach_before_the_run_directory_exists(self):
        future=pathlib.Path(self.temp.name)/'future-run'
        def start_watch(path, **kwargs):
            watcher=watch_module.EventWatcher(path)
            self.assertEqual(watcher.scan(),[])
            future.mkdir()
            (future/'events.jsonl').write_text(json.dumps({'event':'attention_required','reason':'ready'})+'\n')
            self.assertEqual(watcher.scan()[0]['reason'],'ready')
        with mock.patch.object(watch_module,'watch',side_effect=start_watch):
            self.assertEqual(watch_module.main([str(future),'--once']),0)

    def test_waits_for_complete_line_then_reads_remainder(self):
        record = {"run_id": "r1", "event": "attention_required", "reason": "prompt", "qpc_100ns": 9}
        encoded = json.dumps(record).encode("utf-8") + b"\n"
        self.path.write_bytes(encoded[:17])
        watcher = watch_module.EventWatcher(self.path)
        self.assertEqual(watcher.scan(), [])
        with self.path.open("ab") as stream:
            stream.write(encoded[17:])
        found = watcher.scan()
        self.assertEqual(len(found), 1)
        self.assertEqual(found[0]["event"], "attention_required")
        self.assertEqual(found[0]["reason"], "prompt")

    def test_duplicate_scans_emit_once(self):
        self.append({"run_id": "r1", "event": "milestone_reached", "sequence": 4,
                     "case_id": "escape", "qpc_100ns": 19})
        watcher = watch_module.EventWatcher(self.path)
        self.assertEqual(len(watcher.scan()), 1)
        self.assertEqual(watcher.scan(), [])

    def test_noise_is_suppressed_and_success_failure_and_run_are_reported(self):
        self.append({"run_id": "r1", "event": "rpc_completed", "qpc_100ns": 1})
        self.append({"run_id": "r1", "event": "observation", "qpc_100ns": 2})
        self.append({"run_id": "r1", "event": "case_finished", "qpc_100ns": 3,
                     "result": {"id": "good", "status": "observed_pass"}})
        self.append({"run_id": "r1", "event": "case_finished", "qpc_100ns": 4,
                     "result": {"id": "bad", "status": "failed", "error": "stuck"}})
        self.append({"run_id": "r1", "event": "run_finished", "qpc_100ns": 5,
                     "result": {"status": "failed"}})
        found = watch_module.EventWatcher(self.path).scan()
        self.assertEqual([item["case_id"] for item in found[:2]], ["good", "bad"])
        self.assertEqual([item["status"] for item in found], ["observed_pass", "failed", "failed"])
        self.assertEqual([item["event"] for item in found], ["case_finished", "case_finished", "run_finished"])

    def test_case_action_and_both_eye_paths_are_attached(self):
        self.append({"run_id": "r1", "event": "case_started", "qpc_100ns": 1,
                     "case_id": "idroid-exit", "case": {"steps": [{"op": "action", "name": "menus.back"}],
                                                            "after": {"scene": "gameplay"}}})
        self.append({"run_id": "r1", "event": "capture", "qpc_100ns": 2, "eye": "left",
                     "path": "C:\\run\\idroid-exit-failure-left.png"})
        self.append({"run_id": "r1", "event": "capture", "qpc_100ns": 3, "eye": "right",
                     "path": "C:\\run\\idroid-exit-failure-right.png"})
        self.append({"run_id": "r1", "event": "case_finished", "qpc_100ns": 4,
                     "result": {"id": "idroid-exit", "status": "failed", "error": "deadline"}})
        found = watch_module.EventWatcher(self.path).scan()
        self.assertEqual(len(found), 1)
        self.assertEqual(found[0]["action"], "menus.back")
        self.assertEqual(found[0]["expectation"], {"scene": "gameplay"})
        self.assertEqual(len(found[0]["captures"]), 2)

    def test_missing_file_is_empty_snapshot(self):
        watcher = watch_module.EventWatcher(pathlib.Path(self.temp.name) / "not-yet-created.jsonl")
        self.assertEqual(watcher.scan(), [])

    def test_supervisor_and_plan_events_are_important(self):
        events = [
            {"run_id": "r1", "event": "decision_rejected", "qpc_100ns": 10,
             "reason": "snapshot expired", "decision_id": "d1"},
            {"run_id": "r1", "event": "supervisor_waiting", "qpc_100ns": 11,
             "reason": "worker idle", "wait_seconds": 10.2},
            {"run_id": "r1", "event": "plan_interrupted", "qpc_100ns": 12,
             "reason": "visible prompt", "plan_id": "p1", "completed": ["pause-open"]},
            {"run_id": "r1", "event": "plan_finished", "qpc_100ns": 13,
             "plan_id": "p1", "results": [{"status": "observed_pass"}]},
            {"run_id": "r1", "event": "observation", "qpc_100ns": 14},
            {"run_id": "r1", "event": "rpc_completed", "qpc_100ns": 15},
        ]
        for event in events:
            self.append(event)
        found = watch_module.EventWatcher(self.path).scan()
        self.assertEqual([item["event"] for item in found], [
            "decision_rejected", "supervisor_waiting", "plan_interrupted", "plan_finished"
        ])
        self.assertEqual(found[0]["decision_id"], "d1")
        self.assertEqual(found[1]["wait_seconds"], 10.2)
        self.assertEqual(found[2]["completed"], ["pause-open"])
        self.assertEqual(found[3]["results"], [{"status": "observed_pass"}])

    def test_tail_mode_starts_after_existing_events(self):
        self.append({"run_id": "r1", "event": "milestone_reached", "qpc_100ns": 20})
        watcher = watch_module.EventWatcher(self.path, start_at_end=True)
        self.assertEqual(watcher.scan(), [])
        self.append({"run_id": "r1", "event": "plan_finished", "qpc_100ns": 21,
                     "plan_id": "p1"})
        found = watcher.scan()
        self.assertEqual(len(found), 1)
        self.assertEqual(found[0]["event"], "plan_finished")


if __name__ == "__main__":
    unittest.main()
