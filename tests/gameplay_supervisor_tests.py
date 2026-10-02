import json
import pathlib
import sys
import tempfile
import time
import unittest
from unittest import mock

sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from gameplay_bot.core import AttentionRequired, Behaviors, BlankCompositorFrame, BotFault, Events
from gameplay_bot.live import Live, authoritative_controls
from gameplay_bot.supervisor import REVIEW_WINDOW_SECONDS, decision_cases, execute_plan, meaningful_change, observe_decision_state, require_unambiguous_result, run_supervised, signature, validate_decision
from gameplay_bot.startup import capture_released_startup_transition, neutral_startup_transition


class Clock:
    now=0.
    def __call__(self):return self.now
    def sleep(self,dt):self.now+=dt


class Adapter:
    supervised=True
    held={}
    def __init__(self):self.captures=[]
    def observe(self):return {'scene':'menu','idroid':False,'menu':True}
    def capture(self,label):self.captures.append(label);return [label+'-left.png',label+'-right.png']


class Tests(unittest.TestCase):
    def setUp(self):
        self.state={'scene':'menu','idroid':False,'menu':True}
        self.request={'observation_id':'fresh','observed_unix_ns':time.time_ns(),
                      'state_signature':signature(self.state),'captures':[{'sha256':'image'}]}
        self.decision={'id':'one','kind':'case','observation_id':'fresh','reviewed_capture_sha256':'image',
                       'case':{'id':'ack','before':{'scene':'menu','idroid':False},
                               'steps':[{'op':'action','name':'menus.confirm'}],'after':{'scene':'gameplay'}}}
    def test_reviewed_current_case_is_accepted(self):
        self.assertEqual(validate_decision(self.decision,self.request,self.state,set()),'one')
    def test_stale_context_token_image_and_replay_are_rejected(self):
        trials=[({**self.decision,'observation_id':'old'},self.state,set()),
                ({**self.decision,'reviewed_capture_sha256':'old-image'},self.state,set()),
                (self.decision,{**self.state,'idroid':True},set()),
                (self.decision,self.state,{'one'})]
        for decision,state,done in trials:
            with self.subTest(decision=decision):
                with self.assertRaises(RuntimeError):validate_decision(decision,self.request,state,done)
    def test_idle_wait_escalates_after_two_seconds_with_intermediate_milestone(self):
        clock=Clock();adapter=Adapter()
        with tempfile.TemporaryDirectory() as folder:
            events=Events(folder,{},clock=clock)
            behavior=Behaviors(adapter,events,clock=clock,sleep=clock.sleep)
            with self.assertRaises(AttentionRequired):
                behavior.wait_for({'scene':'gameplay'},90,'idroid:exit',
                                  milestones=[{'id':'device-closed','state':{'idroid':False,'menu':True}}])
            self.assertGreaterEqual(clock.now,2.)
            self.assertLess(clock.now,2.2)
            self.assertEqual(len(adapter.captures),1)
            self.assertIn('milestone_reached',events.path.read_text())
            self.assertTrue((pathlib.Path(folder)/'attention.json').is_file())

    def test_shallow_unknown_scene_does_not_republish_same_native_gameplay(self):
        clock=Clock()
        with tempfile.TemporaryDirectory() as folder:
            class Live:
                held={};bindings={}
                def __init__(self):self.captures=[]
                def release(self):pass
                def observe(self,native=False):
                    if native:
                        return {'scene':'gameplay','menu':False,'idroid':False,'title':False,
                                'loading':False,'demo':False,'activation':3,'native':{'mission':30010}}
                    return {'scene':'unknown','menu':False,'idroid':False,'title':False,
                            'loading':False,'demo':False,'activation':3}
                def capture(self,label):
                    paths=[]
                    for eye in ('left','right'):
                        path=pathlib.Path(folder)/(label+'-'+eye+'.fixture')
                        path.write_text('fixture only');paths.append(str(path));self.captures.append(str(path))
                    return paths
            live=Live();events=Events(folder,{},clock=clock)
            behavior=Behaviors(live,events,clock=clock,sleep=clock.sleep)
            run_supervised(behavior,seconds=.5,clock=clock,sleep=clock.sleep)
            rows=[json.loads(line) for line in events.path.read_text().splitlines()]
            self.assertEqual(sum(row['event']=='attention_required' for row in rows),1)
            self.assertEqual(sum(row['event']=='milestone_reached' for row in rows),0)
            self.assertEqual(len(live.captures),2)

    def test_shallow_scene_comparison_still_refreshes_on_native_menu_transition(self):
        clock=Clock()
        with tempfile.TemporaryDirectory() as folder:
            class Live:
                held={};bindings={}
                def __init__(self):self.captures=[];self.shallow_count=0;self.menu=False
                def release(self):pass
                def observe(self,native=False):
                    if not native:
                        self.shallow_count+=1
                        if self.shallow_count>=2:self.menu=True
                    return {'scene':'menu' if native and self.menu else ('unknown' if not native and not self.menu else 'menu'),
                            'menu':self.menu,'idroid':False,'title':False,'loading':False,
                            'demo':False,'activation':3,**({'native':{'mission':30010}} if native else {})}
                def capture(self,label):
                    paths=[]
                    for eye in ('left','right'):
                        path=pathlib.Path(folder)/(label+'-'+eye+'.fixture')
                        path.write_text('fixture only');paths.append(str(path));self.captures.append(str(path))
                    return paths
            live=Live();events=Events(folder,{},clock=clock)
            behavior=Behaviors(live,events,clock=clock,sleep=clock.sleep)
            run_supervised(behavior,seconds=.5,clock=clock,sleep=clock.sleep)
            rows=[json.loads(line) for line in events.path.read_text().splitlines()]
            self.assertEqual(sum(row['event']=='attention_required' for row in rows),2)
            self.assertEqual(sum(row['event']=='milestone_reached' for row in rows),1)
            self.assertEqual(len(live.captures),4)

    def test_unknown_scene_is_ignored_without_masking_other_state_changes(self):
        baseline={'scene':'gameplay','menu':False,'idroid':False,'title':False,
                  'loading':False,'demo':False,'activation':3}
        self.assertFalse(meaningful_change(signature(baseline),{**baseline,'scene':'unknown'}))
        self.assertTrue(meaningful_change(signature(baseline),{**baseline,'scene':'unknown','menu':True}))

    def test_old_image_with_unchanged_native_scene_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError,'older than 180'):
            validate_decision(self.decision,self.request,self.state,set(),
                              now_ns=self.request['observed_unix_ns']+181_000_000_000)

    def test_review_can_finish_after_one_minute_through_exact_three_minute_boundary(self):
        self.assertEqual(REVIEW_WINDOW_SECONDS,180.)
        for age_ns in (0,61_000_000_000,179_999_999_999,180_000_000_000):
            with self.subTest(age_ns=age_ns):
                self.assertEqual(validate_decision(self.decision,self.request,self.state,set(),
                    now_ns=self.request['observed_unix_ns']+age_ns),'one')
        for age_ns in (-1,180_000_000_001):
            with self.subTest(age_ns=age_ns):
                with self.assertRaisesRegex(RuntimeError,'older than 180'):
                    validate_decision(self.decision,self.request,self.state,set(),
                        now_ns=self.request['observed_unix_ns']+age_ns)

    def test_longer_review_still_rejects_changed_native_owner_signature_nonce_hash_and_replay(self):
        state={**self.state,'native':{'mission':40010,'sequence':'Seq_Game_MainGame','popup':False}}
        request={**self.request,'state_signature':signature(state)}
        case={**self.decision['case'],'before':{'scene':'menu','idroid':False,
            'native.mission':40010,'native.sequence':'Seq_Game_MainGame','native.popup':False}}
        decision={**self.decision,'case':case}
        now_ns=request['observed_unix_ns']+45_000_000_000
        self.assertEqual(validate_decision(decision,request,state,set(),now_ns=now_ns),'one')
        trials=[({**decision,'observation_id':'old'},state,set()),
                ({**decision,'reviewed_capture_sha256':'old-image'},state,set()),
                (decision,{**state,'idroid':True},set()),
                (decision,{**state,'native':{**state['native'],'popup':True}},set()),
                (decision,{**state,'native':{**state['native'],'mission':10020}},set()),
                (decision,state,{'one'})]
        for candidate,current,completed in trials:
            with self.subTest(candidate=candidate,current=current,completed=completed):
                with self.assertRaises(RuntimeError):
                    validate_decision(candidate,request,current,completed,now_ns=now_ns)

    def test_review_window_does_not_extend_native_control_freshness(self):
        validate_decision(self.decision,self.request,self.state,set(),
                          now_ns=self.request['observed_unix_ns']+121_000_000_000)
        current={'now_ms':1000,'controls':{'context':'menus','sample_ms':750,'age_ms':250}}
        self.assertEqual(authoritative_controls(current)['age_ms'],250)
        with self.assertRaisesRegex(RuntimeError,'unavailable or stale'):
            authoritative_controls({'now_ms':1000,'controls':{
                'context':'menus','sample_ms':749,'age_ms':251}})

    def test_held_input_still_cannot_produce_a_review_capture(self):
        live=Live.__new__(Live);live.held={'right_trigger':1};live.observe=mock.Mock();live.call=mock.Mock()
        with self.assertRaisesRegex(RuntimeError,'Release input'):
            live.capture('supervisor-held-input')
        live.observe.assert_not_called();live.call.assert_not_called()

    def test_transient_missing_control_publication_reacquired_without_consuming_correct_decision(self):
        clock=Clock(); state={**self.state, "now_ms":1000,
            "native":{"mission":1,"sequence":"Seq_Demo_ConfirmAutoSave","popup":True},
            "controls":{"context":"menus","age_ms":100,"sample_ms":900,"native_buttons":0}}
        request={**self.request,"state_signature":signature(state)}
        case={**self.decision["case"],"before":{"scene":"menu","native.mission":1,
            "native.sequence":"Seq_Demo_ConfirmAutoSave","native.popup":True,"controls.native_buttons":0}}
        decision={**self.decision,"case":case};live=mock.Mock()
        live.observe.side_effect=[{**state,"controls":None},state]
        recovered=observe_decision_state(live,decision,request,clock=clock,sleep=clock.sleep)
        self.assertEqual(validate_decision(decision,request,recovered,set()),"one")
        self.assertEqual(live.observe.call_count,2);self.assertEqual(clock.now,.025)
        live.execute.assert_not_called()

    def test_admission_wait_stops_immediately_on_changed_owner_even_with_missing_controls(self):
        clock=Clock();state={**self.state,"native":{"mission":1,"sequence":"Seq_Demo_ConfirmAutoSave"},"controls":None}
        request={**self.request,"state_signature":signature(state)}
        decision={**self.decision,"case":{**self.decision["case"],
            "before":{"scene":"menu","native.mission":1,"native.sequence":"Seq_Demo_ConfirmAutoSave"}}}
        for changed in ({**state,"idroid":True},
                        {**state,"native":{"mission":40010,"sequence":"Seq_Demo_ConfirmAutoSave"}}):
            with self.subTest(changed=changed):
                live=mock.Mock();live.observe.return_value=changed
                with self.assertRaisesRegex(BotFault,"scene changed"):
                    observe_decision_state(live,decision,request,clock=clock,sleep=clock.sleep)
                self.assertEqual(live.observe.call_count,1);live.execute.assert_not_called()

    def test_admission_missing_controls_timeout_has_control_error_and_dispatches_nothing(self):
        live=mock.Mock();clock=Clock();live.observe.return_value={**self.state,"controls":None}
        with self.assertRaisesRegex(BotFault,"control context is unavailable or stale"):
            observe_decision_state(live,self.decision,self.request,clock=clock,sleep=clock.sleep)
        self.assertGreaterEqual(clock.now,2.);self.assertLess(clock.now,2.001)
        live.execute.assert_not_called()

    def test_admission_reacquires_251ms_sample_without_relaxing250ms_rule(self):
        clock=Clock();live=mock.Mock();state={**self.state,"now_ms":1000,
            "controls":{"context":"menus","age_ms":250,"sample_ms":750}}
        live.observe.side_effect=[{**state,"controls":{**state["controls"],"age_ms":251,"sample_ms":749}},state]
        recovered=observe_decision_state(live,self.decision,self.request,clock=clock,sleep=clock.sleep)
        self.assertEqual(authoritative_controls(recovered)["age_ms"],250)
        live.execute.assert_not_called()

    def test_plan_stops_at_first_unexpected_outcome_and_preserves_completed_cases(self):
        clock=Clock()
        with tempfile.TemporaryDirectory() as folder:
            class Behavior:
                events=Events(folder,{},clock=clock)
                def __init__(self):self.executed=[]
                def case(self,case):
                    self.executed.append(case['id'])
                    clock.sleep(.1)
                    return {'id':case['id'], 'status':'failed' if case['id']=='unexpected' else 'observed_pass',
                            'failure_phase':'outcome'}
            behavior=Behavior();records=[];checkpoints=[]
            cases=[{**self.decision['case'],'id':name} for name in ('first','unexpected','never-dispatched')]
            result=execute_plan(behavior,{'id':'plan','kind':'plan','cases':cases},records,
                                checkpoint=lambda rows:checkpoints.append(len(rows)),clock=clock)
            self.assertEqual(behavior.executed,['first','unexpected'])
            self.assertEqual(checkpoints,[1,2])
            self.assertIn('dependent cases stopped',result)
            self.assertIn('never-dispatched',behavior.events.path.read_text())

    def test_successful_known_plan_needs_no_intermediate_model_decision(self):
        with tempfile.TemporaryDirectory() as folder:
            class Behavior:
                events=Events(folder,{})
                def case(self,case):return {'id':case['id'],'status':'observed_pass'}
            cases=[{**self.decision['case'],'id':str(index)} for index in range(3)]
            records=[]
            execute_plan(Behavior(),{'id':'plan','kind':'plan','cases':cases},records,checkpoint=lambda _:None)
            self.assertEqual([row['id'] for row in records],['0','1','2'])

    def test_duplicate_cases_cannot_form_an_implicit_retry_loop(self):
        with self.assertRaisesRegex(RuntimeError,'Repeated case'):
            decision_cases({'kind':'plan','cases':[self.decision['case']]*2})

    def test_preselected_suite_runs_without_waiting_for_a_model_decision(self):
        clock=Clock()
        with tempfile.TemporaryDirectory() as folder:
            class Live:
                held={};bindings={}
                def __init__(self):self.menu=False;self.actions=[]
                def release(self):pass
                def observe(self,native=False):return {'menu':self.menu,'scene':'menu' if self.menu else 'gameplay'}
                def execute(self,step):self.menu=step['name']=='system.pause';self.actions.append(step['name'])
                def capture(self,label):
                    paths=[]
                    for eye in ('left','right'):
                        path=pathlib.Path(folder)/(label+'-'+eye+'.fixture')
                        path.write_text('fixture only');paths.append(str(path))
                    return paths
            live=Live();events=Events(folder,{},clock=clock)
            suite={'cases':[
                {'id':'open','before':{'menu':False},'steps':[{'op':'action','name':'system.pause'}],'after':{'menu':True}},
                {'id':'close','before':{'menu':True},'steps':[{'op':'action','name':'menus.back'}],'after':{'menu':False}}]}
            result=run_supervised(Behaviors(live,events,clock=clock,sleep=clock.sleep),
                                  seconds=.3,clock=clock,sleep=clock.sleep,initial_suite=suite)
            self.assertEqual(live.actions,['system.pause','menus.back'])
            self.assertEqual([c['status'] for c in result['cases']],['observed_pass']*2)
            self.assertFalse((pathlib.Path(folder)/'decision.json').exists())
            self.assertFalse(live.menu)


