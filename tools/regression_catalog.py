"""Validate public VR regression accounting and block known-broken releases."""
from __future__ import annotations

import argparse
from collections import Counter
import json
import math
from pathlib import Path, PurePosixPath
import re


ROOT = Path(__file__).resolve().parents[1]
CATALOG = "tests/vr_regression_catalog.json"
STATUSES = {
    "automated": {"partial", "none"},
    "native": {"accepted_scoped", "scoped_observation", "retained_scoped", "unverified", "blocked", "not_applicable"},
    "continuous": {"accepted", "historical_partial", "unverified", "blocked", "not_applicable"},
    "headset": {"accepted", "historical_feedback", "unverified", "not_applicable"},
}
GATES = {"blocked", "pending", "experimental", "tooling_only"}
REFERENCE_FIELDS = ("source_paths", "test_sources", "auxiliary_tests", "native_fixtures", "evidence_sources")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def unique_strings(values, label, *, nonempty=False):
    require(isinstance(values, list), f"{label}: expected a list")
    require(all(isinstance(v, str) and v.strip() == v and v for v in values), f"{label}: invalid string")
    require(len(values) == len(set(values)), f"{label}: duplicate reference")
    require(not nonempty or bool(values), f"{label}: must not be empty")
    return set(values)


def public_file(root, name):
    require(isinstance(name, str) and name, "Invalid public path")
    p = PurePosixPath(name)
    require("\\" not in name and ":" not in name and not p.is_absolute()
            and p.as_posix() == name and all(x not in {".", ".."} for x in p.parts),
            f"Unsafe public path: {name}")
    require(p.parts[0] in {"src", "include", "tests", "tools", "docs", "config", "profiles"},
            f"Non-public path: {name}")
    forbidden = {"private", "artifacts", "captures", "play", "build", "dist", ".git", ".deps", ".env"}
    require(not any(x.lower() in forbidden for x in p.parts), f"Private path: {name}")
    require(p.suffix.lower() not in {".exe", ".dll", ".sav", ".fpk", ".fpkd", ".fmdl", ".ftex", ".dds", ".dat", ".dmp", ".pdb"},
            f"Retail or binary path: {name}")
    root = Path(root).resolve()
    target = root.joinpath(*p.parts)
    require(target.resolve().is_relative_to(root), f"Path escapes repository: {name}")
    require(target.is_file(), f"Stale or missing public path: {name}")
    return target


def current_ctests(root):
    text = (Path(root) / "CMakeLists.txt").read_text(encoding="utf-8-sig")
    # CMake uses explicit add_test(NAME ...) declarations; comments are not tests.
    text = re.sub(r"(?m)#.*$", "", text)
    names = re.findall(r"add_test\s*\(\s*NAME\s+([^\s)]+)", text)
    require(len(names) == len(set(names)), "Duplicate CMake test name")
    return set(names)


def source_files(root):
    return {p.relative_to(root).as_posix() for folder in ("src", "include")
            for p in (root / folder).rglob("*") if p.is_file()}


def native_suites(root):
    return {p.relative_to(root).as_posix() for p in (root / "tools/gameplay_bot/suites").rglob("*.json") if p.is_file()}


