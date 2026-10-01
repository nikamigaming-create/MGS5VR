import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from gameplay_bot.campaign import fingerprint, run_campaign, summarize_runs, validate_campaign, unstarted_campaign
from gameplay_bot.core import BotFault
from gameplay_bot.menu_coverage import menu_inventory

IDENTITY = {name: letter * 64 for name, letter in zip(
    ("exe_sha256", "dll_sha256", "controls_sha256", "config_sha256"), "abcd")}
SUITE = {"cases": [{"id": "open", "before": {"scene": "gameplay"},
                    "steps": [{"op": "action", "name": "open"}], "after": {"scene": "menu"}}]}


class Adapter:
    bindings = {"settings": []}
    def __init__(self, state):
        self.state = state
        self.releases = 0
    def release(self):
        self.releases += 1
    def observe(self, native=False):
        return self.state


class Behavior:
    def __init__(self, status="observed_pass", state=None, failure=None):
        self.adapter = Adapter(state or {"scene": "gameplay"})
        self.status, self.failure, self.calls = status, failure, []
    def case(self, case):
        self.calls.append(case)
        row = {"id": case["id"], "status": self.status}
        if self.failure:
            row.update(self.failure)
        return row


class CampaignTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        (self.root / "suite.json").write_text(json.dumps(SUITE))
        self.campaign = {"suites": [
            {"id": "first", "suite": "suite.json", "coverage_refs": ["R01"]},
            {"id": "missing", "coverage_refs": ["R02"], "blocked_reason": "Needs a mission fixture"},
            {"id": "second", "suite": "suite.json", "coverage_refs": ["R03"]}]}
    def tearDown(self):
        self.directory.cleanup()
    def run_it(self, behavior=None, **kwargs):
        return run_campaign(behavior or Behavior(), self.campaign, self.root, lambda _: None, IDENTITY, **kwargs)
    def test_complete_native_suites_leave_missing_coverage_and_visual_gates_open(self):
        result = self.run_it()
        self.assertEqual(result["native_suites_observed"], 2)
        self.assertEqual(result["blocked_suites"], 1)
        self.assertFalse(result["release_ready"])
        self.assertEqual(result["status"], "incomplete")
    def test_cases_are_namespaced_for_unambiguous_capture_and_event_joins(self):
        behavior = Behavior();self.run_it(behavior)
        self.assertEqual([case["id"] for case in behavior.calls], ["first__open", "second__open"])
    def test_wrong_scene_dispatches_nothing_and_retains_blocker(self):
        behavior = Behavior(state={"scene": "cinematic"})
        result = self.run_it(behavior)
        self.assertFalse(behavior.calls)
        self.assertEqual(result["suites"][0]["status"], "blocked")
    def test_ambiguous_dispatch_stops_inputs_but_preserves_remaining_rows(self):
        behavior = Behavior("failed", failure={"failure_phase": "dispatch"})
        result = self.run_it(behavior)
        self.assertEqual(len(behavior.calls), 1)
        self.assertEqual(result["suites"][2]["status"], "not_run")
        self.assertEqual(result["suites"][1]["status"], "blocked")
    def test_resume_rejects_a_changed_dll(self):
        result = self.run_it();result["identity"]["dll_sha256"] = "f" * 64
        with self.assertRaisesRegex(BotFault, "different DLL"):
            self.run_it(resume=result)
    def test_resume_skips_only_same_definition(self):
        first = self.run_it();behavior = Behavior()
        result = self.run_it(behavior, resume=first)
        self.assertFalse(behavior.calls)
        self.assertEqual(result["suites"][0]["status"], "previously_observed")
        changed = copy.deepcopy(SUITE);changed["cases"][0]["steps"][0]["name"] = "changed"
        (self.root / "suite.json").write_text(json.dumps(changed))
        behavior = Behavior();self.run_it(behavior, resume=first)
        self.assertEqual(len(behavior.calls), 2)
    def test_invalid_campaign_fails_before_arrival_or_input(self):
        self.campaign["suites"][1].pop("blocked_reason")
        arrived = []
        with self.assertRaises(BotFault):
            self.run_it(enter_game=lambda: arrived.append(True))
        self.assertFalse(arrived)
    def test_paths_cannot_escape_root(self):
        self.campaign["suites"][0]["suite"] = "../suite.json"
        with self.assertRaisesRegex(BotFault, "inside the workspace"):
            validate_campaign(self.campaign, self.root)
    def test_a_native_pass_does_not_close_an_issue(self):
        run = self.root / "run";run.mkdir()
        (run / "identity.json").write_text(json.dumps(IDENTITY))
        (run / "result.json").write_text(json.dumps(self.run_it()))
        rows = [{"id": f"R0{number}", "status": "unproven"} for number in range(1, 4)]
        report = summarize_runs(self.root, rows, [run], IDENTITY)
        self.assertEqual(report["rows"][0]["status"], "partial_native_evidence")
        self.assertFalse(report["release_ready"])
        newer = {**IDENTITY, "dll_sha256": "f" * 64}
        report = summarize_runs(self.root, rows, [run], newer)
        self.assertEqual(report["rows"][0]["status"], "unproven")
        self.assertFalse(report["rows"][0]["evidence"][0]["current"])
    def test_missing_identity_is_not_accepted(self):
        with self.assertRaises(BotFault):
            fingerprint({"dll_sha256": "c" * 64})

    def test_startup_failure_preserves_the_whole_denominator(self):
        result = unstarted_campaign(self.campaign, self.root, IDENTITY, "OpenXR unavailable")
        self.assertEqual([r["status"] for r in result["suites"]], ["not_run", "blocked", "not_run"])
        self.assertEqual(len(result["suites"]), 3)
        self.assertFalse(result["release_ready"])

    def test_standalone_case_links_require_exact_result_and_case_identity(self):
        from gameplay_bot.campaign import file_hash
        run=self.root/'standalone';run.mkdir()
        (run/'identity.json').write_text(json.dumps(IDENTITY))
        result=run/'result.json'
        result.write_text(json.dumps({'status':'observed_pass','cases':[{'id':'palm','status':'observed_pass'}]}))
        links={'result_sha256':file_hash(result),'cases':[{'case_index':0,'case_id':'palm','coverage_refs':['R01']}]}
        (run/'coverage-links.json').write_text(json.dumps(links))
        report=summarize_runs(self.root,[{'id':'R01','status':'unproven'}],[run],IDENTITY)
        self.assertEqual(report['rows'][0]['status'],'partial_native_evidence')
        self.assertFalse(report['release_ready'])
        result.write_text(json.dumps({'status':'failed','cases':[{'id':'palm','status':'failed'}]}))
        with self.assertRaisesRegex(BotFault,'different result'):
            summarize_runs(self.root,[{'id':'R01','status':'unproven'}],[run],IDENTITY)

    def test_menu_evidence_does_not_certify_other_modes_or_presentations(self):
        rows = menu_inventory(Path(__file__).resolve().parents[1])
        selected = "MENU.TPP.HELICOPTER_DEVELOPMENT.mission.handheld"
        run = self.root / "menu-run"; run.mkdir()
        (run / "identity.json").write_text(json.dumps(IDENTITY))
        (run / "result.json").write_text(json.dumps({"status": "observed_pass", "suites": [
            {"id": "list-back", "status": "observed_pass", "coverage_refs": [selected]}]}))
        report = summarize_runs(self.root, rows, [run], IDENTITY)
        index = {row["id"]: row for row in report["rows"]}
        self.assertEqual(index[selected]["status"], "partial_native_evidence")
        self.assertEqual(index["MENU.TPP.HELICOPTER_DEVELOPMENT.mission.spatial"]["status"], "unproven")
        self.assertEqual(index["MENU.TPP.HELICOPTER_DEVELOPMENT.acc.handheld"]["status"], "unproven")
        self.assertFalse(index[selected]["discovery_complete"])
        self.assertEqual(index[selected]["applicability"], "not_established")
        self.assertFalse(report["release_ready"])

    def test_planned_menu_catalog_keeps_undiscovered_states_and_native_roles(self):
        rows = menu_inventory(Path(__file__).resolve().parents[1])
        modes = {row["mode"] for row in rows}
        self.assertTrue({"title", "acc", "mother_base", "mission", "side_op", "tutorial",
                         "cinematic", "loading", "results", "death", "emplacement",
                         "vehicle_driver", "vehicle_passenger", "helicopter_passenger"} <= modes)
        self.assertEqual(len({row["id"] for row in rows}), len(rows))
        self.assertTrue(all(row["status"] == "unproven" and row["native_page"] == "not_discovered" for row in rows))


if __name__ == "__main__":
    unittest.main()