class StartupTransitionTests(unittest.TestCase):
    @staticmethod
    def state(sequence="Seq_Demo_CreateOrLoadSaveData"):
        return {"scene":"cinematic", "title":False, "menu":False, "idroid":False, "pause":False,
                "camera_active":False, "native":{"mission":1, "sequence":sequence, "title":False,
                    "status_NORMAL_ACTION":False, "player_life":0},
                "controls":{"native_buttons":0, "age_ms":100, "physical":[0]*11,
                    "sticks":[0]*4, "native_triggers":[0]*2}}

    def test_retained_startup_blank_retries_only_released_capture_not_input(self):
        live=mock.Mock(held={}); clock=Clock(); state=self.state()
        live.capture.side_effect=[BlankCompositorFrame("retained blank",state), ["left","right"]]
        self.assertEqual(capture_released_startup_transition(live,"transition",clock=clock,sleep=clock.sleep),
                         ["left","right"])
        self.assertEqual(live.capture.call_count,2)
        live.execute.assert_not_called()
        self.assertEqual(clock.now,.25)

    def test_native_owner_controls_and_gameplay_cannot_admit_blank_retry(self):
        variants=[{"native":{**self.state()["native"],"mission":10020}},
                  {"native":{**self.state()["native"],"sequence":"Seq_Game_MainGame"}},
                  {"native":{**self.state()["native"],"status_NORMAL_ACTION":True}},
                  {"native":{**self.state()["native"],"mission":True}},
                  {"camera_active":True}, {"menu":True}, {"idroid":True},
                  {"controls":{**self.state()["controls"],"age_ms":251}},
                  {"controls":{**self.state()["controls"],"native_buttons":4096}},
                  {"controls":{**self.state()["controls"],"sticks":[0,1,0,0]}},
                  {"controls":{**self.state()["controls"],"physical":[True]+[0]*10}},
                  {"controls":{**self.state()["controls"],"native_triggers":[float("nan"),0]}}]
        for change in variants:
            with self.subTest(change=change):
                live=mock.Mock(held={}); clock=Clock(); state={**self.state(),**change}
                self.assertFalse(neutral_startup_transition(state))
                live.capture.side_effect=BlankCompositorFrame("retained inadmissible blank",state)
                with self.assertRaises(BlankCompositorFrame):
                    capture_released_startup_transition(live,"transition",clock=clock,sleep=clock.sleep)
                self.assertEqual(live.capture.call_count,1);live.execute.assert_not_called()

    def test_blank_transition_deadline_cannot_count_as_ready_or_retry_dispatch(self):
        live=mock.Mock(held={});clock=Clock()
        live.capture.side_effect=BlankCompositorFrame("retained blank",self.state())
        with self.assertRaisesRegex(BotFault,"remained blank"):
            capture_released_startup_transition(live,"transition",timeout=.5,clock=clock,sleep=clock.sleep)
        self.assertEqual(clock.now,.5);self.assertEqual(live.capture.call_count,3)
        live.execute.assert_not_called()

    def test_transport_and_stale_capture_failure_never_retry(self):
        for message in ("operator completion unknown", "Stale compositor view"):
            with self.subTest(message=message):
                live=mock.Mock(held={}); clock=Clock(); live.capture.side_effect=BotFault(message)
                with self.assertRaisesRegex(BotFault,message):
                    capture_released_startup_transition(live,"transition",clock=clock,sleep=clock.sleep)
                self.assertEqual(live.capture.call_count,1);live.execute.assert_not_called()

    def test_known_boot_blank_does_not_terminate_completed_action_as_ambiguous(self):
        result={"id":"autosave", "status":"failed", "failure_phase":"outcome", "dispatch_completed":True,
                "capture_error":"retained blank", "capture_failure_kind":"blank_compositor",
                "capture_failure_state":self.state()}
        require_unambiguous_result(result)
        self.assertEqual(result["status"],"failed")
        for change in ({"release_error":"unknown"}, {"failure_phase":"dispatch"},
                       {"capture_failure_kind":"capture_or_transport"},
                       {"capture_failure_state":{**self.state(),"camera_active":True}}):
            with self.subTest(change=change):
                with self.assertRaises(BotFault):require_unambiguous_result({**result,**change})

    def test_outcome_observation_survives_native_save_fade_then_proves_new_sequence(self):
        clock=Clock();before=self.state("Seq_Demo_ConfirmAutoSave");fade=self.state();after=self.state("Seq_Demo_LogInKonamiServer")
        with tempfile.TemporaryDirectory() as folder:
            class Live:
                held={};supervised=True
                def __init__(self):self.actions=[];self.current=before;self.blanks=0
                def release(self):pass
                def execute(self,step):self.actions.append(step);self.current=fade
                def observe(self,native=False):
                    if self.current is fade and clock.now>2.3:self.current=after
                    return self.current
                def capture(self,label):
                    if self.current is fade:
                        self.blanks+=1;raise BlankCompositorFrame("retained save fade",fade)
                    return [label+"-left",label+"-right"]
            live=Live();events=Events(folder,{},clock=clock)
            case={"id":"autosave", "before":{"native.sequence":"Seq_Demo_ConfirmAutoSave"},
                  "steps":[{"op":"action","name":"menus.confirm"}],
                  "after":{"native.sequence":"Seq_Demo_LogInKonamiServer"},"timeout":8}
            result=Behaviors(live,events,clock=clock,sleep=clock.sleep).case(case)
            self.assertEqual(result["status"],"observed_pass")
            self.assertTrue(result["dispatch_completed"]);self.assertEqual(result["dispatched_steps_completed"],1)
            self.assertEqual(len(live.actions),1);self.assertGreater(live.blanks,0)
            self.assertNotIn("capture_error",result)

    def test_core_retains_typed_blank_failure_and_sampled_dispatch_on_timeout(self):
        clock=Clock();before=self.state("Seq_Demo_ConfirmAutoSave");fade=self.state()
        with tempfile.TemporaryDirectory() as folder:
            class Live:
                held={};supervised=True
                def __init__(self):self.current=before;self.actions=[]
                def release(self):pass
                def execute(self,step):self.actions.append(step);self.current=fade
                def observe(self,native=False):return self.current
                def capture(self,label):
                    if self.current is fade:raise BlankCompositorFrame("retained fade",fade)
                    return [label]
            live=Live();events=Events(folder,{},clock=clock)
            result=Behaviors(live,events,clock=clock,sleep=clock.sleep).case({"id":"autosave",
                "before":{"native.sequence":"Seq_Demo_ConfirmAutoSave"},"steps":[{"op":"action","name":"menus.confirm"}],
                "after":{"native.sequence":"Seq_Demo_LogInKonamiServer"},"timeout":2.5})
            self.assertEqual(result["status"],"failed");self.assertEqual(result["failure_phase"],"outcome")
            self.assertTrue(result["dispatch_completed"]);self.assertEqual(len(live.actions),1)
            self.assertEqual(result["capture_failure_kind"],"blank_compositor")
            self.assertEqual(result["capture_failure_state"]["native"]["sequence"],"Seq_Demo_CreateOrLoadSaveData")
            require_unambiguous_result(result)

    def test_completed_startup_action_waits_for_pixels_without_replaying_input(self):
        clock=Clock();before=self.state("Seq_Demo_ConfirmAutoSave");after=self.state("Seq_Demo_LogInKonamiServer")
        with tempfile.TemporaryDirectory() as folder:
            class Live:
                held={}
                def __init__(self):self.current=before;self.actions=[];self.blanks=0
                def release(self):pass
                def execute(self,step):self.actions.append(step);self.current=after
                def observe(self,native=False):return self.current
                def capture(self,label):
                    if label.endswith("-after") and self.blanks==0:
                        self.blanks+=1;raise BlankCompositorFrame("retained login fade",after)
                    return [label+"-left",label+"-right"]
            live=Live();events=Events(folder,{},clock=clock)
            result=Behaviors(live,events,clock=clock,sleep=clock.sleep).case({"id":"autosave",
                "before":{"native.sequence":"Seq_Demo_ConfirmAutoSave"},"steps":[{"op":"action","name":"menus.confirm"}],
                "after":{"native.sequence":"Seq_Demo_LogInKonamiServer"},"timeout":2.5})
            self.assertEqual(result["status"],"observed_pass");self.assertEqual(len(live.actions),1)
            self.assertEqual(live.blanks,1);self.assertEqual(len(result["captures"]),2)


if __name__=='__main__':unittest.main()
