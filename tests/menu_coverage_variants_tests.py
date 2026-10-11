import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from gameplay_bot.campaign import summarize_runs
from gameplay_bot.core import BotFault
from gameplay_bot.menu_coverage import menu_inventory

IDENTITY = {key: value * 64 for key, value in zip(
    ("exe_sha256", "dll_sha256", "controls_sha256", "config_sha256"), "abcd")}


class MenuVariantTests(unittest.TestCase):
    def setUp(self):
        self.rows = menu_inventory(ROOT)
        self.index = {row["id"]: row for row in self.rows}
        self.development = self.index["MENU.TPP.DEVELOPMENT.acc.handheld"]

    def test_existing_menu_ids_and_unknown_statuses_are_preserved(self):
        ids = "\n".join(row["id"] for row in self.rows)
        self.assertEqual(len(self.rows), 291)
        self.assertEqual(hashlib.sha256(ids.encode()).hexdigest(),
                         "1c81b0e15189c6f332c63c83bb7ae5f6686a6b7339dd9cc90afdbbf1db56e457")
        for row in self.rows:
            for obligation in [row, *row["variant_obligations"]]:
                self.assertEqual(obligation["status"], "unproven")
                self.assertEqual(obligation["applicability"], "not_established")
                self.assertEqual(obligation["native_page"], "not_discovered")
                self.assertFalse(obligation["discovery_complete"])
        self.assertIn("LEFT-hand", self.development["title"])

    def test_dimensions_are_separate_not_cartesian_native_pages(self):
        dimensions = {}
        for variant in self.development["variant_obligations"]:
            dimensions.setdefault(variant["dimension"], set()).add(variant["key"])
        self.assertEqual(dimensions, {
            "progression_guidance": {"first_guidance", "unfinished_guidance", "completed_guidance"},
            "menu_owner": {"normal", "confirmation", "help"},
            "development_state": {"locked", "insufficient_resources", "eligible_undeveloped",
                                  "in_progress", "newly_completed", "already_developed"}})
        self.assertEqual(len(self.development["variant_obligations"]), 12)
        self.assertEqual(len(self.index["MENU.TPP.MAP.acc.handheld"]["variant_obligations"]), 6)
        self.assertEqual(len(self.index["MENU.TPP.HELICOPTER_DEVELOPMENT.acc.handheld"]["variant_obligations"]), 12)

    def test_latest_report_covers_development_acc_and_on_foot_only(self):
        for mode in ("acc", "mother_base", "free_roam_afghanistan", "free_roam_africa", "mission", "side_op"):
            for presentation in ("handheld", "spatial"):
                row = self.index[f"MENU.TPP.DEVELOPMENT.{mode}.{presentation}"]
                report = row["reports"][-1]
                self.assertEqual(report["id"], "development_upgrade_lock")
                self.assertEqual(report["exact_step"], "unknown")
                self.assertEqual(report["progression_state"], "not_established")
        self.assertNotIn("latest_report", self.index["MENU.TPP.DEVELOPMENT.vehicle_driver.handheld"])
        self.assertNotIn("latest_report", self.index["MENU.TPP.HELICOPTER_DEVELOPMENT.mission.handheld"])
        self.assertEqual(self.index["MENU.TPP.ACC_CUSTOMIZATION.acc.handheld"]["reports"][0]["id"],
                         "acc_upgrade_lock")

    def test_native_family_pass_does_not_accept_undiscovered_variants(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            run = root / "run"
            run.mkdir()
            (run / "identity.json").write_text(json.dumps(IDENTITY))
            (run / "result.json").write_text(json.dumps({"status": "observed_pass", "suites": [
                {"id": "ordinary-back", "status": "observed_pass",
                 "coverage_refs": [self.development["id"]]}]}))
            report = summarize_runs(root, self.rows, [run], IDENTITY)
        selected = next(row for row in report["rows"] if row["id"] == self.development["id"])
        self.assertEqual(selected["status"], "partial_native_evidence")
        self.assertTrue(all(variant["status"] == "unproven" and not variant["evidence"]
                            for variant in selected["variant_obligations"]))
        self.assertFalse(report["release_ready"])

    def test_variants_and_report_data_do_not_alias_other_rows(self):
        first = self.development["variant_obligations"][0]
        first["acceptance"].append("only this row")
        first["evidence"].append("only this row")
        self.development["reports"][0]["exact_step"] = "changed locally"
        other = self.index["MENU.TPP.DEVELOPMENT.acc.spatial"]
        self.assertNotIn("only this row", other["variant_obligations"][0]["acceptance"])
        self.assertEqual(other["variant_obligations"][0]["evidence"], [])
        self.assertEqual(other["reports"][0]["exact_step"], "unknown")

    def test_unknown_dimension_and_duplicate_variant_fail_instead_of_omitting_coverage(self):
        catalog = json.loads((ROOT / "tools/gameplay_bot/catalogs/vr-menu-paths.json").read_text())
        for mutation, message in (
            (lambda value: value["default_variant_dimensions"].append("missing"), "Unknown or empty"),
            (lambda value: value["variant_dimensions"]["menu_owner"]["values"].append(
                copy.deepcopy(value["variant_dimensions"]["menu_owner"]["values"][0])), "duplicate menu variant")):
            with self.subTest(message=message), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                path = root / "tools/gameplay_bot/catalogs/vr-menu-paths.json"
                path.parent.mkdir(parents=True)
                changed = copy.deepcopy(catalog)
                mutation(changed)
                path.write_text(json.dumps(changed))
                with self.assertRaisesRegex(BotFault, message):
                    menu_inventory(root)

    def test_html_renders_grouped_missing_variants_and_escapes_labels(self):
        spec = importlib.util.spec_from_file_location("coverage_report", ROOT / "tools/gameplay-coverage.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        row = copy.deepcopy(self.development)
        row["evidence"] = []
        row["variant_obligations"][0]["title"] = "<script>untrusted</script>"
        report = {"rows": [row], "menus_only": True, "target_identity": IDENTITY,
                  "generated_utc": "2026-10-10T00:00:00Z"}
        output = module.render_html(report)
        self.assertIn("Variant coverage", output)
        self.assertIn("Missing checks", output)
        self.assertIn("dimension_title", output)
        self.assertIn("not_discovered", output)
        self.assertNotIn("<script>untrusted</script>", output)
        self.assertIn("escape(v.title)", output)


if __name__ == "__main__":
    unittest.main()
