"""Failure-oriented checks for public nightly packaging and publication gates."""
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import nightly


class NightlyTests(unittest.TestCase):
    def test_only_the_reviewed_blocker_snapshot_is_allowed(self):
        feature = {"id": "example", "title": "Existing issue", "release_gate": {"status": "blocked", "reason": "Known"},
                   "coverage": {}, "missing_checks": ["Headset"]}
        policy = {"schema": 1, "enabled": True, "accepted_blockers": {"example": nightly.blocker_snapshot(feature)}}
        self.assertEqual(nightly.check_policy({"features": [feature]}, policy), [feature])
        for field, value in (("id", "new-issue"), ("missing_checks", ["New crash"]),
                             ("release_gate", {"status": "blocked", "reason": "New failure"})):
            changed = {**feature, field: value}
            with self.assertRaisesRegex(ValueError, "New or changed"):
                nightly.check_policy({"features": [changed]}, policy)
        self.assertEqual(nightly.check_policy({"features": []}, policy), [])
        with self.assertRaisesRegex(ValueError, "disabled"):
            nightly.check_policy({"features": [feature]}, {**policy, "enabled": False})

    def test_ci_report_cannot_hide_failure_skip_or_missing_test(self):
        with tempfile.TemporaryDirectory() as scratch:
            path = Path(scratch) / "tests.xml"
            path.write_text('<testsuite><testcase name="one" status="run"/></testsuite>')
            self.assertEqual(nightly.check_tests(path, {"one"}), 1)
            for body in ('<testcase name="one"><failure/></testcase>',
                         '<testcase name="one"><error/></testcase>',
                         '<testcase name="one"><skipped/></testcase>',
                         '<testcase name="one" status="notrun"/>', '',
                         '<testcase name="one"/><testcase name="one"/>'):
                path.write_text('<testsuite>' + body + '</testsuite>')
                with self.assertRaises(ValueError):
                    nightly.check_tests(path, {"one"})

    def test_archive_tamper_extra_and_private_paths_are_rejected(self):
        with tempfile.TemporaryDirectory() as scratch:
            path = Path(scratch) / "test.zip"
            for name, value, extra in (("dinput8.dll", b"correct", False),
                                       ("dinput8.dll", b"tampered", False),
                                       ("dinput8.dll", b"correct", True),
                                       ("../escape", b"correct", False),
                                       ("private/secret.txt", b"correct", False)):
                manifest = {"files": {name: hashlib.sha256(b"correct").hexdigest()}}
                with zipfile.ZipFile(path, "w") as archive:
                    archive.writestr("MGS5VR/manifest.json", json.dumps(manifest))
                    archive.writestr("MGS5VR/" + name, value)
                    if extra:
                        archive.writestr("MGS5VR/extra.txt", "unmanaged")
                if name == "dinput8.dll" and value == b"correct" and not extra:
                    nightly.verify_zip(path)
                else:
                    with self.assertRaises(ValueError):
                        nightly.verify_zip(path)

    def test_latest_nightly_ignores_drafts_and_reviewed_releases(self):
        nightly_release = {"tag_name": "nightly-2026-10-27-0123456789ab", "draft": False,
                           "prerelease": True, "published_at": "2026-10-27T00:00:00Z"}
        newer_draft = {**nightly_release, "draft": True, "published_at": "2026-10-28T00:00:00Z"}
        reviewed = {**nightly_release, "tag_name": "experimental-2026-10-28", "published_at": "2026-10-28T00:00:00Z"}
        self.assertEqual(nightly.latest_nightly([reviewed, newer_draft, nightly_release]), nightly_release)

    def test_failed_upload_verification_never_publishes_draft(self):
        commit = "0" * 40
        metadata = {"tag": "nightly-2026-10-27-" + commit[:12], "source_commit": commit}
        with tempfile.TemporaryDirectory() as scratch, patch.object(nightly, "OUTPUT", Path(scratch)), \
             patch.object(nightly, "policy"), patch.object(nightly, "verify_payload", return_value=metadata), \
             patch.object(nightly, "verify_uploaded", side_effect=ValueError("bad upload")), \
             patch.object(nightly, "gh") as gh:
            gh.side_effect = lambda *args: "[[]]" if args[0] == "api" else ""
            with self.assertRaisesRegex(ValueError, "bad upload"):
                nightly.publish()
            self.assertFalse(any(call.args[:2] == ("release", "edit") for call in gh.call_args_list))

    def test_published_release_is_verified_without_replacing_assets(self):
        commit = "0" * 40
        metadata = {"tag": "nightly-2026-10-27-" + commit[:12], "source_commit": commit}
        existing = {"tag_name": metadata["tag"], "target_commitish": commit, "draft": False, "prerelease": True}
        with patch.object(nightly, "policy"), patch.object(nightly, "verify_payload", return_value=metadata), \
             patch.object(nightly, "verify_uploaded"), patch.object(nightly, "gh") as gh, \
             patch.dict(nightly.os.environ, {}, clear=True):
            gh.side_effect = [json.dumps([[existing]]), json.dumps({"isDraft": False, "isPrerelease": True,
                               "targetCommitish": commit, "url": "https://example.test/nightly"})]
            nightly.publish()
            self.assertFalse(any(call.args[:2] in (("release", "upload"), ("release", "edit")) for call in gh.call_args_list))


if __name__ == "__main__":
    unittest.main()
