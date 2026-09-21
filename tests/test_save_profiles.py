"""Save preservation tests using disposable data; never touches Steam saves."""
import importlib.util
import pathlib
import tempfile
import unittest
from unittest import mock

spec = importlib.util.spec_from_file_location("profiles", pathlib.Path(__file__).resolve().parents[1] / "tools/test-saves.py")
profiles = importlib.util.module_from_spec(spec)
spec.loader.exec_module(profiles)


class SaveProfiles(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)
        self.account, self.archive, self.store = (self.root / p for p in ("account", "archive", "profiles"))
        for directory, prefix in ((self.account, b"current"), (self.archive, b"field")):
            for relative in profiles.FILES:
                path = directory / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(prefix + relative.encode())
        profiles.snapshot(self.archive, self.store, "field")

    def contents(self):
        return {relative: (self.account / relative).read_bytes() for relative in profiles.FILES}

    def test_switch_and_exact_rollback(self):
        original = self.contents()
        untouched = self.account / "287700/local/TPP_GRAPHICS_CONFIG"
        untouched.write_bytes(b"personal graphics")
        result = profiles.activate(self.account, self.store, "field")
        self.assertNotEqual(original, self.contents())
        self.assertFalse(result["native_state_verified"])
        profiles.activate(self.account, self.store, result["rollback"])
        self.assertEqual(original, self.contents())
        self.assertEqual(untouched.read_bytes(), b"personal graphics")

    def test_corrupt_source_refused_before_touching_live_files(self):
        original = self.contents()
        (self.store / "field" / profiles.FILES[0]).write_bytes(b"corrupt")
        with self.assertRaisesRegex(ValueError, "integrity"):
            profiles.activate(self.account, self.store, "field")
        self.assertEqual(original, self.contents())

    def test_mid_switch_failure_restores_every_applied_file(self):
        original = self.contents()
        real_replace = profiles.os.replace
        calls = 0
        def fail_second(source, destination):
            nonlocal calls
            calls += 1
            if calls == 2:
                raise OSError("simulated locked destination")
            return real_replace(source, destination)
        with mock.patch.object(profiles.os, "replace", side_effect=fail_second):
            with self.assertRaisesRegex(OSError, "locked destination"):
                profiles.activate(self.account, self.store, "field")
        self.assertEqual(original, self.contents())
        self.assertFalse(list(self.account.rglob("*.mgs5vr-*")))

    def test_incomplete_archive_and_path_escape_refused(self):
        (self.archive / profiles.FILES[2]).unlink()
        with self.assertRaisesRegex(ValueError, "Incomplete"):
            profiles.snapshot(self.archive, self.store, "incomplete")
        self.assertFalse((self.store / "incomplete").exists())
        for name in ("../outside", "C:/outside", "field/child"):
            with self.assertRaises(ValueError):
                profiles.profile_path(self.store, name)

    def test_manifest_cannot_name_unrelated_files(self):
        path = self.store / "field/manifest.json"
        manifest = profiles.json.loads(path.read_text())
        manifest["files"][0]["path"] = "../../unrelated"
        profiles.write_json(path, manifest)
        with self.assertRaisesRegex(ValueError, "six allowed"):
            profiles.activate(self.account, self.store, "field")

    def test_both_cloud_mirrors_must_be_disabled(self):
        path = self.account / "7/remote/sharedconfig.vdf"
        path.parent.mkdir(parents=True)
        for first, second in (("0", "0"), ("1", "0"), ("0", "1")):
            path.write_text('"UserRoamingConfigStore" { "Software" { "Valve" { "Steam" { "apps" {'
                            '"287700" { "cloudenabled" "' + first + '" } '
                            '"311340" { "cloudenabled" "' + second + '" } } } } } }')
            if first == second == "0":
                profiles.require_cloud_disabled(self.account)
            else:
                with self.assertRaisesRegex(ValueError, "Disable Steam Cloud"):
                    profiles.require_cloud_disabled(self.account)


if __name__ == "__main__":
    unittest.main()
