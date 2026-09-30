import json
import pathlib
import sys
import tempfile
import time
import unittest

sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from gameplay_bot.core import AttentionRequired, Behaviors, Events
from gameplay_bot.supervisor import decision_cases, execute_plan, meaningful_change, run_supervised, signature, validate_decision


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
        with self.assertRaisesRegex(RuntimeError,'older than 30'):
            validate_decision(self.decision,self.request,self.state,set(),
                              now_ns=self.request['observed_unix_ns']+31_000_000_000)

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


if __name__=='__main__':unittest.main()