def accepted_record(root, catalog, feature, axis, *, minimum_seconds=1, minimum_repetitions=1):
    """Public accounting references a reviewed local record; CI cannot inspect private pixels."""
    ident = feature["id"]
    record = feature.get("acceptance_records", {}).get(axis)
    require(isinstance(record, dict), f"{ident}: {axis} acceptance needs a reviewed record")
    token = record.get("local_acceptance_id")
    require(isinstance(token, str) and re.fullmatch(r"[a-z0-9][a-z0-9-]{2,95}", token),
            f"{ident}: acceptance record must use an opaque local ID, not a private path")
    candidate = catalog.get("baseline", {}).get("current_candidate_id")
    require(isinstance(candidate, str) and candidate and record.get("candidate_id") == candidate,
            f"{ident}: acceptance belongs to a different candidate")
    require(isinstance(record.get("reviewed_on"), str) and re.fullmatch(r"\d{4}-\d{2}-\d{2}", record["reviewed_on"]),
            f"{ident}: acceptance requires a review date")
    for field in ("scope", "reviewed_by"):
        require(isinstance(record.get(field), str) and record[field].strip(), f"{ident}: acceptance missing {field}")
    unique_strings(record.get("limitations"), ident + ".acceptance limitations", nonempty=True)
    require(record.get("native_observed") is True and record.get("visual_reviewed") is True,
            f"{ident}: predicates alone cannot accept native visuals")
    seconds, repetitions = record.get("duration_seconds"), record.get("repetitions")
    require(type(seconds) in (int, float) and math.isfinite(seconds) and seconds >= minimum_seconds,
            f"{ident}: accepted duration is below the required native window")
    require(type(repetitions) is int and repetitions >= minimum_repetitions,
            f"{ident}: accepted repetition count is below the requirement")
    report_name = record.get("public_report")
    require(report_name in feature["evidence_sources"] and str(report_name).startswith("docs/"),
            f"{ident}: acceptance must reference its public evidence report")
    report = public_file(root, report_name).read_text(encoding="utf-8-sig")
    require(f"acceptance-id: {token}" in report and f"candidate-id: {candidate}" in report,
            f"{ident}: public report is missing the acceptance/candidate marker")
    if axis in {"continuous", "soak", "headset"}:
        require(record.get("transitions_reviewed") is True and record.get("both_eyes_reviewed") is True,
                f"{ident}: acceptance requires transition and both-eye review")
    if axis == "headset":
        require(record.get("physical_headset_session") is True, f"{ident}: simulator is not a headset session")
        require(all(isinstance(record.get(k), str) and record[k].strip() for k in ("headset_model", "runtime")),
                f"{ident}: physical headset acceptance needs hardware/runtime identity")
    return record


def computed_summary(catalog):
    features = catalog["features"]
    summary = {
        "feature_families": len(features),
        **{kind: sum(f["kind"] == kind for f in features) for kind in ("gameplay", "runtime", "tooling")},
        "baseline_ctest_groups": len(catalog["baseline_ctest_groups"]),
        "current_ctest_groups": len(catalog["current_ctest_groups"]),
        "public_native_fixture_files": len(catalog["public_native_fixture_files"]),
        "features_with_registered_automated_tests": sum(bool(f["automated_tests"]) for f in features),
        "features_with_auxiliary_tests_only": sum(not f["automated_tests"] and bool(f["auxiliary_tests"]) for f in features),
        "features_without_automated_tests": sum(f["coverage"]["automated"]["status"] == "none" for f in features),
        "features_with_public_native_fixtures": sum(bool(f["native_fixtures"]) for f in features),
        "current_candidate_continuous_passes": sum(f["coverage"]["continuous"]["status"] == "accepted" for f in features),
        "current_candidate_headset_passes": sum(f["coverage"]["headset"]["status"] == "accepted" for f in features),
    }
    for axis in STATUSES:
        summary[axis] = dict(Counter(f["coverage"][axis]["status"] for f in features))
    summary["release_gate"] = dict(Counter(f["release_gate"]["status"] for f in features))
    summary["features_missing_sustained_native_regression"] = sum(f["sustained_regression"]["status"] == "missing" for f in features)
    summary["current_candidate_integrated_soak_passes"] = len({f["acceptance_records"]["soak"]["local_acceptance_id"]
        for f in features if f["sustained_regression"]["soak"]["status"] == "accepted"})
    return summary


