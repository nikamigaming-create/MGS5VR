"""Exercise real filesystem transactions and reject unsafe update/cleanup paths."""
import importlib.util
import argparse
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("workspace", Path(__file__).resolve().parents[1] / "tools/workspace.py")
workspace = importlib.util.module_from_spec(spec)
spec.loader.exec_module(workspace)


class WorkspaceTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        # Hosted Windows runners can return an 8.3 TEMP alias. Deployment
        # resolves its paths; the fault-injection fixture must use that identity.
        self.root = Path(self.temporary.name).resolve()
        self.game = self.root / "game"
        self.play = self.root / "play"
        self.game.mkdir()
        self.play.mkdir()
        for name, content in {"mgsvtpp.exe": b"supported game", "dinput8.dll": b"owned old mod",
                              "mgs5vr_controls.exe": b"owned old checker", "mgs5vr.ini": b"personal VR fit",
                              "mgs5vr-controls.ini": b"personal controls"}.items():
            (self.game / name).write_bytes(content)
        workspace.write_json(self.game / "mgs5vr-install.json", {
            "schema": 1, "product": "MGS5VR theatre preview", "game_dir": str(self.game),
            "files": {n: workspace.digest(self.game / n) for n in workspace.BINARIES}})
        for name in workspace.BINARIES:
            (self.play / name).write_bytes(b"new checked " + name.encode())
        workspace.write_json(self.play / "BUILD.json", {
            "source": {"source_sha256": "checked source"},
            "files": {n: workspace.digest(self.play / n) for n in workspace.BINARIES}})
        self.value = {"game_dir": str(self.game)}
        self.old_record = (self.game / "mgs5vr-install.json").read_bytes()
        self.old = {p.name: p.read_bytes() for p in self.game.iterdir()}
        for key, value in {"ROOT": self.root, "PLAY": self.play,
                           "SCRATCH": self.root / "artifacts/bot", "SETTINGS": self.root / "private/workspace.json",
                           "SUPPORTED_EXE": workspace.digest(self.game / "mgsvtpp.exe")}.items():
            mock = patch.object(workspace, key, value)
            mock.start()
            self.addCleanup(mock.stop)
        self.inventory = patch.object(workspace, "processes", return_value=[])
        self.inventory_mock = self.inventory.start()
        self.addCleanup(self.inventory.stop)
        self.checker = patch.object(workspace, "run")
        self.checker_mock = self.checker.start()
        self.addCleanup(self.checker.stop)

    def test_update_preserves_personal_files_and_records_actual_binary(self):
        workspace.deploy(self.value)
        record = workspace.read_json(self.game / "mgs5vr-install.json")
        for name in ("mgsvtpp.exe", "mgs5vr.ini", "mgs5vr-controls.ini"):
            self.assertEqual((self.game / name).read_bytes(), self.old[name])
        for name in workspace.BINARIES:
            self.assertEqual(record["files"][name], workspace.digest(self.game / name))
            self.assertEqual((self.root / "build/installed-previous" / name).read_bytes(), self.old[name])
        workspace.deploy(self.value)
        self.assertEqual((self.root / "build/installed-previous/mgs5vr-install.json").read_bytes(), self.old_record)

    def test_partial_update_failure_rolls_back_both_binaries_and_record(self):
        real_replace = workspace.os.replace
        def fail_checker(source, destination):
            if Path(destination) == self.game / "mgs5vr_controls.exe":
                raise OSError("simulated locked checker")
            return real_replace(source, destination)
        with patch.object(workspace.os, "replace", side_effect=fail_checker):
            with self.assertRaises(OSError):
                workspace.deploy(self.value)
        for name, content in self.old.items():
            self.assertEqual((self.game / name).read_bytes(), content)

    def test_unrecognized_mod_is_preserved(self):
        (self.game / "dinput8.dll").write_bytes(b"another mod")
        with self.assertRaisesRegex(ValueError, "unknown provenance"):
            workspace.deploy(self.value)
        self.assertEqual((self.game / "dinput8.dll").read_bytes(), b"another mod")
        self.assertFalse((self.root / "build/installed-previous").exists())

    def test_live_game_rejects_file_replacement(self):
        with patch.object(workspace, "processes", return_value=[{"ProcessId": 42}]):
            with self.assertRaisesRegex(ValueError, "Close MGSV"):
                workspace.deploy(self.value)
        self.assertEqual((self.game / "dinput8.dll").read_bytes(), self.old["dinput8.dll"])

    def test_cleanup_cannot_escape_root_or_delete_root(self):
        for path in (self.root, self.root.parent / "another-project"):
            with self.assertRaises(ValueError):
                workspace.remove_tree(path, self.root)

    def test_prune_preserves_active_pinned_and_unmanaged_evidence(self):
        runs = workspace.SCRATCH / "runs"
        for name in ("01", "02", "03", "04", "active", "pinned", "unmanaged"):
            path = runs / name
            path.mkdir(parents=True)
            (path / "proof.png").write_bytes(b"proof")
            if name != "unmanaged":
                workspace.write_json(path / "managed-run.json", {})
            if name != "active":
                workspace.write_json(path / "finished.json", {})
            if name == "pinned":
                (path / "KEEP").touch()
        workspace.prune_runs({"keep_runs": 2}, runs / "04")
        self.assertEqual({p.name for p in runs.iterdir()}, {"03", "04", "active", "pinned", "unmanaged"})

    def test_failed_promotion_restores_current_play(self):
        stage = self.root / "build/play-stage"
        stage.mkdir(parents=True)
        original = self.play / "dinput8.dll"
        before = original.read_bytes()
        for name in workspace.BINARIES:
            (stage / name).write_bytes(b"checked replacement")
        real_replace = workspace.os.replace
        def fail_stage(source, target):
            if Path(target) == self.play / "mgs5vr_controls.exe":
                raise OSError("simulated promotion failure")
            return real_replace(source, target)
        with patch.object(workspace.os, "replace", fail_stage):
            with self.assertRaises(OSError):
                workspace.promote(stage)
        self.assertEqual(original.read_bytes(), before)
        self.assertTrue(stage.is_dir())
        self.assertFalse(list(self.play.rglob("*.mgs5vr-tmp")))

    def test_open_unchanged_launcher_does_not_block_fixed_folder_update(self):
        stage = self.root / "build/play-stage"
        stage.mkdir(parents=True)
        (self.play / "launcher.exe").write_bytes(b"open launcher")
        (stage / "launcher.exe").write_bytes(b"open launcher")
        (stage / "dinput8.dll").write_bytes(b"new checked game DLL")
        folder_identity = self.play.stat().st_ino
        real_replace = workspace.os.replace
        def refuse_launcher(source, target):
            if Path(target) == self.play / "launcher.exe":
                raise OSError("launcher is open")
            return real_replace(source, target)
        with patch.object(workspace.os, "replace", refuse_launcher), \
                patch.object(Path, "rename", side_effect=OSError("directory is open")):
            workspace.promote(stage)
        self.assertEqual(self.play.stat().st_ino, folder_identity)
        self.assertEqual((self.play / "dinput8.dll").read_bytes(), b"new checked game DLL")
        self.assertEqual((self.play / "launcher.exe").read_bytes(), b"open launcher")

    def test_promotion_removes_only_retired_manifest_files(self):
        stage = self.root / "build/play-stage"
        stage.mkdir(parents=True)
        (stage / "dinput8.dll").write_bytes(b"new checked game DLL")
        (self.play / "personal-note.txt").write_bytes(b"keep my note")
        workspace.promote(stage)
        self.assertFalse((self.play / "mgs5vr_controls.exe").exists())
        self.assertEqual((self.play / "personal-note.txt").read_bytes(), b"keep my note")

    def test_preexisting_update_file_is_preserved_on_refusal(self):
        stage = self.root / "build/play-stage"
        stage.mkdir(parents=True)
        (stage / "dinput8.dll").write_bytes(b"new checked game DLL")
        pending = self.play / "dinput8.dll.mgs5vr-tmp"
        pending.write_bytes(b"earlier pending work")
        before = (self.play / "dinput8.dll").read_bytes()
        with self.assertRaisesRegex(ValueError, "preserved"):
            workspace.promote(stage)
        self.assertEqual(pending.read_bytes(), b"earlier pending work")
        self.assertEqual((self.play / "dinput8.dll").read_bytes(), before)

    def test_manual_prune_preserves_stable_latest_pointer_even_over_budget(self):
        latest = workspace.SCRATCH / "runs/04"
        latest.mkdir(parents=True)
        workspace.write_json(latest / "managed-run.json", {})
        workspace.write_json(latest / "finished.json", {})
        workspace.write_json(workspace.SCRATCH / "latest.json", {"path": str(latest)})
        workspace.prune_runs({"keep_runs": 0, "scratch_budget_bytes": 0})
        self.assertTrue(latest.is_dir())

    def test_menu_coverage_uses_fixed_play_and_output_without_game_control(self):
        with patch.object(workspace, "settings", return_value=self.value):
            workspace.coverage(argparse.Namespace(menus=True, run=[]))
        command = self.checker_mock.mock_calls[0].args[0]
        self.assertIn("--menus-only", command)
        self.assertEqual(command[command.index("--candidate-dll") + 1], self.play / "dinput8.dll")
        self.assertEqual(command[command.index("--output") + 1], self.root / "artifacts/dev/coverage")
        self.assertEqual(self.checker_mock.call_count, 1)
        self.inventory_mock.assert_not_called()

    def test_coverage_rejects_unpinned_scratch_before_running_report(self):
        scratch = workspace.SCRATCH / "runs/unreviewed"
        scratch.mkdir(parents=True)
        with patch.object(workspace, "settings", return_value=self.value):
            with self.assertRaisesRegex(ValueError, "Pin the reviewed run"):
                workspace.coverage(argparse.Namespace(menus=True, run=[scratch]))
        self.checker_mock.assert_not_called()

    def test_coverage_accepts_only_retained_runs_inside_managed_folder(self):
        retained = workspace.SCRATCH / "runs/retained"
        retained.mkdir(parents=True)
        for name in ("KEEP", "identity.json", "result.json"):
            (retained / name).write_text("{}")
        with patch.object(workspace, "settings", return_value=self.value):
            workspace.coverage(argparse.Namespace(menus=True, run=[retained]))
        command = self.checker_mock.mock_calls[0].args[0]
        self.assertEqual(command[command.index("--run") + 1], retained)
        self.checker_mock.reset_mock()
        with patch.object(workspace, "settings", return_value=self.value):
            with self.assertRaisesRegex(ValueError, "outside"):
                workspace.coverage(argparse.Namespace(menus=True, run=[self.root / "foreign-run"]))
        self.checker_mock.assert_not_called()


if __name__ == "__main__":
    unittest.main()
