"""Exercise real filesystem transactions and reject unsafe update/cleanup paths."""
import importlib.util
import argparse
import json
import shutil
from pathlib import Path
import tempfile
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))

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
            "validation": {"automated": "passed", "headset": "unproven", "full_game": "unproven"},
            "files": {n: workspace.digest(self.play / n) for n in workspace.BINARIES}})
        self.dev = self.root / "build/dev"
        shutil.copytree(self.play, self.dev)
        self.value = {"game_dir": str(self.game)}
        self.old_record = (self.game / "mgs5vr-install.json").read_bytes()
        self.old = {p.name: p.read_bytes() for p in self.game.iterdir()}
        for key, value in {"ROOT": self.root, "PLAY": self.play, "DEV": self.dev,
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
        self.assertFalse((self.root / "build/install-rollback").exists())
        workspace.deploy(self.value)
        self.assertFalse((self.root / "build/install-rollback").exists())

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

    def set_development_binary(self, content=b"different development build"):
        (self.dev / "dinput8.dll").write_bytes(content)
        manifest = workspace.read_json(self.dev / "BUILD.json")
        manifest["files"]["dinput8.dll"] = workspace.digest(self.dev / "dinput8.dll")
        workspace.write_json(self.dev / "BUILD.json", manifest)

    def pin_current(self):
        with patch.object(workspace, "settings", return_value=self.value):
            workspace.pin_build(argparse.Namespace(current=True))
        self.value = workspace.read_json(workspace.SETTINGS)

    def test_development_install_and_play_restore_keep_pin_and_personal_settings(self):
        self.pin_current()
        pinned_manifest = (self.play / "BUILD.json").read_bytes()
        self.set_development_binary()
        workspace.deploy(self.value, build=self.dev)
        self.assertEqual((self.game / "dinput8.dll").read_bytes(), b"different development build")
        workspace.deploy(self.value)
        self.assertEqual((self.game / "dinput8.dll").read_bytes(), (self.play / "dinput8.dll").read_bytes())
        self.assertEqual((self.play / "BUILD.json").read_bytes(), pinned_manifest)
        for name in ("mgsvtpp.exe", "mgs5vr.ini", "mgs5vr-controls.ini"):
            self.assertEqual((self.game / name).read_bytes(), self.old[name])
        self.assertFalse((self.root / "build/install-rollback").exists())
        self.assertEqual(workspace.read_json(self.game / "mgs5vr-install.json")["build_slot"], "play")

    def test_changed_pinned_binary_or_manifest_is_refused_before_install(self):
        self.pin_current()
        (self.play / "dinput8.dll").write_bytes(b"unexpected change")
        with self.assertRaisesRegex(ValueError, "Checked build file changed"):
            workspace.deploy(self.value)
        manifest = workspace.read_json(self.play / "BUILD.json")
        manifest["files"]["dinput8.dll"] = workspace.digest(self.play / "dinput8.dll")
        workspace.write_json(self.play / "BUILD.json", manifest)
        with self.assertRaisesRegex(ValueError, "Pinned play manifest changed"):
            workspace.deploy(self.value)
        self.assertEqual((self.game / "dinput8.dll").read_bytes(), self.old["dinput8.dll"])

    def test_explicit_promotion_changes_pin_without_claiming_headset_acceptance(self):
        self.pin_current()
        self.set_development_binary()
        with patch.object(workspace, "settings", return_value=self.value):
            workspace.pin_build(argparse.Namespace(current=False))
        pin = workspace.read_json(workspace.SETTINGS)["pinned_build"]
        self.assertEqual(pin["dll_sha256"], workspace.digest(self.dev / "dinput8.dll"))
        self.assertEqual(pin["validation"]["headset"], "unproven")
        self.assertEqual(pin["validation"]["full_game"], "unproven")
        self.assertEqual((self.game / "dinput8.dll").read_bytes(), b"different development build")
        for name in ("pin-stage", "slot-rollback", "install-rollback"):
            self.assertFalse((self.root / "build" / name).exists())

    def test_normal_build_changes_only_development_even_while_game_is_running(self):
        before = {p.name: p.read_bytes() for p in self.play.iterdir()}
        def fake_build(command, **_):
            if "--install" in command:
                stage = Path(command[command.index("--prefix") + 1])
                stage.mkdir(parents=True)
                for name in workspace.BINARIES:
                    (stage / name).write_bytes(b"new development " + name.encode())
        with patch.object(workspace, "settings", return_value=self.value), \
                patch.object(workspace, "run", side_effect=fake_build), \
                patch.object(workspace, "source_identity", return_value={"source_sha256": "new source"}), \
                patch.object(workspace, "processes", side_effect=AssertionError("A normal build must not depend on a running game")):
            workspace.refresh(argparse.Namespace(deploy=False, no_deploy=False))
        self.assertTrue((self.dev / "dinput8.dll").read_bytes().startswith(b"new development"))
        for name, content in before.items():
            self.assertEqual((self.play / name).read_bytes(), content)
        for name, content in self.old.items():
            self.assertEqual((self.game / name).read_bytes(), content)
        self.assertFalse((self.root / "build/slot-rollback").exists())
        self.assertFalse((self.root / "build/dev-stage").exists())

    def test_failed_installer_checks_keep_both_builds_and_remove_staging(self):
        before = (self.dev / "dinput8.dll").read_bytes()
        def fail_installer(command, **_):
            if "--install" in command:
                stage = Path(command[command.index("--prefix") + 1])
                stage.mkdir(parents=True)
                (stage / "dinput8.dll").write_bytes(b"rejected build")
            if command[0] == "powershell.exe":
                raise workspace.subprocess.CalledProcessError(1, command)
        with patch.object(workspace, "settings", return_value=self.value), \
                patch.object(workspace, "run", side_effect=fail_installer):
            with self.assertRaises(workspace.subprocess.CalledProcessError):
                workspace.refresh(argparse.Namespace(deploy=False))
        self.assertEqual((self.dev / "dinput8.dll").read_bytes(), before)
        self.assertEqual((self.play / "dinput8.dll").read_bytes(), before)
        self.assertFalse((self.root / "build/dev-stage").exists())

    def test_cleanup_removes_old_builds_but_retains_settings_notes_and_evidence(self):
        from build_retention import clean
        self.pin_current()
        old = self.root / "build/play-previous"
        shutil.copytree(self.play, old)
        releases = self.root / "build/github-release"
        releases.mkdir()
        (releases / "old.zip").write_bytes(b"obsolete package")
        (releases / "notes.md").write_text("retain release notes")
        evidence = self.root / "private/bot-baseline-test"
        evidence.mkdir(parents=True)
        (evidence / "baseline.json").write_text("{}")
        (evidence / "dinput8.dll").write_bytes(b"historical mod")
        backup = self.game / "mgs5vr-launcher-backups/old"
        backup.mkdir(parents=True)
        for name in (*workspace.BINARIES, "mgs5vr.ini", "mgs5vr-install.json"):
            shutil.copy2(self.game / name, backup / name)
        with patch.object(workspace, "settings", return_value=self.value):
            clean(workspace, argparse.Namespace(apply=True))
        self.assertFalse(old.exists())
        self.assertFalse((releases / "old.zip").exists())
        self.assertTrue((releases / "notes.md").exists())
        self.assertTrue((evidence / "baseline.json").exists())
        self.assertFalse((evidence / "dinput8.dll").exists())
        self.assertEqual((backup / "mgs5vr.ini").read_bytes(), self.old["mgs5vr.ini"])
        self.assertFalse((backup / "dinput8.dll").exists())
        workspace.checked_build(self.play, self.value["pinned_build"])
        workspace.checked_build(self.dev)

    def test_cleanup_refuses_to_delete_anything_until_both_slots_verify(self):
        from build_retention import clean
        self.pin_current()
        old = self.root / "build/play-previous"
        shutil.copytree(self.play, old)
        (self.dev / "dinput8.dll").write_bytes(b"damaged build")
        with patch.object(workspace, "settings", return_value=self.value):
            with self.assertRaisesRegex(ValueError, "Checked build file changed"):
                clean(workspace, argparse.Namespace(apply=True))
        self.assertTrue(old.exists())

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
                workspace.promote(stage, self.play)
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
            workspace.promote(stage, self.play)
        self.assertEqual(self.play.stat().st_ino, folder_identity)
        self.assertEqual((self.play / "dinput8.dll").read_bytes(), b"new checked game DLL")
        self.assertEqual((self.play / "launcher.exe").read_bytes(), b"open launcher")

    def test_promotion_removes_only_retired_manifest_files(self):
        stage = self.root / "build/play-stage"
        stage.mkdir(parents=True)
        (stage / "dinput8.dll").write_bytes(b"new checked game DLL")
        (self.play / "personal-note.txt").write_bytes(b"keep my note")
        workspace.promote(stage, self.play)
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
            workspace.promote(stage, self.play)
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
        self.assertEqual(command[command.index("--candidate-dll") + 1], self.dev / "dinput8.dll")
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

    def test_game_model_reuses_fixed_output_and_never_controls_a_game(self):
        value={**self.value,'navigation':str(self.root/'owned-manifest.json')}
        with patch.object(workspace,'settings',return_value=value):
            workspace.game_model(argparse.Namespace(run=[]))
        command=self.checker_mock.call_args.args[0]
        self.assertEqual(command[command.index('--output')+1],self.root/'artifacts/dev/coverage')
        self.assertEqual(command[command.index('--navigation')+1],self.root/'owned-manifest.json')
        self.assertIn(self.dev/'mgs5vr_controls.exe',command)
        self.inventory_mock.assert_not_called()

    def field_receipt(self):
        state = {'scene': 'gameplay', 'camera_active': True, 'camera_available': True,
                 'menu': False, 'pause': False, 'idroid': False, 'loading': False,
                 'title': False, 'demo': False, 'now_ms': 1000,
                 'transport': {'seconds': .02},
                 'rendered': {'presentation_focused': True, 'player_owner': 123},
                 'native': {'popup': False, 'demo': False, 'game_over': 0, 'saving': False,
                            'player_life': 7801, 'sequence': 'Seq_Game_FreePlay'},
                 'controls': {'context': 'gameplay', 'rig_input': True, 'age_ms': 0,
                              'sample_ms': 1000, 'native_buttons': 0, 'physical': [0] * 11,
                              'sticks': [0] * 4, 'native_axes': [0] * 4, 'native_triggers': [0] * 2}}
        process = {'ProcessId': 42, 'CreationDate': 'same-generation',
                   'ExecutablePath': str(self.game / 'mgsvtpp.exe')}
        receipt = {'schema': 1, 'status': 'released_field', 'unix_ns': workspace.time.time_ns(),
                   'identity': {'process': process, 'dll_sha256': workspace.digest(self.game / 'dinput8.dll')},
                   'state': state}
        output = workspace.SCRATCH / 'runs' / 'field-handoff'
        manifest = {'files': {'dinput8.dll': receipt['identity']['dll_sha256']}}
        self.inventory_mock.return_value = [process]
        return output, manifest, receipt

    def test_failed_case_can_keep_same_released_live_field(self):
        output, manifest, receipt = self.field_receipt()
        workspace.write_json(output / 'released-session.json', receipt)
        self.assertTrue(workspace.retained_field_session(output, self.game, manifest))
        self.checker_mock.assert_not_called()

    def test_field_handoff_rejects_held_input_menu_dead_player_and_stale_control(self):
        import copy
        output, manifest, original = self.field_receipt()
        mutations = [('controls', 'native_triggers', [0, 255]), ('controls', 'native_buttons', 32768),
                     ('controls', 'physical', [0] * 10 + [1]), ('controls', 'native_axes', [0, 32767, 0, 0]),
                     ('native', 'player_life', 0), ('native', 'popup', True),
                     ('rendered', 'presentation_focused', False), ('controls', 'sample_ms', 999)]
        for section, key, value in mutations:
            with self.subTest(section=section, key=key):
                receipt = copy.deepcopy(original)
                receipt['state'][section][key] = value
                workspace.write_json(output / 'released-session.json', receipt)
                self.assertFalse(workspace.retained_field_session(output, self.game, manifest))
        for key in ('menu', 'pause', 'idroid', 'loading', 'demo'):
            with self.subTest(key=key):
                receipt = copy.deepcopy(original)
                receipt['state'][key] = True
                workspace.write_json(output / 'released-session.json', receipt)
                self.assertFalse(workspace.retained_field_session(output, self.game, manifest))

    def test_field_handoff_rejects_old_receipt_reused_pid_and_different_binary(self):
        import copy
        output, manifest, original = self.field_receipt()
        for change in ('age', 'generation', 'binary', 'status'):
            with self.subTest(change=change):
                receipt = copy.deepcopy(original)
                if change == 'age': receipt['unix_ns'] -= 16_000_000_000
                if change == 'generation': receipt['identity']['process']['CreationDate'] = 'old-generation'
                if change == 'binary': receipt['identity']['dll_sha256'] = 'another-dll'
                if change == 'status': receipt['status'] = 'needs_cleanup'
                workspace.write_json(output / 'released-session.json', receipt)
                self.assertFalse(workspace.retained_field_session(output, self.game, manifest))

    def test_missing_release_receipt_requires_cleanup(self):
        output, manifest, _ = self.field_receipt()
        self.assertFalse(workspace.retained_field_session(output, self.game, manifest))


if __name__ == "__main__":
    unittest.main()