def validate_catalog(root=ROOT, catalog=None):
    root = Path(root).resolve()
    if catalog is None:
        catalog = json.loads((root / CATALOG).read_text(encoding="utf-8-sig"))
    require(isinstance(catalog, dict) and catalog.get("schema_version") == 1, "Unsupported regression catalog schema")
    features = catalog.get("features")
    require(isinstance(features, list) and features, "Missing feature inventory")
    baseline = unique_strings(catalog.get("baseline_ctest_groups"), "baseline_ctest_groups", nonempty=True)
    declared = unique_strings(catalog.get("current_ctest_groups"), "current_ctest_groups", nonempty=True)
    require(baseline <= declared, "Historical CTest baseline is missing from current inventory")
    actual = current_ctests(root)
    require(declared == actual, f"CTest inventory mismatch: unmapped={sorted(actual-declared)}, stale={sorted(declared-actual)}")
    fixtures = unique_strings(catalog.get("public_native_fixture_files"), "public_native_fixture_files")
    actual_fixtures = native_suites(root)
    require(fixtures == actual_fixtures, f"Native fixture inventory mismatch: unmapped={sorted(actual_fixtures-fixtures)}, stale={sorted(fixtures-actual_fixtures)}")
    ids, mapped_tests, mapped_sources, mapped_fixtures = set(), set(), set(), set()
    for feature in features:
        require(isinstance(feature, dict), "Feature must be an object")
        ident = feature.get("id")
        require(isinstance(ident, str) and re.fullmatch(r"[a-z][a-z0-9]*(?:-[a-z0-9]+)*", ident), f"Invalid feature ID: {ident}")
        require(ident not in ids, f"Duplicate feature ID: {ident}")
        ids.add(ident)
        require(feature.get("kind") in {"gameplay", "runtime", "tooling"}, f"{ident}: invalid kind")
        require(isinstance(feature.get("title"), str) and feature["title"].strip(), f"{ident}: missing title")
        tests = unique_strings(feature.get("automated_tests"), ident + ".automated_tests")
        require(tests <= actual, f"{ident}: unknown CTest references {sorted(tests-actual)}")
        mapped_tests |= tests
        for field in REFERENCE_FIELDS:
            values = unique_strings(feature.get(field), ident + "." + field, nonempty=field in {"source_paths", "evidence_sources"})
            for value in values:
                public_file(root, value)
            if field == "source_paths":
                mapped_sources |= values
            elif field == "native_fixtures":
                require(values <= fixtures, f"{ident}: native fixture is not a public suite")
                mapped_fixtures |= values
        require(set(feature["auxiliary_tests"]) <= set(feature["test_sources"]), f"{ident}: auxiliary test missing from test_sources")
        require(not tests or feature["test_sources"], f"{ident}: CTest mapping needs test sources")
        coverage = feature.get("coverage", {})
        require(set(coverage) == set(STATUSES), f"{ident}: missing coverage axes")
        for axis, allowed in STATUSES.items():
            row = coverage[axis]
            require(isinstance(row, dict) and row.get("status") in allowed, f"{ident}: invalid {axis} coverage status")
            require(isinstance(row.get("scope"), str) and row["scope"].strip(), f"{ident}: missing {axis} scope")
            require((row["status"] == "not_applicable") == (feature["kind"] == "tooling") if axis != "automated" else True,
                    f"{ident}: invalid not_applicable coverage")
        require((coverage["automated"]["status"] == "partial") == bool(tests or feature["auxiliary_tests"]), f"{ident}: automated coverage has no matching tests")
        gate = feature.get("release_gate", {})
        require(gate.get("status") in GATES and isinstance(gate.get("reason"), str) and gate["reason"].strip(), f"{ident}: invalid release gate")
        blocked = any(row["status"] == "blocked" for row in coverage.values())
        require(blocked == (gate["status"] == "blocked"), f"{ident}: blocked coverage/release gate disagreement")
        require((gate["status"] == "tooling_only") == (feature["kind"] == "tooling"), f"{ident}: tooling gate mismatch")
        for axis in ("native", "continuous", "headset"):
            if coverage[axis]["status"] in {"accepted", "accepted_scoped"}:
                accepted_record(root, catalog, feature, axis,
                    minimum_seconds=1 if axis == "native" else 180,
                    minimum_repetitions=1 if axis == "native" else 5)
                require(not blocked, f"{ident}: current failure prevents accepted coverage")
        unique_strings(feature.get("missing_checks"), ident + ".missing_checks", nonempty=True)
        reports = unique_strings(feature.get("community_reports"), ident + ".community_reports")
        require(all(re.fullmatch(r"R(?:0[1-9]|[1-4][0-9]|5[0-5])", r) for r in reports), f"{ident}: unknown report ID")
        sustained = feature.get("sustained_regression", {})
        tooling = feature["kind"] == "tooling"
        require(sustained.get("status") in ({"not_applicable"} if tooling else {"missing", "accepted"}), f"{ident}: invalid sustained status")
        for key, expected in (("minimum_duration_seconds", 0 if tooling else 180), ("minimum_repetitions", 0 if tooling else 5)):
            require(type(sustained.get(key)) is int and sustained[key] == expected, f"{ident}: invalid {key}")
        if sustained["status"] == "accepted":
            require(coverage["native"]["status"] == "accepted_scoped" and coverage["continuous"]["status"] == "accepted",
                    f"{ident}: sustained acceptance needs accepted native and continuous coverage")
            record = accepted_record(root, catalog, feature, "continuous", minimum_seconds=180, minimum_repetitions=5)
            require(sustained.get("accepted_current_duration_seconds") == record["duration_seconds"]
                    and sustained.get("accepted_current_repetitions") == record["repetitions"],
                    f"{ident}: sustained metrics disagree with the reviewed record")
        else:
            require(coverage["continuous"]["status"] != "accepted", f"{ident}: current continuous acceptance needs a sustained window")
            require(sustained.get("accepted_current_duration_seconds") is None and sustained.get("accepted_current_repetitions") == 0,
                    f"{ident}: unsupported current acceptance metrics")
        for key in ("movement", "interruptions", "transitions"):
            values = unique_strings(sustained.get(key), ident + "." + key, nonempty=not tooling)
            require(not tooling or not values, f"{ident}: external tool has native movement requirements")
        soak = sustained.get("soak", {})
        require(soak.get("status") in ({"not_applicable"} if tooling else {"missing", "accepted"}) and soak.get("minimum_duration_seconds") == (0 if tooling else 600),
                f"{ident}: invalid integrated soak requirement")
        if soak["status"] == "accepted":
            require(sustained["status"] == "accepted", f"{ident}: soak cannot replace missing feature acceptance")
            accepted_record(root, catalog, feature, "soak", minimum_seconds=600, minimum_repetitions=5)
    require(mapped_tests == actual, f"Unmapped CTest groups: {sorted(actual-mapped_tests)}")
    require(mapped_fixtures == fixtures, f"Unmapped native fixtures: {sorted(fixtures-mapped_fixtures)}")
    missing_sources = source_files(root) - mapped_sources
    require(not missing_sources, f"Unmapped source files: {sorted(missing_sources)}")
    require(catalog.get("summary") == computed_summary(catalog), "Catalog summary does not match computed coverage")
    policy = catalog.get("sustained_acceptance_policy", {})
    require(policy.get("accepted_current_feature_windows") == sum(f["sustained_regression"]["status"] == "accepted" for f in features)
            and policy.get("accepted_current_integrated_soaks") == catalog["summary"]["current_candidate_integrated_soak_passes"],
            "Sustained policy counts do not match reviewed acceptance")
    return catalog


