"""Behavior failure tests. These fixtures do not claim native gameplay passed."""
import json
import copy
import pathlib
import struct
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
from gameplay_bot.core import ActionPrerequisiteChanged, Behaviors, BlankCompositorFrame, BotFault, Events, ProgressGuard, astar, atomic_json, matches, scene, reviewed_action_contract
from gameplay_bot.live import Live, NativeReader, authoritative_context, channel, fresh_control_observation, held_input_evidence, released_startup_observation, require_presentation_transition, rotate, static_grip_capture_allowed
from gameplay_bot.recording import Recording, after_verified_arrival, mux_timeline, validate_raw_video
from gameplay_bot.session import run_suite
from gameplay_bot.startup import advance_startup, capture_startup_baseline, wait_for_continue_rack, capture_continue_rack
from gameplay_bot.startup_evidence import StartupEvidence
from gameplay_bot.menus import navigate as navigate_menu, observe as observe_menu
from gameplay_bot.optic_exposure import require_independent_luminance
from operator_recovery_tests import RecoveryTests, LiveRecoveryIntegrationTests
from gameplay_motion_tests import MotionTests


class NativeRecordingLedgerTests(unittest.TestCase):
    @staticmethod
    def fixture():
        origin = 1_000_000_000
        capture_slots = (2, 3, 6, 7)
        samples = [{"qpc_100ns": origin + (slot * 10_000_000 + 29) // 30 + jitter,
                    "capture_slot": slot, "encoded_slot": slot - 2, "repeats_before": fill}
                   for slot, jitter, fill in zip(capture_slots, (10000, 20000, 5000, 19000), (0, 0, 2, 0))]
        return {"schema": 2, "complete": True, "frames": 4, "dropped": 6,
                "first_qpc_100ns": samples[0]["qpc_100ns"],
                "last_qpc_100ns": samples[-1]["qpc_100ns"],
                "capture_schedule_origin_qpc_100ns": origin, "scheduled_slots": 10,
                "missed_source_cadence_slots": 2, "queue_dropped_samples": 2,
                "staging_busy_samples": 1, "staging_discarded_samples": 1,
                "encoded_frames": 6, "repeated_frames": 2, "accepted_samples": samples}

    def test_source_ledger_explains_exact_cfr_slots_and_preserves_copy_clock(self):
        stats = validate_raw_video(self.fixture(), 6, .2, 30.)
        self.assertTrue(stats["native_source_timestamp_ledger_validated"])
        self.assertEqual(stats["cfr_repeat_or_fill_frame_count"], 2)
        self.assertEqual(stats["longest_explicit_cfr_repeat_run"], 2)
        self.assertEqual(stats["native_capture_scheduled_slot_count"], 10)
        self.assertAlmostEqual(stats["native_max_accepted_sample_gap_seconds"], .0985)

    def test_unreported_container_repeat_is_rejected_even_with_large_loss_total(self):
        with self.assertRaisesRegex(BotFault, "ledger"):
            validate_raw_video(self.fixture(), 7, 7 / 30., 30.)

    def lineage_fixture(self):
        video = self.fixture()
        video.update(source_lineage_schema=1, source_sample_clock="steady_ms", eye=1)
        for item, sequence in zip(video["accepted_samples"], (90, 90, 92, 93)):
            item.update(image_epoch=4, image_sequence=sequence, source_sequence=sequence+100,
                        tracking_sequence=sequence+200, activation=2, source_sample_ms=sequence+300,
                        source_eye=1, source_projected=True, source_joined=True)
        return video

    def test_duplicate_source_images_are_counted_separately_from_cfr_fill(self):
        result = validate_raw_video(self.lineage_fixture(), 6, .2, 30.)
        self.assertTrue(result["native_source_lineage_validated"])
        self.assertEqual(result["native_unique_image_transactions"], 3)
        self.assertEqual(result["native_duplicate_image_captures"], 1)
        self.assertEqual(result["cfr_repeat_or_fill_frame_count"], 2)

    def test_source_lineage_rejects_replacement_reordering_wrong_eye_and_missing_identity(self):
        for index, field, value in ((1, "tracking_sequence", 999), (2, "image_sequence", 89),
                                    (2, "image_epoch", 3), (1, "source_eye", 0),
                                    (1, "image_epoch", 0), (1, "source_sample_ms", None)):
            with self.subTest(field=field):
                video = self.lineage_fixture()
                video["accepted_samples"][index][field] = value
                with self.assertRaisesRegex(BotFault, "ledger"):
                    validate_raw_video(video, 6, .2, 30.)

    def test_source_lineage_keeps_epoch_reset_distinct_and_rejects_unknown_clock(self):
        video = self.lineage_fixture()
        video["accepted_samples"][-1].update(image_epoch=5, image_sequence=1)
        self.assertEqual(validate_raw_video(video, 6, .2, 30.)["native_unique_image_transactions"], 3)
        video["source_sample_clock"] = "qpc_100ns"
        with self.assertRaisesRegex(BotFault, "ledger"):
            validate_raw_video(video, 6, .2, 30.)

    def test_source_order_timestamp_slot_and_repeat_changes_are_rejected(self):
        for key, value in (("qpc_100ns", 1_000_000_000), ("capture_slot", 4),
                           ("encoded_slot", 2), ("repeats_before", 1)):
            with self.subTest(key=key):
                video = self.fixture()
                video["accepted_samples"][1][key] = value
                with self.assertRaisesRegex(BotFault, "ledger"):
                    validate_raw_video(video, 6, .2, 30.)
        video = self.fixture()
        video["accepted_samples"][1:3] = reversed(video["accepted_samples"][1:3])
        with self.assertRaisesRegex(BotFault, "ledger"):
            validate_raw_video(video, 6, .2, 30.)

    def test_all_loss_categories_and_source_ledger_length_are_accounted(self):
        for key in ("missed_source_cadence_slots", "queue_dropped_samples",
                    "staging_busy_samples", "staging_discarded_samples", "scheduled_slots",
                    "encoded_frames", "repeated_frames"):
            with self.subTest(key=key):
                video = self.fixture()
                video[key] += 1
                with self.assertRaisesRegex(BotFault, "ledger"):
                    validate_raw_video(video, 6, .2, 30.)
        video = self.fixture()
        video["accepted_samples"].pop()
        with self.assertRaisesRegex(BotFault, "ledger"):
            validate_raw_video(video, 6, .2, 30.)

    def test_wrong_actual_frame_rate_and_nonfinite_duration_are_rejected(self):
        for duration, fps in ((.2, 29.97), (float("nan"), 30.), (.2, float("inf"))):
            with self.subTest(duration=duration, fps=fps), self.assertRaises(BotFault):
                validate_raw_video(self.fixture(), 6, duration, fps)

    def test_reproduced_c960_legacy_failure_is_not_retroactively_admitted(self):
        video = {"frames": 667, "dropped": 16, "first_qpc_100ns": 868114717742,
                 "last_qpc_100ns": 868345073807, "complete": True}
        with self.assertRaisesRegex(BotFault, "accepted and dropped"):
            validate_raw_video(video, 692, 23.066633, 30.)


class NativePopupTransportTests(unittest.TestCase):
    def test_fast_state_transport_preserves_unknown_closed_and_current_popup_values(self):
        for active, numeric, string_id, last_result in ((None,None,None,None),
                                                       (False,None,None,-1),
                                                       (True,0,'0xffffffffffffffff',2)):
            with self.subTest(active=active):
                observer={'reader_verified':active is not None,'owner_verified':active is not None,
                          'sample_ms':99,'coherent_frame':False,'active':active,
                          'numeric_id':numeric,'string_id':string_id,'last_result':last_result}
                body=json.dumps({'schema':1,'now_ms':100,'popup_observer':observer}).encode('utf-8')
                reader=NativeReader.__new__(NativeReader)
                reader.timeout=2.
                reader.module=mock.MagicMock(REQUEST=struct.Struct('<QI'),RESPONSE=struct.Struct('<QiIQQ'),MAX_SCRIPT=1024*1024)
                reader.module.read_exact.side_effect=[reader.module.RESPONSE.pack(42,0,len(body),0,1),body]
                with mock.patch('gameplay_bot.live.time.time_ns',return_value=42):
                    state, _=reader.read('inspect-bot-state')
                self.assertEqual(state['popup_observer'],observer)
                self.assertIs(state['popup_observer']['active'],active)
                self.assertNotIn('selection',state['popup_observer'])
                stream=reader.module.connect.return_value.__enter__.return_value
                stream.write.assert_called_once_with(reader.module.REQUEST.pack(42,len(b'inspect-bot-state'))+b'inspect-bot-state')

    def test_fresh_observation_retains_popup_diagnostics_without_queueing_lua(self):
        observer={'reader_verified':True,'owner_verified':True,'sample_ms':99,
                  'coherent_frame':False,'active':False,'numeric_id':None,'string_id':None,'last_result':2}
        packet=json.loads(json.dumps({'now_ms':100,'title':False,'loading':False,'demo':False,
                                     'menu':False,'camera_active':True,'camera_available':True,
                                     'popup_observer':observer}))
        live=Live.__new__(Live)
        live.background_check=None;live.kernel=mock.Mock();live.kernel.WaitForSingleObject.return_value=258
        live.process_handle=1;live.last_tick=None;live.events=mock.Mock();live.native=mock.Mock()
        live.native.read.return_value=(packet,{})
        state=live.observe()
        self.assertEqual(state['popup_observer'],observer)
        live.native.read.assert_called_once_with('inspect-bot-state')
        self.assertEqual(state['scene'],'gameplay')


class NativeStartupObservationTests(unittest.TestCase):
    def reader(self):
        reader = NativeReader.__new__(NativeReader)
        reader.timeout = 2.
        reader._readonly_scripts = frozenset(('inspect-bot-state', 'authored observation'))
        reader.module = mock.MagicMock(REQUEST=struct.Struct('<QI'), RESPONSE=struct.Struct('<QiIQQ'), MAX_SCRIPT=1024*1024)
        return reader

    def state(self):
        return {'now_ms': 100, 'scene': 'title', 'title': True, 'title_menu': False,
                'camera_active': False, 'menu': False, 'idroid': False, 'pause': False,
                'transport': {'seconds': .01},
                'controls': {'context': 'menus', 'sample_ms': 90, 'age_ms': 10,
                             'native_buttons': 0, 'physical': [0]*11, 'sticks': [0]*4,
                             'native_axes': [0]*4, 'native_triggers': [0]*2,
                             'touches': {str(index): 0 for index in range(10)}}}

    def test_slow_known_observation_has_one_correlated_request_and_shared_bounded_deadline(self):
        reader = self.reader()
        body = b'{"sequence":"Seq_Demo_StartHasTitleMission"}'
        reader.module.read_exact.side_effect = [reader.module.RESPONSE.pack(42, 0, len(body), 3000, 1234), body]
        with mock.patch('gameplay_bot.live.time.time_ns', return_value=42), \
             mock.patch('gameplay_bot.live.time.monotonic', side_effect=[10., 10., 13.1]):
            state, timing = reader.read_startup_observation('authored observation')
        self.assertEqual(state['sequence'], 'Seq_Demo_StartHasTitleMission')
        self.assertAlmostEqual(timing['seconds'], 3.1)
        self.assertTrue(timing['readonly_startup'])
        reader.module.connect.assert_called_once_with(2.)
        pipe = reader.module.connect.return_value.__enter__.return_value
        pipe.write.assert_called_once_with(reader.module.REQUEST.pack(42, len(b'authored observation')) + b'authored observation')
        self.assertEqual([call.args[2] for call in reader.module.read_exact.call_args_list], [18., 18.])

    def test_unknown_script_and_mutation_cannot_enter_long_observation_path(self):
        for script in ('input:a', 'diagnostics:optic-lens-render:off', 'Player.SetLife(1)', 'authored observation '):
            with self.subTest(script=script):
                reader = self.reader()
                with self.assertRaisesRegex(BotFault, 'exact known read-only'):
                    reader.read_startup_observation(script)
                reader.module.connect.assert_not_called()

    def test_action_timeout_keeps_short_deadline_and_unknown_completion(self):
        reader = self.reader()
        reader.module.read_exact.side_effect = RuntimeError('native action response timed out; action completion is unknown')
        with mock.patch('gameplay_bot.live.time.monotonic', return_value=10.):
            with self.assertRaisesRegex(RuntimeError, 'action completion is unknown'):
                reader.read('input:a')
        self.assertEqual(reader.module.read_exact.call_args.args[2], 12.)
        reader.module.connect.assert_called_once()
        reader.module.connect.return_value.__enter__.return_value.write.assert_called_once()

    def test_startup_header_or_body_timeout_never_resubmits_or_extends_again(self):
        timeout = RuntimeError('native action response timed out; action completion is unknown')
        for responses in ([timeout], [struct.pack('<QiIQQ', 42, 0, 20, 0, 0), timeout]):
            with self.subTest(body=len(responses) == 2):
                reader = self.reader()
                reader.module.read_exact.side_effect = responses
                with mock.patch('gameplay_bot.live.time.time_ns', return_value=42), \
                     mock.patch('gameplay_bot.live.time.monotonic', return_value=10.):
                    with self.assertRaisesRegex(BotFault, 'read-only startup observation timed out.*request not replayed'):
                        reader.read_startup_observation('authored observation')
                reader.module.connect.assert_called_once()
                reader.module.connect.return_value.__enter__.return_value.write.assert_called_once()
                self.assertTrue(all(call.args[2] == 18. for call in reader.module.read_exact.call_args_list))

    def test_late_observation_still_rejects_wrong_response_identity(self):
        reader = self.reader()
        reader.module.read_exact.return_value = reader.module.RESPONSE.pack(43, 0, 1, 0, 0)
        with mock.patch('gameplay_bot.live.time.time_ns', return_value=42):
            with self.assertRaisesRegex(BotFault, 'identity/size mismatch'):
                reader.read_startup_observation('authored observation')
        reader.module.read_exact.assert_called_once()

    def test_long_read_requires_fresh_sampled_neutral_title_owner(self):
        self.assertTrue(released_startup_observation(self.state(), {}))
        self.assertFalse(released_startup_observation(self.state(), {'a': 1}))
        changes = [('scene', 'gameplay'), ('camera_active', True), ('title_menu', True),
                   ('menu', True), ('idroid', True), ('pause', True), ('title', None)]
        for key, value in changes:
            state = self.state(); state[key] = value
            with self.subTest(key=key):
                self.assertFalse(released_startup_observation(state, {}))
        for key in ('physical', 'sticks', 'native_axes', 'native_triggers'):
            state = self.state(); state['controls'][key][0] = .1
            with self.subTest(key=key):
                self.assertFalse(released_startup_observation(state, {}))
        state = self.state(); state['controls']['touches']['0'] = 1
        self.assertFalse(released_startup_observation(state, {}))
        state = self.state(); state['transport']['seconds'] = .3
        self.assertFalse(released_startup_observation(state, {}))

    def test_live_uses_readonly_path_without_refreshing_an_aged_control_snapshot(self):
        live = Live.__new__(Live)
        live.background_check = None; live.held = {}; live.process_handle = 1; live.last_tick = None
        live.kernel = mock.Mock(); live.kernel.WaitForSingleObject.return_value = 258
        live.events = mock.Mock(); live.startup_evidence = mock.Mock(); live.startup_evidence.read.return_value = {}
        live.lua_script = 'authored observation'; live.native = mock.Mock(); live.native.timeout = 2.
        packet = self.state()
        live.native.read.return_value = (packet, {'seconds': .01})
        live.native.read_startup_observation.return_value = ({'sequence': 'Seq_Demo_StartHasTitleMission'}, {'seconds': 3.})
        observed = live.observe(native=True)
        live.native.read.assert_called_once_with('inspect-bot-state')
        live.native.read_startup_observation.assert_called_once_with('authored observation')
        self.assertEqual(observed['now_ms'], 100)
        self.assertEqual(observed['controls']['sample_ms'], 90)
        self.assertEqual(observed['controls']['age_ms'], 10)
        self.assertEqual(observed['lua_transport']['seconds'], 3.)
        self.assertTrue(any(call.args[0] == 'startup_observation_wait' for call in live.events.emit.call_args_list))


class OpticExposureOwnershipTests(unittest.TestCase):
    def sample(self):
        return {'head_readbacks':['0x10','0x20','0x30','0x40'],
                'lens_readbacks':['0x50','0x60','0x70','0x80'],
                'head_luminance_textures':['0x100','0x200','0x300','0x400'],
                'lens_luminance_textures':['0x500','0x600','0x700','0x800']}

    def test_independent_gpu_and_cpu_rings_are_required(self):
        require_independent_luminance(self.sample())
        shared=self.sample()
        shared['lens_luminance_textures']=list(shared['head_luminance_textures'])
        with self.assertRaises(BotFault):require_independent_luminance(shared)

    def test_incomplete_or_aliasing_ownership_cannot_certify_the_fix(self):
        for name in ('lens_readbacks','lens_luminance_textures'):
            for invalid in ([],['0x0']*4,['0x500']*4,['missing']*4,None):
                with self.subTest(name=name,invalid=invalid):
                    state=self.sample();state[name]=invalid
                    with self.assertRaises(BotFault):require_independent_luminance(state)


class Clock:
    def __init__(self):
        self.now = 0.
    def __call__(self):
        return self.now
    def sleep(self, duration):
        self.now += duration


class AtomicStatusTests(unittest.TestCase):
    def test_transient_windows_sharing_conflict_preserves_then_replaces_complete_status(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory)/"status.json"
            atomic_json(path, {"generation":1})
            replace = pathlib.Path.replace
            failures = []
            def sharing_conflict(source, destination):
                if len(failures) < 2:
                    self.assertEqual(json.loads(path.read_text()), {"generation":1})
                    error = PermissionError("sharing conflict"); error.winerror = 5
                    failures.append(error); raise error
                return replace(source, destination)
            with mock.patch.object(pathlib.Path, "replace", autospec=True, side_effect=sharing_conflict):
                atomic_json(path, {"generation":2})
            self.assertEqual(json.loads(path.read_text()), {"generation":2})
            self.assertEqual(list(path.parent.glob("*.tmp")), [])

    def test_persistent_status_failure_is_bounded_and_keeps_previous_document(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory)/"status.json"
            atomic_json(path, {"generation":1})
            error = PermissionError("still locked"); error.winerror = 32
            with mock.patch.object(pathlib.Path, "replace", side_effect=error) as replace:
                with mock.patch("gameplay_bot.core.time.monotonic", side_effect=[0.,.6]):
                    with self.assertRaises(PermissionError):
                        atomic_json(path, {"generation":2})
            replace.assert_called_once()
            self.assertEqual(json.loads(path.read_text()), {"generation":1})
            self.assertEqual(list(path.parent.glob("*.tmp")), [])

    @unittest.skipUnless(sys.platform == "win32", "Real Windows delete-sharing contract")
    def test_real_windows_reader_does_not_abort_an_atomic_status_update(self):
        import ctypes, threading, time
        from ctypes import wintypes
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.CreateFileW.argtypes = [wintypes.LPCWSTR,wintypes.DWORD,wintypes.DWORD,
                                      wintypes.LPVOID,wintypes.DWORD,wintypes.DWORD,wintypes.HANDLE]
        kernel.CreateFileW.restype = wintypes.HANDLE
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory)/"status.json"
            atomic_json(path, {"generation":1})
            handle = kernel.CreateFileW(str(path),0x80000000,3,None,3,0x80,None)
            self.assertNotEqual(handle, ctypes.c_void_p(-1).value)
            timer = threading.Timer(.08, lambda: kernel.CloseHandle(handle))
            timer.start()
            try:
                atomic_json(path, {"generation":2})
            finally:
                timer.join()
            self.assertEqual(json.loads(path.read_text()), {"generation":2})
            self.assertEqual(list(path.parent.glob("*.tmp")), [])


