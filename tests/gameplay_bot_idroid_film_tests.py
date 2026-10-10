"""Offline iDroid owner, continuous release, mapping and honest film outcomes."""
import copy
import importlib.util
import json
import pathlib
import sys
import tempfile
import unittest
from contextlib import nullcontext
from types import SimpleNamespace
from unittest.mock import patch

sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from gameplay_bot.core import BotFault
from gameplay_bot.idroid_film import bindings_contract, film_idroid, nearby_guard_gate
from gameplay_bot.navigation import NativeMap


WORLD=(30010,10,'Seq_Game_FreePlay')
BINDINGS={'actions':[
    {'name':n,'contexts':['menus'] if n.startswith('menus.') else ['gameplay','menus'],
     'bindings':[{'gesture':g,'inputs':i,'milliseconds':t}]} for n,g,i,t in [
        ('system.idroid','tap',['menu'],550),('menus.previous_tab','level',['left_grip'],300),
        ('menus.next_tab','level',['right_grip'],300),('menus.back','level',['b'],300)]],
    'axes':[{'name':n,'source':s} for n,s in [('axes.move','left_stick'),('axes.menu','left_stick'),('axes.map','right_stick')]],
    'settings':[{'name':'settings.handheld_menus','value':1}]}
ENEMIES={'mission':30010,'location':10,'normal_life':'0','truncated':False,'unavailable_loaded':0,'soldiers':[]}


class Clock:
    def __init__(self):self.now=0.
    def __call__(self):return self.now
    def sleep(self,seconds):self.now+=seconds


class OfflineLive:
    def __init__(self,output,clock,*,walking=False,bad_packet=False,changed_owner=False,unknown_open=False):
        self.clock=clock;self.walking=walking;self.bad_packet=bad_packet;self.changed_owner=changed_owner
        self.bindings=copy.deepcopy(BINDINGS);self.held={};self.idroid=False;self.opened_menu=None
        self.unknown_open=unknown_open
        self.held_action_recording=object();self.player=[0.,1.,0.];self.inputs=[];self.calls=[];self.keep_records=[]
        self.events=self;self.output=output;self.native=SimpleNamespace(read=lambda *args:None);self.log=[];self.cleanup_count=0
    def emit(self,event,**fields):self.log.append((event,copy.deepcopy(fields)))
    def input(self,values,seconds,*,lease_seconds):
        self.inputs.append(copy.deepcopy(values))
        for v in values:self.held[(v['hand'],v['component'],v.get('sub_component'))]=(v,self.clock()+lease_seconds)
    def release(self,*,keep=()):
        if keep:self.keep_records.append((set(keep),copy.deepcopy(self.held)))
        self.held={k:v for k,v in self.held.items() if k in keep}
    def observe(self,**unused):
        self.clock.sleep(.012)
        self.held={k:v for k,v in self.held.items() if v[1]>self.clock()}
        p=[0]*11;sticks=[0]*4
        for v,_ in self.held.values():
            if v['component']=='Grip':p[7 if v['hand']=='left' else 8]=v['value']
            elif v['component']=='Thumbstick':sticks[1]=v['value']
        if self.walking and not p[7]:self.player[2]-=sticks[1]*.045
        stamp=100000+round(self.clock()*1000)
        native={'mission':30010,'location':10,'sequence':'Seq_Game_FreePlay','helicopter_space':False,
                'player_vehicle_id':65535,'popup':False,'saving':False,'demo':False,'demo_nonplayable':False,
                'title':False,'tutorial_pause':False,'game_over':0,'not_alert':True,'player_life':7801,
                'status_CARRY':False,'status_BINOCLE':False,'status_NORMAL_ACTION':True,
                'status_INJURY_LOWER':False,'status_INJURY_UPPER':False,
                'vr_idroid_player_pad_block':self.idroid,
                **{'player_'+a:v for a,v in zip('xyz',self.player)}}
        owner=2 if self.changed_owner and self.idroid else 1
        frame={'sample_ms':stamp,'player_owner':owner,'presentation_epoch':1,'presentation_focused':True,
               'idroid_device_tracked':self.idroid,'native_pose':{'position':[0,2,0],'orientation':[0,0,0,1]}}
        return {'now_ms':stamp,'native':native,'activation':1,'rendered':frame,'scene':'menu' if self.idroid else 'gameplay',
                'idroid':self.idroid,'menu':self.idroid,'idroid_menu_input_ready':self.idroid,'idroid_handheld':True,
                'idroid_closing':False,'idroid_tutorial_mode':0,'gamepad':False,'camera_active':True,
                'camera_available':True,'loading':False,'title':False,'demo':False,'pause':False,
                'head_tracking_fault_mask':0,'tracking_fault_mask':0,
                'controls':{'sample_ms':stamp,'age_ms':0,'context':'menus' if self.idroid else 'gameplay',
                            'physical':p,'sticks':sticks,'native_axes':[0,round(sticks[1]*32767),0,0],
                            'native_triggers':[0,0],'native_buttons':0x2000 if self.bad_packet and self.held else (0x100 if p[7] else 0)}}
    def call(self,name,args):
        self.calls.append((name,copy.deepcopy(args)))
        return {'pose':{'position':[0,0,0],'orientation':[0,0,0,1]}}
    def text_result(self,value):return value
    def execute(self,step):
        self.calls.append(('semantic',copy.deepcopy(step)))
        if step['name']=='system.idroid':
            self.idroid=True;self.opened_menu='idroid'
            if self.unknown_open:raise BotFault('Opening completion unknown')
        elif step['name']=='menus.back':self.idroid=False;self.opened_menu=None
        else:raise AssertionError('Unexpected action')
    def capture(self,label):
        if self.held:raise AssertionError('Held capture')
        return [str(self.output/(label+'-'+e+'.png')) for e in ['left','right']]
    def cleanup_menus(self):self.cleanup_count+=1;self.idroid=False;self.opened_menu=None