def require_release_ready(root=ROOT, catalog=None, *, allow_known_issues=False):
    catalog = validate_catalog(root, catalog)
    require(type(allow_known_issues) is bool, "Known-issues override must be explicit boolean")
    blocked_records = [f for f in catalog["features"] if f["release_gate"]["status"] == "blocked"]
    blocked = [f["id"] for f in blocked_records]
    require(not blocked or allow_known_issues,
            "Release blocked by current native regressions: " + ", ".join(blocked))
    return {"catalog": CATALOG, "summary": catalog["summary"],
            "known_issues_override": {"requested": allow_known_issues,
                "applied": allow_known_issues and bool(blocked),
                "scope": "Only the refusal for documented blocked feature coverage is waived; catalog validation and package integrity checks remain required."},
            "blocked_features": blocked, "blocked_feature_coverage": blocked_records,
            "pending_features": [f["id"] for f in catalog["features"] if f["release_gate"]["status"] == "pending"],
            "experimental_features": [f["id"] for f in catalog["features"] if f["release_gate"]["status"] == "experimental"],
            "full_game_accepted": False, "physical_headset_accepted": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release", action="store_true", help="also refuse current blocked features")
    args = parser.parse_args()
    try:
        result = require_release_ready() if args.release else validate_catalog()["summary"]
    except (ValueError, KeyError, TypeError, json.JSONDecodeError) as error:
        parser.exit(1, f"Regression catalog: {error}\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