class ControllerFitPoseTests(unittest.TestCase):
    def make_live(self):
        live = Live.__new__(Live)
        live.call = mock.Mock()
        live.events = mock.Mock()
        live.pose_recording = False
        return live

    def test_distinct_controller_grip_and_pointing_bases_reach_operator(self):
        live = self.make_live()
        live.execute({"op": "pose", "hand": "right", "position": [0, -.2, -.5],
                      "orientation": [.5, 0, 0, .8660254], "aim_orientation": [0, 0, 0, 1]})
        calls = live.call.call_args_list
        self.assertEqual(calls[0].args[1]["orientation"], [.5, 0, 0, .8660254])
        self.assertEqual(calls[1].args[1]["orientation"], [0, 0, 0, 1])
        self.assertEqual(calls[1].args[1]["pose_type"], "aim")

    def test_invalid_aim_cannot_partially_move_the_grip(self):
        for aim in ([0, 0, 0, 0], [0, 0, 0, float('nan')], [True, 0, 0, 1], [0, 0, 1]):
            live = self.make_live()
            with self.assertRaises(BotFault):
                live.execute({"op": "pose", "hand": "right", "position": [0, -.2, -.5],
                              "orientation": [0, 0, 0, 1], "aim_orientation": aim})
            live.call.assert_not_called()

    def test_opening_outcome_must_remain_stable_before_visual_capture(self):
        closed = {"scene": "gameplay", "idroid": False}
        opened = {"scene": "menu", "idroid": True}
        adapter = Adapter([closed]*2 + [opened]*6 + [closed] + [opened]*12)
        clock = Clock()
        with tempfile.TemporaryDirectory() as directory:
            behavior = Behaviors(adapter, Events(directory, {}, clock), clock, clock.sleep)
            result = behavior.case({"id": "settled-open", "before": {"scene": "gameplay"},
                                    "steps": [{"op": "action", "name": "system.idroid"}],
                                    "after": {"idroid": True}, "after_stable_samples": 12, "timeout": 4})
        self.assertEqual(result["status"], "observed_pass")
        self.assertGreater(clock.now, 1.8)


class TrackingLossFixtureTests(unittest.TestCase):
    def make_live(self, state=None):
        live = Live.__new__(Live)
        live.held = {}
        live.observe = mock.Mock(return_value=state or {
            "scene": "gameplay", "camera_active": True, "camera_suspended": False,
            "rendered": {"rig_sequence": 42}})
        live.native = mock.Mock(spec=NativeReader)
        live.native.read.return_value = ({"enabled": True, "armed": True, "mask": 4}, {"seconds": .01})
        live.events = mock.Mock()
        live.call = mock.Mock()
        live.input = mock.Mock()
        return live

    def test_valid_bounds_arm_only_the_explicit_native_fixture(self):
        for mask, milliseconds in ((0, 100), (1, 8000), (4, 500), (15, 100)):
            for state in ({"scene": "gameplay"}, {"scene": "cabin"},
                          {"scene": "menu", "idroid": True}):
                with self.subTest(mask=mask, milliseconds=milliseconds, state=state):
                    live = self.make_live()
                    live.observe.return_value.update(state)
                    response = {"enabled": True, "armed": True, "mask": mask}
                    live.native.read.return_value = (response, {"seconds": .01})
                    live.execute({"op": "tracking_loss", "mask": mask, "milliseconds": milliseconds})
                    live.native.read.assert_called_once_with(f"diagnostics:controller-tracking-loss:{mask}:{milliseconds}")
                    event = live.events.emit.call_args
                    self.assertEqual(event.args, ("synthetic_tracking_loss",))
                    self.assertEqual(event.kwargs["response"], response)
                    self.assertEqual(event.kwargs["mask"], mask)
                    self.assertEqual(event.kwargs["milliseconds"], milliseconds)
                    self.assertIn("not physical Quest 3", event.kwargs["evidence_limit"])
                    live.call.assert_not_called()
                    live.input.assert_not_called()

    def test_invalid_mask_duration_or_held_input_reject_before_observation(self):
        invalid = [(value, 500) for value in (None, -1, 16, True, 4.0, "4")]
        invalid += [(4, value) for value in (None, 99, 8001, False, 500.0, "500")]
        for mask, milliseconds in invalid:
            with self.subTest(mask=mask, milliseconds=milliseconds):
                live = self.make_live()
                with self.assertRaisesRegex(BotFault, "released input.*bounded valid mask"):
                    live.execute({"op": "tracking_loss", "mask": mask, "milliseconds": milliseconds})
                live.observe.assert_not_called()
                live.native.read.assert_not_called()
                live.events.emit.assert_not_called()
        live = self.make_live()
        live.held = {("right", "A", None): channel("a")}
        with self.assertRaisesRegex(BotFault, "released input"):
            live.execute({"op": "tracking_loss", "mask": 4, "milliseconds": 500})
        live.observe.assert_not_called()
        live.native.read.assert_not_called()

    def test_unowned_inactive_suspended_or_unpublished_rig_cannot_arm(self):
        changes = [{"scene": name} for name in ("title", "loading", "demo", "unknown", "menu")]
        changes += [{"scene": "menu", "idroid": True, flag: True}
                    for flag in ("pause", "title", "loading", "demo")]
        changes += [{"camera_active": False}, {"camera_active": None}, {"camera_suspended": True},
                    {"rendered": {}}, {"rendered": {"rig_sequence": 0}},
                    {"rendered": {"rig_sequence": None}}]
        for change in changes:
            with self.subTest(change=change):
                live = self.make_live()
                live.observe.return_value.update(change)
                with self.assertRaisesRegex(BotFault, "current accepted playable rig"):
                    live.execute({"op": "tracking_loss", "mask": 4, "milliseconds": 500})
                live.native.read.assert_not_called()
                live.events.emit.assert_not_called()
                live.input.assert_not_called()

    def test_disabled_unarmed_or_wrong_mask_response_never_claims_success(self):
        responses = ({}, {"enabled": True, "mask": 4}, {"armed": True, "mask": 4},
                     {"enabled": False, "armed": True, "mask": 4},
                     {"enabled": True, "armed": False, "mask": 4},
                     {"enabled": True, "armed": True, "mask": 1})
        for response in responses:
            with self.subTest(response=response):
                live = self.make_live()
                live.native.read.return_value = (response, {})
                with self.assertRaisesRegex(BotFault, "diagnostic was not armed"):
                    live.execute({"op": "tracking_loss", "mask": 4, "milliseconds": 500})
                live.native.read.assert_called_once()
                live.events.emit.assert_not_called()
                live.input.assert_not_called()


class MenuNavigationTests(unittest.TestCase):
    def fixture(self, source="left_stick", fault=None):
        clock = Clock()
        live = mock.Mock()
        live.bindings = {"axes": [{"name": "axes.menu", "source": source}]}
        active = []
        def release():
            active.clear()
        def input_(values, seconds, **kwargs):
            active[:] = values
            if fault == "rpc":
                raise TimeoutError("operator failure")
        def state(native=False):
            sticks = [0.] * 4
            if active and fault != "unsampled":
                value = active[0]
                axis = (0 if value["hand"] == "left" else 2) + (value["sub_component"] == "Y")
                sticks[axis] = value["value"]
            if not active and fault == "stuck_release" and live.input.called:
                sticks[1] = -.8
            return {"idroid": fault != "owner", "idroid_menu_input_ready": True, "idroid_handheld":True, "pause":False,
                    "controls": {"context": "menus", "age_ms": 0,
                                 "sample_ms": int(clock.now * 1000), "sticks": sticks,
                                 "physical": [0.] * 11}}
        live.input.side_effect = input_
        live.release.side_effect = release
        live.observe.side_effect = state
        return live, clock, active

    def test_effective_right_stick_moves_only_the_mapped_menu_axis(self):
        live, clock, active = self.fixture("right_stick")
        navigate_menu(live, {"direction": "down"}, clock=clock, sleep=clock.sleep)
        payload, = live.input.call_args.args[0]
        self.assertEqual(payload, {"hand": "right", "component": "Thumbstick", "sub_component": "Y", "value": -1.})
        self.assertEqual(active, [])
        self.assertGreaterEqual(clock.now, .10)
        self.assertLess(clock.now, .20)
        live.capture.assert_not_called()

    def test_idroid_cursor_edge_releases_before_native_list_repeat(self):
        live, clock, active = self.fixture()
        original = live.observe.side_effect
        started, changes = [None], []
        def state(native=False):
            if active:
                if started[0] is None:
                    started[0] = clock.now
                    changes.append("first row")
                elif clock.now - started[0] >= .15:
                    changes.append("repeated row")
            return original(native)
        live.observe.side_effect = state
        navigate_menu(live, {"direction":"down"}, clock=clock, sleep=clock.sleep)
        self.assertEqual(changes, ["first row"])
        self.assertEqual(active, [])

    def test_spatial_idroid_requires_explicit_mode_and_samples_effective_axis(self):
        live, clock, active = self.fixture("right_stick")
        original = live.observe.side_effect
        live.observe.side_effect = lambda native=False: {**original(native),
            "idroid_handheld":False, "idroid_menu_input_ready":False,
            "native":{"mission":40010,"popup":False}}
        result = navigate_menu(live, {"direction":"down", "native_before":{"mission":40010,"popup":False}},
                               clock=clock, sleep=clock.sleep)
        self.assertEqual(live.input.call_args.args[0], [{"hand":"right", "component":"Thumbstick",
                                                       "sub_component":"Y", "value":-1.}])
        self.assertFalse(result["idroid_handheld"])
        self.assertFalse(result["idroid_menu_input_ready"])
        self.assertEqual(active, [])
        self.assertTrue(any(call.args[0] == "menu_navigation_sampled" for call in live.events.emit.call_args_list))
        self.assertTrue(any(call.args[0] == "menu_navigation_released" for call in live.events.emit.call_args_list))

    def test_idroid_unknown_mode_or_inconsistent_readiness_never_dispatches(self):
        for mode, ready in ((None,False),(None,True),(True,False),(False,True),(False,None),(0,False),("false",False)):
            with self.subTest(mode=mode, ready=ready):
                live, clock, _ = self.fixture()
                original = live.observe.side_effect
                live.observe.side_effect = lambda native=False: {**original(native),
                    "idroid_handheld":mode, "idroid_menu_input_ready":ready}
                with self.assertRaisesRegex(BotFault, "native input owner"):
                    navigate_menu(live, {"direction":"down"}, clock=clock, sleep=clock.sleep)
                live.input.assert_not_called()

    def test_spatial_idroid_rejects_mode_change_or_pause_and_releases_input(self):
        for when in ("admission", "held", "released"):
            for fault in ("mode", "pause", "readiness"):
                with self.subTest(when=when, fault=fault):
                    live, clock, active = self.fixture()
                    original = live.observe.side_effect
                    def state(native=False):
                        value = {**original(native), "idroid_handheld":False, "idroid_menu_input_ready":False}
                        failing = when == "admission" or (when == "held" and bool(active)) or (when == "released" and live.input.called and not active)
                        if failing:
                            if fault == "mode":
                                value["idroid_handheld"] = None if when == "admission" else True
                                value["idroid_menu_input_ready"] = True
                            elif fault == "pause":
                                value["pause"] = True
                            else:
                                value["idroid_menu_input_ready"] = None
                        return value
                    live.observe.side_effect = state
                    with self.assertRaisesRegex(BotFault, "native input owner"):
                        navigate_menu(live, {"direction":"down"}, clock=clock, sleep=clock.sleep)
                    self.assertEqual(active, [])
                    self.assertEqual(live.input.call_count, 0 if when == "admission" else 1)

    def test_idroid_scroll_hold_is_explicit_and_bounded(self):
        live, clock, active = self.fixture()
        navigate_menu(live, {"direction":"down", "mode":"hold", "seconds":.16},
                      clock=clock, sleep=clock.sleep)
        self.assertGreaterEqual(clock.now, .28)
        self.assertEqual(active, [])

    def test_idroid_native_guard_is_checked_before_dispatch(self):
        live, clock, _ = self.fixture()
        original = live.observe.side_effect
        def state(native=False):
            return {**original(native), "native":{"mission":10040,"popup":False}}
        live.observe.side_effect = state
        with self.assertRaisesRegex(BotFault, "native scene prerequisite"):
            navigate_menu(live, {"direction":"down", "native_before":{"mission":40010}},
                          clock=clock, sleep=clock.sleep)
        live.input.assert_not_called()

    def test_idroid_navigation_rejects_overlapping_or_unidentified_pause_owner(self):
        for paused in (True, None):
            live, clock, _ = self.fixture()
            original = live.observe.side_effect
            live.observe.side_effect = lambda native=False: {**original(native), "pause":paused}
            with self.assertRaisesRegex(BotFault, "native input owner"):
                navigate_menu(live, {"direction":"down"}, clock=clock, sleep=clock.sleep)
            live.input.assert_not_called()

    def test_idroid_native_guard_is_rechecked_after_release_without_held_lua_reads(self):
        live, clock, active = self.fixture()
        original = live.observe.side_effect
        held_native_reads = []
        def state(native=False):
            if native and active:
                held_native_reads.append(True)
            value = original(native)
            value["native"] = {"mission":10040 if live.input.called and not active else 40010}
            return value
        live.observe.side_effect = state
        with self.assertRaisesRegex(BotFault, "native scene prerequisite"):
            navigate_menu(live, {"direction":"down", "native_before":{"mission":40010}},
                          clock=clock, sleep=clock.sleep)
        self.assertEqual(held_native_reads, [])
        self.assertEqual(active, [])

    def pause_fixture(self, change=None):
        live, clock, active = self.fixture("right_stick")
        original = live.observe.side_effect
        def state(native=False):
            value = original(native)
            value.update(scene="menu", menu=True, pause=True, idroid=False,
                         title=False, loading=False, native={"mission":10040, "popup":False})
            if change:
                change(value, bool(active))
            return value
        live.observe.side_effect = state
        return live, clock, active

    def test_pause_navigation_uses_effective_axis_and_checks_native_owner(self):
        live, clock, active = self.pause_fixture()
        navigate_menu(live, {"owner":"pause", "direction":"down",
                            "native_before":{"mission":10040}}, clock=clock, sleep=clock.sleep)
        payload, = live.input.call_args.args[0]
        self.assertEqual(payload["hand"], "right")
        self.assertEqual(payload["component"], "Thumbstick")
        self.assertEqual(active, [])
        self.assertTrue(live.observe.call_args_list[0].kwargs.get("native"))
        self.assertTrue(live.observe.call_args_list[-1].kwargs.get("native"))
        self.assertTrue(any(not call.kwargs.get("native") for call in live.observe.call_args_list))

    def help_fixture(self, source="left_stick", change=None):
        live, clock, active = self.fixture(source)
        live.supervised = True
        state_guard = {"scene":"menu", "menu":True, "idroid":True, "pause":True,
                       "idroid_menu_input_ready":True, "title":False, "loading":False,
                       "controls.context":"menus", "controls.native_buttons":0,
                       "popup_observer.reader_verified":True, "popup_observer.owner_verified":True,
                       "popup_observer.active":False}
        native_guard = {"mission":40010, "sequence":"Seq_Game_MainGame", "helicopter_space":True,
                        "title":False, "popup":False, "tutorial_pause":False, "saving":False}
        original = live.observe.side_effect
        def state(native=False):
            value = original(native)
            value.update(scene="menu", menu=True, pause=True, title=False, loading=False,
                         native=dict(native_guard), now_ms=int(clock.now * 1000))
            value["controls"]["native_buttons"] = 0
            value["popup_observer"] = {"reader_verified":True, "owner_verified":True,
                                       "active":False, "sample_ms":value["now_ms"]}
            if change:
                change(value, bool(active), live.input.called)
            return value
        live.observe.side_effect = state
        return live, clock, active, {"owner":"mother_base_help", "direction":"right",
                                    "state_before":state_guard, "native_before":native_guard}

    def test_reviewed_help_uses_effective_axis_and_releases_without_held_lua(self):
        for source in ("left_stick", "right_stick"):
            with self.subTest(source=source):
                live, clock, active, step = self.help_fixture(source)
                original = live.observe.side_effect
                held_native_reads = []
                def state(native=False):
                    if native and active:
                        held_native_reads.append(True)
                    return original(native)
                live.observe.side_effect = state
                result = navigate_menu(live, step, clock=clock, sleep=clock.sleep)
                self.assertEqual(live.input.call_args.args[0][0]["hand"], source.split("_")[0])
                self.assertTrue(result["pause"])
                self.assertTrue(result["idroid"])
                self.assertEqual(active, [])
                self.assertEqual(held_native_reads, [])
                self.assertTrue(live.observe.call_args_list[-1].kwargs.get("native"))

    def test_help_requires_explicit_supervised_review_and_every_scene_guard(self):
        live, clock, _, step = self.help_fixture()
        live.supervised = False
        with self.assertRaisesRegex(BotFault, "supervised reviewed Help"):
            navigate_menu(live, step, clock=clock, sleep=clock.sleep)
        live.input.assert_not_called()
        for group in ("state_before", "native_before"):
            _, _, _, template = self.help_fixture()
            for key in template[group]:
                for variant in ("missing", "unknown", "wrong_type"):
                    with self.subTest(group=group, key=key, variant=variant):
                        live, clock, _, step = self.help_fixture()
                        if variant == "missing":
                            del step[group][key]
                        else:
                            value = step[group][key]
                            step[group][key] = None if variant == "unknown" else [value] if isinstance(value, str) else str(value)
                        with self.assertRaisesRegex(BotFault, "complete ACC prerequisites"):
                            navigate_menu(live, step, clock=clock, sleep=clock.sleep)
                        live.input.assert_not_called()

    def test_help_owner_changes_stop_before_input_or_after_release(self):
        changes = (("pause",False),("idroid",False),("menu",False),("loading",True),
                   ("idroid_menu_input_ready",False),("native.saving",True),
                   ("native.popup",True),("native.tutorial_pause",None),
                   ("native.sequence","Seq_Game_WeaponCustomize"))
        for field, replacement in changes:
            for after_dispatch in (False, True):
                with self.subTest(field=field, after_dispatch=after_dispatch):
                    def change(state, held, called):
                        if not after_dispatch or (called and not held):
                            target = state
                            path = field.split(".")
                            for part in path[:-1]:
                                target = target[part]
                            target[path[-1]] = replacement
                    live, clock, active, step = self.help_fixture(change=change)
                    with self.assertRaises(BotFault):
                        navigate_menu(live, step, clock=clock, sleep=clock.sleep)
                    self.assertEqual(active, [])
                    self.assertEqual(live.input.call_count, int(after_dispatch))

    def test_paused_lua_read_cannot_extend_the_held_menu_edge(self):
        live, clock, active = self.pause_fixture()
        original = live.observe.side_effect
        held_native_reads = []
        def state(native=False):
            if native and active:
                held_native_reads.append(True)
                clock.sleep(.5)
            return original(native)
        live.observe.side_effect = state
        navigate_menu(live, {"owner":"pause", "direction":"down",
                            "native_before":{"mission":10040}}, clock=clock, sleep=clock.sleep)
        self.assertEqual(held_native_reads, [])
        self.assertEqual(active, [])

    def test_reviewed_pause_popup_navigation_returns_the_real_held_axis_sample(self):
        live, clock, active = self.pause_fixture(lambda state, held: state["native"].update(popup=True))
        result = navigate_menu(live, {"owner":"pause_popup", "direction":"left",
                                     "native_before":{"mission":10040,"popup":True},
                                     "while_held":{"controls.sticks":[0.,0.,-1.,0.]}},
                               clock=clock, sleep=clock.sleep)
        self.assertEqual(result["state"]["controls"]["sticks"], [0.,0.,-1.,0.])
        self.assertEqual(result["captures"], [])
        self.assertEqual(active, [])

    def test_popup_navigation_requires_explicit_popup_and_fast_held_predicates(self):
        for guard, during in (({"mission":10040},None),
                              ({"mission":10040,"popup":True},{"native.popup":True})):
            live, clock, _ = self.pause_fixture(lambda state, held: state["native"].update(popup=True))
            with self.assertRaises(BotFault):
                navigate_menu(live, {"owner":"pause_popup", "direction":"left",
                                    "native_before":guard, "while_held":during},
                              clock=clock, sleep=clock.sleep)
            live.input.assert_not_called()

    def selector_fixture(self, popup=False, change=None):
        def customize(state, held):
            state['pause'] = False
            state['native'].update(mission=40010, helicopter_space=True,
                                   sequence='Seq_Game_WeaponCustomize',
                                   customization_kind='helicopter', popup=popup, saving=False)
            if change:
                change(state, held)
        return self.pause_fixture(customize)

    def test_customization_and_its_popup_use_the_effective_vr_axis_without_pause(self):
        for popup in (False, True):
            with self.subTest(popup=popup):
                live, clock, active = self.selector_fixture(popup)
                guard = {'mission':40010, 'helicopter_space':True,
                         'sequence':'Seq_Game_WeaponCustomize',
                         'customization_kind':'helicopter', 'popup':popup}
                navigate_menu(live, {'owner':'customization_popup' if popup else 'customization',
                                    'direction':'left', 'native_before':guard},
                              clock=clock, sleep=clock.sleep)
                self.assertEqual(live.input.call_args.args[0][0]['hand'], 'right')
                self.assertEqual(active, [])
                self.assertTrue(live.observe.call_args_list[-1].kwargs.get('native'))

    def test_customization_rejects_incomplete_target_and_other_menu_owners_before_input(self):
        guard = {'mission':40010, 'helicopter_space':True,
                 'sequence':'Seq_Game_WeaponCustomize', 'customization_kind':'helicopter'}
        for key in guard:
            live, clock, _ = self.selector_fixture()
            incomplete = {k:v for k,v in guard.items() if k != key}
            with self.assertRaises(BotFault):
                navigate_menu(live, {'owner':'customization', 'direction':'down',
                                    'native_before':incomplete}, clock=clock, sleep=clock.sleep)
            live.input.assert_not_called()
        for field, value in (('pause',True), ('idroid',True), ('loading',True),
                             ('native',{'mission':40010,'popup':True}), ('menu',False)):
            live, clock, _ = self.selector_fixture(change=lambda state, held:state.update({field:value}))
            with self.assertRaises(BotFault):
                navigate_menu(live, {'owner':'customization','direction':'down',
                                    'native_before':guard}, clock=clock, sleep=clock.sleep)
            live.input.assert_not_called()

    def test_customization_target_change_after_release_cannot_count_as_navigation(self):
        live, clock, active = self.selector_fixture()
        original = live.observe.side_effect
        def state(native=False):
            value = original(native)
            if native and live.input.called and not active:
                value['native']['customization_kind'] = 'vehicle'
            return value
        live.observe.side_effect = state
        with self.assertRaisesRegex(BotFault, 'native input owner or scene prerequisite'):
            navigate_menu(live, {'owner':'customization','direction':'down',
                                'native_before':{'mission':40010,'helicopter_space':True,
                                    'sequence':'Seq_Game_WeaponCustomize','customization_kind':'helicopter'}},
                          clock=clock, sleep=clock.sleep)
        self.assertEqual(active, [])

    def popup_identity_fixture(self, change=None):
        live, clock, active = self.selector_fixture(popup=True)
        original = live.observe.side_effect
        def state(native=False):
            value = original(native)
            value['now_ms'] = int(clock.now * 1000)
            value['popup_observer'] = {'reader_verified':True, 'owner_verified':True,
                'active':True, 'numeric_id':6, 'string_id':'0xe70e65b85f7a',
                'sample_ms':value['now_ms'], 'last_result':0, 'coherent_frame':False}
            value['reviewed_row'] = 'discard' if live.input.called else 'cancel'
            if change:
                change(value, bool(active), live.input.called)
            return value
        live.observe.side_effect = state
        step = {'owner':'customization_popup', 'direction':'left',
            'native_before':{'mission':40010, 'helicopter_space':True,
                'sequence':'Seq_Game_WeaponCustomize', 'customization_kind':'helicopter', 'popup':True},
            'state_before':{'popup_observer.reader_verified':True, 'popup_observer.owner_verified':True,
                'popup_observer.active':True, 'popup_observer.numeric_id':6,
                'popup_observer.string_id':'0xe70e65b85f7a', 'reviewed_row':'cancel'}}
        return live, clock, active, step

    def test_navigation_state_prerequisite_is_checked_at_fresh_admission(self):
        live, clock, active, step = self.popup_identity_fixture()
        step['state_before']['reviewed_row'] = 'a different page'
        with self.assertRaisesRegex(ActionPrerequisiteChanged, 'navigation prerequisite'):
            navigate_menu(live, step, clock=clock, sleep=clock.sleep)
        live.input.assert_not_called()
        self.assertEqual(active, [])

    def test_navigation_rejects_a_changed_popup_identity_before_any_input(self):
        for field, value in (('numeric_id',1), ('string_id','0x4dc3cae5b486'), ('active',False)):
            with self.subTest(field=field):
                live, clock, active, step = self.popup_identity_fixture(
                    lambda state, held, called:state['popup_observer'].update({field:value}))
                with self.assertRaises(ActionPrerequisiteChanged):
                    navigate_menu(live, step, clock=clock, sleep=clock.sleep)
                live.input.assert_not_called()
                self.assertEqual(active, [])

    def test_navigation_popup_identity_needs_verified_reader_and_owner(self):
        for field in ('reader_verified','owner_verified'):
            for value in (False, None, 1):
                with self.subTest(field=field, value=value):
                    live, clock, active, step = self.popup_identity_fixture(
                        lambda state, held, called:state['popup_observer'].update({field:value}))
                    # Even a partial ID prerequisite cannot borrow an unproven owner.
                    step['state_before'] = {'popup_observer.numeric_id':6}
                    with self.assertRaisesRegex(ActionPrerequisiteChanged, 'popup identity'):
                        navigate_menu(live, step, clock=clock, sleep=clock.sleep)
                    live.input.assert_not_called()
                    self.assertEqual(active, [])
        live, clock, active, step = self.popup_identity_fixture(
            lambda state, held, called:state.pop('popup_observer'))
        with self.assertRaises(ActionPrerequisiteChanged):
            navigate_menu(live, step, clock=clock, sleep=clock.sleep)
        live.input.assert_not_called()

    def test_popup_guard_cannot_admit_unknown_or_numeric_activity(self):
        for active_value in (None, 1, False):
            live, clock, active, step = self.popup_identity_fixture(
                lambda state, held, called:state['popup_observer'].update(active=active_value))
            step['state_before'] = {'popup_observer.owner_verified':True}
            with self.assertRaisesRegex(ActionPrerequisiteChanged, 'popup identity'):
                navigate_menu(live, step, clock=clock, sleep=clock.sleep)
            live.input.assert_not_called()
            self.assertEqual(active, [])

    def test_navigation_popup_identity_rejects_stale_future_and_unknown_samples(self):
        for sampled in (None, True, -1, 99, 0):
            with self.subTest(sampled=sampled):
                live, clock, active, step = self.popup_identity_fixture(
                    lambda state, held, called:state['popup_observer'].update(sample_ms=sampled))
                clock.now = .3
                if sampled == 99:
                    # A sample ahead of the packet clock is not current evidence.
                    clock.now = .05
                with self.assertRaisesRegex(ActionPrerequisiteChanged, 'popup identity'):
                    navigate_menu(live, step, clock=clock, sleep=clock.sleep)
                live.input.assert_not_called()
                self.assertEqual(active, [])

    def test_popup_identity_is_rechecked_after_cursor_release_without_held_lua(self):
        for field, value in (('numeric_id',1), ('string_id','0x4dc3cae5b486'),
                             ('owner_verified',False), ('active',None)):
            with self.subTest(field=field):
                def change(state, held, called):
                    if called and not held:
                        state['popup_observer'][field] = value
                live, clock, active, step = self.popup_identity_fixture(change)
                held_native_reads = []
                original = live.observe.side_effect
                def state(native=False):
                    if native and active:
                        held_native_reads.append(True)
                    return original(native)
                live.observe.side_effect = state
                with self.assertRaisesRegex(BotFault, 'popup identity') as failure:
                    navigate_menu(live, step, clock=clock, sleep=clock.sleep)
                self.assertNotIsInstance(failure.exception, ActionPrerequisiteChanged)
                live.input.assert_called_once()
                self.assertEqual(active, [])
                self.assertEqual(held_native_reads, [])

    def test_unchanged_popup_allows_cursor_movement_and_new_sample_and_last_result(self):
        def change(state, held, called):
            if called:
                state['popup_observer']['last_result'] = 2
        live, clock, active, step = self.popup_identity_fixture(change)
        step['state_before']['popup_observer.last_result'] = 0
        result = navigate_menu(live, step, clock=clock, sleep=clock.sleep)
        self.assertEqual(result['reviewed_row'], 'discard')
        self.assertEqual(result['popup_observer']['last_result'], 2)
        self.assertGreater(result['popup_observer']['sample_ms'], 0)
        self.assertEqual(active, [])

    def test_navigation_without_popup_guard_preserves_legacy_owner_contract(self):
        live, clock, active = self.selector_fixture(popup=True)
        result = navigate_menu(live, {'owner':'customization_popup','direction':'left',
            'native_before':{'mission':40010, 'helicopter_space':True,
                'sequence':'Seq_Game_WeaponCustomize', 'customization_kind':'helicopter', 'popup':True}},
            clock=clock, sleep=clock.sleep)
        self.assertNotIn('popup_observer', result)
        self.assertEqual(active, [])

    def test_invalid_navigation_state_guard_never_dispatches(self):
        for guard in ({}, [], True, 'unknown'):
            live, clock, _ = self.fixture()
            with self.assertRaisesRegex(BotFault, 'state prerequisite'):
                navigate_menu(live, {'direction':'left', 'state_before':guard}, clock=clock, sleep=clock.sleep)
            live.input.assert_not_called()

    def test_customization_requires_explicit_idle_save_at_fresh_admission(self):
        # Suite entry and navigation admission are different observations.
        # A save may begin between them, even if the caller's target guard
        # omits the saving field. Unknown is also not proof of an idle save.
        for saving in (True, None, 0, 'unavailable:save state'):
            with self.subTest(saving=saving):
                live, clock, active = self.selector_fixture(
                    change=lambda state, held:state['native'].update(saving=saving))
                with self.assertRaisesRegex(BotFault, 'scene prerequisite'):
                    navigate_menu(live, {'owner':'customization', 'direction':'down',
                                        'native_before':{'mission':40010,'helicopter_space':True,
                                            'sequence':'Seq_Game_WeaponCustomize',
                                            'customization_kind':'helicopter'}},
                                  clock=clock, sleep=clock.sleep)
                live.input.assert_not_called()
                self.assertEqual(active, [])

    def test_customization_save_change_after_release_fails_without_held_lua_reads(self):
        live, clock, active = self.selector_fixture(popup=True)
        original = live.observe.side_effect
        held_native_reads = []
        def state(native=False):
            value = original(native)
            if native and active:
                held_native_reads.append(True)
            if native and live.input.called and not active:
                value['native']['saving'] = True
            return value
        live.observe.side_effect = state
        with self.assertRaisesRegex(BotFault, 'scene prerequisite'):
            navigate_menu(live, {'owner':'customization_popup','direction':'left',
                                'native_before':{'mission':40010,'helicopter_space':True,
                                    'sequence':'Seq_Game_WeaponCustomize',
                                    'customization_kind':'helicopter','popup':True}},
                          clock=clock, sleep=clock.sleep)
        self.assertEqual(held_native_reads, [])
        self.assertEqual(active, [])

    def test_navigation_cannot_pass_a_different_held_axis_and_always_releases(self):
        live, clock, active = self.pause_fixture()
        with self.assertRaisesRegex(BotFault, "held outcome was not observed"):
            navigate_menu(live, {"owner":"pause", "direction":"left",
                                "native_before":{"mission":10040},
                                "while_held":{"controls.sticks":[-1.,0.,0.,0.]}},
                          clock=clock, sleep=clock.sleep)
        self.assertEqual(active, [])

    def test_pause_navigation_rejects_unidentified_scene_popup_or_changed_mission(self):
        changes = [("pause",None),("pause",False),("idroid",True),("title",True),
                   ("loading",True),("menu",False),("scene","cabin"),
                   ("native",{"mission":40060,"popup":False}),
                   ("native",{"mission":10040,"popup":True})]
        for field, value in changes:
            with self.subTest(field=field, value=value):
                live, clock, _ = self.pause_fixture(lambda state, held: state.update({field:value}))
                with self.assertRaises(BotFault):
                    navigate_menu(live, {"owner":"pause", "direction":"down",
                                        "native_before":{"mission":10040}}, clock=clock, sleep=clock.sleep)
                live.input.assert_not_called()
        live, clock, _ = self.pause_fixture()
        with self.assertRaisesRegex(BotFault, "native scene prerequisites"):
            navigate_menu(live, {"owner":"pause", "direction":"down"}, clock=clock, sleep=clock.sleep)
        live.input.assert_not_called()

    def test_pause_navigation_releases_if_native_owner_changes_while_held(self):
        live, clock, active = self.pause_fixture(lambda state, held: state.update(pause=not held))
        with self.assertRaisesRegex(BotFault, "lost its native input owner"):
            navigate_menu(live, {"owner":"pause", "direction":"down",
                                "native_before":{"mission":10040}}, clock=clock, sleep=clock.sleep)
        live.input.assert_called_once()
        self.assertEqual(active, [])

    def test_invalid_navigation_never_dispatches_input(self):
        for step in ({"direction": "forward"}, {"direction": "down", "seconds": True},
                     {"direction": "down", "seconds": float("nan")},
                     {"direction": "down", "seconds": 1.}):
            live, clock, _ = self.fixture()
            with self.assertRaises(BotFault):
                navigate_menu(live, step, clock=clock, sleep=clock.sleep)
            live.input.assert_not_called()

    def test_missing_mapping_or_native_owner_never_dispatches_input(self):
        for source, fault in (("disabled", None), ("left_stick", "owner")):
            live, clock, _ = self.fixture(source, fault)
            with self.assertRaises(BotFault):
                navigate_menu(live, {"direction": "down"}, clock=clock, sleep=clock.sleep)
            live.input.assert_not_called()

    def test_unsampled_input_expires_and_releases(self):
        live, clock, active = self.fixture(fault="unsampled")
        with self.assertRaisesRegex(BotFault, "before expiry"):
            navigate_menu(live, {"direction": "down"}, clock=clock, sleep=clock.sleep)
        self.assertLess(clock.now, 2.1)
        self.assertEqual(active, [])

    def test_failed_input_rpc_still_releases(self):
        live, clock, active = self.fixture(fault="rpc")
        with self.assertRaises(TimeoutError):
            navigate_menu(live, {"direction": "down"}, clock=clock, sleep=clock.sleep)
        self.assertEqual(active, [])

    def test_release_rpc_alone_does_not_prove_neutral_input(self):
        live, clock, _ = self.fixture(fault="stuck_release")
        with self.assertRaisesRegex(BotFault, "stayed held"):
            navigate_menu(live, {"direction": "down"}, clock=clock, sleep=clock.sleep)

    def test_idle_observation_dispatches_no_input_and_releases(self):
        live, clock, active = self.fixture()
        observe_menu(live, {"seconds": 1.}, clock=clock, sleep=clock.sleep)
        live.input.assert_not_called()
        self.assertEqual(active, [])
        self.assertEqual(clock.now, 1.)

    def test_idle_observation_rejects_missing_or_invalid_neutral_samples(self):
        for field, invalid in (("sticks", []), ("sticks", [0.] * 3),
                               ("sticks", [0., float("nan"), 0., 0.]),
                               ("physical", []), ("physical", [False] * 11),
                               ("physical", [1.] + [0.] * 10)):
            with self.subTest(field=field, invalid=invalid):
                live, clock, active = self.fixture()
                state = live.observe()
                state["controls"][field] = invalid
                live.observe.side_effect = None
                live.observe.return_value = state
                with self.assertRaisesRegex(BotFault, "sampled neutral"):
                    observe_menu(live, {"seconds": 1.}, clock=clock, sleep=clock.sleep)
                live.input.assert_not_called()
                self.assertEqual(active, [])


