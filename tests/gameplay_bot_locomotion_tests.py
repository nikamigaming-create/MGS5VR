"""Deterministic contracts for bounded, native-feedback locomotion."""
import pathlib
import sys
import unittest
import copy
from unittest.mock import Mock, patch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
from gameplay_bot.core import BotFault
from gameplay_bot.locomotion import _validate_field_state, crawl_hospital, move_local
from gameplay_bot.interval import observe_interval
from gameplay_bot.hospital import follow_rail
from gameplay_bot.combat import RigPublicationDelay


class Clock:
    def __init__(self):
        self.now = 0.0

    def __call__(self):
        return self.now

    def sleep(self, duration):
        self.now += duration


class Events:
    def __init__(self):
        self.values = []

    def emit(self, event, **fields):
        self.values.append((event, fields))


class Live:
    def __init__(self, clock, *, movement=True, step=0.1, bad_context_at=None,
                 bad_gamepad=False, malformed_physical=False, dynamics=False,
                 release_latency=0.0, native_latency_at=None, fast_latency=0.005,
                 startup_delay=0.0, collision_distance=None,
                 startup_suppressed_pulses=0, publication_stalls_at_pulse=None,
                 consume_while_stalled=False, movement_threshold=0.0):
        self.bindings = {"axes": [{"name": "axes.move", "source": "left_stick"}]}
        self.clock = clock
        self.events = Events()
        self.movement = movement
        self.step = step
        self.bad_context_at = bad_context_at
        self.bad_gamepad = bad_gamepad
        self.malformed_physical = malformed_physical
        self.dynamics = dynamics
        self.release_latency = release_latency
        self.native_latency_at = native_latency_at or {}
        self.fast_latency = fast_latency
        self.startup_delay = startup_delay
        self.collision_distance = collision_distance
        self.startup_suppressed_pulses = startup_suppressed_pulses
        self.publication_stalls_at_pulse = publication_stalls_at_pulse or {}
        self.publication_stalled_until = 0.0
        self.consume_while_stalled = consume_while_stalled
        self.movement_threshold = movement_threshold
        self.last_controls = None
        self.pulse_started_at = 0.0
        self.native_reads = 0
        self.native_reads_while_held = 0
        self.expires_at = 0.0
        self.held = False
        self.current_value = 0.0
        self.velocity = 0.0
        self.last_motion_time = clock.now
        self.releases = 0
        self.observe_count = 0
        self.position = [10.0, 20.0, 30.0]
        self.activation = 4
        self.input_values = None
        self.input_duration = None
        self.input_history = []
        self.leases = []

    def _advance_motion(self):
        now = self.clock.now
        elapsed = max(0.0, now - self.last_motion_time)
        self.last_motion_time = now
        if (not self.movement or len(self.input_history) <= self.startup_suppressed_pulses
                or (now < self.publication_stalled_until and not self.consume_while_stalled)):
            if now >= self.expires_at:
                self.held = False
                self.current_value = 0.0
            return
        if not self.dynamics:
            if self.held and now < self.expires_at and self.current_value >= self.movement_threshold:
                self.position[0] += self.step * self.current_value / .30
            if now >= self.expires_at:
                self.held = False
                self.current_value = 0.0
            return
        remaining = elapsed
        while remaining > 0:
            dt = min(.01, remaining)
            remaining -= dt
            if self.held and now - remaining < self.expires_at:
                desired_speed = (self.current_value * 3.0 if
                    self.current_value >= self.movement_threshold and
                    now - remaining - self.pulse_started_at >= self.startup_delay else 0.0)
                self.velocity += (desired_speed - self.velocity) * min(1.0, dt / .25)
            else:
                self.velocity = max(0.0, self.velocity - 4.0 * dt)
            self.position[0] += self.velocity * dt
            if self.collision_distance is not None and self.position[0] >= 10.0 + self.collision_distance:
                self.position[0] = 10.0 + self.collision_distance
                self.velocity = 0.0
        if now >= self.expires_at:
            self.held = False
            self.current_value = 0.0

    def release(self):
        self.releases += 1
        self._advance_motion()
        if self.held and self.release_latency:
            self.clock.now += self.release_latency
            self._advance_motion()
        self.held = False
        self.current_value = 0.0
        self.last_motion_time = self.clock.now

    def input(self, values, duration, *, lease_seconds=None):
        self.input_values = values
        self.input_duration = duration
        value = values[0]["value"]
        self.input_history.append(value)
        self.leases.append(lease_seconds)
        self.clock.now += .02
        self._advance_motion()
        self.held = True
        self.current_value = value
        self.expires_at = self.clock.now + lease_seconds
        self.pulse_started_at = self.clock.now
        self.publication_stalled_until = self.clock.now+self.publication_stalls_at_pulse.get(len(self.input_history),0.)
        self.last_motion_time = self.clock.now

    def observe(self, native=False):
        self.observe_count += 1
        if native:
            self.native_reads += 1
            self.native_reads_while_held += int(self.held)
            self.clock.now += self.native_latency_at.get(self.native_reads, .005)
        else:
            self.clock.now += self.fast_latency
        self._advance_motion()
        now_ms = 10000 + round(self.clock() * 1000)
        buttons = [0.0] * 11
        physical = {"buttons": buttons} if self.malformed_physical else buttons
        stick_index = 1 if self.bindings["axes"][0]["source"] == "left_stick" else 3
        if self.input_values and self.input_values[0].get("sub_component") == "X":
            stick_index -= 1
        sticks = [0.0, 0.0, 0.0, 0.0]
        sticks[stick_index] = self.current_value if self.held else 0.0
        context = "menus" if self.bad_context_at == self.observe_count else "gameplay"
        state = {
            "scene": "gameplay",
            "camera_active": True,
            "camera_available": True,
            "menu": False,
            "idroid": False,
            "gamepad": self.bad_gamepad,
            "activation": self.activation,
            "now_ms": now_ms,
            "controls": {
                "context": context,
                "age_ms": 0,
                "sample_ms": now_ms,
                "rig_input": True,
                "travel_mode": 1,
                "physical": physical,
                "sticks": sticks,
                "native_buttons": 0,
            },
            "native": {
                "mission": 30010,
                "location": 10,
                "sequence": "Seq_Game_FreePlay",
                "title": False,
                "status_NORMAL_ACTION": True,
                "saving": False,
                "popup": False,
                "game_over": 0,
                "player_vehicle_id": 65535,
                "player_x": self.position[0],
                "player_y": self.position[1],
                "player_z": self.position[2],
            },
        }
        if not native:
            del state["native"]
        if self.clock.now < self.publication_stalled_until:
            controls = copy.deepcopy(self.last_controls)
            age = now_ms-controls["sample_ms"]
            if age > 250:
                state["controls"] = None
            else:
                controls["age_ms"] = age
                state["controls"] = controls
        else:
            self.last_controls = copy.deepcopy(state["controls"])
        return state