class IdroidFilmTests(unittest.TestCase):
    def test_exact_effective_bindings_are_required_before_input(self):
        self.assertIn('menus.previous_tab',bindings_contract(BINDINGS))
        for key,value in [('gesture','hold'),('inputs',['left_grip','menu']),('milliseconds',550)]:
            b=copy.deepcopy(BINDINGS);b['actions'][1]['bindings'][0][key]=value
            with self.subTest(key=key),self.assertRaises(BotFault):bindings_contract(b)
        b=copy.deepcopy(BINDINGS);b['settings'][0]['value']=0
        with self.assertRaises(BotFault):bindings_contract(b)
        b=copy.deepcopy(BINDINGS);b['axes'][0]['source']='right_stick'
        with self.assertRaises(BotFault):bindings_contract(b)

    def test_incomplete_unknown_world_or_nearby_active_guard_is_not_a_film_stage(self):
        nearby_guard_gate(ENEMIES,WORLD,[0,1,0])
        for mutation in [{'mission':1},{'unavailable_loaded':1},{'truncated':True},
                         {'soldiers':[{'life':'0','position':[45,1,0]}]},
                         {'soldiers':[{'life':None,'position':[100,1,0]}]}]:
            with self.subTest(mutation=mutation),self.assertRaises(BotFault):
                nearby_guard_gate({**ENEMIES,**mutation},WORLD,[0,1,0])

    def run_case(self,**options):
        goal=options.pop('goal',1)
        with tempfile.TemporaryDirectory() as directory:
            output=pathlib.Path(directory);clock=Clock();live=OfflineLive(output,clock,**options)
            nav=NativeMap([{'source':'owned-synthetic','sha256':'a'*64,'groups':[{'id':0,'nodes':[[0,0,0],[0,0,-3]],
                         'edges':[{'from':0,'to':1,'flags':0}]}]}])
            def read(adapter,*unused):
                state=adapter.observe(native=True)
                return state,(30010,10,1,state['rendered']['player_owner'],1)
            with patch('gameplay_bot.idroid_film.fresh_rig',side_effect=read), \
                 patch('gameplay_bot.idroid_film.read_enemy_snapshot',return_value=copy.deepcopy(ENEMIES)), \
                 patch('gameplay_bot.idroid_film.preserved_tracking_poses',return_value=nullcontext()):
                result=film_idroid(live,nav,enemy_script='owned-readonly',world=WORLD,goal_node=goal,output=output,
                                  clock=clock,sleep=clock.sleep)
            saved=json.loads((output/'result.json').read_text());self.assertEqual(saved['status'],result['status'])
            return result,live

    def test_current_stationary_outcome_is_not_promoted_to_requested_walking_pass(self):
        result,live=self.run_case()
        self.assertEqual(result['status'],'observed_current_control_probe')
        self.assertFalse(result['requested_walk_while_open_native_forward_displacement_observed'])
        self.assertFalse(result['requested_release_to_walk_native_forward_displacement_observed'])
        self.assertFalse(result['requested_behavior_acceptance']);self.assertFalse(result['physical_headset_acceptance'])
        self.assertIn('pending',result['visual_acceptance']);self.assertFalse(live.idroid)
        self.assertEqual(len(result['phases']),3)
        self.assertTrue(all(p['page_and_cursor_change']=='pending_pixel_review' for p in result['phases']))

    def test_same_stick_lease_survives_explicit_grip_release_without_reapply(self):
        result,live=self.run_case(walking=True)
        self.assertEqual(result['status'],'observed_current_control_probe')
        self.assertTrue(result['requested_release_to_walk_native_forward_displacement_observed'])
        self.assertFalse(result['requested_behavior_acceptance'])
        self.assertEqual(len(live.inputs),3);self.assertEqual(len(live.keep_records),1)
        held=result['phases'][-1]
        self.assertFalse(held['axis_stopped_and_reapplied'])
        self.assertTrue(all(x['state']['controls']['physical'][7]==0 for x in held['grip_released_same_axis_samples']))
        self.assertTrue(all(x['state']['controls']['sticks'][1]==.55 for x in held['grip_released_same_axis_samples']))

    def test_changed_owner_or_unexpected_native_packet_stops_and_closes_test_menu(self):
        for options in [{'changed_owner':True},{'bad_packet':True}]:
            with self.subTest(options=options):
                result,live=self.run_case(**options)
                self.assertEqual(result['status'],'failed');self.assertFalse(live.idroid)
                self.assertEqual(live.cleanup_count,1);self.assertFalse(live.held)
                self.assertLessEqual(len(live.inputs),1)

    def test_invalid_floor_goal_prevents_menu_open_or_motion(self):
        result,live=self.run_case(goal=2)
        self.assertEqual(result['status'],'failed');self.assertEqual(live.inputs,[])
        self.assertFalse(any(name=='semantic' for name,_ in live.calls))

    def test_unknown_open_completion_is_not_replayed_and_still_owns_observed_cleanup(self):
        result,live=self.run_case(unknown_open=True)
        self.assertEqual(result['status'],'failed');self.assertIn('unknown',result['error'])
        self.assertFalse(live.idroid);self.assertEqual(live.cleanup_count,1)
        self.assertEqual(len([step for name,step in live.calls if name=='semantic']),1)
        self.assertEqual(live.inputs,[])