class Adapter:
    def __init__(self, states, error=None):
        self.states, self.error = iter(states), error
        self.last = {}
        self.releases = self.actions = 0
    def observe(self, native=False):
        self.last = next(self.states, self.last)
        return self.last
    def release(self):
        self.releases += 1
    def execute(self, step):
        self.actions += 1
        if self.error:
            raise self.error
    def capture(self, label):
        return [label]


class BotTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.clock = Clock()
        self.events = Events(self.directory.name, {"fixture": True}, self.clock)

    def behavior(self, adapter):
        return Behaviors(adapter, self.events, self.clock, self.clock.sleep)

    def test_acc_helicopter_back_cannot_dispatch_on_field_idroid_weapon_or_unknown_target(self):
        path=pathlib.Path(__file__).resolve().parents[1]/'tools/gameplay_bot/suites/acc-helicopter-customization-back.json'
        case=json.loads(path.read_text())['cases'][0]
        good={'scene':'menu','menu':True,'idroid':False,'pause':False,'controls':{'context':'menus'},
              'native':{'mission':40010,'title':False,'helicopter_space':True,'sequence':'Seq_Game_WeaponCustomize',
                        'customization_kind':'helicopter','popup':False,'saving':False}}
        changes=[{'helicopter_space':False},{'sequence':'Seq_Game_MainGame'},
                 {'customization_kind':'weapon'},{'customization_kind':None},{'popup':True},{'saving':True}]
        for change in changes:
            state=copy.deepcopy(good);state['native'].update(change)
            adapter=Adapter([state])
            result=self.behavior(adapter).case(case)
            self.assertEqual(result['failure_phase'],'entry')
            self.assertEqual(adapter.actions,0)
        state=copy.deepcopy(good);state['idroid']=True
        adapter=Adapter([state]);self.behavior(adapter).case(case)
        self.assertEqual(adapter.actions,0)

    def test_acc_cancel_must_observe_native_main_cabin_instead_of_a_closed_terminal_only(self):
        path=pathlib.Path(__file__).resolve().parents[1]/'tools/gameplay_bot/suites/acc-helicopter-customization-back.json'
        case=json.loads(path.read_text())['cases'][-1]
        opened={'scene':'menu','menu':True,'idroid':False,'pause':False,'controls':{'context':'menus'},
                'native':{'mission':40010,'title':False,'helicopter_space':True,'sequence':'Seq_Game_WeaponCustomize',
                          'customization_kind':'helicopter','popup':True,'saving':False}}
        # iDroid is already closed on this selector: its close bit cannot prove Cancel.
        adapter=Adapter([opened]*2)
        result=self.behavior(adapter).case(case)
        self.assertEqual(result['failure_phase'],'outcome')
        self.assertEqual(adapter.actions,2)
        self.assertEqual(result['status'],'failed')

    def overnight_pause_suite(self):
        path=pathlib.Path(__file__).resolve().parents[1]/'tools/gameplay_bot/suites/overnight-menu-field-pause-roundtrip.json'
        return json.loads(path.read_text())

    def predicate_state(self, predicate):
        state={}
        for path, value in predicate.items():
            target=state
            parts=path.split('.')
            for part in parts[:-1]:
                target=target.setdefault(part,{})
            target[parts[-1]]=copy.deepcopy(value['near'] if isinstance(value,dict) and 'near' in value else value)
        return state

    def test_overnight_pause_rejects_tutorial_popup_saving_and_unknown_before_any_input(self):
        for case in self.overnight_pause_suite()['cases']:
            for key in ('tutorial_pause','popup','saving'):
                for value in (True,None):
                    with self.subTest(case=case['id'],key=key,value=value):
                        state=self.predicate_state(case['before'])
                        state['native'][key]=value
                        adapter=Adapter([state])
                        result=self.behavior(adapter).case(case)
                        self.assertEqual(result['failure_phase'],'entry')
                        self.assertEqual(adapter.actions,0)

    def test_overnight_pause_back_requires_actual_camera_and_gameplay_owner_return(self):
        for case in (self.overnight_pause_suite()['cases'][1],self.overnight_pause_suite()['cases'][3]):
            for path, value in (('camera_active',False),('camera_suspended',True),
                                ('camera_awaiting_player',True),('controls.rig_input',False),
                                ('controls.context','menus')):
                with self.subTest(case=case['id'],path=path):
                    before=self.predicate_state(case['before'])
                    after=self.predicate_state(case['after'])
                    target=after
                    parts=path.split('.')
                    for part in parts[:-1]:target=target[part]
                    target[parts[-1]]=value
                    adapter=Adapter([before]*2+[after]*100)
                    result=self.behavior(adapter).case(case)
                    self.assertEqual(result['failure_phase'],'outcome')
                    self.assertEqual(adapter.actions,1)
                    self.assertEqual(result['status'],'failed')

    def test_overnight_pause_dispatch_and_result_guards_cover_the_same_native_fixture(self):
        cases=self.overnight_pause_suite()['cases']
        self.assertEqual([case['steps'][0]['name'] for case in cases],
                         ['system.pause','menus.back','system.pause','menus.back'])
        for index, case in enumerate(cases):
            guard=case['steps'][0]['native_before']
            self.assertEqual(guard['mission'],10020)
            self.assertEqual(guard['sequence'],'Seq_Game_RescueMiller')
            for key in ('title','helicopter_space','popup','tutorial_pause','saving','demo','demo_nonplayable'):
                self.assertIs(guard[key],False)
                self.assertIs(case['before']['native.'+key],False)
            if index:
                self.assertEqual(case['depends_on'],[cases[index-1]['id']])

    def test_unknown_scene_is_not_gameplay(self):
        self.assertEqual(scene({"camera_active": True, "camera_available": True}), "unknown")
        state = dict(title=False, loading=False, menu=False, demo=False, camera_active=True, camera_available=True)
        self.assertEqual(scene(state), "gameplay")
        self.assertEqual(scene({**state, "demo": True}), "cinematic")
        self.assertEqual(scene({**state, "menu": True, "demo": True}), "menu")
        self.assertEqual(scene({**state, "loading": True, "menu": True}), "loading")

    def test_missing_or_wrong_type_does_not_satisfy_predicate(self):
        self.assertFalse(matches({}, {"menu": False}))
        self.assertFalse(matches({"menu": 0}, {"menu": False}))
        self.assertFalse(matches({"native": {}}, {"native.mission": 10010}))
        with self.assertRaises(BotFault):
            matches({}, {})

    def test_explicit_pose_tolerance_accepts_serialization_error_but_rejects_wrong_pose(self):
        predicate = {"pose": {"near": [.01, -.18, -.5], "tolerance": .0001}}
        self.assertTrue(matches({"pose": [.00999998, -.18000001, -.5]}, predicate))
        self.assertFalse(matches({"pose": [0., -.18, -.5]}, predicate))
        self.assertFalse(matches({"pose": [.01, -.18]}, predicate))
        self.assertFalse(matches({"pose": True}, {"pose": {"near": 1, "tolerance": .01}}))
        self.assertFalse(matches({"pose": [True, -.18, -.5]}, predicate))
        self.assertFalse(matches({"pose": [float("nan"), -.18, -.5]}, predicate))
        self.assertFalse(matches({"pose": [.00999998, -.18, -.5]}, {"pose": [.01, -.18, -.5]}))

    def test_invalid_pose_tolerance_does_not_weaken_the_state_gate(self):
        for near, tolerance in (([], .001), ([.01], 0), ([.01], -1), ([.01], float("inf")), ([True], .001)):
            with self.subTest(near=near, tolerance=tolerance), self.assertRaises(BotFault):
                matches({"pose": [.01]}, {"pose": {"near": near, "tolerance": tolerance}})
    def test_native_playable_quad_does_not_require_an_active_vr_camera(self):
        state = dict(title=False, loading=False, menu=False, demo=False,
                     camera_active=False, camera_available=True)
        self.assertEqual(scene(state), "unknown")
        state["native"] = {"title": False, "mission": 30010, "sequence": "Seq_Game_FreePlay",
                           "status_NORMAL_ACTION": True}
        self.assertEqual(scene(state), "gameplay")
        self.assertEqual(scene({**state, "menu": True}), "menu")
        state["native"]["status_NORMAL_ACTION"] = False
        self.assertEqual(scene(state), "unknown")

    def test_quad_native_evidence_cannot_override_unknown_or_cinematic_scene(self):
        native = {"title": False, "mission": 30010, "sequence": "Seq_Game_FreePlay",
                  "status_NORMAL_ACTION": True}
        self.assertEqual(scene({"native": native, "camera_active": False}), "unknown")
        self.assertEqual(scene(dict(title=False, loading=False, menu=False, demo=True,
                                    camera_active=False, native=native)), "cinematic")

    def test_ready_wait_advances_without_deadline_sleep(self):
        adapter = Adapter([{"scene": "loading"}, {"scene": "gameplay"}, {"scene": "gameplay"}])
        self.behavior(adapter).wait_for({"scene": "gameplay"}, 20, "ready")
        self.assertAlmostEqual(self.clock.now, .2)

    def test_xr_startup_plain_text_not_ready_is_retried_without_gameplay_input(self):
        live = Live.__new__(Live)
        live.events = self.events
        live.native = mock.Mock()
        live.native.read.return_value = ({"lua_ready": True}, {})
        live.call = mock.Mock(side_effect=[
            {"content": [{"type": "text", "text": "No valid XR session active"}]},
            {"content": [{"type": "text", "text": json.dumps({"state": {"name": "XR_SESSION_STATE_FOCUSED"}})}]},
            {"content": [{"type": "text", "text": json.dumps({"flags": {"position_valid": True, "orientation_valid": True}})}]},
        ])
        with mock.patch("gameplay_bot.live.time.monotonic", self.clock), \
                mock.patch("gameplay_bot.live.time.sleep", self.clock.sleep):
            live.ready(timeout=2.)
        self.assertEqual([call.args[0] for call in live.call.call_args_list],
                         ["get_session_info", "get_session_info", "get_head_pose"])
        self.assertEqual(self.clock.now, .25)

    def test_xr_focus_waits_for_native_startup_without_queuing_lua(self):
        live = Live.__new__(Live)
        live.events = self.events
        live.native = mock.Mock()
        live.native.read.side_effect = [({"lua_ready": False}, {}),
                                       ({"lua_ready": False}, {}),
                                       ({"lua_ready": True}, {})]
        live.call = mock.Mock(side_effect=[
            {"content": [{"type": "text", "text": json.dumps({"state": {"name": "XR_SESSION_STATE_FOCUSED"}})}]},
            {"content": [{"type": "text", "text": json.dumps({"flags": {"position_valid": True, "orientation_valid": True}})}]},
        ])
        with mock.patch("gameplay_bot.live.time.monotonic", self.clock), \
                mock.patch("gameplay_bot.live.time.sleep", self.clock.sleep):
            live.ready(timeout=2.)
        self.assertEqual(live.native.read.call_args_list,
                         [mock.call("inspect-bot-state")] * 3)
        self.assertEqual(self.clock.now, .5)
        self.assertEqual([call.args[0] for call in live.call.call_args_list],
                         ["get_session_info", "get_head_pose"])

    def test_native_startup_never_ready_stops_without_gameplay_input(self):
        live = Live.__new__(Live)
        live.events = self.events
        live.native = mock.Mock()
        live.native.read.return_value = ({"lua_ready": False}, {})
        live.call = mock.Mock(side_effect=[
            {"content": [{"type": "text", "text": json.dumps({"state": {"name": "XR_SESSION_STATE_FOCUSED"}})}]},
            {"content": [{"type": "text", "text": json.dumps({"flags": {"position_valid": True, "orientation_valid": True}})}]},
        ])
        with mock.patch("gameplay_bot.live.time.monotonic", self.clock), \
                mock.patch("gameplay_bot.live.time.sleep", self.clock.sleep):
            with self.assertRaisesRegex(BotFault, "Native Lua readiness deadline"):
                live.ready(timeout=.5)
        self.assertEqual(self.clock.now, .5)
        self.assertTrue(all(call.args[0] == "inspect-bot-state" for call in live.native.read.call_args_list))
        self.assertEqual(len(live.call.call_args_list), 2)

    def test_black_native_boot_fade_waits_for_real_compositor_pixels(self):
        live = mock.Mock(events=self.events)
        blank = BlankCompositorFrame("retained black frame", {
            "scene": "unknown", "camera_active": False,
            "native": {"mission": 65535, "sequence": None}})
        boot_mission = BlankCompositorFrame("retained next boot fade", {
            "scene": "unknown", "camera_active": False,
            "native": {"mission": 1, "sequence": None, "player_life": 0,
                       "status_NORMAL_ACTION": False}})
        live.capture.side_effect = [blank, boot_mission, ["left.png", "right.png"]]
        result = capture_startup_baseline(live, clock=self.clock, sleep=self.clock.sleep)
        self.assertEqual(result, ["left.png", "right.png"])
        self.assertEqual(self.clock.now, .5)
        live.execute.assert_not_called()

    def test_black_gameplay_is_never_relabelled_as_a_startup_fade(self):
        live = mock.Mock(events=self.events)
        live.capture.side_effect = BlankCompositorFrame("blank gameplay", {
            "scene": "gameplay", "camera_active": True,
            "native": {"mission": 10040, "sequence": "Seq_Game_Main"}})
        with self.assertRaisesRegex(BlankCompositorFrame, "blank gameplay"):
            capture_startup_baseline(live, clock=self.clock, sleep=self.clock.sleep)
        self.assertEqual(self.clock.now, 0)
        self.assertEqual(live.capture.call_count, 1)

    def test_persistent_black_startup_stops_at_a_bounded_deadline(self):
        live = mock.Mock(events=self.events)
        live.capture.side_effect = BlankCompositorFrame("blank title", {
            "scene": "title", "camera_active": False})
        with self.assertRaisesRegex(BotFault, "remained blank"):
            capture_startup_baseline(live, timeout=.5, clock=self.clock, sleep=self.clock.sleep)
        self.assertEqual(self.clock.now, .5)
        live.execute.assert_not_called()

    def test_transient_match_does_not_pass_stability_gate(self):
        adapter = Adapter([{"ready": True}, {"ready": False}])
        with self.assertRaisesRegex(BotFault, "deadline"):
            self.behavior(adapter).wait_for({"ready": True}, .3, "ready")

    @staticmethod
    def case():
        return {"id": "menu", "before": {"menu": False}, "after": {"menu": True},
                "steps": [{"op": "action", "name": "system.pause"}], "timeout": .3}

    def test_rpc_completion_without_outcome_fails_and_releases(self):
        adapter = Adapter([{"menu": False}])
        result = self.behavior(adapter).case(self.case())
        self.assertEqual(result["status"], "failed")
        self.assertEqual(adapter.actions, 1)
        self.assertEqual(adapter.releases, 2)
        self.assertEqual(result["failure_phase"], "outcome")
        self.assertTrue(result["entry_predicates_still_match"])

    def test_ambiguous_action_is_not_retried(self):
        adapter = Adapter([{"menu": False}], TimeoutError("completion unknown"))
        result = self.behavior(adapter).case(self.case())
        self.assertEqual(result["status"], "failed")
        self.assertEqual(adapter.actions, 1)
        self.assertEqual(adapter.releases, 2)

    def test_observed_transition_still_requires_visual_review(self):
        adapter = Adapter([{"menu": False}, {"menu": False}, {"menu": True}, {"menu": True}])
        result = self.behavior(adapter).case(self.case())
        self.assertEqual(result["status"], "observed_pass")
        self.assertEqual(result["visual_acceptance"], "pending")

    def test_already_satisfied_outcome_does_not_credit_action(self):
        case = self.case()
        case["before"] = {"menu": True}
        adapter = Adapter([{"menu": True}])
        self.assertEqual(self.behavior(adapter).case(case)["status"], "failed")
        self.assertEqual(adapter.actions, 0)

    def test_event_archive_cannot_be_overwritten(self):
        self.events.emit("test")
        with self.assertRaises(BotFault):
            Events(self.directory.name, {})

    def test_actual_input_tokens_and_local_transform(self):
        self.assertEqual(channel("left_stick_down"), {"hand": "left", "component": "Thumbstick", "sub_component": "Y", "value": -1.})
        self.assertEqual(channel("y")["hand"], "left")
        self.assertEqual(rotate([0, 0, 0, 1], [1, 2, 3]), [1, 2, 3])

    def test_action_context_comes_from_fresh_resolver_audit(self):
        state = {"scene": "gameplay", "now_ms": 500,
                 "controls": {"context": "equipment", "age_ms": 8, "sample_ms": 492}}
        self.assertEqual(authoritative_context(state), "equipment")
        for controls in (None, {"context": "gameplay", "age_ms": 251},
                         {"context": "unrecognized", "age_ms": 1}):
            with self.subTest(controls=controls), self.assertRaises(BotFault):
                authoritative_context({"scene": "gameplay", "now_ms": 500,
                                       "controls": {"sample_ms": 500, **(controls or {})}})
        with self.assertRaisesRegex(BotFault, "age is inconsistent"):
            authoritative_context({"now_ms": 500, "controls": {
                "context": "gameplay", "age_ms": 8, "sample_ms": 491}})

    @staticmethod
    def audit_state(button=0., native_buttons=0, sticks=None, sample_ms=900, context="gameplay", age_ms=12):
        return {"controls": {"context": context, "age_ms": age_ms, "sample_ms": sample_ms,
                "physical": [button] + [0.] * 10,
                "sticks": sticks or [0., 0., 0., 0.], "native_buttons": native_buttons}}

    def test_static_scope_capture_refuses_actions_motion_stale_and_remapped_grips(self):
        state = self.audit_state()
        state.update(camera_active=True, menu=False)
        state["controls"].update(rig_input=True)
        state["controls"]["physical"][8] = 1.
        held = {("right", "Grip", None): channel("right_grip")}
        bindings = {"actions": [{"name": "gameplay.ready_weapon", "modifier": True,
            "contexts": ["gameplay"], "bindings": [{"inputs": ["right_grip"]}]}]}
        self.assertTrue(static_grip_capture_allowed(held, state, bindings))
        import copy
        for index in (0, 4, 5, 9, 10):
            changed = copy.deepcopy(state);changed["controls"]["physical"][index] = 1.
            self.assertFalse(static_grip_capture_allowed(held, changed, bindings))
        for axis in range(4):
            changed = copy.deepcopy(state);changed["controls"]["sticks"][axis] = .7
            self.assertFalse(static_grip_capture_allowed(held, changed, bindings))
        for updates in ({"age_ms": 251}, {"context": "equipment"}, {"rig_input": False}):
            changed = copy.deepcopy(state);changed["controls"].update(updates)
            self.assertFalse(static_grip_capture_allowed(held, changed, bindings))
        changed = copy.deepcopy(state);changed["controls"]["physical"][8] = 0.
        self.assertFalse(static_grip_capture_allowed(held, changed, bindings))
        changed = copy.deepcopy(state);changed["controls"]["physical"][7] = 1.
        self.assertFalse(static_grip_capture_allowed(held, changed, bindings))
        changed = copy.deepcopy(state);changed["controls"]["native_buttons"] = 0x1000
        self.assertFalse(static_grip_capture_allowed(held, changed, bindings))
        remapped = copy.deepcopy(bindings)
        remapped["actions"].append({"name": "gameplay.fire_or_cqc", "modifier": False,
            "contexts": ["gameplay"], "bindings": [{"inputs": ["right_grip"]}]})
        self.assertFalse(static_grip_capture_allowed(held, state, remapped))
        unsafe = {**held, ("right", "Trigger", None): channel("right_trigger")}
        self.assertFalse(static_grip_capture_allowed(unsafe, state, bindings))

    def test_touch_bindings_cannot_be_faked_by_the_simulator(self):
        for token in ("a_touch", "b_touch", "x_touch", "y_touch", "left_stick_touch",
                      "right_stick_touch", "left_trigger_touch", "right_trigger_touch",
                      "left_thumbrest", "right_thumbrest"):
            with self.subTest(token=token), self.assertRaisesRegex(BotFault, "cannot synthesize capacitive"):
                channel(token)

    def test_static_grip_capture_requires_mapped_touch_contacts_to_be_released(self):
        import copy
        state = self.audit_state()
        state.update(camera_active=True, menu=False)
        state["controls"].update(rig_input=True)
        state["controls"]["physical"][8] = 1.
        held = {("right", "Grip", None): channel("right_grip")}
        bindings = {"actions": [
            {"name": "gameplay.ready_weapon", "modifier": True, "contexts": ["gameplay"],
             "bindings": [{"inputs": ["right_grip"]}]},
            {"name": "gameplay.interact", "modifier": False, "contexts": ["gameplay"],
             "bindings": [{"inputs": ["a_touch"]}]}]}
        self.assertFalse(static_grip_capture_allowed(held, state, bindings))
        state["controls"]["touches"] = {"a_touch": 0.}
        self.assertTrue(static_grip_capture_allowed(held, state, bindings))
        for value in (1., -.5, float("nan")):
            changed = copy.deepcopy(state)
            changed["controls"]["touches"]["a_touch"] = value
            self.assertFalse(static_grip_capture_allowed(held, changed, bindings))

    def test_held_physical_audit_records_xr_published_gamepad_sample(self):
        evidence = held_input_evidence(self.audit_state(button=1., native_buttons=0x1000), ["a"])
        self.assertEqual(evidence["physical"]["buttons"][0], 1.)
        self.assertEqual(evidence["xr_published_gamepad"], {"buttons": 0x1000})
        self.assertEqual(evidence["requested_inputs"], ["a"])

    def test_gamepad_sample_without_physical_input_is_not_gesture_evidence(self):
        state = self.audit_state(button=0., native_buttons=0x1000)
        self.assertIsNone(held_input_evidence(state, ["a"]))
        self.assertIsNone(held_input_evidence(self.audit_state(button=1.), []))

    def test_held_audit_must_advance_past_admission_sample(self):
        state = self.audit_state(button=1., sample_ms=900)
        self.assertIsNone(held_input_evidence(state, ["a"], after_sample_ms=900))
        self.assertIsNone(held_input_evidence(state, ["a"], after_sample_ms=901))
        self.assertIsNotNone(held_input_evidence(state, ["a"], after_sample_ms=899))

    def test_nonfinite_physical_and_stick_samples_are_rejected(self):
        button = self.audit_state(button=float("nan"))
        self.assertIsNone(held_input_evidence(button, ["a"]))
        stick = self.audit_state(sticks=[0., float("inf"), 0., 0.])
        self.assertIsNone(held_input_evidence(stick, ["left_stick_down"]))
        huge = self.audit_state(button=10**1000)
        self.assertIsNone(held_input_evidence(huge, ["a"]))

    def picker_live(self, outcome="success"):
        live = Live.__new__(Live)
        live.bindings = {"actions": [
            {"name": "equipment.open", "modifier": True, "bindings": [{"gesture": "level", "inputs": ["right_grip"]}]},
            {"name": "equipment.primary", "bindings": [{"gesture": "level", "inputs": ["right_grip", "right_stick_up"]}]}],
            "axes": [{"name": "axes.equipment", "source": "right_stick"}]}
        live.events = self.events
        active, calls, releases = {}, [], []
        def key(value):
            return value["hand"], value["component"], value.get("sub_component")
        def observe(native=False):
            sample = 1000 if outcome == "stalled" else 1000 + int(self.clock.now * 1000)
            state = self.audit_state(sample_ms=sample, context="equipment" if active else "gameplay")
            state.update(scene="gameplay", menu=False, camera_active=True)
            state["controls"]["physical"][8] = float(key(channel("right_grip")) in active)
            for name, component, value in (("right_stick_up", 3, 1.), ("right_stick_left", 2, -1.)):
                if key(channel(name)) in active:
                    state["controls"]["sticks"][component] = value
            if outcome == "context_changed" and active:
                state["controls"]["context"] = "menus"
            return state
        def dispatch(values, duration, **options):
            calls.append((values, duration, options))
            if outcome == "rpc_failure" and len(calls) == 2:
                raise TimeoutError("completion unknown")
            active.update({key(v): v for v in values})
        def release(*, keep=()):
            releases.append(set(keep))
            for held in list(active):
                if held not in keep:
                    del active[held]
        live.observe, live.input, live.release = observe, dispatch, release
        return live, active, calls, releases

    def test_picker_preserves_modifier_until_card_then_releases_everything(self):
        live, active, calls, releases = self.picker_live()
        with mock.patch("gameplay_bot.live.time.monotonic", self.clock), \
                mock.patch("gameplay_bot.live.time.sleep", self.clock.sleep):
            live.execute({"op": "equipment_select", "category": "primary", "card": "left"})
        self.assertEqual(len(calls), 3)
        self.assertEqual(releases[1], {("right", "Grip", None)})
        self.assertEqual(releases[-1], set())
        self.assertEqual(active, {})
        self.assertEqual(calls[0][2]["lease_seconds"], 5.)
        self.assertTrue(all(0 < c[2]["lease_seconds"] <= 5. for c in calls))

    def test_picker_stalled_sampling_or_wrong_context_expires_without_selecting(self):
        for outcome in ("stalled", "context_changed"):
            with self.subTest(outcome=outcome):
                self.clock.now = 0.
                live, active, calls, releases = self.picker_live(outcome)
                with mock.patch("gameplay_bot.live.time.monotonic", self.clock), \
                        mock.patch("gameplay_bot.live.time.sleep", self.clock.sleep), \
                        self.assertRaisesRegex(BotFault, "before expiry"):
                    live.execute({"op": "equipment_select", "category": "primary", "card": "left"})
                self.assertLess(self.clock.now, 5.1)
                self.assertEqual(len(calls), 1)
                self.assertEqual(active, {})
                self.assertEqual(releases[-1], set())

    def test_picker_rpc_failure_releases_modifier_and_does_not_send_card(self):
        live, active, calls, releases = self.picker_live("rpc_failure")
        with mock.patch("gameplay_bot.live.time.monotonic", self.clock), \
                mock.patch("gameplay_bot.live.time.sleep", self.clock.sleep), \
                self.assertRaises(TimeoutError):
            live.execute({"op": "equipment_select", "category": "primary", "card": "left"})
        self.assertEqual(len(calls), 2)
        self.assertEqual(active, {})
        self.assertEqual(releases[-1], set())

    def test_picker_unsupported_touch_binding_dispatches_nothing(self):
        live, active, calls, releases = self.picker_live()
        live.bindings["actions"][0]["bindings"][0]["inputs"] = ["right_trigger_touch"]
        with self.assertRaises(BotFault):
            live.execute({"op": "equipment_select", "category": "primary", "card": "left"})
        self.assertEqual((active, calls, releases), ({}, [], []))

    def test_partial_release_keeps_only_the_leased_picker_modifier(self):
        live = Live.__new__(Live)
        live.events = self.events
        modifier = ("right", "Grip", None)
        live.held = {modifier: channel("right_grip"), ("right", "A", None): channel("a")}
        calls = []
        live.call = lambda method, value: calls.append(value)
        live.release(keep={modifier})
        self.assertEqual(set(live.held), {modifier})
        self.assertTrue(any(v["component"] == "A" and v["value"] == 0 for v in calls))
        self.assertFalse(any(v["hand"] == "right" and v["component"] == "Grip" for v in calls))

    def fake_live(self, states):
        live = Live.__new__(Live)
        live.bindings = {"actions": [{"name": "test.action", "label": "Test action",
            "contexts": ["equipment"], "bindings": [{"gesture": "tap", "milliseconds": 300,
            "inputs": ["a"]}]}]}
        live.events = self.events
        live.held = {}
        live.background_check = None
        states = iter(states)
        last = {}
        def observe(native=False):
            nonlocal last
            if live.background_check:
                live.background_check()
            last = next(states, last)
            return last
        live.observe = observe
        calls, releases = [], []
        live.input = lambda values, duration, **options: calls.append((values, duration))
        live.release = lambda: releases.append(True)
        return live, calls, releases

    def test_execute_uses_resolver_context_and_records_held_sample(self):
        initial = {"scene": "gameplay", "now_ms": 900,
                   "controls": {"context": "equipment", "age_ms": 4, "sample_ms": 896}}
        held = {"scene": "gameplay", **self.audit_state(button=1., native_buttons=0x1000,
                                                           sample_ms=901, context="menus")}
        live, calls, releases = self.fake_live([initial, held])
        live.execute({"op": "action", "name": "test.action", "seconds": .03})
        self.assertEqual(len(calls), 1)
        self.assertEqual(len(releases), 1)
        events = [json.loads(line) for line in self.events.path.read_text(encoding="utf-8").splitlines()]
        audit = next(event for event in events if event["event"] == "action_input_audit")
        self.assertEqual(audit["admitted_context"], "equipment")
        self.assertEqual(audit["sampled_context"], "menus")
        self.assertEqual(audit["admission_sample_ms"], 900)
        self.assertEqual(audit["physical"]["buttons"][0], 1.)
        self.assertEqual(audit["sample_ms"], 901)
        self.assertEqual(audit["xr_published_gamepad"]["buttons"], 0x1000)

    def test_tracking_fixture_leaves_normal_semantic_action_and_release_intact(self):
        initial = {"scene": "gameplay", "now_ms": 900, "camera_active": True,
                   "camera_suspended": False, "rendered": {"rig_sequence": 42},
                   "controls": {"context": "equipment", "age_ms": 4, "sample_ms": 896}}
        held = {"scene": "gameplay", **self.audit_state(button=1., native_buttons=0x1000,
                                                       sample_ms=901, context="menus")}
        live, calls, releases = self.fake_live([initial, initial, held])
        live.native = mock.Mock(spec=NativeReader)
        live.native.read.return_value = ({"enabled": True, "armed": True, "mask": 4}, {})
        with mock.patch("gameplay_bot.live.time.monotonic", self.clock), \
             mock.patch("gameplay_bot.live.time.sleep", self.clock.sleep):
            live.execute({"op": "tracking_loss", "mask": 4, "milliseconds": 500})
            live.execute({"op": "action", "name": "test.action", "seconds": .03})
        live.native.read.assert_called_once_with("diagnostics:controller-tracking-loss:4:500")
        self.assertEqual(calls, [([channel("a")], .03)])
        self.assertEqual(releases, [True])
        events = [json.loads(line) for line in self.events.path.read_text(encoding="utf-8").splitlines()]
        self.assertEqual([event["event"] for event in events],
                         ["synthetic_tracking_loss", "semantic_action", "action_channels_held", "action_input_audit"])

    def test_idroid_tap_uses_longer_pulse_below_personal_hold_boundary(self):
        for boundary, expected in ((550, .30), (200, .12)):
            with self.subTest(boundary=boundary):
                self.clock.now = 0.
                initial = {"scene":"cabin", "now_ms":900,
                           "controls":{"context":"equipment", "age_ms":0, "sample_ms":900}}
                held = {"scene":"cabin", **self.audit_state(button=1., sample_ms=901)}
                live, calls, releases = self.fake_live([initial, held])
                action = live.bindings["actions"][0]
                action.update(name="system.idroid")
                action["bindings"][0]["milliseconds"] = boundary
                with mock.patch("gameplay_bot.live.time.monotonic", self.clock), \
                        mock.patch("gameplay_bot.live.time.sleep", self.clock.sleep):
                    live.execute({"op":"action", "name":"system.idroid"})
                self.assertAlmostEqual(calls[0][1], expected)
                self.assertLess(calls[0][1], boundary/1000)
                self.assertEqual(releases, [True])

    def test_operator_forms_menu_chord_before_sending_native_face_edge(self):
        live = Live.__new__(Live)
        live.held = {}
        calls = []
        live.call = lambda method, value: calls.append(value)
        live.input([channel("a"), channel("menu")], .7)
        self.assertEqual([value["component"] for value in calls], ["Menu", "A"])
        self.assertTrue(all(value["value"] == 1. for value in calls))

    def test_grip_chord_does_not_emit_an_unmodified_interact_or_stick_click(self):
        live = Live.__new__(Live)
        live.held = {}
        live.events = self.events
        calls = []
        live.call = lambda method, value: calls.append(value)
        live.input([channel("y"), channel("left_grip")], .5)
        self.assertEqual([value["component"] for value in calls], ["Grip", "Y"])
        calls.clear()
        live.release()
        components = [value["component"] for value in calls]
        self.assertGreater(components.index("Grip"), components.index("Y"))
        self.assertGreater(components.index("Grip"), components.index("ThumbstickClick"))

    def test_partial_chord_rpc_failure_releases_before_failure_capture(self):
        initial = {"scene": "gameplay", "now_ms": 900,
                   "controls": {"context": "equipment", "age_ms": 4, "sample_ms": 896}}
        live, _, releases = self.fake_live([initial])
        live.input = mock.Mock(side_effect=TimeoutError("second channel failed"))
        with self.assertRaisesRegex(TimeoutError, "second channel"):
            live.execute({"op": "action", "name": "test.action", "seconds": .03})
        self.assertEqual(len(releases), 1)

    def test_neutralization_releases_menu_after_chord_children(self):
        live = Live.__new__(Live)
        live.held = {}
        live.events = self.events
        calls = []
        live.call = lambda method, value: calls.append(value)
        live.release()
        self.assertEqual(calls[-1]["component"], "Menu")
        self.assertTrue(all(value["value"] == 0 for value in calls))

    def test_movement_release_is_sent_before_idle_button_cleanup(self):
        live = Live.__new__(Live)
        live.held = {("left", "Thumbstick", "Y"): channel("left_stick_up")}
        live.events = self.events
        calls = []
        live.call = lambda method, value: calls.append(value)
        live.release()
        self.assertEqual((calls[0]["hand"], calls[0]["component"], calls[0]["sub_component"], calls[0]["value"]),
                         ("left", "Thumbstick", "Y", 0))
        self.assertFalse(live.held)

    def test_backend_expiry_is_armed_even_if_runner_cannot_release(self):
        live = Live.__new__(Live)
        live.held = {}
        clock = Clock()
        calls = []
        def call(method, values):
            calls.append((clock.now, values))
            clock.sleep(.04)
        live.call = call
        with mock.patch("gameplay_bot.live.time.monotonic", clock):
            live.input([channel("y"), channel("left_grip")], .8, lease_seconds=2.8)
        self.assertTrue(all(value["auto_release"] for _, value in calls))
        expiry = {value["component"]: started + value["hold_duration"] for started, value in calls}
        self.assertAlmostEqual(expiry["Y"], 2.8)
        self.assertGreater(expiry["Grip"], expiry["Y"])

    def test_failed_release_retains_channel_and_blocks_capture(self):
        live = Live.__new__(Live)
        live.held = {("left", "Thumbstick", "Y"): channel("left_stick_up")}
        live.events = self.events
        live.call = mock.Mock(side_effect=TimeoutError("transport unavailable"))
        with self.assertRaisesRegex(BotFault, "Input release failed"):
            live.release()
        self.assertTrue(live.held)
        live.call.reset_mock()
        with self.assertRaisesRegex(BotFault, "Release input"):
            live.capture("failed-release")
        live.call.assert_not_called()

    def test_ambiguous_operator_timeout_does_not_queue_more_work(self):
        live = Live.__new__(Live)
        live.kernel = mock.Mock()
        live.kernel.WaitForSingleObject.return_value = 258
        live.process_handle = 1
        live.events = self.events
        live.transport_error = None
        live.operator = mock.Mock()
        live.operator.call.side_effect = TimeoutError("completion unknown")
        with self.assertRaises(TimeoutError):
            live.call("set_controller_input", channel("a"))
        with self.assertRaisesRegex(BotFault, "needs recovery"):
            live.call("set_controller_input", {**channel("a"), "value": 0})
        self.assertEqual(live.operator.call.call_count, 1)
        self.assertEqual(live.operator.call.call_args.kwargs["timeout"], 2.)

    def test_neutral_release_has_loading_budget_without_extending_nonzero_input(self):
        live = Live.__new__(Live)
        live.kernel = mock.Mock()
        live.kernel.WaitForSingleObject.return_value = 258
        live.process_handle = 1
        live.events = self.events
        live.transport_error = None
        live.operator = mock.Mock()
        live.call("set_controller_input", {**channel("right_grip"), "value": 0, "auto_release": False})
        self.assertEqual(live.operator.call.call_args.kwargs["timeout"], 15.)
        live.call("set_controller_input", {**channel("right_grip"), "value": 1, "hold_duration": .2})
        self.assertEqual(live.operator.call.call_args.kwargs["timeout"], 2.)

    def test_long_gesture_uses_sampled_hold_time_when_xr_frames_are_slow(self):
        initial = {"scene": "gameplay", "now_ms": 900,
                   "controls": {"context": "equipment", "age_ms": 4, "sample_ms": 896}}
        held = [self.audit_state(button=1., sample_ms=901 + n*100) for n in range(8)]
        live, _, releases = self.fake_live([initial, *held])
        binding = live.bindings["actions"][0]["bindings"][0]
        binding.update(gesture="hold", milliseconds=550)
        clock = Clock()
        with mock.patch("gameplay_bot.live.time.monotonic", clock), \
                mock.patch("gameplay_bot.live.time.sleep", lambda _: clock.sleep(.2)):
            live.execute({"op": "action", "name": "test.action", "seconds": .7})
        self.assertGreater(clock.now, .7)
        self.assertEqual(len(releases), 1)
        events = [json.loads(line) for line in self.events.path.read_text().splitlines()]
        completion = next(event for event in events if event["event"] == "action_hold_completed")
        self.assertEqual(completion["sampled_milliseconds"], 700)

    def test_stalled_xr_sample_cannot_count_as_a_completed_hold(self):
        initial = {"scene": "gameplay", "now_ms": 900,
                   "controls": {"context": "equipment", "age_ms": 4, "sample_ms": 896}}
        live, _, releases = self.fake_live([initial, self.audit_state(button=1., sample_ms=901)])
        live.bindings["actions"][0]["bindings"][0].update(gesture="hold", milliseconds=550)
        clock = Clock()
        with mock.patch("gameplay_bot.live.time.monotonic", clock), \
                mock.patch("gameplay_bot.live.time.sleep", lambda _: clock.sleep(.2)):
            with self.assertRaisesRegex(BotFault, "hold duration"):
                live.execute({"op": "action", "name": "test.action", "seconds": .7})
        self.assertLess(clock.now, 3.)
        self.assertEqual(len(releases), 1)

    def test_missing_audit_does_not_erase_sampled_physical_hold(self):
        initial = {"scene": "gameplay", "now_ms": 900,
                   "controls": {"context": "equipment", "age_ms": 4, "sample_ms": 896}}
        live, _, releases = self.fake_live([initial, self.audit_state(button=1., sample_ms=901),
                                           {"controls": None}, self.audit_state(button=1., sample_ms=1601)])
        live.bindings["actions"][0]["bindings"][0].update(gesture="hold", milliseconds=550)
        with mock.patch("gameplay_bot.live.time.monotonic", self.clock), \
                mock.patch("gameplay_bot.live.time.sleep", self.clock.sleep):
            live.execute({"op": "action", "name": "test.action", "seconds": .7})
        self.assertEqual(len(releases), 1)

    def test_fresh_released_sample_breaks_hold_even_when_later_held_again(self):
        initial = {"scene": "gameplay", "now_ms": 900,
                   "controls": {"context": "equipment", "age_ms": 4, "sample_ms": 896}}
        live, _, releases = self.fake_live([initial, self.audit_state(button=1., sample_ms=901),
                                           self.audit_state(button=0., sample_ms=1401),
                                           self.audit_state(button=1., sample_ms=1601)])
        live.bindings["actions"][0]["bindings"][0].update(gesture="hold", milliseconds=550)
        with mock.patch("gameplay_bot.live.time.monotonic", self.clock), \
                mock.patch("gameplay_bot.live.time.sleep", lambda _: self.clock.sleep(.2)):
            with self.assertRaisesRegex(BotFault, "hold duration"):
                live.execute({"op": "action", "name": "test.action", "seconds": .7})
        self.assertEqual(len(releases), 1)

    def test_presentation_recovery_never_turns_off_a_camera_waiting_for_player(self):
        waiting = {"camera_available": True, "camera_active": False, "camera_pending": False,
                   "camera_awaiting_player": True, "camera_reason": 7}
        for target in ("immersive", "quad", None):
            with self.subTest(target=target), self.assertRaises(BotFault):
                require_presentation_transition(waiting, target)
        manual_quad = {**waiting, "camera_awaiting_player": False, "camera_reason": 1}
        require_presentation_transition(manual_quad, "immersive")
        require_presentation_transition({**waiting, "camera_active": True}, "quad")
        for state in ({**manual_quad, "camera_reason": 2}, {}, {**manual_quad, "camera_pending": True}):
            with self.subTest(state=state), self.assertRaises(BotFault):
                require_presentation_transition(state, "immersive")

    def test_held_action_records_checkpoint_without_blocking_release_for_capture(self):
        initial = {"scene": "gameplay", "now_ms": 900,
                   "controls": {"context": "equipment", "age_ms": 4, "sample_ms": 896}}
        held = {**self.audit_state(button=1., sample_ms=901), "native": {"aim": True}}
        live, calls, releases = self.fake_live([initial, held])
        live.bindings["actions"][0]["bindings"][0].update(gesture="level", milliseconds=0)
        live.capture = mock.Mock(side_effect=TimeoutError("capture must never block held input"))
        live.pose_recording = True
        live.pose_snapshot = mock.Mock(side_effect=TimeoutError("pose query must not block held input"))
        with mock.patch("gameplay_bot.live.time.monotonic", self.clock), \
                mock.patch("gameplay_bot.live.time.sleep", self.clock.sleep):
            observed = live.execute({"op": "action", "name": "test.action", "seconds": .05,
                                     "while_held": {"native.aim": True}, "capture_while_held": "aim"})
        self.assertEqual(observed["state"]["native"], {"aim": True})
        self.assertEqual(observed["captures"], [])
        self.assertEqual(observed["visual_checkpoint"]["source"], "state_only_no_held_video")
        self.assertFalse(observed["visual_checkpoint"]["final_compositor_capture"])
        live.capture.assert_not_called()
        live.pose_snapshot.assert_not_called()
        self.assertEqual(len(releases), 1)

    def test_explicit_grip_capture_releases_both_hands_on_success_failure_and_expiry(self):
        import copy
        for outcome in ("success", "capture_failure", "expired"):
            with self.subTest(outcome=outcome):
                initial = {"scene": "gameplay", "now_ms": 900,
                           "controls": {"context": "gameplay", "age_ms": 4, "sample_ms": 896}}
                held = self.audit_state(sample_ms=901)
                held.update(camera_active=True, menu=False, rendered={"weapon_support_attached": True})
                held["controls"].update(rig_input=True)
                held["controls"]["physical"][7] = held["controls"]["physical"][8] = 1.
                live, calls, releases = self.fake_live([initial, held])
                live.bindings = {"actions": [
                    {"name": "gameplay.ready_weapon", "label": "Ready", "modifier": True,
                     "contexts": ["gameplay"], "bindings": [
                         {"gesture": "level", "milliseconds": 0, "inputs": ["right_grip"]}]},
                    {"name": "gameplay.support_grip", "label": "Support", "modifier": True,
                     "contexts": ["gameplay"], "bindings": [
                         {"gesture": "level", "milliseconds": 0, "inputs": ["left_grip"]}]}]}
                original = copy.deepcopy(live.bindings)
                def dispatch(values, duration, **options):
                    calls.append((values, duration))
                    live.held = {(v["hand"], v["component"], v.get("sub_component")): v for v in values}
                def release():
                    releases.append(True)
                    live.held.clear()
                def capture(*args, **options):
                    if outcome == "capture_failure":
                        raise TimeoutError("capture failed")
                    if outcome == "expired":
                        held["controls"]["physical"][8] = 0.
                    return ["left.png", "right.png"]
                live.input, live.release = dispatch, release
                live.capture = mock.Mock(side_effect=capture)
                step = {"op": "action", "name": "gameplay.ready_weapon", "seconds": .05,
                        "include_support": True, "static_grip_capture": True,
                        "while_held": {"rendered.weapon_support_attached": True}, "capture_while_held": "support"}
                with mock.patch("gameplay_bot.live.time.monotonic", self.clock), \
                        mock.patch("gameplay_bot.live.time.sleep", self.clock.sleep):
                    if outcome == "success":
                        observed = live.execute(step)
                        self.assertEqual(observed["captures"], ["left.png", "right.png"])
                        self.assertEqual(observed["visual_checkpoint"]["source"], "final_compositor_static_grips")
                    else:
                        expected = TimeoutError if outcome == "capture_failure" else BotFault
                        with self.assertRaises(expected):
                            live.execute(step)
                self.assertEqual({v["hand"] for v in calls[0][0]}, {"left", "right"})
                self.assertEqual(live.bindings, original)
                live.capture.assert_called_once_with("support", static_grips=True)
                self.assertEqual(len(releases), 1)
                self.assertFalse(live.held)

    def test_missing_held_outcome_releases_without_crediting_dispatch(self):
        initial = {"scene": "gameplay", "now_ms": 900,
                   "controls": {"context": "equipment", "age_ms": 4, "sample_ms": 896}}
        held = {**self.audit_state(button=1., sample_ms=901), "native": {"aim": False}}
        live, _, releases = self.fake_live([initial, held])
        live.bindings["actions"][0]["bindings"][0].update(gesture="level", milliseconds=0)
        with mock.patch("gameplay_bot.live.time.monotonic", self.clock), \
                mock.patch("gameplay_bot.live.time.sleep", self.clock.sleep):
            with self.assertRaisesRegex(BotFault, "while-held native outcome"):
                live.execute({"op": "action", "name": "test.action", "seconds": .05,
                              "while_held": {"native.aim": True}})
        self.assertEqual(len(releases), 1)

    def test_case_checks_held_transition_then_neutral_exit(self):
        adapter = Adapter([{"native": {"aim": False}}])
        adapter.execute = mock.Mock(return_value={"state": {"native": {"aim": True}}, "captures": ["held"]})
        case = {"id": "aim", "before": {"native.aim": False}, "during": {"native.aim": True},
                "after": {"native.aim": False}, "steps": [{"op": "action", "name": "aim"}]}
        result = self.behavior(adapter).case(case)
        self.assertEqual(result["status"], "observed_pass")
        self.assertTrue(result["during"]["native"]["aim"])
        self.assertFalse(result["after"]["native"]["aim"])
        self.assertEqual(result["during_captures"], ["held"])

    def test_case_cannot_credit_held_state_that_was_already_active(self):
        adapter = Adapter([{"native": {"aim": True}}])
        case = {"id": "aim", "before": {"native.aim": True}, "during": {"native.aim": True},
                "after": {"native.aim": False}, "steps": [{"op": "action", "name": "aim"}]}
        result = self.behavior(adapter).case(case)
        self.assertEqual(result["status"], "failed")
        self.assertEqual(adapter.actions, 0)

    def test_execute_rejects_successful_rpc_without_observed_physical_hold(self):
        initial = {"scene": "gameplay", "now_ms": 900,
                   "controls": {"context": "equipment", "age_ms": 4, "sample_ms": 896}}
        unobserved = {"scene": "gameplay", **self.audit_state(button=0., native_buttons=0x1000,
                                                                  sample_ms=901)}
        live, calls, releases = self.fake_live([initial, unobserved])
        with self.assertRaisesRegex(BotFault, "never observed held"):
            live.execute({"op": "action", "name": "test.action", "seconds": .03})
        self.assertEqual(len(calls), 1)
        self.assertEqual(len(releases), 1)

    def test_execute_rejects_old_held_sample_and_still_releases(self):
        initial = {"scene": "gameplay", "now_ms": 900,
                   "controls": {"context": "equipment", "age_ms": 4, "sample_ms": 896}}
        old_sample = {"scene": "gameplay", **self.audit_state(button=1., native_buttons=0x1000,
                                                                 sample_ms=900)}
        live, calls, releases = self.fake_live([initial, old_sample])
        with self.assertRaisesRegex(BotFault, "never observed held"):
            live.execute({"op": "action", "name": "test.action", "seconds": .03})
        self.assertEqual(len(calls), 1)
        self.assertEqual(len(releases), 1)

    def test_recording_fault_aborts_on_next_observation_and_releases_input(self):
        initial = {"scene": "gameplay", "now_ms": 900,
                   "controls": {"context": "equipment", "age_ms": 4, "sample_ms": 896}}
        held = {"scene": "gameplay", **self.audit_state(button=1., native_buttons=0x1000,
                                                           sample_ms=901)}
        live, calls, releases = self.fake_live([initial, held])
        failed = [False]
        def check_recording():
            if failed[0]:
                raise BotFault("Native recording failed during run: disk reserve")
        live.background_check = check_recording
        def begin_input(values, duration, **options):
            calls.append((values, duration))
            failed[0] = True
        live.input = begin_input
        with self.assertRaisesRegex(BotFault, "disk reserve"):
            live.execute({"op": "action", "name": "test.action", "seconds": .03})
        self.assertEqual(len(calls), 1)
        self.assertEqual(len(releases), 1)

    def test_held_stick_audit_preserves_direction(self):
        state = self.audit_state(sticks=[0., -1., 0., 0.])
        evidence = held_input_evidence(state, ["left_stick_down"])
        self.assertEqual(evidence["physical"]["sticks"][1], -1.)
        self.assertIsNone(held_input_evidence(self.audit_state(sticks=[0., 1., 0., 0.]), ["left_stick_down"]))

    def test_recording_rotation_changes_request_only_after_ownership_check(self):
        game = pathlib.Path(self.directory.name) / "game"
        game.mkdir()
        class Process:
            def poll(self):
                return None
            def wait(self, timeout=None):
                return 0
        recording = Recording(game, self.events, 123)
        with mock.patch("gameplay_bot.recording.subprocess.Popen", return_value=Process()) as launch:
            recording.new_segment()
            first = recording.segments[0]
            recording.new_segment()
            second = recording.segments[1]
            self.assertTrue(first["stop"].exists())
            self.assertEqual(recording.request.read_text(encoding="utf-8"), str(second["video"]))
            recording.request.write_text("foreign-request.mp4", encoding="utf-8")
            with self.assertRaisesRegex(BotFault, "ownership changed"):
                recording.new_segment()
            self.assertEqual(len(recording.segments), 2)
            self.assertEqual(launch.call_count, 2)
            self.assertEqual(recording.request.read_text(encoding="utf-8"), "foreign-request.mp4")

    def test_recording_rotation_rechecks_owner_after_starting_candidate(self):
        game = pathlib.Path(self.directory.name) / "game"
        game.mkdir()
        class Process:
            def poll(self):
                return None
            def wait(self, timeout=None):
                return 0
        recording = Recording(game, self.events, 123)
        calls = 0
        def launch(*args, **kwargs):
            nonlocal calls
            calls += 1
            if calls == 2:
                recording.request.write_text("external-owner.mp4", encoding="utf-8")
            return Process()
        with mock.patch("gameplay_bot.recording.subprocess.Popen", side_effect=launch):
            recording.new_segment()
            with self.assertRaisesRegex(BotFault, "during rotation"):
                recording.new_segment()
        self.assertEqual(len(recording.segments), 1)
        self.assertFalse(recording.segments[0]["stop"].exists())
        self.assertTrue((recording.output / "segment-0001.stop").exists())
        self.assertEqual(recording.request.read_text(encoding="utf-8"), "external-owner.mp4")

    def test_recording_error_or_incomplete_segment_fails_run_contract(self):
        self.assertEqual(Recording.failure_reason({"error": "audio recorder exited", "segments": []}),
                         "audio recorder exited")
        self.assertEqual(Recording.failure_reason({"error": None, "segments": [{"status": "failed", "video": "segment.mp4"}]}),
                         "Recording segment failed: segment.mp4")
        self.assertIsNone(Recording.failure_reason({"error": None, "segments": [{"status": "captured"}]}))

    def test_recording_worker_error_is_visible_to_next_observation(self):
        game = pathlib.Path(self.directory.name) / "game"
        game.mkdir()
        recording = Recording(game, self.events, 123)
        recording.error = "disk reserve"
        with self.assertRaisesRegex(BotFault, "failed during run: disk reserve"):
            recording.check()

    def test_recording_start_waits_for_post_request_encoded_first_frame(self):
        game = pathlib.Path(self.directory.name) / "game"
        game.mkdir()
        recording = Recording(game, self.events, 123)
        clock = Clock()

        class NoThread:
            def __init__(self, **_): pass
            def start(self): pass

        def prepare_segment():
            video = recording.output / "segment-0000.mp4"
            recording.segments.append({"video": video})
            recording.request.write_text(str(video), encoding="utf-8")

        calls = []
        def advance(duration):
            clock.sleep(duration)
            calls.append(clock.now)
            if len(calls) == 1:
                with recording.log_path.open("ab") as log:
                    log.write((f"1790520000000 Native video recording {recording.segments[0]['video']} at 1920x1080 / 30 FPS\n"
                               "1790520000001 Native video first frame encoded D:\\foreign\\segment.mp4 qpc_100ns=1234\n"
                               f"1790520000002 Native video first frame encoded {recording.segments[0]['video']} qpc_100ns=5678").encode())
            elif len(calls) == 2:
                with recording.log_path.open("ab") as log:
                    log.write(b"\r\n")

        with mock.patch.object(recording, "budget"), \
             mock.patch.object(recording, "new_segment", side_effect=prepare_segment), \
             mock.patch("gameplay_bot.recording.threading.Thread", NoThread):
            recording.start(timeout=.5, clock=clock, sleep=advance)
            dispatch_at = clock.now  # The real runner reaches case dispatch only after start returns.

        self.assertEqual(dispatch_at, .1)
        events = [json.loads(line) for line in (pathlib.Path(self.directory.name) / "events.jsonl").read_text().splitlines()]
        ready = [event for event in events if event["event"] == "recording_segment_ready"]
        self.assertEqual(len(ready), 1)
        self.assertEqual(ready[0]["first_frame_qpc_100ns"], 5678)
        self.assertEqual(ready[0]["source"], "native_video_first_frame_encoded")

    def test_recording_armed_or_stale_or_foreign_marker_never_unlocks(self):
        game = pathlib.Path(self.directory.name) / "game"
        game.mkdir()
        recording = Recording(game, self.events, 123)
        video = recording.output / "segment-0000.mp4"
        recording.segments.append({"video": video})
        recording.request.write_text(str(video), encoding="utf-8")
        stale = f"1790510000000 Native video first frame encoded {video} qpc_100ns=4567\n".encode()
        recording.log_path.write_bytes(stale)
        recording.log_start_offset = len(stale)
        with recording.log_path.open("ab") as log:
            log.write((f"1790520000000 Native video recording {video} at 1920x1080 / 30 FPS\n"
                       "1790520000001 Native video first frame encoded D:\\foreign\\segment.mp4 qpc_100ns=5678\n").encode())
        clock = Clock()
        with self.assertRaisesRegex(BotFault, "first-frame readiness timed out"):
            recording.wait_for_first_frame(timeout=.1, clock=clock, sleep=clock.sleep)
        self.assertAlmostEqual(clock.now, .1)
        events_path = pathlib.Path(self.directory.name) / "events.jsonl"
        self.assertFalse(events_path.exists())

    def test_first_frame_wait_stops_if_recording_request_is_replaced(self):
        game = pathlib.Path(self.directory.name) / "game"
        game.mkdir()
        recording = Recording(game, self.events, 123)
        recording.segments.append({"video": recording.output / "segment-0000.mp4"})
        recording.request.write_text("D:\\foreign\\owned.mp4", encoding="utf-8")
        with self.assertRaisesRegex(BotFault, "ownership changed before first source frame"):
            recording.wait_for_first_frame(timeout=.1, clock=Clock(), sleep=Clock().sleep)

    def test_mux_pads_only_measured_audio_tail_and_never_shortens_video(self):
        video = {"first_qpc_100ns": 100_000_000, "last_qpc_100ns": 408_010_000,
                 "frames": 849}
        audio = {"first_qpc_100ns": 99_017_639, "frames": 1_441_920, "sample_rate": 48_000}
        plan = mux_timeline(video, audio)
        self.assertAlmostEqual(plan["video_duration_seconds"], 30.8343333)
        self.assertAlmostEqual(plan["source_audio_duration_seconds"], 30.04)
        self.assertAlmostEqual(plan["audio_offset_seconds"], -.0982361)
        self.assertAlmostEqual(plan["audio_tail_padding_seconds"], .8925694)
        self.assertEqual(plan["audio_filter"], "atrim=start=0.0982361,asetpts=PTS-STARTPTS,apad=pad_dur=0.8925694")
        self.assertIn("not captured game audio", plan["audio_tail_padding_reason"])

        # The raw MP4 duration is authoritative for its muxed copy; QPC may
        # round by about a millisecond at the container's final sample.
        raw = mux_timeline(video, audio, 30.8333)
        self.assertEqual(raw["video_duration_seconds"], 30.8333)
        self.assertAlmostEqual(raw["audio_tail_padding_seconds"], .8915361)

    def test_native_accepted_samples_are_distinct_from_cfr_encoded_frames(self):
        # Actual source-showcase-03 values: MF emitted an 804-frame CFR stream
        # spanning 26.8s after accepting 796 source samples and reporting 8
        # dropped samples. CFR fill frames must not make a valid take fail.
        video = {"frames": 796, "dropped": 8,
                 "first_qpc_100ns": 264021688948,
                 "last_qpc_100ns": 264289378966}
        stats = validate_raw_video(video, 804, 26.8, 30.)
        self.assertEqual(stats["native_accepted_sample_count"], 796)
        self.assertEqual(stats["native_dropped_sample_count"], 8)
        self.assertEqual(stats["raw_video_encoded_frame_count"], 804)
        self.assertEqual(stats["cfr_repeat_or_fill_frame_count"], 8)
        self.assertAlmostEqual(stats["native_qpc_span_seconds"], 26.7690018, places=5)
        with self.assertRaisesRegex(BotFault, "QPC sample window"):
            validate_raw_video(video, 900, 30., 30.)

    def test_recording_close_muxes_and_reports_accepted_and_encoded_counts(self):
        game = pathlib.Path(self.directory.name) / "game"
        game.mkdir()
        recording = Recording(game, self.events, 123)
        video_path = recording.output / "segment-0000.mp4"
        audio_path = recording.output / "segment-0000.wav"
        stop_path = recording.output / "segment-0000.stop"
        video_path.write_bytes(b"native mp4 fixture")
        audio_path.write_bytes(b"native pcm fixture")
        video = {"frames": 796, "dropped": 8, "width": 1920, "height": 1080,
                 "first_qpc_100ns": 264021688948, "last_qpc_100ns": 264289378966,
                 "complete": True, "eye": 1}
        audio = {"first_qpc_100ns": 264021973034, "last_qpc_100ns": 264287473034,
                 "frames": 1274880, "sample_rate": 48000, "gaps": 0}
        pathlib.Path(str(video_path) + ".json").write_text(json.dumps(video), encoding="utf-8")
        pathlib.Path(str(audio_path) + ".json").write_text(json.dumps(audio), encoding="utf-8")

        class Process:
            def wait(self, timeout=None):
                return 0

        recording.segments.append({"video": video_path, "audio": audio_path,
                                   "stop": stop_path, "process": Process()})
        recording.request.write_text(str(video_path), encoding="utf-8")
        def fake_run(command, **_kwargs):
            if command[0] == "ffmpeg":
                pathlib.Path(command[-1]).write_bytes(b"muxed mp4 fixture")
                return mock.Mock(stdout="")
            is_mux = str(video_path) not in command
            payload = {"streams": [{"duration": "26.800000", "nb_frames": "804"}]}
            if not is_mux:
                payload["streams"][0]["avg_frame_rate"] = "30/1"
            return mock.Mock(stdout=json.dumps(payload))

        with mock.patch("gameplay_bot.recording.subprocess.run", side_effect=fake_run):
            result = recording.close()
        self.assertIsNone(Recording.failure_reason(result))
        segment = result["segments"][0]
        self.assertEqual(segment["status"], "captured")
        self.assertEqual(segment["native_accepted_sample_count"], 796)
        self.assertEqual(segment["native_dropped_sample_count"], 8)
        self.assertEqual(segment["raw_video_encoded_frame_count"], 804)
        self.assertEqual(segment["muxed_video_frame_count"], 804)
        self.assertEqual(segment["cfr_repeat_or_fill_frame_count"], 8)
        self.assertEqual(segment["audio_tail_padding_samples"], 10156)
        self.assertIn("not captured game audio", segment["audio_tail_padding_reason"])
        self.assertTrue(pathlib.Path(segment["output"]).is_file())
        self.assertEqual(recording.request.read_text(encoding="utf-8"), "")

    def test_session_recording_gate_starts_only_after_verified_arrival(self):
        events = []
        def enter():
            events.append("continue")
            return {"status": "observed_arrival"}
        gate = after_verified_arrival(enter, lambda: events.append("record"))
        self.assertEqual(gate(), {"status": "observed_arrival"})
        self.assertEqual(events, ["continue", "record"])

        events.clear()
        def blocked():
            events.append("continue")
            return {"status": "attention_required"}
        with self.assertRaisesRegex(BotFault, "recording and cases were not started"):
            after_verified_arrival(blocked, lambda: events.append("record"))()
        self.assertEqual(events, ["continue"])

    def test_mux_delays_late_audio_and_adds_no_padding_when_measured_audio_covers_video(self):
        video = {"first_qpc_100ns": 100_000_000, "last_qpc_100ns": 100_000_000,
                 "frames": 1}
        audio = {"first_qpc_100ns": 110_000_000, "frames": 10_000, "sample_rate": 48_000}
        plan = mux_timeline(video, audio)
        self.assertEqual(plan["audio_filter"], "adelay=48000S:all=1")
        self.assertEqual(plan["audio_tail_padding_seconds"], 0.)
        self.assertIsNone(plan["audio_tail_padding_reason"])

    def graph(self):
        return {"identity": "world-A", "capability": "standing-player", "nodes": {n: {} for n in "abcd"},
                "edges": [{"from": "a", "to": "d", "cost": .1, "validated": False},
                          {"from": "a", "to": "b", "cost": 1, "validated": True},
                          {"from": "b", "to": "c", "cost": 1, "validated": True},
                          {"from": "c", "to": "d", "cost": 1, "validated": True}]}

    def route(self, graph, **extra):
        return astar(graph, "a", "d", identity="world-A", capability="standing-player", **extra)

    def test_unknown_shortcut_cannot_cut_through_obstacle(self):
        self.assertEqual(self.route(self.graph()), list("abcd"))

    def test_blocked_route_and_other_scene_are_rejected(self):
        with self.assertRaisesRegex(BotFault, "No validated route"):
            self.route(self.graph(), blocked=[("b", "c")])
        graph = self.graph()
        graph["identity"] = "world-B"
        with self.assertRaises(BotFault):
            self.route(graph)

    def test_stacked_floors_are_distinct_nodes(self):
        graph = self.graph()
        graph["nodes"]["d"] = {"position": [0, 3, 0], "floor": "upper"}
        graph["nodes"]["a"] = {"position": [0, 0, 0], "floor": "lower"}
        self.assertEqual(len(self.route(graph)), 4)

    def test_no_motion_triggers_bounded_recovery(self):
        guard = ProgressGuard(timeout=2)
        guard.observe(10, 0)
        guard.observe(9, 1)
        guard.observe(9.02, 2)
        with self.assertRaisesRegex(BotFault, "No native movement"):
            guard.observe(9.01, 3)


