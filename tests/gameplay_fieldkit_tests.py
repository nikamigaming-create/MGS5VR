"""Supervisor ownership, real-evidence references, catalog and local API contracts."""
import json
import pathlib
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch
import urllib.error
import urllib.request

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
from gameplay_bot.core import BotFault
from gameplay_bot.fieldkit import FieldKit, CATALOG, make_server, suite_for


class FieldKitTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.kit = FieldKit("game", "proxy", "controls", self.tmp.name, worker=False)
        self.kit.connected = True
        self.kit.observation = dict(id="fresh", time=time.time(), captures=[dict(sha256="actual-image")])

    def tearDown(self):
        self.tmp.cleanup()

    def decision(self, **values):
        return dict(action="run", epoch=self.kit.epoch, scenarios=["binoculars"],
                    observation_id="fresh", reviewed_capture_sha256="actual-image", **values)

    def test_identical_snapshot_for_both_readers(self):
        a = self.kit.snapshot()
        a["observation"]["id"] = "tampered"
        self.assertEqual(self.kit.snapshot()["observation"]["id"], "fresh")

    def test_human_can_run_without_an_llm(self):
        self.assertTrue(self.kit.admit("human", self.decision())["accepted"])
        self.assertTrue(self.kit.busy)
        self.assertEqual(self.kit.commands.get()[0], "run")

    def test_llm_needs_selected_ownership(self):
        with self.assertRaisesRegex(BotFault, "Supervisor changed"):
            self.kit.admit("llm", self.decision())
        self.kit.admit("human", dict(action="set_mode", mode="llm"))
        self.assertTrue(self.kit.admit("llm", self.decision())["accepted"])

    def test_handoff_stops_active_input_and_invalidates_old_decision(self):
        old = self.decision()
        self.kit.admit("human", old)
        self.kit.admit("human", dict(action="set_mode", mode="llm"))
        self.assertTrue(self.kit.cancel.is_set())
        with self.assertRaises(BotFault):
            self.kit.admit("llm", old)
        self.assertTrue(self.kit.admit("human", dict(action="stop"))["accepted"])

    def test_old_or_unreviewed_images_rejected(self):
        for change in (dict(observation_id="old"), dict(reviewed_capture_sha256="invented")):
            request = {**self.decision(), **change}
            with self.assertRaises(BotFault):
                self.kit.admit("human", request)
        self.kit.observation["time"] -= 31
        with self.assertRaisesRegex(BotFault, "30 seconds"):
            self.kit.admit("human", self.decision())

    def test_no_arbitrary_script_or_duplicate_cases(self):
        for identifiers in (["binoculars", "binoculars"], ["../../shell"], ["pause"], [dict(op="exec")]):
            with self.assertRaises(BotFault):
                self.kit.admit("human", {**self.decision(), "scenarios": identifiers})

    def test_catalog_preserves_vr_bindings_and_never_opens_pause(self):
        for entry in CATALOG:
            suite = suite_for(entry["id"])
            self.assertFalse(suite["continue_after_outcome_failure"])
            actions = [step["name"] for case in suite["cases"] for step in case["steps"]]
            self.assertNotIn("system.pause", actions)
            self.assertNotIn("system.toggle_vr", actions)
        optic = suite_for("binoculars")
        self.assertEqual(optic["cases"][0]["steps"][0]["name"], "gameplay.equip_binoculars")
        self.assertEqual(optic["cases"][1]["depends_on"], [optic["cases"][0]["id"]])

    def test_failed_case_halts_queue_and_retains_cleanup_failure(self):
        class Adapter:
            bindings = {}
            def observe(self, **_):
                return {"scene": "gameplay"}
        self.kit.live = Adapter()
        with patch('gameplay_bot.fieldkit.run_suite', return_value={"status":"failed","cases":[]}) as run:
            with patch.object(self.kit, '_cleanup', side_effect=BotFault('release failed')):
                self.kit._run(['equipment', 'commands'])
        self.assertEqual(run.call_count, 1)
        self.assertEqual(self.kit.results[0]['cleanup_error'], 'release failed')
        self.assertFalse(self.kit.results[0]['release_ready'])

    def test_stop_before_queue_does_not_send_any_input(self):
        self.kit.cancel.set()
        with patch('gameplay_bot.fieldkit.run_suite') as run:
            with self.assertRaisesRegex(BotFault, 'Stopped'):
                self.kit._run(['binoculars'])
            run.assert_not_called()

    def test_binocular_cleanup_stows_owned_optic_without_pause(self):
        class Adapter:
            def __init__(self):
                self.state = {"scene":"gameplay", "native":{"mission":10040}, "controls":{"context":"binoculars"}}
                self.actions, self.releases = [], 0
                self.events = type('Events', (), {'emit': lambda *_args, **_kw: None})()
            def release(self):
                self.releases += 1
            def observe(self, **_):
                return self.state
            def execute(self, step):
                self.actions.append(step['name'])
                self.state['controls']['context'] = 'gameplay'
        adapter = self.kit.live = Adapter()
        self.kit._cleanup('binoculars', {"native":{"mission":10040}})
        self.assertEqual(adapter.actions, ['binoculars.stow'])
        self.assertGreaterEqual(adapter.releases, 2)

    def test_cleanup_does_not_stow_another_mission_or_unsolicited_equipment(self):
        class Adapter:
            def release(self): pass
            def observe(self, **_):
                return {"scene":"gameplay", "native":{"mission":20000}, "controls":{"context":"binoculars"}}
            def execute(self, _):
                raise AssertionError('Cross-mission cleanup must not send an action')
        self.kit.live = Adapter()
        self.kit._cleanup('binoculars', {"native":{"mission":10040}})

    def test_archive_reloads_identity_and_rejects_changed_image(self):
        import hashlib
        folder = pathlib.Path(self.tmp.name) / 'saved-run'
        folder.mkdir()
        image = folder / 'left.png'
        image.write_bytes(b'file-identity-fixture')
        record = dict(scenario='equipment', status='observed_pass', finished=1, identity={'dll_sha256':'build'},
                      observation={'captures':[{'path':str(image),'sha256':hashlib.sha256(image.read_bytes()).hexdigest()}]})
        (folder/'result.json').write_text(json.dumps(record))
        reopened = FieldKit('game','proxy','controls',self.tmp.name,worker=False)
        self.assertEqual(len(reopened.results),1)
        self.assertEqual(reopened.results[0]['identity']['dll_sha256'],'build')
        image.write_bytes(b'changed')
        corrupt = FieldKit('game','proxy','controls',self.tmp.name,worker=False)
        self.assertEqual(corrupt.results,[])

    def test_local_http_auth_origin_same_payload_and_path_bounds(self):
        server, conn = make_server(self.kit, 0)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        def get(path, role=None, origin=None):
            headers = {}
            if role:
                headers["Authorization"] = "Bearer " + conn["tokens"][role]
            if origin:
                headers["Origin"] = origin
            with urllib.request.urlopen(urllib.request.Request(conn["url"]+path, headers=headers)) as response:
                return json.load(response)
        try:
            self.assertEqual(get("/api/state", "human"), get("/api/state", "llm"))
            for path, role, origin, code in [("/api/state", None, None, 401), ("/api/state", "human", "http://evil.invalid", 403),
                                              ("/media/../../AGENTS.md", "human", None, 404)]:
                with self.assertRaises(urllib.error.HTTPError) as caught:
                    get(path, role, origin)
                self.assertEqual(caught.exception.code, code)
        finally:
            server.shutdown()
            server.server_close()
            thread.join()


if __name__ == "__main__":
    unittest.main()
