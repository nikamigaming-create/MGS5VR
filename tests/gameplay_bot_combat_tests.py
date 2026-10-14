"""Native/XR basis joins, occlusion rejection and actor eligibility contracts."""
import math
import pathlib
import sys
import unittest
from unittest.mock import Mock, patch
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from gameplay_bot.combat import AimProbeUnavailable, active_targets, admit, aim_error, aim_quaternion, aimed_support_frame, arm, battle_checkpoint, controller_pair, corrected_aim, effective_pitch, facing_error, fire, fresh_rig, looking_head, mul, paired_aim_position, pointing, support_poses, sight, track_and_fire
from gameplay_bot.core import BotFault
from gameplay_bot.live import rotate
from gameplay_bot.autonomy import choose_vantage
from gameplay_bot.navigation import NativeMap
from gameplay_bot.health import HealthRecoveryNeeded, critical_wound, needs_recovery, retreat_goal
from gameplay_bot.commander import CommanderBattleChanged, ask_commander, validate_guidance
from gameplay_bot.emplacement import check_guards, require_unburdened_field, verify_fixture
from gameplay_bot.recon import marked_lines
from gameplay_bot.route import body_heading_error, set_posture
from gameplay_bot.cover import floor_candidates, select as select_cover, verified_candidates


class CombatTests(unittest.TestCase):
    def test_exact_mission_target_cannot_substitute_another_actor_or_bypass_visibility(self):
        enemies={'truncated':False,'unavailable_loaded':0,'normal_life':'0',
            'soldiers':[{'id':1,'name':'sol_guard','life':'0','camp':'camp'},
                        {'id':2,'name':'sol_target_0000','life':'0','camp':'camp'}]}
        geometry={'targets':[{'id':1,'query_valid':True,'unoccluded':True,'distance':5},
                             {'id':2,'query_valid':True,'unoccluded':False,'distance':20}]}
        self.assertEqual(active_targets(enemies,geometry,camp='camp',target_name='sol_target_0000'),[])
        targets=active_targets(enemies,geometry,camp='camp',target_name='sol_target_0000',require_clear=False)
        self.assertEqual([x['id'] for x in targets],[2])
        self.assertFalse(targets[0]['sight']['unoccluded'])
        self.assertEqual(active_targets(enemies,geometry,camp='other',target_name='sol_target_0000',require_clear=False),[])
        self.assertEqual(active_targets(enemies,geometry,target_name='sol_missing'),[])
        self.assertEqual(active_targets(enemies,geometry,target_name='sol_target_0000',radius=10,require_clear=False),[])
        for name in ('', 'hos_diamond_0000', 'sol_target\";return true'):
            with self.assertRaises(BotFault): active_targets(enemies,geometry,target_name=name)

    def test_crawl_facing_distinguishes_view_forward_from_native_body_backwards(self):
        self.assertAlmostEqual(body_heading_error({'player_yaw':90},[0,0,0],[1,0,0]),0.)
        self.assertGreater(body_heading_error({'player_yaw':-12},[0,0,0],[.206,0,-.978]),179.)
        self.assertAlmostEqual(body_heading_error({'player_yaw':179},[0,0,0],[math.sin(math.radians(-179)),0,math.cos(math.radians(-179))]),2.)
        self.assertIsNone(body_heading_error({'player_yaw':None},[0,0,0],[1,0,0]))

    def test_active_camp_diagnostic_is_explicit_and_does_not_pass_camp_clearance(self):
        live=Mock();snapshot={'normal_life':'0','soldiers':[{'camp':'camp','life':'0','position':[12,0,0]}]}
        with patch('gameplay_bot.emplacement.read_enemy_snapshot',return_value=snapshot):
            self.assertEqual(check_guards(live,'native script','camp',[0,0,0],diagnostic_active_camp=True),snapshot)
            with self.assertRaises(BotFault):check_guards(live,'native script','camp',[0,0,0])
        self.assertFalse(live.events.emit.call_args.kwargs['camp_secured'])
        live.input.assert_not_called()

    def test_short_stance_with_actual_native_transition_is_not_replayed(self):
        before={'activation':3,'native':{'mission':30010,'location':10,'sequence':'Seq_Game_FreePlay',
                'status_STAND':True,'status_CARRY':False,'player_vehicle_id':65535}}
        after={'activation':3,'native':{**before['native'],'status_STAND':False,'status_SQUAT':True}}
        live=Mock();live.observe.return_value=before
        live.execute.side_effect=BotFault('Requested physical input was never observed held by the native XR control sampler')
        with patch('gameplay_bot.route.fresh_control_observation',return_value=(after,{})), \
             patch('gameplay_bot.route._validate_xr_field_state'):
            set_posture(live,'crouch',sleep=lambda _:None)
        self.assertEqual(live.execute.call_count,1)
        self.assertFalse(live.events.emit.call_args.kwargs['physical_held_sample_observed'])
        self.assertFalse(live.events.emit.call_args.kwargs['input_replayed'])

    def test_unknown_stance_effect_remains_failed_without_replay(self):
        before={'activation':3,'native':{'mission':30010,'location':10,'sequence':'Seq_Game_FreePlay',
                'status_STAND':True,'status_CARRY':False,'player_vehicle_id':65535}}
        for after in (before,{**before,'activation':4},
                      {'activation':3,'native':{**before['native'],'status_CARRY':True,'status_SQUAT':True}}):
            live=Mock();live.observe.return_value=before
            live.execute.side_effect=BotFault('Requested physical input was never observed held by the native XR control sampler')
            with patch('gameplay_bot.route.fresh_control_observation',return_value=(after,{})), \
                 patch('gameplay_bot.route._validate_xr_field_state'), self.assertRaises(BotFault):
                set_posture(live,'crouch',sleep=lambda _:None)
            self.assertEqual(live.execute.call_count,1)

    def test_measured_barrel_feedback_corrects_different_authored_weapon_axes(self):
        for yaw in (-149.,0.,61.):
            camera={'orientation':[0,math.sin(math.radians(yaw)/2),0,math.cos(math.radians(yaw)/2)]}
            basis=mul(camera['orientation'],[0,1,0,0])
            current=pointing([.2,.3,-1.]);target=[4.,3.,11.]
            for pitch in (-11.,0.,6.):
                offset=[math.sin(math.radians(pitch)/2),0,0,math.cos(math.radians(pitch)/2)]
                muzzle={'position':[1.,1.,2.],'orientation':mul(mul(basis,current),offset)}
                next_aim=corrected_aim(camera,muzzle,target,current)
                observed={**muzzle,'orientation':mul(mul(basis,next_aim),offset)}
                along,lateral=aim_error(observed,target)
                self.assertGreater(along,1.);self.assertLess(lateral,1e-7)

    def test_barrel_feedback_handles_opposite_direction_without_unknown_axis(self):
        camera={'orientation':[0,0,0,1]};muzzle={'position':[0,0,0],'orientation':[0,0,0,1]}
        current=[0,1,0,0];corrected=corrected_aim(camera,muzzle,[0,0,-10],current)
        observed={**muzzle,'orientation':mul([0,1,0,0],corrected)}
        self.assertLess(aim_error(observed,[0,0,-10])[1],1e-7)

    def test_on_foot_transition_waits_without_replaying_inputs(self):
        import copy
        native={'status_NORMAL_ACTION':False,'status_CARRY':False,'status_CQC_HOLD':False,
                'status_CQC_CONTINUOUS':False,'saving':False,'popup':False,'game_over':0,
                'player_vehicle_id':65535,'title':False,'sequence':'Seq_Game_FreePlay',
                'status_STAND':True,'player_life':5597}
        state={'native':native,'now_ms':1000,'rendered':{'presentation_focused':True,'sample_ms':1000,
              'raw_right_grip_tracked':True,'raw_right_aim_tracked':True}}
        settled=copy.deepcopy(state);settled['native']['status_NORMAL_ACTION']=True
        live=Mock();now=[0.];anchor=(30010,10,3,4,5)
        with patch('gameplay_bot.combat.fresh_rig',side_effect=[(state,anchor),(settled,anchor)]), \
             patch('gameplay_bot.combat._validate_field_state',side_effect=[BotFault('transition'),None]), \
             patch('gameplay_bot.locomotion._validate_xr_field_state') as xr:
            actual,_=admit(live,anchor,clock=lambda:now[0],sleep=lambda d:now.__setitem__(0,now[0]+d))
        self.assertTrue(actual['native']['status_NORMAL_ACTION']);xr.assert_called_once_with(state)
        live.input.assert_not_called();live.execute.assert_not_called();live.call.assert_not_called()
        self.assertFalse(live.events.emit.call_args.kwargs['input_replayed'])

    def test_unknown_carried_or_non_field_state_does_not_become_an_ordinary_wait(self):
        live=Mock();anchor=(30010,10,3,4,5)
        state={'native':{'status_NORMAL_ACTION':False,'status_CARRY':True}}
        with patch('gameplay_bot.combat.fresh_rig',return_value=(state,anchor)), \
             patch('gameplay_bot.combat._validate_field_state',side_effect=BotFault('carry')):
            with self.assertRaisesRegex(BotFault,'carry'):admit(live,anchor)
        live.input.assert_not_called();live.execute.assert_not_called()

    def test_native_scripted_horse_shotgun_admits_non_normal_action_only_in_its_owner(self):
        import copy
        from gameplay_bot_locomotion_tests import Clock, Live
        state=Live(Clock()).observe(native=True)
        state['native'].update(mission=10010,location=30,story=0,
            sequence='Seq_Game_VolginRide',hospital_volgin_ride=True,weapon_PRIMARY_HIP=70,
            status_NORMAL_ACTION=False,status_PARTS_ACTIVE=True,status_SQUAT=True,
            status_CARRY=False,status_INTRUDE=False,status_SUBJECT=False,player_life=6000)
        state['rendered']={'presentation_focused':True,'sample_ms':state['now_ms'],
            'raw_right_grip_tracked':True,'raw_right_aim_tracked':True}
        anchor=(10010,30,3,4,5);live=Mock()
        with patch('gameplay_bot.combat.fresh_rig',return_value=(state,anchor)):
            actual,_=admit(live,anchor,witnessed_hospital_ride=True)
        self.assertIs(actual,state)
        for change in ({'hospital_volgin_ride':False},{'mission':30010},
                       {'sequence':'Seq_Game_VolginInEntrance'},{'weapon_PRIMARY_HIP':3}):
            wrong=copy.deepcopy(state);wrong['native'].update(change)
            with self.subTest(change=change),patch('gameplay_bot.combat.fresh_rig',return_value=(wrong,anchor)):
                with self.assertRaisesRegex(BotFault,'ride and shotgun owner'):
                    admit(live,anchor,witnessed_hospital_ride=True)
        live.input.assert_not_called();live.execute.assert_not_called()

    def test_battle_recording_checkpoint_never_blocks_on_compositor_or_claims_stereo(self):
        live=Mock();live.held_action_recording=Mock()
        self.assertEqual(battle_checkpoint(live,'detected',{'native':{'not_alert':False}}),[])
        live.held_action_recording.check.assert_called_once();live.capture.assert_not_called()
        self.assertFalse(live.events.emit.call_args.kwargs['final_compositor_captured'])
        live.held_action_recording=None;live.capture.return_value=['left.png','right.png']
        self.assertEqual(battle_checkpoint(live,'nonrecorded',{'native':{'not_alert':False}}),['left.png','right.png'])
        self.assertEqual(live.capture.call_count,1)

    def test_recovery_skips_a_walked_point_outside_nav_and_selects_connected_native_floor(self):
        nav=NativeMap([{'source':'owned-fixture','sha256':'a'*64,'groups':[{'id':0,
            'nodes':[[0,0,0],[-30,0,0]],'edges':[{'from':0,'to':1,'flags':0}]}]}])
        row=lambda x:{'player_x':x,'player_y':1,'player_z':0,'player_life':7801,'not_alert':True}
        self.assertEqual(retreat_goal([0,1,0],[row(-30),row(-35)],7801,nav),[-30,0,0])
        with self.assertRaisesRegex(BotFault,'retreat'):
            retreat_goal([0,1,0],[row(-35)],7801,nav)

    def test_unknown_body_carry_health_loss_or_detection_blocks_independent_emplacement(self):
        native={'status_CARRY':False,'status_NORMAL_ACTION':True,'not_alert':True,'player_life':7801}
        require_unburdened_field({'native':native},7801)
        for changes in ({'status_CARRY':True},{'status_CARRY':None},{'status_NORMAL_ACTION':False},
                        {'not_alert':False},{'player_life':7800}):
            with self.subTest(changes=changes),self.assertRaises(BotFault):
                require_unburdened_field({'native':{**native,**changes}},7801)

    def test_commander_wait_refreshes_both_eyes_and_only_accepts_the_current_request(self):
        import json
        import tempfile
        now=[0.];requests=[]
        nav=NativeMap([{'source':'owned-fixture','sha256':'a'*64,'groups':[{'id':0,
            'nodes':[[0,0,0],[10,0,0]],'edges':[{'from':0,'to':1,'flags':0}]}]}])
        anchor=(30010,10,3,4,5)
        state={'activation':3,'rendered':{'player_owner':4,'presentation_epoch':5},
               'native':{'mission':30010,'location':10,'player_x':0,'player_y':0,'player_z':0,
                         'player_life':7801,'not_alert':True}}
        live=Mock()
        with tempfile.TemporaryDirectory() as directory:
            output=pathlib.Path(directory)
            def capture(label):
                paths=[output/(label+'-'+eye+'.png') for eye in ('left','right')]
                for path in paths:path.write_bytes(path.name.encode())
                return paths
            live.capture.side_effect=capture
            def sleep(duration):
                now[0]+=duration
                request=json.loads((output/'commander/request.json').read_text())
                if not requests:requests.append(request)
                if request['request_id']!=requests[0]['request_id'] and len(requests)==1:
                    requests.append(request)
                    decision={'request_id':request['request_id'],'kind':'mapped_waypoint',
                        'map_identity':nav.identity,'reviewed_both_capture_sha256':[r['sha256'] for r in request['captures']],
                        'goal':[10,0,0],'lesson':'Use the connected side route after checking the current eyes'}
                    stale={**decision,'request_id':requests[0]['request_id']}
                    with self.assertRaisesRegex(BotFault,'another request'):
                        validate_guidance(stale,request,nav,state,now_ns=1_000_000_000+round(now[0]*1e9))
                    (output/'commander/decision.json').write_text(json.dumps(decision))
            with patch('gameplay_bot.commander.admit',return_value=(state,anchor)), \
                 patch('gameplay_bot.commander.time.time_ns',side_effect=lambda:1_000_000_000+round(now[0]*1e9)):
                goal,plan=ask_commander(live,nav,output=output,anchor=anchor,reason='blocked',stages=[],
                                       deadline=80.,wait_seconds=40.,clock=lambda:now[0],sleep=sleep)
            self.assertEqual(goal,[10,0,0]);self.assertEqual(len(requests),2)
            self.assertNotEqual(requests[0]['captures'],requests[1]['captures'])
            self.assertEqual(plan['request_id'],requests[1]['request_id'])
            self.assertEqual(live.capture.call_count,2)
            live.input.assert_not_called();live.execute.assert_not_called()

    def test_detection_and_damage_interrupt_commander_wait_without_dispatching_inputs(self):
        import tempfile
        for detected in (True,False):
            with self.subTest(detected=detected),tempfile.TemporaryDirectory() as directory:
                output=pathlib.Path(directory);now=[0.];live=Mock();anchor=(30010,10,3,4,5)
                captures=[output/'left.png',output/'right.png']
                for path in captures:path.write_bytes(path.name.encode())
                live.capture.return_value=captures
                def admit(*unused):
                    return {'native':{'not_alert':not (detected and now[0]>.05),
                                      'player_life':7800 if not detected and now[0]>.05 else 7801}},anchor
                with patch('gameplay_bot.commander.admit',side_effect=admit):
                    with self.assertRaises(CommanderBattleChanged):
                        ask_commander(live,Mock(identity='owned-map'),output=output,anchor=anchor,
                            reason='blocked',stages=[],deadline=10.,clock=lambda:now[0],
                            sleep=lambda duration:now.__setitem__(0,now[0]+duration))
                self.assertEqual(live.capture.call_count,1)
                live.input.assert_not_called();live.execute.assert_not_called()

    @staticmethod
    def cover_fixture():
        nav=NativeMap([{'source':'owned-fixture','sha256':'a'*64,'groups':[{'id':0,
            'nodes':[[0,0,0],[3,0,0],[6,0,0],[3,3,0]],
            'edges':[{'from':0,'to':1,'flags':0},{'from':1,'to':2,'flags':0}]}]}])
        state={'now_ms':1000,'activation':2,'rendered':{'player_owner':99,'presentation_epoch':4},
               'native':{'player_x':0,'player_y':0,'player_z':0}}
        ray=lambda blocked:{'actor_id':1167,'query_valid':True,'query_reason':9 if blocked else 3,
            'terrain_obstructed':blocked,'ray_result':1 if blocked else 0,
            'distance':10.,'raw_along':5. if blocked else 0.,'raw_lateral_squared':0.}
        rows=[{'node':1,'point':[3,0,0],'eye':[3,1,0],'rays':[ray(True)]},
              {'node':2,'point':[6,0,0],'eye':[6,1,0],'rays':[ray(False)]}]
        data={'schema':1,'source':'native_marker_job_terrain_obstruction','available':True,'nonce':7,
              'activation':2,'player_owner':99,'presentation_epoch':4,'now_ms':1000,
              'sample_ms':1000,'frame_sample_ms':1000,'observed_player_ms':1000,
              'issued_ms':1000,'expires_ms':2500,'observed_player':[0,0,0],'eye_offset':[0,1,0],
              'persons':[{'id':1167}],'candidates':rows}
        return nav,state,data

    def test_cover_neighbourhood_cannot_jump_to_a_disconnected_upper_floor(self):
        nav,state,data=self.cover_fixture()
        self.assertEqual(set(floor_candidates(nav,[0,0,0],[0,0,0])),{0,1,2})
        self.assertNotIn(3,floor_candidates(nav,[0,0,0],[0,0,0]))

    def test_cover_result_rejects_stale_owner_geometry_and_false_obstruction(self):
        import copy
        nav,state,data=self.cover_fixture()
        self.assertEqual(len(verified_candidates(data,nav,[1,2],state,7)),2)
        for changes in ({'nonce':8},{'player_owner':100},{'sample_ms':849},{'observed_player_ms':749},
                        {'available':False},{'eye_offset':[0,3.1,0]}):
            with self.assertRaises(BotFault):verified_candidates({**data,**changes},nav,[1,2],state,7)
        for key,value in (('point',[3,3,0]),('eye',[3,1.2,0])):
            other=copy.deepcopy(data);other['candidates'][0][key]=value
            with self.assertRaises(BotFault):verified_candidates(other,nav,[1,2],state,7)
        other=copy.deepcopy(data);other['candidates'][0]['rays'][0]['query_valid']=False
        with self.assertRaises(BotFault):verified_candidates(other,nav,[1,2],state,7)
        with self.assertRaises(BotFault):verified_candidates(data,nav,[1,2],state,7,transport_seconds=.151)

    def test_cover_and_peek_have_different_visibility_requirements_without_claiming_ai_concealment(self):
        nav,state,data=self.cover_fixture();rows=data['candidates']
        home=select_cover(rows,{1167},[0,0,0],[6,0,0])
        peek=select_cover(rows,{1167},[0,0,0],[3,0,0],expose_id=1167)
        self.assertEqual(home['node'],1);self.assertEqual(peek['node'],2)
        self.assertTrue(home['observed_marker_cover']);self.assertFalse(peek['observed_marker_cover'])
        self.assertFalse(home['enemy_ai_concealment_proven']);self.assertFalse(home['shadows_proven'])
        partial=select_cover(rows,{1167,1214},[0,0,0],[6,0,0])
        self.assertEqual(partial['missing_active_guard_ids'],[1214]);self.assertFalse(partial['observed_marker_cover'])
        rows[1]['rays'][0]['distance']=50.
        self.assertIsNone(select_cover(rows,{1167},[0,0,0],[3,0,0],expose_id=1167))

    def test_moving_guard_is_reacquired_before_fire_and_ambiguous_discharge_is_never_replayed(self):
        live=Mock();actor={'id':1167};options=dict(pair={},support_pair={},ready_inputs=['right_grip','left_grip'])
        with patch('gameplay_bot.combat.face_and_aim') as aim, \
             patch('gameplay_bot.combat.fire',side_effect=[AimProbeUnavailable('moved before dispatch'),
                                                         {'rounds':1}]) as trigger:
            self.assertEqual(track_and_fire(live,actor,None,-30,**options),{'rounds':1})
            self.assertEqual(aim.call_count,2);self.assertEqual(trigger.call_count,2)
        with patch('gameplay_bot.combat.face_and_aim') as aim, \
             patch('gameplay_bot.combat.fire',side_effect=BotFault('trigger completion unknown')) as trigger:
            with self.assertRaisesRegex(BotFault,'completion unknown'):
                track_and_fire(live,actor,None,-30,**options)
            self.assertEqual(aim.call_count,1);self.assertEqual(trigger.call_count,1)
        live.input.assert_not_called()

    def test_rig_gap_waits_readonly_for_a_new_same_owner_frame_without_accepting_stale_pose(self):
        anchor=(30010,10,2,99,4);now=[0.]
        state=lambda sample,owner=99:{'now_ms':1000,'activation':2,
            'rendered':{'sample_ms':sample,'player_owner':owner,'presentation_epoch':4,'presentation_focused':True},
            'native':{'mission':30010,'location':10,'player_life':7801}}
        live=Mock()
        with patch('gameplay_bot.combat.fresh_control_observation',
                   side_effect=[(state(792),{}),(state(980),{})]):
            result,owner=fresh_rig(live,anchor,clock=lambda:now[0],
                                  sleep=lambda d:now.__setitem__(0,now[0]+d))
        self.assertEqual(result['rendered']['sample_ms'],980);self.assertEqual(owner,anchor)
        self.assertEqual(now[0],.025)
        with patch('gameplay_bot.combat.fresh_control_observation',
                   side_effect=[(state(792),{}),(state(980,100),{})]):
            with self.assertRaisesRegex(BotFault,'owner changed'):
                fresh_rig(live,anchor,clock=lambda:now[0],sleep=lambda d:now.__setitem__(0,now[0]+d))
        live.input.assert_not_called();live.execute.assert_not_called()

    def test_mortar_registry_category_and_game_object_type_have_distinct_domains(self):
        policy={'id':41473,'type':81,'registry_kind':5}
        observed={'id':41473,'type':81,'declared_type':81,'registry_kind':5,'lookup_status':True,'broken':False}
        self.assertEqual(verify_fixture(observed,policy),observed)
        for record in ({**observed,'registry_kind':81},{**observed,'declared_type':5},
                       {**observed,'type':82},{**observed,'id':41472},
                       {**observed,'lookup_status':1},{**observed,'lookup_status':False}):
            with self.assertRaises(BotFault):verify_fixture(record,policy)

    def test_expired_same_owner_observation_lease_requires_a_new_native_sample(self):
        anchor=(30010,10,7,99,4)
        base={'activation':7,'player_owner':99,'presentation_epoch':4,'now_ms':1100,
              'sample_ms':1049,'muzzle_tracked':True,'targets':[]}
        live=Mock();now=[0.]
        live.native.read.side_effect=[({**base,'enabled':False},{}),
            ({'armed':True,'now_ms':1050},{}),({**base,'enabled':True},{}),
            ({**base,'enabled':True,'sample_ms':1100},{})]
        state={'now_ms':1100,'rendered':{'sample_ms':1100}}
        with patch('gameplay_bot.combat.admit',return_value=(state,anchor)), \
             patch('gameplay_bot.combat.time.sleep',side_effect=lambda d:now.__setitem__(0,now[0]+d)):
            data,_=sight(live,anchor,clock=lambda:now[0])
        self.assertEqual(data['sample_ms'],1100)
        self.assertEqual(live.native.read.call_args_list[1].args[0],'diagnostics:combat-sight:3000')
        live.input.assert_not_called()

    def test_unarmed_observer_can_start_from_zero_owner_then_requires_fresh_current_owner(self):
        anchor=(30010,10,7,99,4)
        blank={'enabled':False,'activation':0,'player_owner':0,'presentation_epoch':0,
               'sample_ms':0,'now_ms':1000,'muzzle_tracked':False}
        observed={'enabled':True,'activation':7,'player_owner':99,'presentation_epoch':4,
                  'sample_ms':1100,'now_ms':1100,'muzzle_tracked':True,'targets':[]}
        state={'now_ms':1100,'rendered':{'sample_ms':1100}}
        for owner in (99,100):
            live=Mock()
            live.native.read.side_effect=[(blank,{}),({'armed':True,'now_ms':1050},{}),
                ({**observed,'player_owner':owner},{})]
            with patch('gameplay_bot.combat.admit',return_value=(state,anchor)), \
                 patch('gameplay_bot.combat.time.sleep'):
                if owner==99:
                    self.assertEqual(sight(live,anchor)[0]['player_owner'],99)
                else:
                    with self.assertRaises(BotFault):sight(live,anchor)
            self.assertEqual(live.native.read.call_args_list[1].args[0],'diagnostics:combat-sight:3000')
            live.input.assert_not_called()

    def test_native_bolt_cycle_waits_for_fresh_muzzle_without_replaying_trigger(self):
        anchor=(30010,10,7,99,4);live=Mock();now=[0.]
        def native_sample(_):
            stamp=1000+round(now[0]*1000)
            return {'enabled':True,'activation':7,'player_owner':99,'presentation_epoch':4,
                    'sample_ms':stamp,'now_ms':stamp,'muzzle_tracked':now[0]>=1.3,'targets':[]},{}
        def state_sample(*_):
            stamp=1000+round(now[0]*1000)
            return {'now_ms':stamp,'rendered':{'sample_ms':stamp}},anchor
        live.native.read.side_effect=native_sample
        with patch('gameplay_bot.combat.admit',side_effect=state_sample), \
             patch('gameplay_bot.combat.time.sleep',side_effect=lambda d:now.__setitem__(0,now[0]+d)):
            data,_=sight(live,anchor,clock=lambda:now[0])
        self.assertGreaterEqual(now[0],1.3);self.assertLess(now[0],2.8)
        self.assertTrue(data['muzzle_tracked']);self.assertEqual(data['sample_ms'],data['now_ms'])
        self.assertTrue(all(c.args==('inspect-combat-sight',) for c in live.native.read.call_args_list))
        live.input.assert_not_called();live.execute.assert_not_called();live.call.assert_not_called()

    def test_recon_requires_a_positive_native_mark_for_the_exact_soldier(self):
        text='\n'.join(['Physical optic native person mark id=12140 result=1 acquired=1',
                        'Physical optic native person mark id=1214 result=1 acquired=0',
                        'Physical optic native person mark id=1214 result=1 acquired=1'])
        self.assertEqual(marked_lines(text,1214),[text.splitlines()[-1]])
        self.assertEqual(marked_lines('Physical optic native person mark id=1214 result=1 acquired=true',1214),
                         ['Physical optic native person mark id=1214 result=1 acquired=true'])
        self.assertEqual(marked_lines('old unknown marker event',1214),[])

    def test_authored_local_support_contact_preserves_the_left_controller_pair(self):
        pair=controller_pair({'position':[-.3,1.4,-.4],'orientation':[.5,0,0,math.sqrt(.75)]},
                             {'position':[-.3,1.43,-.495],'orientation':[0,0,0,1]})
        contact={'position':[.17,1.52,-.31],'orientation':pointing([1,.2,-.5])}
        grip,aim=support_poses({'weapon_support_tracked':True,'weapon_support_grip':contact},pair)
        self.assertEqual(grip['position'],contact['position'])
        joined=controller_pair(grip,aim)
        for key in ('position','orientation'):
            for a,b in zip(joined[key],pair[key]):self.assertAlmostEqual(a,b,places=7)
        with self.assertRaisesRegex(BotFault,'tracked'):
            support_poses({'weapon_support_tracked':False,'weapon_support_grip':contact},pair)

    def test_rifle_support_turns_with_primary_instead_of_holding_the_old_barrel(self):
        head={'position':[.1,.2,.05],'orientation':pointing([.6,.1,-1.])}
        old={'position':[.3,-.25,-.5],'orientation':pointing([-.3,.1,-1.])}
        contact={'position':[.2,-.3,-.9],'orientation':pointing([-.2,-.1,-1.])}
        frame={'head_pose':head,'right_grip':old,'weapon_support_tracked':True,
               'weapon_support_grip':contact}
        pair={'orientation':[.5,0,0,math.sqrt(.75)],'position':[.01,-.1,.03]}
        attitude=pointing([1.,.2,-.6]);view_position=[.22,-.25,-.45]
        moved=aimed_support_frame(frame,view_position,attitude,pair)['weapon_support_grip']
        target_q=mul(head['orientation'],mul(attitude,pair['orientation']))
        target_p=[h+d for h,d in zip(head['position'],rotate(head['orientation'],view_position))]
        old_local=rotate([-v for v in old['orientation'][:3]]+[old['orientation'][3]],
                         [s-p for s,p in zip(contact['position'],old['position'])])
        expected=[p+d for p,d in zip(target_p,rotate(target_q,old_local))]
        for a,b in zip(moved['position'],expected):self.assertAlmostEqual(a,b,places=7)
        self.assertGreater(math.dist(moved['position'],contact['position']),.2)
        self.assertAlmostEqual(math.dist(moved['position'],target_p),math.dist(contact['position'],old['position']))
        self.assertEqual(frame['weapon_support_grip'],contact)
        left_pair={'orientation':[0,0,.3,math.sqrt(.91)],'position':[.02,-.1,.04]}
        grip,aim=support_poses({**frame,'weapon_support_grip':moved},left_pair)
        for key in ('position','orientation'):
            for a,b in zip(controller_pair(grip,aim)[key],left_pair[key]):self.assertAlmostEqual(a,b,places=7)

    def test_paired_rifle_motion_refuses_missing_or_invalid_source_poses(self):
        pair={'orientation':[0,0,0,1],'position':[0,0,0]}
        valid={'position':[0,0,0],'orientation':[0,0,0,1]}
        frame={'head_pose':valid,'right_grip':valid,'weapon_support_tracked':True,'weapon_support_grip':valid}
        for broken in ({**frame,'head_pose':None},{**frame,'right_grip':None},
                       {**frame,'weapon_support_tracked':False},
                       {**frame,'right_grip':{'position':[float('nan'),0,0],'orientation':[0,0,0,1]}}):
            with self.assertRaises(BotFault):aimed_support_frame(broken,[.2,-.25,-.45],[0,0,0,1],pair)

    def test_detached_support_hand_cannot_fire_even_with_clear_aligned_geometry(self):
        actor={'id':1214,'name':'sol_guard'}
        state={'rendered':{'native_pose':{'position':[0,0,0],'orientation':[0,0,0,1]},
                           'weapon_support_attached':False}}
        geometry={'muzzle':{'position':[0,0,0],'orientation':[0,0,0,1]},
                  'targets':[{'id':1214,'query_valid':True,'unoccluded':True,'aim':[0,0,10]}]}
        live=Mock()
        with patch('gameplay_bot.combat.admit',return_value=(state,None)), \
             patch('gameplay_bot.combat.read_actor',return_value=({'life':0},{})), \
             patch('gameplay_bot.combat.sight',return_value=(geometry,state)):
            with self.assertRaisesRegex(BotFault,'support hand detached'):
                fire(live,actor,None,['right_grip','left_grip'])
        live.input.assert_not_called()

    def test_native_bolt_cycle_must_finish_before_ready_without_a_second_trigger(self):
        now=[0.]
        def observe(*args):
            return ({'now_ms':int(now[0]*1000),'rendered':{'native_firearm':True,
                'weapon_native_observer':{'muzzle_solved':now[0]>=1.8}},
                'controls':{'native_triggers':[255,0]}},None)
        live=Mock()
        with patch('gameplay_bot.combat.admit',side_effect=observe), \
             patch('gameplay_bot.combat.held_input_evidence',side_effect=lambda *a,**kw:{'sample_ms':int(now[0]*1000)}):
            arm(live,['right_grip'],None,clock=lambda:now[0],sleep=lambda duration:now.__setitem__(0,now[0]+duration))
        self.assertGreaterEqual(now[0],2.)
        self.assertLess(now[0],3.)
        self.assertEqual(live.input.call_count,2)
        self.assertTrue(all(v['component']=='Grip' for call in live.input.call_args_list for v in call.args[0]))

    def test_sampled_ready_weapon_renews_without_another_readiness_pause(self):
        state={'rendered':{'native_firearm':True,'weapon_native_observer':{'muzzle_solved':True}},
               'controls':{'native_triggers':[255,0]}}
        live=Mock();sleep=Mock()
        with patch('gameplay_bot.combat.admit',return_value=(state,None)), \
             patch('gameplay_bot.combat.held_input_evidence',return_value={'sample_ms':1000}):
            self.assertEqual(arm(live,['right_grip','left_grip'],None,sleep=sleep),state)
        sleep.assert_not_called();self.assertEqual(live.input.call_count,1)
        self.assertEqual(live.input.call_args.kwargs['lease_seconds'],3.)

    def test_trigger_expiry_does_not_shorten_support_or_replay_fire_during_outcome_wait(self):
        state={'now_ms':1000,'rendered':{'native_pose':{'position':[0,0,0],'orientation':[0,0,0,1]},
                                       'weapon_support_attached':True},
               'native':{'player_life':7801,'player_yaw':0,'ammo_in_weapon_PRIMARY_HIP':31,'ammo_in_weapon_PRIMARY_BACK':198,
                         'ammo_in_weapon_SECONDARY':9},'controls':{'native_triggers':[255,255]}}
        after={**state,'native':{**state['native'],'ammo_in_weapon_SECONDARY':8}}
        geometry={'muzzle':{'position':[0,0,0],'orientation':[0,0,0,1]},
                  'targets':[{'id':1167,'query_valid':True,'unoccluded':True,'aim':[0,0,10]}]}
        live=Mock();actor={'id':1167,'name':'sol_guard'}
        with patch('gameplay_bot.combat.admit',side_effect=[(state,None),(after,None),(after,None)]), \
             patch('gameplay_bot.combat.read_actor',side_effect=[({'life':0},{}),({'life':3},{})]), \
             patch('gameplay_bot.combat.sight',return_value=(geometry,state)), \
             patch('gameplay_bot.combat.held_input_evidence',return_value={'sample_ms':1001}), \
             patch('gameplay_bot.combat.time.monotonic',side_effect=[0.,0.,.15]):
            outcome=fire(live,actor,None,['right_grip','left_grip'],seconds=.12,expected_slot='SECONDARY')
        self.assertEqual(outcome['rounds'],1)
        self.assertEqual([v['component'] for v in live.input.call_args_list[0].args[0]],['Trigger'])
        self.assertEqual([v['component'] for v in live.input.call_args_list[1].args[0]],['Grip','Grip'])
        self.assertEqual(live.input.call_args_list[1].kwargs['lease_seconds'],3.)

    def test_mortar_waits_for_the_entire_camp_and_nearby_guard_roster(self):
        live=Mock()
        snapshot={'normal_life':'0','soldiers':[{'camp':'camp','life':'3','position':[12,0,0]}]}
        with patch('gameplay_bot.emplacement.read_enemy_snapshot',return_value=snapshot):
            self.assertEqual(check_guards(live,'native script','camp',[0,0,0]),snapshot)
        for rows in ([], [{'camp':'camp','life':'0','position':[60,0,0]}],
                     [*snapshot['soldiers'],{'camp':'other','life':'0','position':[20,0,0]}]):
            with patch('gameplay_bot.emplacement.read_enemy_snapshot',return_value={**snapshot,'soldiers':rows}):
                with self.assertRaises(BotFault):check_guards(live,'native script','camp',[0,0,0])
        live.input.assert_not_called()

    def test_close_elevated_target_turns_the_actual_view_in_pitch_and_yaw(self):
        basis=[0.,1.,0.,0.]
        for angle in (-179,-49.5,0,116):
            camera={'position':[81.6,333.55,103.1],
                    'orientation':[0,math.sin(math.radians(angle)/2),0,math.cos(math.radians(angle)/2)]}
            for target in ([85,336,99],[82,334,107],[78,332,100]):
                head=looking_head([0,0,0,1],camera,target)
                next_camera={**camera,'orientation':mul(camera['orientation'],mul(mul(basis,head),basis))}
                self.assertLess(facing_error(next_camera,target),.00001)

    def test_navigation_turn_composes_onto_current_head_instead_of_undoing_it(self):
        basis=[0.,1.,0.,0.];head=[0,math.sin(math.radians(60)/2),0,math.cos(math.radians(60)/2)]
        target=[10,1,0]
        for base_yaw in (-49.,-25.,0.,30.,60.):
            half=math.radians(base_yaw)/2;base=[0,math.sin(half),0,math.cos(half)]
            camera={'position':[0,1,0],'orientation':mul(base,mul(mul(basis,head),basis))}
            head=looking_head(head,camera,target)
            after={**camera,'orientation':mul(base,mul(mul(basis,head),basis))}
            self.assertLess(facing_error(after,target),.00001)

    def test_damage_takes_priority_before_any_fire_input(self):
        healthy={'player_life':7801,'status_INJURY_LOWER':False,'status_INJURY_UPPER':False}
        self.assertFalse(needs_recovery(healthy,7801))
        for native in ({**healthy,'player_life':7330}, {**healthy,'status_INJURY_UPPER':True}):
            live=Mock()
            with patch('gameplay_bot.combat.admit',return_value=({'native':native},None)):
                with self.assertRaises(HealthRecoveryNeeded):
                    fire(live,{'id':1214,'name':'sol_guard'},None,['right_grip'],healthy_life=7801)
            live.input.assert_not_called()
        with self.assertRaisesRegex(BotFault,'unavailable'):
            critical_wound({'status_INJURY_LOWER':False})

    def test_retreat_uses_only_observed_healthy_ground_with_alert_clear(self):
        row=lambda x,life,alert:{'player_x':x,'player_y':0,'player_z':0,'player_life':life,'not_alert':alert}
        visited=[row(-35,7801,True),row(-31,7801,False),row(-26,7330,True),row(-2,7801,True)]
        self.assertEqual(retreat_goal([0,0,0],visited,7801),[-35,0,0])
        with self.assertRaisesRegex(BotFault,'retreat'):
            retreat_goal([0,0,0],visited[1:],7801)

    def test_commander_clue_cannot_replay_older_eyes_or_change_world(self):
        nav=NativeMap([{'source':'owned-fixture','sha256':'a'*64,'groups':[{'id':0,
            'nodes':[[0,0,0],[10,0,0],[20,0,0]],
            'edges':[{'from':0,'to':1,'flags':0},{'from':1,'to':2,'flags':0}]}]}])
        state={'activation':3,'rendered':{'player_owner':4,'presentation_epoch':5},
               'native':{'mission':30010,'location':10,'player_x':0,'player_y':0,'player_z':0,
                         'player_life':7801,'not_alert':True}}
        request={'request_id':'current','issued_unix_ns':1000000000,'world_owner':[30010,10,3,4,5],
                 'captures':[{'sha256':'a'*64},{'sha256':'b'*64}]}
        decision={'request_id':'current','kind':'mapped_waypoint','map_identity':nav.identity,
                  'reviewed_both_capture_sha256':['a'*64,'b'*64],'goal':[20,0,0],'lesson':'Use the verified side route'}
        goal,plan=validate_guidance(decision,request,nav,state,now_ns=2000000000)
        self.assertEqual(goal,[20,0,0]);self.assertEqual(plan['route_nodes'],3)
        bad=[{**decision,'request_id':'old'},{**decision,'map_identity':'other'},
             {**decision,'goal':[50,0,0]},{**decision,'reviewed_both_capture_sha256':['a'*64]*2},
             {**decision,'kind':'teleport'},{**decision,'goal':[math.nan,0,0]}]
        for item in bad:
            with self.assertRaises(BotFault):validate_guidance(item,request,nav,state,now_ns=2000000000)
        with self.assertRaisesRegex(BotFault,'fresh'):
            validate_guidance(decision,request,nav,state,now_ns=32000000000)
        with self.assertRaisesRegex(BotFault,'owner'):
            validate_guidance(decision,request,nav,{**state,'activation':6},now_ns=2000000000)

    def test_controller_pair_stays_rigid_across_successive_head_and_aim_turns(self):
        grip={'position':[.3,-.33,-.46],'orientation':[.5,0,0,math.sqrt(.75)]}
        aim={'position':[.3,-.3,-.555],'orientation':[0,0,0,1]}
        pair=controller_pair(grip,aim)
        for direction in ([0,0,-1],[1,0,0],[-1,1,0],[0,0,1]):
            attitude=pointing(direction)
            new_aim={'position':paired_aim_position([.22,-.25,-.45],attitude,pair),'orientation':attitude}
            new_grip={'position':[.22,-.25,-.45],'orientation':mul(attitude,pair['orientation'])}
            observed=controller_pair(new_grip,new_aim)
            for key in ('position','orientation'):
                for a,b in zip(observed[key],pair[key]):self.assertAlmostEqual(a,b,places=7)

    def test_approach_preserves_connected_edges_and_native_supporting_floor(self):
        nav=NativeMap([{'source':'owned-fixture','sha256':'a'*64,'groups':[{
            'id':0,'nodes':[[0,0,0],[10,3,0],[20,8,0],[35,8,0],[55,8,0]],
            'edges':[{'from':i,'to':i+1,'flags':0} for i in range(4)]}]}])
        guards=[{'id':1214,'name':'sol_live_guard','position':[40,8,5],'life':0}]
        goal,plan=choose_vantage(nav,[0,0,0],{'position':[55,8,0]},30.,threats=guards)
        self.assertEqual(goal,[20,8,0]);self.assertEqual(plan['node'],2)
        with self.assertRaisesRegex(BotFault,'bounded'):choose_vantage(nav,[0,0,0],{'position':[55,8,0]},1.)
    def test_xr_minus_z_points_at_each_axis_without_roll_guess(self):
        for direction in ([0,0,-1],[1,0,0],[0,0,1],[-1,0,0],[0,1,0],[0,-1,0],[2,1,-3]):
            actual=rotate(pointing(direction),[0,0,-1]);length=math.sqrt(sum(v*v for v in direction))
            for a,b in zip(actual,direction):self.assertAlmostEqual(a,b/length,places=7)

    def test_native_head_basis_and_exported_pitch_are_both_inverted(self):
        for yaw in (-178.,-49.5,0.,117.):
            half=math.radians(yaw)/2;camera={'orientation':[0,math.sin(half),0,math.cos(half)]}
            for pitch in (-30.,0.,35.):
                origin=[108.,334.,99.];target=[91.,334.5,83.]
                raw=aim_quaternion(camera,origin,target,pitch)
                calibration=[math.sin(math.radians(pitch)/2),0,0,math.cos(math.radians(pitch)/2)]
                world=mul(mul(camera['orientation'],[0,1,0,0]),mul(raw,calibration))
                ray=rotate(world,[0,0,-1]);delta=[b-a for a,b in zip(origin,target)]
                length=math.sqrt(sum(v*v for v in delta))
                for a,b in zip(ray,delta):self.assertAlmostEqual(a,b/length,places=6)

    def test_barrel_forward_is_native_plus_z_and_reversed_aim_cannot_pass(self):
        muzzle={'position':[1,2,3],'orientation':[0,0,0,1]}
        self.assertEqual(aim_error(muzzle,[1,2,13]),(10.,0.))
        self.assertEqual(aim_error(muzzle,[1.2,2,-7])[0],-10.)
        self.assertAlmostEqual(aim_error(muzzle,[1.2,2,13])[1],.2)

    def test_invalid_geometry_and_missing_effective_calibration_fail(self):
        for direction in ([0,0,0],[math.nan,0,1],[math.inf,0,0]):
            with self.assertRaises(BotFault):pointing(direction)
        for settings in ([],[{'name':'settings.weapon_aim_pitch_degrees','value':math.nan}],
                         [{'name':'settings.weapon_aim_pitch_degrees','value':-30}]*2):
            with self.assertRaises(BotFault):effective_pitch({'settings':settings})
        self.assertEqual(effective_pitch({'settings':[{'name':'settings.weapon_aim_pitch_degrees','value':-30}]}),-30)

    def test_only_active_authored_guards_with_valid_clear_terrain_are_candidates(self):
        enemies={'normal_life':'0','truncated':False,'unavailable_loaded':0,'soldiers':[
            {'id':1,'name':'sol_a','camp':'a','life':'0'},
            {'id':2,'name':'sol_sleep','camp':'a','life':'3'},
            {'id':3,'name':'sol_occluded','camp':'a','life':'0'},
            {'id':4,'name':'sol_other','camp':'b','life':'0'},
            {'id':5,'name':'prisoner_a','camp':'a','life':'0'},
            {'id':6,'name':'sol_unavailable','camp':'a','life':'0'}]}
        targets=[{'id':i,'distance':10+i,'query_valid':i!=6,'unoccluded':i!=3} for i in range(1,7)]
        result=active_targets(enemies,{'targets':targets},camp='a')
        self.assertEqual([r['id'] for r in result],[1])
        probes=active_targets(enemies,{'targets':targets},camp='a',require_clear=False)
        self.assertEqual([r['id'] for r in probes],[1,3])
        enemies['truncated']=True
        with self.assertRaisesRegex(BotFault,'incomplete'):active_targets(enemies,{'targets':targets})

    def test_obstructed_or_unavailable_aim_probe_never_authorizes_trigger(self):
        actor={'id':1214,'name':'sol_guard'}
        live=Mock()
        for valid,clear in ((True,False),(False,True),(False,False)):
            geometry={'targets':[{'id':1214,'query_valid':valid,'unoccluded':clear}]}
            with patch('gameplay_bot.combat.admit',return_value=({},None)), \
                 patch('gameplay_bot.combat.read_actor',return_value=({'life':0},{})), \
                 patch('gameplay_bot.combat.sight',return_value=(geometry,{})):
                with self.assertRaisesRegex(BotFault,'visibility'):
                    fire(live,actor,None,['right_grip'])
            live.input.assert_not_called()

    def test_barrel_aligned_behind_the_player_view_never_authorizes_trigger(self):
        actor={'id':1214,'name':'sol_guard'}
        camera={'position':[0,0,0],'orientation':[0,0,0,1]}
        self.assertAlmostEqual(facing_error(camera,[0,0,10]),0.)
        self.assertAlmostEqual(facing_error(camera,[0,0,-10]),180.)
        state={'rendered':{'native_pose':camera}}
        geometry={'muzzle':{'position':[0,0,0],'orientation':[0,1,0,0]},
                  'targets':[{'id':1214,'query_valid':True,'unoccluded':True,'aim':[0,0,-10]}]}
        live=Mock()
        with patch('gameplay_bot.combat.admit',return_value=(state,None)), \
             patch('gameplay_bot.combat.read_actor',return_value=({'life':0},{})), \
             patch('gameplay_bot.combat.sight',return_value=(geometry,state)):
            with self.assertRaisesRegex(BotFault,'faces away'):
                fire(live,actor,None,['right_grip'])
        live.input.assert_not_called()


if __name__=='__main__':unittest.main()