class LocomotionTests(unittest.TestCase):
    def test_cardinal_walk_uses_only_selected_axis_and_retains_sign_when_braking(self):
        for source in ("left_stick", "right_stick"):
            for direction in ("up", "down", "left", "right"):
                with self.subTest(source=source, direction=direction):
                    clock = Clock()
                    live = Live(clock, dynamics=True, movement_threshold=-1.)
                    live.bindings["axes"][0]["source"] = source
                    result = self.run_move(live, clock, direction=direction,
                                           target_distance=.55, brake_magnitude=.30)
                    sign = -1 if direction in ("down", "left") else 1
                    component = "X" if direction in ("left", "right") else "Y"
                    self.assertEqual(live.input_values[0]["sub_component"], component)
                    self.assertTrue(all(v * sign > 0 for v in live.input_history))
                    self.assertIn(sign * .30, live.input_history)
                    self.assertEqual(result["input"]["direction"], "stick_" + direction)
                    self.assertFalse(live.held)

    def test_carry_requires_both_explicit_witness_and_native_carry_state(self):
        state=Live(Clock()).observe(native=True)
        state['native'].update(status_NORMAL_ACTION=False,status_CARRY=True)
        for witness in (False,1,'yes'):
            with self.subTest(witness=witness),self.assertRaises(BotFault):
                _validate_field_state(state,witnessed_carry=witness)
        _validate_field_state(state,witnessed_carry=True)
        state['native']['status_CARRY']=False
        with self.assertRaises(BotFault):_validate_field_state(state,witnessed_carry=True)

    def test_hospital_move_lesson_keeps_exact_crawl_ownership(self):
        state=Live(Clock()).observe(native=True)
        state['native'].update(mission=10010,location=30,story=0,
            sequence='Seq_Game_EscapeFromAwakeRoom',status_NORMAL_ACTION=False,
            status_CRAWL=True,status_PARTS_ACTIVE=True)
        _validate_field_state(state)
        state['native']['sequence']='Seq_Game_AfterHeliDemo'
        _validate_field_state(state)
        state['native']['sequence']='Seq_Game_AfterVolginDemo'
        with self.assertRaises(BotFault):_validate_field_state(state)
        _validate_field_state(state,witnessed_hospital_rail=True)
        for posture in ('SQUAT','STAND'):
            changed=copy.deepcopy(state)
            changed['native'].update(status_CRAWL=False,status_STAND=False,status_SQUAT=False)
            changed['native']['status_'+posture]=True
            with self.subTest(posture=posture):
                with self.assertRaises(BotFault):_validate_field_state(changed)
                _validate_field_state(changed,witnessed_hospital_rail=True)
        changed=copy.deepcopy(state)
        changed['native'].update(status_CRAWL=False,status_STAND=False,status_SQUAT=False)
        with self.assertRaises(BotFault):_validate_field_state(changed,witnessed_hospital_rail=True)
        state['native']['sequence']='Seq_Game_AfterHeliDemo'
        for field,value in [('mission',10195),('sequence','Seq_Game_MainGame'),
                            ('story',1),('status_CRAWL',False),('status_PARTS_ACTIVE',False)]:
            changed=copy.deepcopy(state);changed['native'][field]=value
            with self.subTest(field=field),self.assertRaises(BotFault):
                _validate_field_state(changed)

    def hospital_live(self, clock, *, cutscene_at=None):
        live=Live(clock,dynamics=True)
        observe=live.observe
        def hospital_observation(native=False):
            state=observe(native=native)
            state['native'].update(mission=10010,location=30,story=0,
                sequence='Seq_Game_AfterHeliDemo',status_NORMAL_ACTION=False,
                status_CRAWL=True,status_PARTS_ACTIVE=True)
            if cutscene_at is not None and clock()>=cutscene_at:
                state['native']['sequence']='Seq_Demo_Heli'
                state['demo']=True
            return state
        live.observe=hospital_observation
        return live

    def test_hospital_hold_waits_while_the_async_input_remains_active(self):
        clock=Clock();live=self.hospital_live(clock)
        result=crawl_hospital(live,2.,clock=clock,sleep=clock.sleep)
        self.assertGreaterEqual(result['held_seconds'],2.)
        self.assertGreater(live.native_reads_while_held,10)
        self.assertGreater(result['native_displacement'],.5)

    def test_free_smoke_corridor_hold_requires_its_explicit_witness_and_crawl_owner(self):
        clock=Clock();live=self.hospital_live(clock)
        observe=live.observe
        def smoke_observation(native=False):
            state=observe(native=native)
            state['native'].update(sequence='Seq_Game_EnterSmokeRoom',status_CRAWL=True,
                                   status_SQUAT=False,status_NORMAL_ACTION=False)
            return state
        live.observe=smoke_observation
        with self.assertRaises(BotFault):crawl_hospital(live,1.,clock=clock,sleep=clock.sleep)
        self.assertIsNone(live.input_values)
        result=crawl_hospital(live,1.,witnessed_smoke_corridor=True,clock=clock,sleep=clock.sleep)
        self.assertGreaterEqual(result['held_seconds'],1.)
        self.assertGreater(result['native_displacement'],.2)
        self.assertFalse(live.held)

    def test_witnessed_hospital_route_keeps_medication_and_cover_movement(self):
        clock=Clock();state=self.hospital_live(clock).observe(native=True)
        state['native'].update(sequence='Seq_Game_AfterHeliKillMobDemo',status_CRAWL=False,
                               status_SQUAT=True,status_NORMAL_ACTION=False)
        with self.assertRaises(BotFault):_validate_field_state(state)
        _validate_field_state(state,witnessed_hospital_route=True)
        for field,value in [('mission',10180),('location',20),('story',1),('status_PARTS_ACTIVE',False)]:
            changed=copy.deepcopy(state);changed['native'][field]=value
            with self.subTest(field=field),self.assertRaises(BotFault):
                _validate_field_state(changed,witnessed_hospital_route=True)

    def test_volgin_fatal_animation_refuses_movement_despite_protected_health(self):
        state=self.hospital_live(Clock()).observe(native=True)
        state['native'].update(sequence='Seq_Game_VolginInEntrance',player_life=6000,
                               status_CRAWL=False,status_STAND=True,
                               hospital_volgin_fatal_attack=True)
        with self.assertRaisesRegex(BotFault,'fatal attack'):
            _validate_field_state(state,witnessed_hospital_route=True)

    def test_hospital_crouched_rail_releases_and_reacquires_after_a_rig_delay(self):
        clock=Clock();live=self.hospital_live(clock)
        observe=live.observe
        def crouched_observation(native=False):
            state=observe(native=native)
            state['native'].update(sequence='Seq_Game_AfterVolginDemo',status_CRAWL=False,status_SQUAT=True)
            state['rendered']={'native_pose':{'position':[0,0,0],'orientation':[0,0,0,1]},
                               'head_pose':{'position':[0,0,0],'orientation':[0,0,0,1]}}
            state['native']['player_yaw']=0
            return state
        live.observe=crouched_observation
        # The operator's latest request is ahead of the rendered head pose.
        pose={'position':[0,0,0],'orientation':[0,.5,0,.8660254]}
        live.call=Mock(return_value={'pose':pose})
        live.text_result=lambda value:value
        live.native=Mock()
        live.native.read.return_value=({'mission':10010,'location':30,'rail_active':True},{})
        calls=[0]
        def fresh(*args,**kwargs):
            calls[0]+=1
            if calls[0]==2:raise RigPublicationDelay('delayed publication')
            return live.observe(native=True),(10010,30,1,99,4)
        with patch('gameplay_bot.hospital.fresh_rig',side_effect=fresh), \
             patch('gameplay_bot.hospital.looking_head',return_value=[0,0,0,1]) as steer:
            result=follow_rail(live,1.,clock=clock,sleep=clock.sleep)
        self.assertEqual(steer.call_args_list[0].args[0],[0,0,0,1])
        self.assertEqual(result['recoveries'],1)
        self.assertGreaterEqual(live.releases,3)
        self.assertFalse(live.held)
        delay=next(fields for event,fields in live.events.values if event=='hospital_rail_publication_delay')
        self.assertFalse(delay['input_replayed'])
        self.assertFalse(live.held)

    def test_hospital_scene_handoff_during_rig_acquisition_releases_normally(self):
        clock=Clock();live=self.hospital_live(clock)
        observe=live.observe
        def sample(native=False):
            state=observe(native=native)
            state['native']['player_yaw']=0
            state['rendered']={'native_pose':{'position':[0,0,0],'orientation':[0,0,0,1]}}
            return state
        live.observe=sample
        live.call=Mock(return_value={'pose':{'position':[0,0,0],'orientation':[0,0,0,1]}})
        live.text_result=lambda value:value
        live.native=Mock()
        live.native.read.return_value=({'mission':10010,'location':30,'rail_active':True},{})
        count=[0]
        def fresh(*args,**kwargs):
            count[0]+=1
            state=live.observe(native=True)
            if count[0]>1:
                state['native']['sequence']='Seq_Demo_UnderBed'
                state['demo']=True
            return state,(10010,30,1,99,4)
        with patch('gameplay_bot.hospital.fresh_rig',side_effect=fresh):
            result=follow_rail(live,1.,clock=clock,sleep=clock.sleep)
        self.assertEqual(result['reason'],'native_scene_transition')
        self.assertFalse(live.held)
        self.assertIsNone(live.input_values)

    def test_under_bed_movement_cue_requires_the_exact_witnessed_owner(self):
        clock=Clock();live=self.hospital_live(clock)
        observe=live.observe
        def sample(native=False):
            state=observe(native=native)
            state['native'].update(sequence='Seq_Game_UnderBed',status_NORMAL_ACTION=True)
            if clock()>=.4:
                state['native']['sequence']='Seq_Demo_UnderBed2'
                state['demo']=True
            return state
        live.observe=sample
        with self.assertRaises(BotFault):crawl_hospital(live,1.,clock=clock,sleep=clock.sleep)
        self.assertIsNone(live.input_values)
        result=crawl_hospital(live,1.,witnessed_under_bed=True,clock=clock,sleep=clock.sleep)
        self.assertLess(result['held_seconds'],1.)
        self.assertEqual(result['after']['native']['sequence'],'Seq_Demo_UnderBed2')
        self.assertFalse(live.held)

    def test_hospital_hold_releases_early_when_the_cutscene_takes_over(self):
        clock=Clock();live=self.hospital_live(clock,cutscene_at=.8)
        result=crawl_hospital(live,2.,clock=clock,sleep=clock.sleep)
        self.assertLess(result['held_seconds'],1.)
        self.assertEqual(result['after']['native']['sequence'],'Seq_Demo_Heli')
        self.assertFalse(live.held)

    def run_move(self, live, clock, **kwargs):
        return move_local(live, clock=clock, sleep=clock.sleep, **kwargs)

    def test_default_cruise_crosses_native_deadzone_and_releases_at_endpoint(self):
        # The live field reproduction sampled .30 correctly but never moved;
        # .45 moved under the same ordinary native owner. Keep endpoint and
        # expiry checks active when choosing the usable default magnitude.
        clock = Clock()
        live = Live(clock, step=.025, movement_threshold=.40)
        with self.assertRaisesRegex(BotFault, "No native movement progress"):
            self.run_move(live, clock, cruise_magnitude=.30)
        self.assertFalse(live.held)

        clock = Clock()
        live = Live(clock, step=.025, movement_threshold=.40)
        result = self.run_move(live, clock)
        self.assertTrue(result["target_reached"])
        self.assertGreaterEqual(result["distance_native_units"], .25)
        self.assertLessEqual(result["distance_native_units"], .75)
        self.assertTrue(result["settled_after_release"])
        self.assertTrue(all(lease <= .25 for lease in live.leases))
        self.assertEqual(live.native_reads_while_held, 0)
        self.assertFalse(live.held)

    def test_short_move_uses_effective_axis_and_measures_neutral_endpoint(self):
        clock = Clock()
        live = Live(clock, dynamics=True, release_latency=.05)
        result = self.run_move(live, clock, target_distance=.35,
                               cruise_magnitude=.45, brake_magnitude=.30)

        self.assertEqual(result["status"], "observed_displacement")
        self.assertEqual(result["input"]["axis"], "axes.move")
        self.assertEqual(result["input"]["source"], "left_stick")
        self.assertEqual(result["input"]["hand"], "left")
        self.assertEqual(result["input"]["direction"], "stick_up")
        self.assertEqual(result["input"]["cruise_value"], .45)
        self.assertEqual(result["input"]["brake_value"], .30)
        self.assertEqual(result["origin"], [10.0, 20.0, 30.0])
        self.assertGreaterEqual(result["distance_native_units"], 0.35)
        self.assertLessEqual(result["distance_native_units"], 0.75)
        self.assertTrue(result["target_reached"])
        self.assertGreaterEqual(result["input_samples_by_phase"]["cruise"], 1)
        self.assertGreaterEqual(result["input_samples_by_phase"]["braking"], 1)
        self.assertEqual(list(dict.fromkeys(live.input_history)), [.45, .30])
        self.assertTrue(all(.03 <= lease <= .25 for lease in live.leases))
        self.assertEqual(live.native_reads_while_held, 0)
        self.assertTrue(result["settled_after_release"])
        self.assertGreaterEqual(result["xr_sampled_input_count"], 2)
        self.assertTrue(result["held_audit_timeline"])
        self.assertIs(result["xinput_consumption_proven"], False)
        self.assertIs(result["route_or_collision_proof"], False)
        self.assertFalse(live.held)
        self.assertGreaterEqual(live.releases, 2)

    def test_right_stick_source_is_sampled_in_its_own_axis(self):
        clock = Clock()
        live = Live(clock, dynamics=True)
        live.bindings["axes"][0]["source"] = "right_stick"
        result = self.run_move(live, clock, cruise_magnitude=.45, brake_magnitude=.30)

        self.assertEqual(result["input"]["source"], "right_stick")
        self.assertEqual(result["input"]["hand"], "right")
        samples = [sample for sample in result["held_audit_timeline"]
                   if sample.get("phase") in ("cruise", "braking")]
        self.assertTrue(samples)
        self.assertTrue(all(sample["xr_sampled_sticks"][3] > 0 for sample in samples))
        self.assertTrue(all(sample["xr_sampled_sticks"][1] == 0 for sample in samples))
        self.assertFalse(live.held)

    def test_braking_keeps_larger_short_target_inside_hard_limit(self):
        clock = Clock()
        live = Live(clock, dynamics=True, release_latency=.05)
        result = self.run_move(live, clock, target_distance=.55,
                               cruise_magnitude=.45, brake_magnitude=.30)

        self.assertGreaterEqual(result["distance_native_units"], .55)
        self.assertLessEqual(result["distance_native_units"], .75)
        self.assertTrue(result["settled_after_release"])
        self.assertEqual(result["input"]["brake_value"], .30)
        self.assertFalse(live.held)

    def test_position_crossing_cannot_credit_an_unobserved_braking_input(self):
        clock = Clock()
        live = Live(clock, dynamics=True)
        original_observe = live.observe
        def observe(native=False):
            state = original_observe(native=native)
            if live.current_value == .15:
                state["controls"]["sticks"][1] = .30
            return state
        live.observe = observe
        with self.assertRaisesRegex(BotFault, "each movement phase"):
            self.run_move(live, clock, target_distance=.10, pulse_seconds=.10)
        self.assertFalse(live.held)

    def test_no_native_progress_releases_before_failing(self):
        clock = Clock()
        live = Live(clock, movement=False)

        with self.assertRaisesRegex(BotFault, "No native movement progress"):
            self.run_move(live, clock, target_distance=0.25, max_displacement=0.75,
                          max_seconds=1.0, progress_timeout=0.2)

        self.assertFalse(live.held)
        self.assertGreaterEqual(live.releases, 2)

    def test_field_identity_change_releases_input(self):
        clock = Clock()
        live = Live(clock, bad_context_at=3)

        with self.assertRaisesRegex(BotFault, "gameplay rig"):
            self.run_move(live, clock)

        self.assertFalse(live.held)
        self.assertEqual(live.releases, 2)

    def test_delayed_native_queue_cannot_keep_movement_held(self):
        clock = Clock()
        live = Live(clock, dynamics=True, native_latency_at={2: 1.0})
        result = self.run_move(live, clock, target_distance=.10, max_seconds=3.,
                               cruise_magnitude=.45, brake_magnitude=.30)
        self.assertEqual(result["status"], "observed_displacement")
        self.assertEqual(live.native_reads_while_held, 0)
        self.assertLessEqual(result["distance_native_units"], .75)
        self.assertTrue(all(lease <= .25 for lease in live.leases))
        self.assertFalse(live.held)

    def test_expiry_stops_movement_even_when_release_rpc_is_delayed(self):
        clock = Clock()
        live = Live(clock, dynamics=True, release_latency=1.0)
        result = self.run_move(live, clock, target_distance=.25, max_seconds=3.,
                               cruise_magnitude=.45, brake_magnitude=.30)
        self.assertEqual(result["status"], "observed_displacement")
        self.assertGreaterEqual(result["distance_native_units"], .25)
        self.assertEqual(live.native_reads_while_held, 0)
        self.assertLess(live.position[0] - 10., .75)
        self.assertTrue(all(lease <= .25 for lease in live.leases))
        self.assertFalse(live.held)

    def test_fast_read_delayed_past_expiry_cannot_credit_old_intent(self):
        clock = Clock()
        live = Live(clock, dynamics=True, fast_latency=1.0)
        with self.assertRaisesRegex(BotFault, "did not resume bounded fresh cadence"):
            self.run_move(live, clock, max_seconds=3.,
                          cruise_magnitude=.45, brake_magnitude=.30)
        self.assertEqual(len(live.input_history), 1)
        self.assertEqual(live.native_reads_while_held, 0)
        self.assertLess(live.position[0] - 10., .75)
        self.assertFalse(live.held)

    def test_pulse_duration_cannot_restore_a_long_movement_lease(self):
        clock = Clock()
        live = Live(clock)
        with self.assertRaisesRegex(BotFault, "bounds"):
            self.run_move(live, clock, pulse_seconds=.5)
        self.assertIsNone(live.input_values)

    def test_tenth_second_pulse_cannot_overcome_native_startup_delay(self):
        clock = Clock()
        live = Live(clock, dynamics=True, startup_delay=.15)
        with self.assertRaisesRegex(BotFault, "No native movement progress"):
            self.run_move(live, clock, pulse_seconds=.10, max_seconds=3., progress_timeout=.4)
        self.assertEqual(live.position, [10., 20., 30.])
        self.assertEqual(live.native_reads_while_held, 0)
        self.assertFalse(live.held)

    def test_bounded_longer_pulses_survive_native_startup_delay(self):
        clock = Clock()
        live = Live(clock, dynamics=True, startup_delay=.15)
        result = self.run_move(live, clock, pulse_seconds=.25,
            target_distance=.10, max_seconds=5., progress_timeout=1.,
            cruise_magnitude=.45, brake_magnitude=.30)
        self.assertTrue(result["target_reached"])
        self.assertLessEqual(result["distance_native_units"], .75)
        self.assertTrue(all(lease == .25 for lease in live.leases))
        self.assertEqual(live.native_reads_while_held, 0)
        self.assertFalse(live.held)

    def test_shorter_startup_pulse_can_still_fail_braking_without_relaxing_deadline(self):
        clock = Clock()
        live = Live(clock, dynamics=True, startup_delay=.15)
        with self.assertRaisesRegex(BotFault, "bounded segment deadline"):
            self.run_move(live, clock, pulse_seconds=.20,
                target_distance=.10, max_seconds=5., progress_timeout=1.,
                cruise_magnitude=.45, brake_magnitude=.30)
        self.assertGreater(live.position[0]-10., 0.)
        self.assertLess(live.position[0]-10., .10)
        self.assertEqual(list(dict.fromkeys(live.input_history)), [.45, .30])
        self.assertTrue(all(lease == .20 for lease in live.leases))
        self.assertEqual(live.native_reads_while_held, 0)
        self.assertFalse(live.held)

    def test_actual_slow_neutral_reads_do_not_consume_unsent_movement_budget(self):
        clock = Clock()
        live = Live(clock, dynamics=True, startup_suppressed_pulses=1,
                    native_latency_at={2:1.422, 3:.407, 4:.033})
        result = self.run_move(live, clock, pulse_seconds=.25, target_distance=.10,
                               max_seconds=5., progress_timeout=1.,
                               cruise_magnitude=.45, brake_magnitude=.30)
        self.assertEqual(result["pulses"][0]["settled_distance"], 0.)
        self.assertGreaterEqual(len(result["pulses"]), 2)
        self.assertTrue(result["target_reached"])
        self.assertEqual(result["progress_clock"], "sampled_expiring_input_seconds")
        self.assertAlmostEqual(result["commanded_input_seconds"], len(result["pulses"])*.25)
        self.assertLess(clock(), 5.)
        self.assertTrue(any(s.get("observation_seconds", 0) >= 1.422-.0001
                            for s in result["held_audit_timeline"]))
        self.assertEqual(live.native_reads_while_held, 0)
        self.assertFalse(live.held)

    def test_wall_blocks_sampled_inputs_without_turning_them_into_movement(self):
        clock = Clock()
        live = Live(clock, dynamics=True, collision_distance=0.)
        with self.assertRaisesRegex(BotFault, "No native movement progress"):
            self.run_move(live, clock, pulse_seconds=.25, max_seconds=5., progress_timeout=.5,
                          cruise_magnitude=.45, brake_magnitude=.30)
        self.assertEqual(live.position, [10., 20., 30.])
        self.assertEqual(len(live.input_history), 2)
        self.assertEqual(live.native_reads_while_held, 0)
        self.assertFalse(live.held)

    def test_wall_after_cruise_stops_braking_without_extending_lease(self):
        clock = Clock()
        live = Live(clock, dynamics=True, collision_distance=.15)
        with self.assertRaisesRegex(BotFault, "No native movement progress"):
            self.run_move(live, clock, pulse_seconds=.25, max_seconds=5., progress_timeout=.5,
                          cruise_magnitude=.45, brake_magnitude=.30)
        self.assertEqual(list(dict.fromkeys(live.input_history)), [.45, .30])
        self.assertAlmostEqual(live.position[0]-10., .15)
        self.assertTrue(all(lease == .25 for lease in live.leases))
        self.assertEqual(live.native_reads_while_held, 0)
        self.assertFalse(live.held)

    def test_long_neutral_read_still_exhausts_five_second_wall_deadline(self):
        clock = Clock()
        live = Live(clock, dynamics=True, native_latency_at={2:4.8})
        with self.assertRaisesRegex(BotFault, "bounded segment deadline"):
            self.run_move(live, clock, pulse_seconds=.25, max_seconds=5.,
                          cruise_magnitude=.45, brake_magnitude=.30)
        self.assertEqual(len(live.input_history), 1)
        self.assertLessEqual(live.position[0]-10., .75)
        self.assertEqual(live.native_reads_while_held, 0)
        self.assertFalse(live.held)

    def test_larger_pulse_does_not_relax_distance_time_or_expiry_caps(self):
        for bounds in ({"pulse_seconds":.250001}, {"max_displacement":1.001}, {"max_seconds":5.001}):
            with self.subTest(bounds=bounds):
                clock = Clock()
                live = Live(clock)
                with self.assertRaisesRegex(BotFault, "bounds"):
                    self.run_move(live, clock, **bounds)
                self.assertIsNone(live.input_values)

    def test_whole_pulse_swallowed_by_actual_submit_delay_waits_neutral_then_retries(self):
        clock = Clock()
        live = Live(clock, dynamics=True, publication_stalls_at_pulse={1:.654})
        result = self.run_move(live, clock, pulse_seconds=.25, max_seconds=5.,
                              target_distance=.10, cruise_magnitude=.45, brake_magnitude=.30)
        self.assertTrue(result["target_reached"])
        self.assertEqual(result["unsampled_attempts"],1)
        self.assertEqual(result["pulses"][0]["sampled_input_count"],0)
        self.assertEqual(result["pulses"][0]["settled_distance"],0.)
        self.assertEqual(result["pulses"][0]["outcome"],"unsampled_unchanged_endpoint_retry_admitted")
        self.assertAlmostEqual(result["attempted_input_seconds"]-result["sampled_input_seconds"],.25)
        cadence = [v for name,v in live.events.values if name=="locomotion_neutral_cadence"]
        self.assertGreaterEqual(cadence[-1]["advancing_samples"],3)
        self.assertGreaterEqual(cadence[-1]["span_ms"],60)
        self.assertTrue(all(lease==.25 for lease in live.leases))
        self.assertEqual(live.native_reads_while_held,0)
        self.assertLess(clock(),5.)
        self.assertFalse(live.held)

    def test_repeated_unsampled_submit_attempts_exhaust_explicit_retry_budget(self):
        clock = Clock()
        live = Live(clock, dynamics=True, publication_stalls_at_pulse={1:.654,2:.654})
        with self.assertRaisesRegex(BotFault,"retry budget exhausted"):
            self.run_move(live,clock,pulse_seconds=.25,max_seconds=5.,
                          max_unsampled_retries=1,cruise_magnitude=.45,brake_magnitude=.30)
        self.assertEqual(len(live.input_history),2)
        self.assertEqual(live.position,[10.,20.,30.])
        self.assertEqual(live.native_reads_while_held,0)
        self.assertFalse(live.held)

    def test_no_explicit_retry_budget_stops_after_first_unsampled_attempt(self):
        clock = Clock()
        live = Live(clock,dynamics=True,publication_stalls_at_pulse={1:.654})
        with self.assertRaisesRegex(BotFault,"retry budget exhausted"):
            self.run_move(live,clock,max_seconds=5.,max_unsampled_retries=0)
        self.assertEqual(len(live.input_history),1)
        self.assertFalse(live.held)

    def test_unsampled_native_displacement_cannot_authorize_replaying_axis(self):
        clock = Clock()
        live = Live(clock,dynamics=True,publication_stalls_at_pulse={1:.654},consume_while_stalled=True)
        with self.assertRaisesRegex(BotFault,"changed native position; do not retry"):
            self.run_move(live,clock,max_seconds=5.,cruise_magnitude=.45,brake_magnitude=.30)
        self.assertEqual(len(live.input_history),1)
        self.assertGreater(live.position[0]-10.,.015)
        self.assertEqual(live.native_reads_while_held,0)
        self.assertFalse(live.held)

    def test_frozen_neutral_publication_cannot_qualify_as_advancing_cadence(self):
        clock = Clock()
        live = Live(clock,dynamics=True,publication_stalls_at_pulse={1:2.})
        with self.assertRaisesRegex(BotFault,"did not resume bounded fresh cadence"):
            self.run_move(live,clock,max_seconds=5.)
        self.assertEqual(len(live.input_history),1)
        self.assertEqual(live.position,[10.,20.,30.])
        self.assertEqual(live.native_reads_while_held,0)
        self.assertFalse(live.held)

    def test_unknown_input_rpc_completion_is_released_and_never_replayed(self):
        clock=Clock()
        live=Live(clock,dynamics=True)
        original_input=live.input
        def failed_input(*args,**kwargs):
            original_input(*args,**kwargs)
            raise TimeoutError("completion unknown")
        live.input=failed_input
        with self.assertRaisesRegex(TimeoutError,"completion unknown"):
            self.run_move(live,clock,max_seconds=5.)
        self.assertEqual(len(live.input_history),1)
        self.assertFalse(live.held)

    def test_recovery_cannot_issue_native_reads_after_short_wall_budget(self):
        clock=Clock()
        live=Live(clock,dynamics=True,publication_stalls_at_pulse={1:.654})
        with self.assertRaisesRegex(BotFault,"bounded.*(deadline|cadence)"):
            self.run_move(live,clock,max_seconds=.5,progress_timeout=.2)
        self.assertEqual(len(live.input_history),1)
        self.assertEqual(live.native_reads,1)
        self.assertEqual(live.native_reads_while_held,0)
        self.assertFalse(live.held)

    def test_camera_change_while_publication_missing_aborts_retry(self):
        clock=Clock()
        live=Live(clock,dynamics=True,publication_stalls_at_pulse={1:.654})
        original_observe=live.observe
        def observe(native=False):
            state=original_observe(native=native)
            if clock()>.35:state["activation"]+=1
            return state
        live.observe=observe
        with self.assertRaisesRegex(BotFault,"Camera generation"):
            self.run_move(live,clock,max_seconds=5.)
        self.assertEqual(len(live.input_history),1)
        self.assertFalse(live.held)

    def test_recovered_publication_with_non_neutral_controls_cannot_retry(self):
        clock=Clock()
        live=Live(clock,dynamics=True,publication_stalls_at_pulse={1:.654})
        original_observe=live.observe
        def observe(native=False):
            state=original_observe(native=native)
            if clock()>=live.publication_stalled_until and live.input_history:
                state["controls"]["sticks"][1]=.45
            return state
        live.observe=observe
        with self.assertRaisesRegex(BotFault,"Center the sticks"):
            self.run_move(live,clock,max_seconds=5.)
        self.assertEqual(len(live.input_history),1)
        self.assertFalse(live.held)

    def test_retry_budget_cannot_be_unbounded_or_boolean(self):
        for budget in (-1,3,True,1.0):
            with self.subTest(budget=budget):
                clock=Clock()
                live=Live(clock)
                with self.assertRaisesRegex(BotFault,"bounds"):
                    self.run_move(live,clock,max_unsampled_retries=budget)
                self.assertIsNone(live.input_values)

    def test_native_mission_change_rejects_the_next_pulse(self):
        clock = Clock()
        live = Live(clock, dynamics=True)
        original_observe = live.observe
        def observe(native=False):
            state = original_observe(native=native)
            if native and live.native_reads == 2:
                state["native"]["mission"] = 30011
            return state
        live.observe = observe
        with self.assertRaisesRegex(BotFault, "Mission, location, sequence"):
            self.run_move(live, clock)
        self.assertEqual(len(live.input_history), 1)
        self.assertEqual(live.native_reads_while_held, 0)
        self.assertFalse(live.held)

    def test_camera_generation_change_during_fast_sampling_releases(self):
        clock = Clock()
        live = Live(clock, dynamics=True)
        original_observe = live.observe
        def observe(native=False):
            state = original_observe(native=native)
            if not native:
                state["activation"] += 1
            return state
        live.observe = observe
        with self.assertRaisesRegex(BotFault, "Camera generation"):
            self.run_move(live, clock)
        self.assertEqual(len(live.input_history), 1)
        self.assertFalse(live.held)

    def test_non_neutral_physical_gamepad_rejects_before_stick_input(self):
        clock = Clock()
        live = Live(clock, bad_gamepad=True)

        with self.assertRaisesRegex(BotFault, "physical XInput"):
            self.run_move(live, clock)

        self.assertIsNone(live.input_values)
        self.assertFalse(live.held)

    def test_malformed_physical_audit_rejects_before_input(self):
        clock = Clock()
        live = Live(clock, malformed_physical=True)

        with self.assertRaisesRegex(BotFault, "physical input audit"):
            self.run_move(live, clock)

        self.assertIsNone(live.input_values)
        self.assertFalse(live.held)

    def test_hard_displacement_limit_releases_input(self):
        clock = Clock()
        live = Live(clock, step=0.8)

        with self.assertRaisesRegex(BotFault, "exceeded the bounded movement envelope"):
            self.run_move(live, clock)

        self.assertFalse(live.held)
        self.assertEqual(live.releases, 2)