class SessionTests(unittest.TestCase):
    @staticmethod
    def suite():
        return {"cases": [
            {"id": "open", "before": {"menu": False}, "after": {"menu": True},
             "steps": [{"op": "action", "name": "system.pause"}]},
            {"id": "close", "before": {"menu": True}, "after": {"menu": False},
             "steps": [{"op": "action", "name": "menus.back"}]},
        ]}

    def test_arrival_and_all_actions_share_adapter_without_fixed_pauses(self):
        adapter = Adapter([{"menu": flag} for flag in (False, False, True, True, True, True, False, False)])
        clock = Clock()
        behavior = Behaviors(adapter, mock.Mock(), clock, clock.sleep)
        arrival = mock.Mock(return_value={"status": "observed_arrival"})
        checkpoints = []
        result = run_suite(behavior, self.suite(), lambda records: checkpoints.append(len(records)), arrival)
        self.assertEqual(result["status"], "observed_pass")
        self.assertEqual(result["not_run"], [])
        self.assertEqual(adapter.actions, 2)
        self.assertEqual(checkpoints, [1, 2])
        arrival.assert_called_once_with()
        self.assertAlmostEqual(clock.now, .4)

    def test_ambiguous_case_stops_dependent_queue_without_replay(self):
        behavior = mock.Mock()
        behavior.case.return_value = {"id": "open", "status": "failed", "error": "outcome unknown"}
        result = run_suite(behavior, self.suite(), mock.Mock())
        self.assertEqual(result["status"], "failed")
        self.assertEqual(result["not_run"], ["close"])
        behavior.case.assert_called_once()

    def test_bad_queue_is_rejected_before_arrival(self):
        suite = self.suite()
        suite["cases"][1]["id"] = "open"
        arrival = mock.Mock()
        with self.assertRaises(BotFault):
            run_suite(mock.Mock(), suite, mock.Mock(), arrival)
        arrival.assert_not_called()

    def test_suite_setting_requirement_is_checked_before_startup_or_input(self):
        suite = self.suite()
        suite["required_settings"] = {"settings.handheld_menus": 1}
        adapter = Adapter([])
        adapter.bindings = {"settings": [{"name": "settings.handheld_menus", "value": 0}]}
        behavior = Behaviors(adapter, mock.Mock())
        arrival = mock.Mock()
        with self.assertRaisesRegex(BotFault, "effective setting is 0"):
            run_suite(behavior, suite, mock.Mock(), arrival)
        arrival.assert_not_called()
        self.assertEqual(adapter.actions, 0)

    def test_failed_arrival_never_dispatches_actions(self):
        behavior = mock.Mock()
        with self.assertRaisesRegex(BotFault, "did not observe arrival"):
            run_suite(behavior, self.suite(), mock.Mock(), lambda: {"status": "failed"})
        behavior.case.assert_not_called()

    def test_known_outcome_failure_can_skip_dependency_and_run_independent_case(self):
        suite = self.suite()
        suite["continue_after_outcome_failure"] = True
        suite["cases"][1]["depends_on"] = ["open"]
        suite["cases"].append({**suite["cases"][0], "id": "independent"})
        behavior = mock.Mock()
        behavior.case.side_effect = [
            {"id": "open", "status": "failed", "failure_phase": "outcome", "entry_predicates_still_match": True},
            {"id": "independent", "status": "observed_pass"},
        ]
        result = run_suite(behavior, suite, mock.Mock())
        self.assertEqual([case["status"] for case in result["cases"]], ["failed", "skipped", "observed_pass"])
        self.assertEqual(result["status"], "failed")
        self.assertEqual(behavior.case.call_count, 2)
        self.assertEqual(result["not_run"], [])

    def test_independent_policy_never_continues_ambiguous_dispatch_or_release(self):
        for details in ({"failure_phase": "dispatch"},
                        {"failure_phase": "outcome", "release_error": "release not acknowledged"}):
            with self.subTest(details=details):
                suite = self.suite()
                suite["continue_after_outcome_failure"] = True
                behavior = mock.Mock()
                behavior.case.return_value = {"id": "open", "status": "failed", "entry_predicates_still_match": True, **details}
                result = run_suite(behavior, suite, mock.Mock())
                self.assertEqual(result["not_run"], ["close"])
                behavior.case.assert_called_once()


