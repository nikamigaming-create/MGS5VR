"""Reject stale coverage accounting and known-broken release packaging offline."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import regression_catalog as catalog


class CatalogTests(unittest.TestCase):
    def setUp(self):
        self.data = json.loads((ROOT / catalog.CATALOG).read_text(encoding="utf-8-sig"))
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        paths = {p for f in self.data["features"] for field in catalog.REFERENCE_FIELDS for p in f[field]}
        for name in paths:
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text("fixture", encoding="utf-8")
        (self.root / "CMakeLists.txt").write_text((ROOT / "CMakeLists.txt").read_text(encoding="utf-8-sig"), encoding="utf-8")

    def reject(self, fragment):
        with self.assertRaisesRegex(ValueError, fragment):
            catalog.validate_catalog(self.root, self.data)

    def test_repository_inventory_and_computed_counts(self):
        value = catalog.validate_catalog(ROOT)
        self.assertEqual(value["summary"], catalog.computed_summary(value))

    def test_isolated_valid_public_inventory(self):
        self.assertEqual(catalog.validate_catalog(self.root, self.data), self.data)

    def test_duplicate_feature_id_is_rejected(self):
        self.data["features"][1]["id"] = self.data["features"][0]["id"]
        self.reject("Duplicate feature ID")

    def test_duplicate_reference_is_rejected(self):
        self.data["features"][0]["source_paths"] *= 2
        self.reject("duplicate reference")

    def test_private_absolute_and_traversal_paths_are_rejected(self):
        for name in ("private/evidence.json", "docs/private/evidence.json", "D:/retail/mgsvtpp.exe", "src/../docs/CURRENT.md", "src\\core.cpp"):
            with self.subTest(name=name):
                with self.assertRaises(ValueError):
                    catalog.public_file(self.root, name)

    def test_deleted_source_reference_is_rejected(self):
        (self.root / self.data["features"][0]["source_paths"][0]).unlink()
        self.reject("Stale or missing public path")

    def test_new_source_requires_feature_mapping(self):
        (self.root / "src/unaccounted.cpp").write_text("// new behavior", encoding="utf-8")
        self.reject("Unmapped source files")

    def test_new_ctest_requires_inventory_and_feature_mapping(self):
        with (self.root / "CMakeLists.txt").open("a", encoding="utf-8") as stream:
            stream.write("\nadd_test(NAME new_behavior COMMAND test)\n")
        self.reject("CTest inventory mismatch")
        self.data["current_ctest_groups"].append("new_behavior")
        self.reject("Unmapped CTest groups")

    def test_deleted_ctest_is_stale(self):
        text = (self.root / "CMakeLists.txt").read_text(encoding="utf-8")
        text = text.replace("add_test(NAME contracts COMMAND mgs5vr_tests)", "")
        (self.root / "CMakeLists.txt").write_text(text, encoding="utf-8")
        self.reject("CTest inventory mismatch")

    def test_unknown_ctest_reference_is_rejected(self):
        self.data["features"][0]["automated_tests"].append("imaginary_pass")
        self.reject("unknown CTest")

    def test_new_native_suite_requires_inventory_and_feature_mapping(self):
        path = "tools/gameplay_bot/suites/unreviewed.json"
        (self.root / path).write_text("{}", encoding="utf-8")
        self.reject("Native fixture inventory mismatch")
        self.data["public_native_fixture_files"].append(path)
        self.reject("Unmapped native fixtures")

    def test_deleted_native_fixture_is_rejected(self):
        (self.root / self.data["public_native_fixture_files"][0]).unlink()
        self.reject("Native fixture inventory mismatch")

    def test_fabricated_coverage_status_is_rejected(self):
        self.data["features"][0]["coverage"]["headset"]["status"] = "fully_verified"
        self.reject("invalid headset coverage status")

    def test_pending_gate_cannot_hide_a_known_failure(self):
        self.data["features"][0]["coverage"]["native"]["status"] = "blocked"
        self.data["features"][0]["release_gate"]["status"] = "pending"
        self.reject("blocked coverage/release gate disagreement")

    def test_missing_tests_cannot_be_counted_as_automated(self):
        feature = self.data["features"][0]
        feature["automated_tests"] = []
        feature["auxiliary_tests"] = []
        self.reject("automated coverage has no matching tests")

    def test_stale_summary_is_rejected(self):
        self.data["summary"]["feature_families"] += 1
        self.reject("summary does not match")

    def test_shortened_or_fabricated_sustained_acceptance_is_rejected(self):
        sustained = self.data["features"][0]["sustained_regression"]
        sustained["minimum_duration_seconds"] = 1
        self.reject("invalid minimum_duration_seconds")
        sustained["minimum_duration_seconds"] = 180
        sustained["accepted_current_repetitions"] = 5
        self.reject("unsupported current acceptance metrics")

    def promote_fixture(self):
        feature = self.data["features"][0]
        candidate = "isolated-reviewed-candidate"
        self.data["baseline"]["current_candidate_id"] = candidate
        record = {"candidate_id": candidate, "local_acceptance_id": "isolated-native-review",
                  "reviewed_on": "2026-10-02", "reviewed_by": "isolated test reviewer",
                  "scope": "One bounded native fixture, not full-game acceptance",
                  "limitations": ["Physical headset remains unverified"],
                  "native_observed": True, "visual_reviewed": True,
                  "duration_seconds": 180, "repetitions": 5,
                  "transitions_reviewed": True, "both_eyes_reviewed": True,
                  "public_report": feature["evidence_sources"][0]}
        feature["acceptance_records"] = {"native": dict(record), "continuous": dict(record)}
        feature["coverage"]["native"]["status"] = "accepted_scoped"
        feature["coverage"]["continuous"]["status"] = "accepted"
        feature["sustained_regression"].update(status="accepted", accepted_current_duration_seconds=180,
                                               accepted_current_repetitions=5)
        (self.root / record["public_report"]).write_text(
            "acceptance-id: isolated-native-review\ncandidate-id: " + candidate, encoding="utf-8")
        self.data["summary"] = catalog.computed_summary(self.data)
        self.data["sustained_acceptance_policy"]["accepted_current_feature_windows"] = 1
        return feature

    def test_reviewed_current_sustained_scope_can_be_promoted_without_headset_claim(self):
        self.promote_fixture()
        result = catalog.validate_catalog(self.root, self.data)
        self.assertEqual(result["summary"]["current_candidate_continuous_passes"], 1)
        self.assertEqual(result["summary"]["current_candidate_headset_passes"], 0)

    def test_accepted_scope_needs_matching_public_report_and_candidate(self):
        feature = self.promote_fixture()
        feature["acceptance_records"]["continuous"]["candidate_id"] = "old-candidate"
        self.reject("different candidate")
        feature["acceptance_records"]["continuous"]["candidate_id"] = self.data["baseline"]["current_candidate_id"]
        (self.root / feature["evidence_sources"][0]).write_text("A claimed pass without its record", encoding="utf-8")
        self.reject("missing the acceptance/candidate marker")

    def test_still_only_or_short_capture_cannot_promote_continuous_acceptance(self):
        feature = self.promote_fixture()
        record = feature["acceptance_records"]["continuous"]
        record["transitions_reviewed"] = False
        self.reject("transition and both-eye review")
        record["transitions_reviewed"] = True
        record["duration_seconds"] = 15
        self.reject("below the required native window")

    def test_sustained_metrics_must_match_reviewed_evidence(self):
        feature = self.promote_fixture()
        feature["sustained_regression"]["accepted_current_repetitions"] = 100
        self.reject("metrics disagree")

    def test_simulator_cannot_be_promoted_as_physical_headset(self):
        feature = self.promote_fixture()
        feature["coverage"]["headset"]["status"] = "accepted"
        feature["acceptance_records"]["headset"] = dict(feature["acceptance_records"]["continuous"])
        self.reject("simulator is not a headset session")

    def test_soak_counts_only_complete_reviewed_ten_minute_record(self):
        feature = self.promote_fixture()
        feature["sustained_regression"]["soak"]["status"] = "accepted"
        record = dict(feature["acceptance_records"]["continuous"])
        feature["acceptance_records"]["soak"] = record
        self.reject("below the required native window")
        record["duration_seconds"] = 600
        self.data["summary"] = catalog.computed_summary(self.data)
        self.data["sustained_acceptance_policy"]["accepted_current_integrated_soaks"] = 1
        self.assertEqual(catalog.validate_catalog(self.root, self.data)["summary"]["current_candidate_integrated_soak_passes"], 1)

    def test_release_refuses_known_blocker_even_with_valid_counts(self):
        feature = self.data["features"][0]
        feature["coverage"]["native"]["status"] = "blocked"
        feature["release_gate"] = {"status": "blocked", "reason": "Reproduced native transition failure"}
        self.data["summary"] = catalog.computed_summary(self.data)
        with self.assertRaisesRegex(ValueError, "Release blocked by current native regressions"):
            catalog.require_release_ready(self.root, self.data)

    def test_pending_scope_is_reported_without_full_game_or_headset_claim(self):
        for feature in self.data["features"]:
            if feature["release_gate"]["status"] == "blocked":
                feature["release_gate"] = {"status": "pending", "reason": "Repair unaccepted; no current failure asserted in this isolated fixture"}
                for axis in ("native", "continuous"):
                    feature["coverage"][axis]["status"] = "unverified"
        self.data["summary"] = catalog.computed_summary(self.data)
        result = catalog.require_release_ready(self.root, self.data)
        self.assertTrue(result["pending_features"])
        self.assertTrue(result["experimental_features"])
        self.assertFalse(result["full_game_accepted"])
        self.assertFalse(result["physical_headset_accepted"])

    def test_packager_checks_gate_before_reading_build_or_writing_output(self):
        spec = importlib.util.spec_from_file_location("release_package_test", ROOT / "tools/package-release.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        output = self.root / "output"
        with patch.object(module, "require_release_ready", side_effect=ValueError("known blocker")) as gate, \
                patch.object(module, "read_json") as read, patch.object(module, "source_identity") as source:
            with self.assertRaisesRegex(ValueError, "known blocker"):
                module.package("experimental-2026-10-02", output)
        gate.assert_called_once_with(module.ROOT)
        read.assert_not_called()
        source.assert_not_called()
        self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