class IntervalTests(unittest.TestCase):
    def fixture(self, clock, failed_at=None):
        class Observer:
            held = False
            events = Events()

            def observe(self, native=False):
                return {'camera_active': failed_at is None or clock() < failed_at,
                        'native': {'mission': 10010}}
        return Observer()

    def test_observes_complete_loss_window(self):
        clock = Clock()
        live = self.fixture(clock)
        result = observe_interval(live, {'seconds': 6, 'state_during': {
            'camera_active': True, 'native.mission': 10010}}, clock=clock, sleep=clock.sleep)
        self.assertEqual(result['observed_seconds'], 6)
        self.assertGreaterEqual(result['samples'], 60)

    def test_rejects_mid_interval_camera_failure(self):
        clock = Clock()
        live = self.fixture(clock, failed_at=2.5)
        with self.assertRaisesRegex(BotFault, 'invariant failed'):
            observe_interval(live, {'seconds': 6, 'state_during': {'camera_active': True}},
                             clock=clock, sleep=clock.sleep)
        self.assertLess(clock(), 3)

    def test_refuses_observation_with_held_input(self):
        clock = Clock()
        live = self.fixture(clock)
        live.held = True
        with self.assertRaisesRegex(BotFault, 'released input'):
            observe_interval(live, {'seconds': 6, 'state_during': {'camera_active': True}},
                             clock=clock, sleep=clock.sleep)
        self.assertFalse(live.events.values)


if __name__ == "__main__":
    unittest.main()