class MenuCleanupTests(unittest.TestCase):
    @staticmethod
    def fixture(depth, popup=False):
        live = Live.__new__(Live)
        live.opened_menu = "idroid"
        live.release = mock.Mock()
        live.events = mock.Mock()
        remaining = [depth]
        def state(native=False):
            active = remaining[0] > 0
            return {"menu":active,"idroid":active,"pause":False,
                    "scene":"cabin","camera_active":True,"camera_suspended":False,
                    "camera_awaiting_player":False,"controls":{"rig_input":True},
                    "native":{"popup":popup,"tutorial_pause":False}}
        live.observe = mock.Mock(side_effect=state)
        def execute(step):
            remaining[0] -= 1
        live.execute = mock.Mock(side_effect=execute)
        return live

    def test_three_native_levels_unwind_with_ordinary_guarded_back(self):
        live = self.fixture(3)
        with mock.patch("gameplay_bot.live.time.sleep"):
            live.cleanup_menus()
        self.assertIsNone(live.opened_menu)
        self.assertEqual(live.execute.call_count, 3)
        for call in live.execute.call_args_list:
            self.assertEqual(call.args[0]["name"], "menus.back")
            self.assertEqual(call.args[0]["native_before"], {"popup":False,"tutorial_pause":False})

    def test_cleanup_stops_at_unknown_prompt_and_is_bounded_on_nonresponsive_menu(self):
        live = self.fixture(3, popup=True)
        with self.assertRaisesRegex(BotFault, "needs review"):
            live.cleanup_menus()
        live.execute.assert_not_called()
        live = self.fixture(10)
        with mock.patch("gameplay_bot.live.time.sleep"), self.assertRaisesRegex(BotFault, "did not close"):
            live.cleanup_menus()
        self.assertEqual(live.execute.call_count, 4)

    def test_close_after_cleanup_attempt_releases_without_replaying_navigation(self):
        live = self.fixture(3)
        live.cleanup_menus = mock.Mock(side_effect=BotFault("completion unknown"))
        live.operator = mock.Mock()
        live.process_handle = None
        live.close(cleanup=False)
        live.release.assert_called_once()
        live.cleanup_menus.assert_not_called()
        live.operator.close.assert_called_once()

    def test_idroid_help_overlay_cannot_receive_automatic_cleanup_input(self):
        live = self.fixture(3)
        live.observe.return_value = {"menu":True,"idroid":True,"pause":True,
                                   "native":{"popup":False,"tutorial_pause":False}}
        live.observe.side_effect = None
        with self.assertRaisesRegex(BotFault, "needs review"):
            live.cleanup_menus()
        live.execute.assert_not_called()

    def test_closed_terminal_with_stale_camera_cannot_pass_cleanup(self):
        live = self.fixture(0)
        live.observe.side_effect = None
        live.observe.return_value = {"menu":False,"scene":"unknown","camera_active":False,
                                    "camera_awaiting_player":True,"controls":{"rig_input":False},
                                    "native":{"popup":False,"tutorial_pause":False}}
        with mock.patch("gameplay_bot.live.time.monotonic", side_effect=[0,4]), \
                self.assertRaisesRegex(BotFault, "without restoring the live VR camera"):
            live.cleanup_menus()
        live.execute.assert_not_called()
        self.assertEqual(live.opened_menu,"idroid")

    def test_expired_observation_during_stow_requires_three_new_playable_samples(self):
        for missing in ("controls", "native"):
            with self.subTest(missing=missing):
                live = self.fixture(0)
                ready = live.observe(native=True)
                pending = {**ready, missing:None}
                live.observe.reset_mock()
                live.observe.side_effect = [ready, pending, ready, ready, ready]
                clock = Clock()
                with mock.patch("gameplay_bot.live.time.monotonic", clock), \
                        mock.patch("gameplay_bot.live.time.sleep", clock.sleep):
                    live._finish_menu_cleanup(pending)
                self.assertEqual(live.observe.call_count, 5)
                self.assertIsNone(live.opened_menu)
                live.execute.assert_not_called()

    def test_missing_observation_never_passes_cleanup_and_expires_without_input(self):
        for missing in ("controls", "native"):
            with self.subTest(missing=missing):
                live = self.fixture(0)
                pending = {**live.observe(native=True), missing:None}
                live.observe.side_effect = None
                live.observe.return_value = pending
                clock = Clock()
                with mock.patch("gameplay_bot.live.time.monotonic", clock), \
                        mock.patch("gameplay_bot.live.time.sleep", clock.sleep), \
                        self.assertRaisesRegex(BotFault, "without restoring the live VR camera"):
                    live._finish_menu_cleanup(pending)
                self.assertGreaterEqual(clock.now, 3)
                self.assertEqual(live.opened_menu, "idroid")
                live.execute.assert_not_called()