def load_tool(name,relative):
    spec=importlib.util.spec_from_file_location(name,pathlib.Path(__file__).resolve().parents[1]/relative)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module


class ManagedFilmTests(unittest.TestCase):
    def test_only_fresh_or_unopened_managed_output_is_admitted(self):
        tool=load_tool('idroid_film_cli','tools/gameplay-idroid-film.py')
        with tempfile.TemporaryDirectory() as directory:
            output=pathlib.Path(directory)/'fresh'
            tool.validate_output(output);self.assertFalse(output.exists())
            output.mkdir()
            with self.assertRaises(ValueError):tool.validate_output(output)
            managed={'schema':1,'source':{'commit':'test'},'started_utc':'2026-10-04T00:00:00Z'}
            (output/'managed-run.json').write_text(json.dumps(managed))
            (output/'runner.log').write_text('')
            tool.validate_output(output)
            for name in ['events.jsonl','identity.json','result.json','finished.json','STOP','recording','extra.txt']:
                with self.subTest(name=name):
                    candidate=output/name
                    if name=='recording':candidate.mkdir()
                    else:candidate.write_text('retained')
                    with self.assertRaises(ValueError):tool.validate_output(output)
                    if candidate.is_dir():candidate.rmdir()
                    else:candidate.unlink()
            (output/'managed-run.json').write_text('[]')
            with self.assertRaises(ValueError):tool.validate_output(output)

    def workspace_case(self,operation,*,code=0,diagnostic=False):
        workspace=load_tool('workspace_film_tests','tools/workspace.py')
        with tempfile.TemporaryDirectory() as directory:
            root=pathlib.Path(directory);play=root/'play';play.mkdir()
            (play/'dinput8.dll').write_bytes(b'test')
            (play/'mgs5vr_controls.exe').write_bytes(b'test')
            (play/'BUILD.json').write_text(json.dumps({'schema':1,'source':{'commit':'test'},
                'validation':{'automated':'passed'},'files':{'dinput8.dll':'test','mgs5vr_controls.exe':'test'}}))
            policy=root/'reviewed-mortar.json';policy.write_text('{}')
            args=SimpleNamespace(operation=operation,record=True,seconds=90. if operation=='idroid-film' else 120.,
                                 goal_node=7,policy=policy,diagnostic_active_camp=diagnostic)
            with patch.object(workspace,'ROOT',root),patch.object(workspace,'DEV',play), \
                 patch.object(workspace,'SCRATCH',root/'artifacts/bot'), \
                 patch.object(workspace,'settings',return_value={'game_dir':str(root/'game'),'operator_proxy':str(root/'proxy.exe'),
                                                              'navigation':str(root/'navigation.json')}), \
                 patch.object(workspace,'digest',return_value='test'),patch.object(workspace,'prune_runs') as prune, \
                 patch.object(workspace.subprocess,'run',return_value=SimpleNamespace(returncode=code)) as dispatch:
                if code:
                    with self.assertRaisesRegex(ValueError,'Case needs correction; live session kept for recovery'):workspace.bot(args)
                else:workspace.bot(args)
                command=dispatch.call_args.args[0]
                latest=workspace.read_json(workspace.SCRATCH/'latest.json')
                output=pathlib.Path(latest['path'])
                self.assertEqual(workspace.read_json(output/'finished.json')['exit_code'],code)
                self.assertTrue((output/'managed-run.json').is_file());self.assertTrue((output/'runner.log').is_file())
                self.assertEqual(prune.call_count,2)
                workspace.pin_latest(None);self.assertTrue((output/'KEEP').is_file())
                return command,dispatch.call_args.kwargs

    def test_workspace_film_exact_world_record_and_navigation_dispatch_and_retention(self):
        command,kwargs=self.workspace_case('idroid-film')
        for key,value in [('--goal-node','7'),('--seconds','90.0'),('--expected-mission','30010'),
                          ('--expected-location','10'),('--expected-sequence','Seq_Game_FreePlay')]:
            self.assertEqual(command[command.index(key)+1],value)
        self.assertTrue(command[1].endswith('gameplay-idroid-film.py'))
        self.assertIn('--record',command);self.assertIn('--navigation-manifest',command)
        self.assertIn('MGS5VR_OPERATOR_URL',kwargs['env'])

    def test_workspace_mortar_flag_is_explicit_and_failed_run_remains_retained(self):
        for diagnostic in [False,True]:
            command,_=self.workspace_case('emplacement',code=1 if diagnostic else 0,diagnostic=diagnostic)
            self.assertTrue(command[1].endswith('gameplay-emplacement.py'))
            self.assertEqual('--diagnostic-active-camp' in command,diagnostic)
            self.assertIn('--policy',command);self.assertIn('--manifest',command);self.assertIn('--record',command)
            self.assertEqual(command[command.index('--seconds')+1],'120.0')

    def test_workspace_bad_bounds_do_not_create_a_run_or_prune_evidence(self):
        workspace=load_tool('workspace_film_preflight_tests','tools/workspace.py')
        for operation,values in [('idroid-film',{'seconds':91.}),('idroid-film',{'seconds':float('nan')}),
                                 ('idroid-film',{'goal_node':-1}),('idroid-film',{'record':False}),
                                 ('emplacement',{'seconds':121.}),('emplacement',{'policy':pathlib.Path('missing-policy.json')})]:
            args=SimpleNamespace(operation=operation,record=True,seconds=90.,goal_node=7,policy=pathlib.Path('missing-policy.json'))
            for key,value in values.items():setattr(args,key,value)
            with self.subTest(operation=operation,values=values), \
                 patch.object(workspace,'settings',side_effect=AssertionError('preflight must run before workspace operations')):
                with self.assertRaises(ValueError):workspace.bot(args)


if __name__=='__main__':unittest.main()
