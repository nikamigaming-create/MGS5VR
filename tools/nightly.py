"""Publish checked, asset-free CI nightlies without promoting the local Play pin."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import xml.etree.ElementTree as ET
import zipfile

from regression_catalog import current_ctests, validate_catalog

ROOT = Path(__file__).resolve().parents[1]
REPO = "nikamigaming-create/MGS5VR"
OUTPUT = ROOT / "build/nightly-release"
SKIPPED = {"d3d11_mailbox", "independent_render_window", "ground_zeroes_render_window"}
TAG = re.compile(r"nightly-\d{4}-\d{2}-\d{2}-[0-9a-f]{12}")
_spec = importlib.util.spec_from_file_location("release_package", ROOT / "tools/package-release.py")
_package = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_package)


def run(*args):
    return subprocess.check_output(args, cwd=ROOT, text=True, encoding="utf-8").strip()


def gh(*args):
    return run("gh", *args)


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def read(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def write(path, value):
    Path(path).write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def blocker_snapshot(feature):
    # Include the recorded failure and missing checks, not just a feature ID.
    return {key: feature.get(key) for key in
            ("title", "release_gate", "coverage", "missing_checks", "historical_failure_record")}


def check_policy(catalog, policy):
    if policy.get("schema") != 1 or policy.get("enabled") is not True:
        raise ValueError("Nightly publication is disabled or the policy is invalid")
    blocked = [f for f in catalog["features"] if f["release_gate"]["status"] == "blocked"]
    for feature in blocked:
        if policy.get("accepted_blockers", {}).get(feature["id"]) != blocker_snapshot(feature):
            raise ValueError(f"New or changed nightly blocker requires review: {feature['id']}")
    return blocked


def policy():
    catalog = validate_catalog(ROOT)
    return catalog, check_policy(catalog, read(ROOT / "config/nightly-policy.json"))


def latest_nightly(releases):
    candidates = [r for r in releases if not r["draft"] and r["prerelease"]
                  and TAG.fullmatch(r["tag_name"])]
    return max(candidates, key=lambda r: r["published_at"], default=None)


def plan():
    policy()
    commit = run("git", "rev-parse", "HEAD")
    pages = json.loads(gh("api", "--paginate", "--slurp", f"repos/{REPO}/releases?per_page=100"))
    releases = [release for page in pages for release in page]
    previous = latest_nightly(releases)
    published = any(not r["draft"] and TAG.fullmatch(r["tag_name"])
                    and r["target_commitish"] == commit for r in releases)
    values = {"publish": str(not published).lower(),
              "tag": f"nightly-{datetime.now(timezone.utc):%Y-%m-%d}-{commit[:12]}",
              "previous": previous["target_commitish"] if previous else "experimental-2026-10-19.1"}
    if os.environ.get("GITHUB_OUTPUT"):
        with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as stream:
            for key, value in values.items():
                stream.write(f"{key}={value}\n")
    print(json.dumps(values))


def check_tests(report, expected):
    cases = ET.parse(report).getroot().findall(".//testcase")
    names = [case.get("name") for case in cases]
    if set(names) != set(expected) or len(names) != len(set(names)):
        raise ValueError("CI test report has missing, extra or duplicate tests")
    for case in cases:
        if any(case.find(tag) is not None for tag in ("failure", "error", "skipped")):
            raise ValueError(f"CI test did not pass: {case.get('name')}")
        if case.get("status", "run") not in {"run", "passed"}:
            raise ValueError(f"CI test has unexpected status: {case.get('name')}")
    return len(cases)


def checked_tree(tree, install_manifest):
    files = {}
    for path in tree.rglob("*"):
        if path.is_symlink():
            raise ValueError(f"Linked package path: {path}")
        if path.is_file():
            name = path.relative_to(tree).as_posix()
            _package.checked_name(name)
            files[name] = digest(path)
    installed = {Path(line).resolve() for line in install_manifest.read_text(encoding="utf-8").splitlines() if line}
    if installed != {p.resolve() for p in tree.rglob("*") if p.is_file()}:
        raise ValueError("Installation manifest and package contents differ")
    required = {"dinput8.dll", "MGS5VR-Launcher.exe", "MGS5VR-FieldKit.exe", "mgs5vr_import.exe",
                "mgs5vr_controls.exe", "mgs5vr_probe.exe", "Install.cmd", "Launch-Headset.cmd",
                "tools/setup.ps1", "tools/install.ps1", "LICENSE", "licenses/OpenXR.txt",
                "licenses/MinHook.txt", "licenses/WebView2-LICENSE.txt",
                "launcher-ui/vendor/three-r180/LICENSE", "launcher-ui/assets/LICENSE-webxr-input-profiles.md"}
    if not required <= files.keys():
        raise ValueError(f"Incomplete installation: {sorted(required - files.keys())}")
    for name in ("mgs5vr.ini", "mgs5vr-controls.ini"):
        expected = digest(ROOT / "config" / name)
        if files.get(name) != expected or files.get("config/" + name) != expected:
            raise ValueError("Nightlies must contain public defaults, not personal settings")
    return files


def package(tag, previous):
    if not TAG.fullmatch(tag):
        raise ValueError("Invalid nightly tag")
    commit = run("git", "rev-parse", "HEAD")
    if not tag.endswith("-" + commit[:12]) or os.environ.get("GITHUB_SHA") != commit:
        raise ValueError("Nightly tag and CI commit must match the checkout")
    if run("git", "status", "--porcelain"):
        raise ValueError("Nightlies require clean committed source")
    catalog, blocked = policy()
    report = ROOT / "build/ci-tests.xml"
    count = check_tests(report, current_ctests(ROOT) - SKIPPED)
    tree = ROOT / "dist/MGS5VR-development"
    files = checked_tree(tree, ROOT / "build/install_manifest_MGS5VR.txt")
    OUTPUT.mkdir(parents=True, exist_ok=False)
    source_zip = OUTPUT / "MGS5VR-source.zip"
    run("git", "archive", "--format=zip", "--prefix=MGS5VR-source/", f"--output={source_zip}", commit)
    with zipfile.ZipFile(source_zip) as source:
        for item in source.infolist():
            _package.checked_name(item.filename)
            if (item.external_attr >> 16) & 0o170000 == 0o120000:
                raise ValueError("Linked source path")
        if source.testzip():
            raise ValueError("Corrupt source archive")
    run_url = f"https://github.com/{REPO}/actions/runs/{os.environ['GITHUB_RUN_ID']}"
    validation = {"hosted_windows_automated": "passed", "ctest_groups_passed": count,
                  "excluded_gpu_tests": sorted(SKIPPED), "installer": "passed",
                  "simulator": "not_run", "physical_headset": "unverified", "full_game": "unverified",
                  "workflow_run": run_url}
    metadata = {"schema": 1, "channel": "nightly", "tag": tag, "source_commit": commit,
                "created_utc": datetime.now(timezone.utc).isoformat(), "validation": validation,
                "dll_sha256": files["dinput8.dll"], "accepted_known_blockers": [f["id"] for f in blocked],
                "previous": previous, "archive": f"MGS5VR-{tag}.zip"}
    # BUILD.json deliberately does not claim the full local automated check set passed.
    extras = {"BUILD.json": json.dumps({"schema": 1, "source": {"commit": commit},
               "files": files, "validation": validation}, indent=2).encode(),
              "RELEASE.json": json.dumps(metadata, indent=2).encode(),
              "SOURCE_REVISION.txt": (commit + "\n").encode(),
              "NIGHTLY_VALIDATION.xml": report.read_bytes(),
              "NIGHTLY_KNOWN_ISSUES.json": json.dumps({"blocked_features": blocked,
                   "coverage_summary": catalog["summary"]}, indent=2).encode(),
              "MGS5VR-source.zip": source_zip.read_bytes()}
    hashes = {**files, **{name: hashlib.sha256(data).hexdigest() for name, data in extras.items()}}
    archive = OUTPUT / metadata["archive"]
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as bundle:
        for name in sorted(files):
            bundle.write(tree / name, "MGS5VR/" + name)
        for name, data in extras.items():
            bundle.writestr("MGS5VR/" + name, data)
        bundle.writestr("MGS5VR/manifest.json", json.dumps({"schema": 1, "source_commit": commit, "files": hashes}, indent=2))
    verify_zip(archive)
    metadata["archive_sha256"] = digest(archive)
    write(OUTPUT / "release.json", metadata)
    changes = run("git", "log", "--format=%h %s", "-30", f"{previous}..{commit}")
    notes = [f"# MGS5VR nightly — {tag.removeprefix('nightly-')}", "",
             "Experimental development snapshot. Opt in for recent fixes and features; regressions are possible.",
             "This is not a reviewed milestone release or a physical-headset/full-game acceptance pass.", "",
             f"Download **{archive.name}**, extract it and run **Install.cmd**. Do not use GitHub's source-only ZIP to install.",
             "Your game files are imported locally from your own installation. No retail assets or personal settings are included.", "",
             f"Source: `{commit}` · [Build and checks]({run_url})", "",
             f"Windows build, {count} CPU-compatible CTest groups, Python checks and packaged installer checks passed.",
             f"The {len(SKIPPED)} GPU-dependent tests, native gameplay, simulator and physical headset were not exercised by this CI run.", "",
             "## Included game support", "",
             "- **The Phantom Pain 1.0.15.4:** the current VR rig, cabin interactions, controls and settings.",
             "- **Ground Zeroes 1.0.0.5:** independent first-person stereo, tracked hands and weapon aim, left-hand iDroid, forearm HUD, physical binocular acquisition/marking/intel, and normal setup with local owned-asset import.",
             "Ground Zeroes has scoped simulator observations; remaining native effects, vehicle/equipment/mission coverage and physical-headset acceptance are unfinished. TPP-only buddies and powered arms are not added to GZ.", "",
             "## Known issues and acceptance limits", "",
             "These catalog entries retain their original evidence/build scope; a historical block is not a new reproduction:", ""]
    notes += [f"- **{f['title']}**: {f['release_gate']['reason']}" for f in blocked]
    notes += ["", "Other weapons, outfits, terrain, progression and whole missions retain their documented coverage gaps.",
              f"[Full field-fix record](https://github.com/{REPO}/blob/{commit}/docs/FIELD_FIXES.md).", "",
              "## Changes", "", f"[All changes since the previous download](https://github.com/{REPO}/compare/{previous}...{commit})", "",
              "Recent commits (commit descriptions are not acceptance claims):", ""]
    notes += ["- " + line for line in changes.splitlines()]
    notes += ["", "When reporting a problem, include the nightly tag, headset/runtime, and steps to reproduce.",
              "The recommended release and local Play pin are unchanged by nightly publication.", ""]
    (OUTPUT / "release-notes.md").write_text("\n".join(notes), encoding="utf-8")
    (OUTPUT / "ci-tests.xml").write_bytes(report.read_bytes())
    sums = [f"{digest(path)}  {path.name}" for path in sorted(OUTPUT.iterdir()) if path.is_file()]
    (OUTPUT / "SHA256SUMS.txt").write_text("\n".join(sums) + "\n", encoding="ascii")
    print(json.dumps(metadata))


def verify_zip(archive):
    with zipfile.ZipFile(archive) as bundle:
        manifest = json.loads(bundle.read("MGS5VR/manifest.json"))
        names = bundle.namelist()
        expected = {"MGS5VR/" + name for name in manifest["files"]} | {"MGS5VR/manifest.json"}
        if set(names) != expected or len(names) != len(expected):
            raise ValueError("Archive has missing, extra or duplicate entries")
        for name, wanted in manifest["files"].items():
            _package.checked_name(name)
            if hashlib.sha256(bundle.read("MGS5VR/" + name)).hexdigest() != wanted:
                raise ValueError(f"Archive checksum mismatch: {name}")


def verify_payload(directory):
    expected = {}
    for line in (directory / "SHA256SUMS.txt").read_text(encoding="ascii").splitlines():
        wanted, name = line.split("  ", 1)
        if not re.fullmatch(r"[0-9a-f]{64}", wanted) or Path(name).name != name or name in expected:
            raise ValueError("Invalid checksum manifest")
        _package.checked_name(name)
        expected[name] = wanted
    actual = {p.name for p in directory.iterdir()}
    if actual != set(expected) | {"SHA256SUMS.txt"}:
        raise ValueError("Unexpected release payload files")
    for name, wanted in expected.items():
        if digest(directory / name) != wanted:
            raise ValueError(f"Payload checksum mismatch: {name}")
    metadata = read(directory / "release.json")
    if not TAG.fullmatch(metadata["tag"]) or metadata["source_commit"] != os.environ.get("GITHUB_SHA"):
        raise ValueError("Payload is not from this workflow commit")
    if metadata["archive"] not in expected or expected[metadata["archive"]] != metadata["archive_sha256"]:
        raise ValueError("Archive identity mismatch")
    verify_zip(directory / metadata["archive"])
    return metadata


def verify_uploaded(tag, directory):
    with tempfile.TemporaryDirectory(prefix="mgs5vr-nightly-verify-") as scratch:
        gh("release", "download", tag, "--repo", REPO, "--dir", scratch)
        fetched = Path(scratch)
        if {p.name for p in fetched.iterdir()} != {p.name for p in directory.iterdir()}:
            raise ValueError("Uploaded asset inventory differs")
        for path in directory.iterdir():
            if digest(path) != digest(fetched / path.name):
                raise ValueError(f"Uploaded asset checksum mismatch: {path.name}")


def publish():
    policy()
    metadata = verify_payload(OUTPUT)
    tag, commit = metadata["tag"], metadata["source_commit"]
    if not tag.endswith("-" + commit[:12]):
        raise ValueError("Tag/commit mismatch")
    # Listing distinguishes a missing release from auth/network failure.
    pages = json.loads(gh("api", "--paginate", "--slurp", f"repos/{REPO}/releases?per_page=100"))
    existing = next((r for page in pages for r in page if r["tag_name"] == tag), None)
    if existing and (existing["target_commitish"] != commit or not existing["prerelease"]):
        raise ValueError("Existing release has a different identity")
    if not existing:
        gh("release", "create", tag, "--repo", REPO, "--target", commit, "--draft", "--prerelease",
           "--latest=false", "--title", f"MGS5VR Nightly {tag.removeprefix('nightly-')}",
           "--notes-file", str(OUTPUT / "release-notes.md"))
    if not existing or existing["draft"]:
        gh("release", "upload", tag, "--repo", REPO, "--clobber", *[str(p) for p in sorted(OUTPUT.iterdir())])
        verify_uploaded(tag, OUTPUT)
        gh("release", "edit", tag, "--repo", REPO, "--draft=false", "--prerelease", "--latest=false")
    verify_uploaded(tag, OUTPUT)
    result = json.loads(gh("release", "view", tag, "--repo", REPO, "--json", "url,isDraft,isPrerelease,targetCommitish"))
    if result["isDraft"] or not result["isPrerelease"] or result["targetCommitish"] != commit:
        raise ValueError("Published release identity did not verify")
    print(result["url"])
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as stream:
            stream.write(f"Published and download-verified: {result['url']}\n\nSHA-256: `{metadata['archive_sha256']}`\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("plan")
    pack = sub.add_parser("package")
    pack.add_argument("--tag", required=True)
    pack.add_argument("--previous", required=True)
    sub.add_parser("publish")
    args = parser.parse_args()
    if args.command == "plan":
        plan()
    elif args.command == "package":
        package(args.tag, args.previous)
    else:
        publish()