class StartupTests(unittest.TestCase):
    @staticmethod
    def stage(sequence, popup=False, menu=False):
        return {"scene": "title", "title_menu": menu, "press_start_ready": True,
                "native": {"sequence": sequence, "title": True, "popup": popup}}

    def test_prompts_appearing_after_logo_are_handled_once(self):
        prepare = self.stage("Seq_Mission_Prepare")
        autosave = self.stage("Seq_Demo_ConfirmAutoSave", popup=True)
        start = self.stage("Seq_Demo_StartHasTitleMission")
        menu = self.stage("Seq_Demo_StartHasTitleMission", menu=True)
        live = Adapter([prepare, prepare, autosave, autosave, start, start, menu])
        live.events = mock.Mock()
        live.execute = mock.Mock()
        clock = Clock()
        self.assertIs(advance_startup(live, clock=clock, sleep=clock.sleep), menu)
        self.assertEqual(live.execute.call_count, 2)
        self.assertEqual([call.args[0]["native_before"]["sequence"] for call in live.execute.call_args_list],
                         ["Seq_Demo_ConfirmAutoSave", "Seq_Demo_StartHasTitleMission"])
        self.assertEqual([call.args[0]["name"] for call in live.execute.call_args_list],
                         ["menus.confirm", "system.idroid"])
        self.assertEqual(live.execute.call_args_list[-1].args[0]["state_before"],
                         {"title_menu": False, "press_start_ready": True})
        self.assertLess(clock.now, 1.)

    def test_login_progress_and_results_complete_without_bot_confirmation(self):
        login = {"scene": "cinematic", "title_menu": False,
                 "native": {"mission": 1, "sequence": "Seq_Demo_LogInKonamiServer",
                            "title": False, "popup": True}}
        closed = {**login, "native": {**login["native"], "popup": False}}
        menu = self.stage("Seq_Demo_StartHasTitleMission", menu=True)
        live = Adapter([login, login, closed, login, login, menu])
        live.events = mock.Mock(); live.capture = mock.Mock(); clock = Clock()
        self.assertIs(advance_startup(live, clock=clock, sleep=clock.sleep), menu)
        self.assertEqual(live.actions, 0)
        live.capture.assert_not_called()

    def test_verified_completed_login_notice_is_confirmed_once_with_identity_guard(self):
        notice = {"scene": "cinematic", "title_menu": False,
                  "native": {"mission": 1, "sequence": "Seq_Demo_LogInKonamiServer",
                             "title": False, "popup": True},
                  "popup_observer": {"reader_verified": True, "owner_verified": True,
                                     "active": True, "numeric_id": 1,
                                     "string_id": "0x4dc3cae5b486", "last_result": 0}}
        menu = self.stage("Seq_Demo_StartHasTitleMission", menu=True)
        live = mock.Mock(); clock = Clock()
        live.observe.side_effect = [notice, notice, menu]
        self.assertIs(advance_startup(live, clock=clock, sleep=clock.sleep), menu)
        live.execute.assert_called_once()
        step = live.execute.call_args.args[0]
        self.assertEqual(step["name"], "menus.confirm")
        self.assertEqual(step["state_before"]["popup_observer.string_id"], "0x4dc3cae5b486")
        self.assertEqual(step["native_before"]["sequence"], "Seq_Demo_LogInKonamiServer")

    def test_login_identity_rejects_unknown_progress_result_closed_and_unverified_readers(self):
        for change in ({"string_id": "0x1234"}, {"string_id": None},
                       {"numeric_id": 2}, {"active": False}, {"active": None},
                       {"reader_verified": False}, {"owner_verified": None}):
            with self.subTest(change=change):
                notice = {"scene": "cinematic", "title_menu": False,
                          "native": {"mission": 1, "sequence": "Seq_Demo_LogInKonamiServer",
                                     "title": False, "popup": True},
                          "popup_observer": {"reader_verified": True, "owner_verified": True,
                                             "active": True, "numeric_id": 1,
                                             "string_id": "0x4dc3cae5b486", **change}}
                live = Adapter([notice]); live.events = mock.Mock(); clock = Clock()
                with self.assertRaisesRegex(BotFault, "Native startup deadline"):
                    advance_startup(live, timeout=.3, clock=clock, sleep=clock.sleep)
                self.assertEqual(live.actions, 0)

    def test_completed_login_notice_that_changes_at_dispatch_gets_no_repeated_confirm(self):
        notice = {"scene": "cinematic", "title_menu": False,
                  "native": {"mission": 1, "sequence": "Seq_Demo_LogInKonamiServer",
                             "title": False, "popup": True},
                  "popup_observer": {"reader_verified": True, "owner_verified": True,
                                     "active": True, "numeric_id": 1,
                                     "string_id": "0x4dc3cae5b486"}}
        unknown = {**notice, "popup_observer": {**notice["popup_observer"], "string_id": None}}
        menu = self.stage("Seq_Demo_StartHasTitleMission", menu=True)
        live = mock.Mock(); clock = Clock()
        live.observe.side_effect = [notice, unknown, menu]
        live.execute.side_effect = ActionPrerequisiteChanged("Popup changed before dispatch")
        self.assertIs(advance_startup(live, clock=clock, sleep=clock.sleep), menu)
        live.execute.assert_called_once()

    def test_login_popup_that_never_completes_stops_without_input(self):
        for change in ({}, {"mission": 40010}, {"title": True}, {"mission": None}):
            with self.subTest(change=change):
                state = {"scene": "cinematic", "title_menu": False,
                         "native": {"mission": 1, "sequence": "Seq_Demo_LogInKonamiServer",
                                    "title": False, "popup": True, **change}}
                live = Adapter([state]); live.events = mock.Mock(); clock = Clock()
                with self.assertRaisesRegex(BotFault, "Native startup deadline"):
                    advance_startup(live, timeout=2., clock=clock, sleep=clock.sleep)
                self.assertEqual(live.actions, 0)

    def test_popup_that_closes_during_capture_returns_to_observation_without_credit(self):
        notice = self.stage("Seq_Demo_ConfirmAutoSave", popup=True)
        login = {"scene": "cinematic", "title_menu": False,
                 "native": {"mission": 1, "sequence": "Seq_Demo_LogInKonamiServer",
                            "title": False, "popup": True}}
        menu = self.stage("Seq_Demo_StartHasTitleMission", menu=True)
        live = mock.Mock(); clock = Clock()
        live.observe.side_effect = [notice, login, menu]
        live.execute.side_effect = ActionPrerequisiteChanged("Native action prerequisite changed")
        self.assertIs(advance_startup(live, clock=clock, sleep=clock.sleep), menu)
        live.execute.assert_called_once()
        emitted = [c for c in live.events.emit.call_args_list if c.args[0]=="startup_admission_changed"]
        self.assertEqual(len(emitted), 1)

    def test_startup_never_retries_a_transport_or_post_dispatch_failure(self):
        notice = self.stage("Seq_Demo_ConfirmAutoSave", popup=True)
        live = mock.Mock(); live.observe.return_value = notice
        live.execute.side_effect = BotFault("operator completion unknown")
        clock = Clock()
        with self.assertRaisesRegex(BotFault, "completion unknown"):
            advance_startup(live, clock=clock, sleep=clock.sleep)
        live.execute.assert_called_once()

    def test_existing_selectable_menu_never_receives_start_confirm(self):
        menu = self.stage("Seq_Demo_StartHasTitleMission", menu=True)
        live = Adapter([menu]); live.events = mock.Mock()
        clock = Clock()
        self.assertIs(advance_startup(live, clock=clock, sleep=clock.sleep), menu)
        self.assertEqual(live.actions, 0)
        self.assertEqual(clock.now, 0.)

    def test_early_title_sequence_waits_for_native_prompt_show_event(self):
        early = {**self.stage("Seq_Demo_StartHasTitleMission"), "press_start_ready": False}
        ready = self.stage("Seq_Demo_StartHasTitleMission")
        menu = self.stage("Seq_Demo_StartHasTitleMission", menu=True)
        live = Adapter([early, early, ready, menu])
        live.events = mock.Mock()
        clock = Clock()
        dispatched_at = []
        live.execute = lambda step: dispatched_at.append(clock.now)
        self.assertIs(advance_startup(live, clock=clock, sleep=clock.sleep), menu)
        self.assertEqual(dispatched_at, [.2])

    def test_prompt_log_is_bound_to_process_and_complete_lines(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "game.log"
            path.write_bytes(b"100 Native Press Start prompt opened\n")
            observer = StartupEvidence(path, "/Date(200)/")
            self.assertFalse(observer.read()["press_start_ready"])
            with path.open("ab") as stream:
                stream.write(b"250 Native Press Start prompt ")
            self.assertFalse(observer.read()["press_start_ready"])
            with path.open("ab") as stream:
                stream.write(b"opened\r\n")
            self.assertEqual(observer.read()["press_start_opened_unix_ms"], 250)
            path.write_bytes(b"new log\n")
            self.assertFalse(observer.read()["press_start_ready"])

    def test_unrecognized_popup_does_not_receive_confirm(self):
        live = Adapter([self.stage("Seq_Demo_StartHasTitleMission", popup=True)])
        live.events = mock.Mock()
        clock = Clock()
        with self.assertRaisesRegex(BotFault, "Native startup deadline"):
            advance_startup(live, timeout=.3, clock=clock, sleep=clock.sleep)
        self.assertEqual(live.actions, 0)

    def test_native_show_hook_waits_for_title_fade_before_start(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "game.log"
            path.write_bytes(b"250 Native Press Start prompt opened\n")
            now = [250]
            observer = StartupEvidence(path, "/Date(200)/", clock_ms=lambda: now[0])
            self.assertFalse(observer.read()["press_start_ready"])
            now[0] = 2249
            self.assertFalse(observer.read()["press_start_ready"])
            now[0] = 2250
            self.assertTrue(observer.read()["press_start_ready"])

    def test_already_playing_returns_without_a_fixed_delay(self):
        state = {"scene": "gameplay", "native": {"sequence": "Seq_Game_FreePlay"}}
        live = Adapter([state]); live.events = mock.Mock()
        clock = Clock()
        self.assertIs(advance_startup(live, clock=clock, sleep=clock.sleep), state)
        self.assertEqual(clock.now, 0.)
        self.assertEqual(live.actions, 0)

    def test_continue_rack_waits_for_fresh_physical_telemetry_without_input(self):
        waiting = {"scene": "title", "title_menu": True, "title_cabin": True,
                   "opening_assets": False, "native": {"popup": False}}
        ready = {"scene": "title", "title_menu": True, "title_cabin": True,
                 "opening_assets": True, "opening_position": [1., 2., 3.],
                 "opening_orientation": [0., 0., 0., 1.], "native": {"popup": False}}
        live = Adapter([ready])
        live.events = mock.Mock()
        live.capture = mock.Mock()
        clock = Clock()
        result = wait_for_continue_rack(live, waiting, timeout=1., clock=clock, sleep=clock.sleep)
        self.assertIs(result, ready)
        self.assertEqual(clock.now, .1)
        self.assertEqual(live.actions, 0)
        live.capture.assert_not_called()

    def test_continue_rack_deadline_captures_and_sends_no_input(self):
        waiting = {"scene": "title", "title_menu": True, "title_cabin": True,
                   "opening_assets": False, "native": {"popup": False}}
        live = Adapter([waiting])
        live.events = mock.Mock()
        live.capture = mock.Mock(return_value=["deadline-eye"])
        clock = Clock()
        with self.assertRaisesRegex(BotFault, "bounded deadline; no input sent"):
            wait_for_continue_rack(live, waiting, timeout=.25, clock=clock, sleep=clock.sleep)
        live.capture.assert_called_once_with("continue-rack-readiness-timeout")
        self.assertEqual(live.actions, 0)

    def test_continue_rack_popup_or_menu_change_stops_without_input(self):
        cases = [
            ({"scene": "title", "title_menu": True, "native": {"popup": True}},
             "Unexpected title popup"),
            ({"scene": "title", "title_menu": False, "native": {"popup": False}},
             "Title menu ownership changed"),
        ]
        for state, reason in cases:
            with self.subTest(reason=reason):
                live = Adapter([])
                live.capture = mock.Mock()
                with self.assertRaisesRegex(BotFault, reason):
                    wait_for_continue_rack(live, state, clock=Clock(), sleep=Clock().sleep)
                self.assertEqual(live.actions, 0)
                live.capture.assert_called_once()


class ContinueRackPixelsTests(unittest.TestCase):
    @staticmethod
    def rack():
        return {"scene": "title", "title": True, "title_menu": True,
                "title_cabin": True, "opening_assets": True,
                "native": {"sequence": "Seq_Game_TitleMenu", "title": True, "popup": False}}

    def test_native_rack_fade_requires_real_final_eye_images_before_input(self):
        live = mock.Mock()
        live.capture.side_effect = [BlankCompositorFrame("retained native fade", self.rack()),
                                    ["real-left", "real-right"]]
        clock = Clock()
        self.assertEqual(capture_continue_rack(live, clock=clock, sleep=clock.sleep),
                         ["real-left", "real-right"])
        self.assertEqual(clock.now, .1)
        live.input.assert_not_called()
        live.execute.assert_not_called()

    def test_persistent_native_rack_black_stops_without_input(self):
        live = mock.Mock()
        live.capture.side_effect = BlankCompositorFrame("retained black", self.rack())
        clock = Clock()
        with self.assertRaisesRegex(BotFault, "remained blank.*no input sent"):
            capture_continue_rack(live, timeout=.25, clock=clock, sleep=clock.sleep)
        self.assertEqual(clock.now, .25)
        live.input.assert_not_called()

    def test_blank_gameplay_popup_unknown_owner_and_missing_rack_are_not_retried(self):
        states = [dict(self.rack(), scene="gameplay"), dict(self.rack(), title_cabin=False),
                  dict(self.rack(), opening_assets=False),
                  dict(self.rack(), native={"sequence": "Seq_Game_TitleMenu", "title": True, "popup": True}),
                  dict(self.rack(), native={"sequence": "Seq_Game_TitleMenu", "title": True}),
                  dict(self.rack(), native={"sequence": "Seq_Demo_LogInKonamiServer", "title": True, "popup": False})]
        for state in states:
            with self.subTest(state=state):
                live = mock.Mock()
                live.capture.side_effect = BlankCompositorFrame("retained rejected blank", state)
                clock = Clock()
                with self.assertRaises(BlankCompositorFrame):
                    capture_continue_rack(live, clock=clock, sleep=clock.sleep)
                live.capture.assert_called_once()
                live.input.assert_not_called()

    def test_transport_failure_is_never_retried_as_a_fade(self):
        live = mock.Mock()
        live.capture.side_effect = BotFault("operator completion unknown")
        with self.assertRaisesRegex(BotFault, "completion unknown"):
            capture_continue_rack(live)
        live.capture.assert_called_once()
        live.input.assert_not_called()


class ControlAcquisitionTests(unittest.TestCase):
    @staticmethod
    def fresh():
        return {"scene": "gameplay", "now_ms": 1000,
                "controls": {"context": "gameplay", "sample_ms": 995, "age_ms": 5}}

    def test_capture_gap_is_reobserved_without_any_input(self):
        ready = self.fresh()
        adapter = Adapter([{"controls": None}, ready])
        clock = Clock()
        state, controls = fresh_control_observation(adapter.observe, clock=clock, sleep=clock.sleep)
        self.assertIs(state, ready)
        self.assertEqual(controls["age_ms"], 5)
        self.assertEqual(adapter.actions, 0)
        self.assertEqual(clock.now, .025)

    def test_persistent_stale_publication_has_a_short_deadline(self):
        adapter = Adapter([{"controls": {"context": "gameplay", "sample_ms": 1, "age_ms": 251}}])
        clock = Clock()
        with self.assertRaisesRegex(BotFault, "unavailable or stale"):
            fresh_control_observation(adapter.observe, timeout=.1, clock=clock, sleep=clock.sleep)
        self.assertAlmostEqual(clock.now, .1)
        self.assertEqual(adapter.actions, 0)

    def test_unknown_context_is_not_treated_as_a_capture_gap(self):
        invalid = self.fresh()
        invalid["controls"]["context"] = "unrecognized"
        adapter = Adapter([invalid, self.fresh()])
        clock = Clock()
        with self.assertRaisesRegex(BotFault, "context is unknown"):
            fresh_control_observation(adapter.observe, clock=clock, sleep=clock.sleep)
        self.assertEqual(clock.now, 0.)

    def test_fast_native_read_admits_the_original_consistent_packet(self):
        packet = self.fresh()
        original = copy.deepcopy(packet)
        adapter = Adapter([packet])
        clock = Clock()
        def observe(native=False):
            self.assertTrue(native)
            clock.sleep(.030)
            return adapter.observe(native=native)
        state, controls = fresh_control_observation(observe, native=True, clock=clock, sleep=clock.sleep)
        self.assertIs(state, packet)
        self.assertIs(controls, packet["controls"])
        self.assertEqual(packet, original)
        self.assertEqual(controls["age_ms"], state["now_ms"] - controls["sample_ms"])
        self.assertEqual(adapter.actions, 0)

    def test_successful_slow_native_query_cannot_admit_an_old_control_packet(self):
        packet = self.fresh()
        packet["controls"].update(sample_ms=980, age_ms=20)
        adapter = Adapter([packet])
        clock = Clock()
        def observe(native=False):
            self.assertTrue(native)
            clock.sleep(.300)
            return adapter.observe(native=native)
        with self.assertRaisesRegex(BotFault, "aged during observation"):
            fresh_control_observation(observe, native=True, timeout=.1, clock=clock, sleep=clock.sleep)
        self.assertEqual(packet["now_ms"], 1000)
        self.assertEqual(packet["controls"]["age_ms"], 20)
        self.assertEqual(adapter.actions, 0)
        self.assertAlmostEqual(clock.now, .300)

    def test_slow_read_reacquires_a_fast_native_packet_without_replaying_input(self):
        first, ready = self.fresh(), self.fresh()
        ready["now_ms"] = 1330
        ready["controls"].update(sample_ms=1325)
        durations = iter((.300, .005))
        calls = []
        adapter = Adapter([first, ready])
        clock = Clock()
        def observe(native=False):
            calls.append(native)
            clock.sleep(next(durations))
            return adapter.observe(native=native)
        state, controls = fresh_control_observation(observe, native=True, clock=clock, sleep=clock.sleep)
        self.assertIs(state, ready)
        self.assertIs(controls, ready["controls"])
        self.assertEqual(calls, [True, True])
        self.assertEqual(adapter.actions, 0)
        self.assertAlmostEqual(clock.now, .330)

    def test_existing_native_age_and_sub250ms_transport_are_added(self):
        packet = self.fresh()
        packet["controls"].update(sample_ms=920, age_ms=80)
        adapter = Adapter([packet])
        clock = Clock()
        def observe(native=False):
            clock.sleep(.200)
            return adapter.observe(native=native)
        with self.assertRaisesRegex(BotFault, "aged during observation"):
            fresh_control_observation(observe, timeout=.1, clock=clock, sleep=clock.sleep)
        self.assertEqual(adapter.actions, 0)

    def test_exact_age_budget_passes_but_fractional_overage_is_not_truncated(self):
        for duration, accepted in ((.200, True), (.200001, False)):
            with self.subTest(duration=duration):
                packet = self.fresh()
                packet["controls"].update(sample_ms=950, age_ms=50)
                adapter = Adapter([packet])
                clock = Clock()
                def observe(native=False):
                    clock.sleep(duration)
                    return adapter.observe(native=native)
                if accepted:
                    state, _ = fresh_control_observation(observe, timeout=.1, clock=clock, sleep=clock.sleep)
                    self.assertIs(state, packet)
                else:
                    with self.assertRaisesRegex(BotFault, "aged during observation"):
                        fresh_control_observation(observe, timeout=.1, clock=clock, sleep=clock.sleep)
                self.assertEqual(adapter.actions, 0)

    def test_nonfinite_or_backward_read_clock_never_admits_controls(self):
        for finish in (float('nan'), float('inf'), float('-inf'), -.001, True):
            with self.subTest(finish=finish):
                adapter = Adapter([self.fresh()])
                clock = mock.Mock(side_effect=[0., 0., finish])
                sleep = mock.Mock()
                with self.assertRaisesRegex(BotFault, "clock is invalid"):
                    fresh_control_observation(adapter.observe, clock=clock, sleep=sleep)
                self.assertEqual(adapter.actions, 0)
                sleep.assert_not_called()

    def test_nonfinite_native_age_is_not_repaired_by_a_fast_transport(self):
        packet = self.fresh()
        packet["controls"]["age_ms"] = float('nan')
        adapter = Adapter([packet])
        clock = Clock()
        with self.assertRaisesRegex(BotFault, "unavailable or stale"):
            fresh_control_observation(adapter.observe, timeout=.05, clock=clock, sleep=clock.sleep)
        self.assertEqual(adapter.actions, 0)
        self.assertAlmostEqual(clock.now, .05)

    def test_read_clock_cannot_go_backwards_between_reacquisition_attempts(self):
        observe = mock.Mock(return_value=self.fresh())
        clock = mock.Mock(side_effect=[0., 0., .300, .200])
        sleep = mock.Mock()
        with self.assertRaisesRegex(BotFault, "clock is invalid"):
            fresh_control_observation(observe, clock=clock, sleep=sleep)
        observe.assert_called_once_with(native=False)
        sleep.assert_called_once_with(.025)

    def test_slow_lua_prerequisite_read_cannot_dispatch_the_semantic_action(self):
        clock = Clock()
        packet = self.fresh()
        packet["controls"].update(sample_ms=980, age_ms=20)
        packet["native"] = {"mission":10020}
        live = Live.__new__(Live)
        live.bindings = {"actions":[{"name":"gameplay.interact", "contexts":["gameplay"],
                                      "bindings":[{"gesture":"level", "milliseconds":300,
                                                   "inputs":["y"]}]}]}
        live.events = mock.Mock()
        live.input = mock.Mock()
        def observe(native=False):
            self.assertTrue(native)
            clock.sleep(.300)
            return packet
        live.observe = observe
        with mock.patch('gameplay_bot.live.time.monotonic', clock), \
                mock.patch('gameplay_bot.live.time.sleep', clock.sleep):
            with self.assertRaisesRegex(BotFault, "aged during observation"):
                live.execute({"op":"action", "name":"gameplay.interact",
                              "native_before":{"mission":10020}})
        live.input.assert_not_called()
        live.events.emit.assert_not_called()


class ReviewedConfirmTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.clock = Clock()
        self.events = Events(self.directory.name, {"fixture":True}, self.clock)

    @staticmethod
    def case(mode=False, action="menus.confirm"):
        fast = {"scene":"menu", "menu":True, "idroid":True, "pause":False, "title":False,
                "loading":False, "demo":False, "gamepad":False, "camera_active":True,
                "camera_suspended":False, "controls.context":"menus", "controls.native_buttons":0,
                "rendered.presentation_focused":True, "idroid_tutorial_mode":0,
                "idroid_handheld":mode, "idroid_menu_input_ready":mode}
        native = {"mission":40010, "sequence":"Seq_Game_MainGame", "helicopter_space":True,
                  "title":False, "popup":False, "saving":False, "tutorial_pause":False,
                  "fob_tutorial_state":127, "vr_idroid_player_pad_block":False,
                  "idroid_development_active":True}
        combined = {**fast, **{"native."+key:value for key,value in native.items()}}
        return {"id":"reviewed-"+action.split(".")[-1], "reviewed_action_only":True,
                "before":combined, "after":copy.deepcopy(combined),
                "steps":[{"op":"action", "name":action, "seconds":.12,
                          "state_before":fast, "native_before":native}], "timeout":.4}

    def fixture(self, fault=None, mode=False, action="menus.confirm", token=None):
        case = self.case(mode, action)
        button = 0x1000 if action == "menus.confirm" else 0x2000
        token = token or ("a" if action == "menus.confirm" else "b")
        state = {}
        for key, value in case["before"].items():
            target = state
            parts = key.split(".")
            for part in parts[:-1]:target = target.setdefault(part, {})
            target[parts[-1]] = value
        live = Live.__new__(Live)
        live.supervised = True
        live.events, live.held = self.events, {}
        live.bindings = {"actions":[{"name":action, "label":token.upper(), "contexts":["menus"],
            "bindings":[{"gesture":"level", "milliseconds":300, "inputs":[token]}]}]}
        calls, active, expiry, last_held_sample = [], [False], [0.], [0]
        def input_(values, duration, **options):
            calls.append((values, duration, options))
            active[0] = True
            expiry[0] = self.clock()+options["lease_seconds"]
            live.held = {"fixture":True}
            if fault == "rpc":raise TimeoutError("unknown input completion")
        def release():
            active[0] = False
            live.held.clear()
        def observe(native=False):
            self.clock.sleep(.005)
            if active[0] and fault == "late":self.clock.sleep(.2)
            current = copy.deepcopy(state)
            held = active[0] and (self.clock() < expiry[0] or fault == "late") and fault != "unsampled"
            sample = 1000+round(self.clock()*1000)
            packet = button if held else 0
            if held:last_held_sample[0] = sample
            if held and fault == "wrong_packet":packet = 0x2000 if button == 0x1000 else 0x1000
            if calls and not active[0] and fault == "stuck_release":packet = button
            if calls and not active[0] and fault == "no_release_sample":sample = last_held_sample[0]
            physical = [0.]*11
            physical[{"a":0, "b":1, "x":2, "y":3}[token]] = float(held)
            if held and fault == "wrong_physical":physical = [float(held)]+[0.]*10
            current["now_ms"] = sample
            current["controls"].update(age_ms=0, sample_ms=sample,
                native_packet_source="xr_runtime_final_mapped_packet", native_buttons=packet,
                physical=physical, sticks=[0.]*4,
                native_axes=[0.]*4, native_triggers=[0.]*2)
            if calls and not active[0] and fault == "owner_changed":current["native"]["popup"] = True
            return current
        live.input, live.release, live.observe = input_, release, observe
        live.capture = lambda label:[label+"-left", label+"-right"]
        return case, live, calls

    def run_case(self, live, case, reviewed=True):
        with mock.patch("gameplay_bot.live.time.monotonic", self.clock), \
                mock.patch("gameplay_bot.live.time.sleep", self.clock.sleep):
            return Behaviors(live, self.events, self.clock, self.clock.sleep).case(case, reviewed_action=reviewed)

    def test_strict_confirm_proves_input_and_release_without_page_outcome(self):
        for mode in (False, True):
            case, live, calls = self.fixture(mode=mode)
            result = self.run_case(live, case)
            self.assertEqual(result["status"], "observed_pass")
            self.assertEqual(result["scope"], "physical_input_and_release_only")
            self.assertFalse(result["page_outcome_proven"])
            self.assertEqual(result["visual_acceptance"], "pending")
            self.assertTrue(result["visual_review_required"])
            self.assertEqual(len(result["captures"]), 2)
            self.assertEqual(calls, [([channel("a")], .12, {"lease_seconds":.12})])
            evidence = result["action_observation"]
            self.assertGreaterEqual(evidence["released_neutral_milliseconds"], 100)
            self.assertGreater(evidence["released_state"]["controls"]["sample_ms"], evidence["held_evidence"]["sample_ms"])
            self.assertEqual(live.held, {})

    def test_reviewed_contract_rejects_arbitrary_operations_holds_and_incomplete_guards(self):
        changes = [lambda c:c.update(reviewed_action_only=1),
                   lambda c:c.update(setup=[{"op":"pose"}]),
                   lambda c:c.update(during={"controls.native_buttons":4096}),
                   lambda c:c["steps"].append(copy.deepcopy(c["steps"][0])),
                   lambda c:c["steps"][0].update(name="system.idroid"),
                   lambda c:c["steps"][0].update(op="menu_navigate"),
                   lambda c:c["steps"][0].update(seconds=.151),
                   lambda c:c["steps"][0].update(seconds=True),
                   lambda c:c["steps"][0].update(while_held={"idroid":True}),
                   lambda c:c["before"].pop("native.saving"),
                   lambda c:c["after"].pop("native.popup"),
                   lambda c:c["steps"][0]["state_before"].pop("controls.native_buttons"),
                   lambda c:c["steps"][0]["native_before"].update(mission=10020),
                   lambda c:c["after"].update(idroid_handheld=True, idroid_menu_input_ready=True),
                   lambda c:c["before"].update(idroid_menu_input_ready=True)]
        for change in changes:
            case, live, calls = self.fixture()
            change(case)
            result = self.run_case(live, case)
            self.assertEqual(result["status"], "failed")
            self.assertEqual(calls, [])

    def test_unsupervised_or_preselected_case_cannot_bypass_outcome_rule(self):
        for reviewed, supervised in ((False, True), (True, False)):
            case, live, calls = self.fixture()
            live.supervised = supervised
            result = self.run_case(live, case, reviewed)
            self.assertEqual(result["status"], "failed")
            self.assertEqual(calls, [])

    def test_unsampled_wrong_mapped_packet_and_late_proof_fail_without_replay(self):
        for fault in ("unsampled", "wrong_packet", "late", "rpc"):
            with self.subTest(fault=fault):
                case, live, calls = self.fixture(fault)
                result = self.run_case(live, case)
                self.assertEqual(result["status"], "failed")
                self.assertEqual(result["failure_phase"], "dispatch")
                self.assertEqual(len(calls), 1)
                self.assertEqual(calls[0][2], {"lease_seconds":.12})
                self.assertEqual(live.held, {})

    def test_stuck_mapped_a_or_changed_native_owner_cannot_pass_release(self):
        for fault in ("stuck_release", "owner_changed"):
            case, live, calls = self.fixture(fault)
            result = self.run_case(live, case)
            self.assertEqual(result["status"], "failed")
            self.assertEqual(result["failure_phase"], "dispatch")
            self.assertEqual(len(calls), 1)
            self.assertEqual(live.held, {})

    def test_effective_chord_or_hold_binding_is_rejected_before_input(self):
        for changed in ({"gesture":"hold"}, {"gesture":"press"}, {"inputs":["left_grip", "a"]}):
            case, live, calls = self.fixture()
            live.bindings["actions"][0]["bindings"][0].update(changed)
            self.assertEqual(self.run_case(live, case)["status"], "failed")
            self.assertEqual(calls, [])

    def test_two_released_final_eyes_are_required(self):
        case, live, calls = self.fixture()
        live.capture = lambda label:[label+"-left"]
        result = self.run_case(live, case)
        self.assertEqual(result["status"], "failed")
        self.assertEqual(result["failure_phase"], "capture")
        self.assertEqual(len(calls), 1)

    def test_current_two_eye_review_is_required_and_plans_are_rejected(self):
        from gameplay_bot.supervisor import signature, validate_decision
        case, live, _ = self.fixture()
        state = live.observe()
        request = {"observation_id":"current", "observed_unix_ns":100,
                   "state_signature":signature(state), "captures":[{"sha256":"left"}, {"sha256":"right"}]}
        decision = {"id":"one", "kind":"case", "case":case, "observation_id":"current",
                    "reviewed_capture_sha256":"left", "reviewed_both_capture_sha256":["left", "right"]}
        self.assertEqual(validate_decision(decision, request, state, set(), now_ns=101), "one")
        for both in (None, ["left"], ["left", "left"], ["left", "old"]):
            changed = {**decision, "reviewed_both_capture_sha256":both}
            with self.assertRaises(BotFault):validate_decision(changed, request, state, set(), now_ns=101)
        with self.assertRaises(BotFault):
            validate_decision({**decision, "kind":"plan", "cases":[case]}, request, state, set(), now_ns=101)

    def test_strict_back_checks_native_b_and_effective_physical_binding_in_both_modes(self):
        for mode, token in ((False, "b"), (True, "x")):
            with self.subTest(mode=mode, token=token):
                case, live, calls = self.fixture(mode=mode, action="menus.back", token=token)
                result = self.run_case(live, case)
                self.assertEqual(result["status"], "observed_pass")
                self.assertEqual(result["scope"], "physical_input_and_release_only")
                self.assertTrue(result["page_outcome_pending"])
                self.assertFalse(result["page_outcome_proven"])
                proof = result["action_observation"]
                self.assertEqual(proof["held_evidence"]["xr_published_gamepad"]["buttons"], 0x2000)
                self.assertEqual(proof["released_state"]["controls"]["native_buttons"], 0)
                self.assertGreaterEqual(proof["released_neutral_milliseconds"], 100)
                self.assertEqual(calls, [([channel(token)], .12, {"lease_seconds":.12})])
                self.assertEqual(live.held, {})

    def test_back_rejects_missing_late_wrong_native_or_wrong_physical_press_without_retry(self):
        for fault in ("unsampled", "late", "wrong_packet", "wrong_physical", "rpc"):
            with self.subTest(fault=fault):
                case, live, calls = self.fixture(fault, action="menus.back")
                result = self.run_case(live, case)
                self.assertEqual(result["status"], "failed")
                self.assertEqual(result["failure_phase"], "dispatch")
                self.assertEqual(len(calls), 1)
                self.assertEqual(calls[0][2], {"lease_seconds":.12})
                self.assertEqual(live.held, {})

    def test_back_requires_new_neutral_release_and_unchanged_native_owner(self):
        for fault in ("stuck_release", "no_release_sample", "owner_changed"):
            with self.subTest(fault=fault):
                case, live, calls = self.fixture(fault, action="menus.back")
                result = self.run_case(live, case)
                self.assertEqual(result["status"], "failed")
                self.assertEqual(result["failure_phase"], "dispatch")
                self.assertEqual(len(calls), 1)
                self.assertEqual(live.held, {})

    def test_back_rejects_held_outcome_extensions_and_incomplete_mode_or_focus_guards(self):
        changes = [lambda c:c.update(during={"controls.native_buttons":0x2000}),
                   lambda c:c["steps"][0].update(while_held={"idroid":True}),
                   lambda c:c.update(setup=[{"op":"action", "name":"menus.back"}]),
                   lambda c:c["steps"][0].update(seconds=.151),
                   lambda c:c["steps"][0].update(seconds=.029),
                   lambda c:c["steps"][0]["state_before"].pop("rendered.presentation_focused"),
                   lambda c:c["steps"][0]["native_before"].pop("tutorial_pause"),
                   lambda c:c["after"].update(idroid_handheld=True, idroid_menu_input_ready=True),
                   lambda c:c["before"].update(idroid_menu_input_ready=True)]
        for change in changes:
            case, live, calls = self.fixture(action="menus.back")
            change(case)
            self.assertEqual(self.run_case(live, case)["status"], "failed")
            self.assertEqual(calls, [])

    def test_back_cannot_run_as_an_initial_or_ordinary_unreviewed_suite(self):
        from gameplay_bot.supervisor import run_supervised
        case, live, calls = self.fixture(action="menus.back")
        behavior = Behaviors(live, self.events, self.clock, self.clock.sleep)
        with mock.patch("gameplay_bot.supervisor.run_suite") as initial:
            with self.assertRaisesRegex(BotFault, "not an initial suite"):
                run_supervised(behavior, initial_suite={"cases":[case]}, clock=self.clock)
            initial.assert_not_called()
        result = self.run_case(live, case, reviewed=False)
        self.assertEqual(result["status"], "failed")
        self.assertEqual(calls, [])

    def test_back_needs_fresh_distinct_current_two_eye_review_and_cannot_be_a_plan(self):
        from gameplay_bot.supervisor import signature, validate_decision, REVIEW_WINDOW_SECONDS
        case, live, _ = self.fixture(action="menus.back")
        state = live.observe()
        request = {"observation_id":"current", "observed_unix_ns":100,
                   "state_signature":signature(state), "captures":[{"sha256":"left"}, {"sha256":"right"}]}
        decision = {"id":"back-one", "kind":"case", "case":case, "observation_id":"current",
                    "reviewed_capture_sha256":"left", "reviewed_both_capture_sha256":["left", "right"]}
        self.assertEqual(validate_decision(decision, request, state, set(), now_ns=101), "back-one")
        for both in (None, ["left"], ["left", "left"], ["left", "old"]):
            with self.assertRaises(BotFault):
                validate_decision({**decision, "reviewed_both_capture_sha256":both}, request, state, set(), now_ns=101)
        with self.assertRaises(BotFault):
            validate_decision(decision, request, state, set(), now_ns=int((REVIEW_WINDOW_SECONDS+1)*1e9))
        with self.assertRaises(BotFault):
            validate_decision({**decision, "kind":"plan", "cases":[case]}, request, state, set(), now_ns=101)


if __name__ == "__main__":
    unittest.main()
