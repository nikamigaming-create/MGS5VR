"""Recon keeps native acquisition/outcomes while using the supported pose path."""
import copy
from contextlib import contextmanager
import pathlib
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from gameplay_bot.core import BotFault
from gameplay_bot.recon import inspect, observed


class Clock:
    def __init__(self): self.now=0.
    def __call__(self): self.now+=.01; return self.now
    def sleep(self, seconds): self.now+=seconds


class Live:
    def __init__(self, game):
        self.game=game; self.trace=[]; self.events=self; self.support=False
        self.bindings={'settings':[{'name':'settings.binocular_auto_mark','value':1}]}
        self.state={'controls':{'context':'gameplay'},
                    'rendered':{'binocular_held':False,'binocular_magnification':2}}
        self.fail_capture=False; self.zoom_changes=True
    def emit(self, event, **data): self.trace.append(('event',event,data))
    def release(self): self.trace.append(('release',)); self.support=False
    def execute(self, step):
        self.trace.append(('action',step['name']))
        if step['name']=='gameplay.equip_binoculars':
            self.state['controls']['context']='binoculars'; self.state['rendered']['binocular_held']=True
        elif step['name']=='binoculars.stow':
            self.state['controls']['context']='gameplay'; self.state['rendered']['binocular_held']=False
        else: raise AssertionError('Only existing equip/stow belongs in execute: '+step['name'])
    def capture(self, label, *, static_grips=False):
        self.trace.append(('capture',label,static_grips,self.support))
        assert static_grips and self.support, 'capture must retain actual side-cup support'
        if self.fail_capture: raise BotFault('retained compositor capture failure')
        return [label+'-left.png',label+'-right.png']


class Presentation:
    def __init__(self, live, observe): self.live=live; self.observe=observe; live.trace.append(('presentation',))
    def raise_to_eye(self): self.live.trace.append(('raise',))
    def align_to_ocular(self): self.live.trace.append(('align',))
    def hold_support(self): self.live.support=True; self.live.trace.append(('support',))
    def look_at(self, target): self.live.trace.append(('look',target))
    def support_observation(self):
        self.hold_support(); return {'sampled_left_grip':True,'actual_contact_flag_published':False}
    def press(self, name, **options):
        assert self.live.support
        if name=='binoculars.mark': assert options.get('acquired') is True
        self.live.trace.append(('press',name,options))
        if name=='binoculars.zoom' and self.live.zoom_changes:
            self.live.state['rendered']['binocular_magnification']=4
    def restore(self):
        assert not self.live.state['rendered']['binocular_held']
        self.observe(); self.live.trace.append(('smooth_restore',))


class ReconTests(unittest.TestCase):
    def run_recon(self, *, acquisition=True, explicit=True, capture_failure=False, zoom_changes=True):
        temporary=tempfile.TemporaryDirectory(); self.addCleanup(temporary.cleanup)
        root=pathlib.Path(temporary.name); live=Live(root); live.fail_capture=capture_failure; live.zoom_changes=zoom_changes
        marker='Physical optic native person mark id=1166 result=1 acquired=true'
        missing='Physical optic native person mark id=1214 result=1 acquired=true'
        texts=[marker if acquisition else missing,marker if explicit else 'Physical optic native person mark id=1166 result=1 acquired=false']
        class Cursor:
            def __init__(self,path):
                self.text=texts.pop(0) if texts else missing
            def read(self): return self.text
        @contextmanager
        def preserve(adapter):
            adapter.trace.append(('preserved_enter',))
            try: yield
            finally:
                assert adapter.state['rendered']['binocular_held'] is False
                adapter.trace.append(('preserved_exit',))
        def observation(adapter,*args,**kwargs):
            if kwargs.get('cleanup'):adapter.trace.append(('cleanup_observation',))
            return copy.deepcopy(adapter.state)
        clock=Clock()
        with patch('gameplay_bot.recon.BinocularPresentation',Presentation), \
             patch('gameplay_bot.recon.MarkerLog',Cursor), \
             patch('gameplay_bot.recon.preserved_tracking_poses',preserve), \
             patch('gameplay_bot.recon.observed',observation), \
             patch('gameplay_bot.recon.time.monotonic',clock), \
             patch('gameplay_bot.recon.time.sleep',clock.sleep):
            result=inspect(live,{'normal_life':'0','soldiers':[
                {'id':1166,'name':'sol_slopedWest_0000','camp':'camp','life':'0','position':[10.,20.,30.]},
                {'id':1214,'name':'sol_slopedWest_0002','camp':'camp','life':'1','position':[1.,2.,3.]}]},
                output=root/'result',anchor=(30010,10,2,123,4),camp='camp',healthy_life=7801)
        return live,result

    def test_native_acquisition_explicit_mark_zoom_and_both_hand_capture_survive_integration(self):
        live,result=self.run_recon()
        self.assertEqual(result['status'],'observed_physical_recon')
        self.assertEqual(result['native_marked_count'],1)
        self.assertEqual(result['zoom'],{'before':2,'after':4})
        self.assertEqual([x for x in live.trace if x[0]=='look'],[('look',[10.,21.35,30.])])
        self.assertEqual([x[1] for x in live.trace if x[0]=='press'],['binoculars.mark','binoculars.zoom'])
        self.assertTrue(all(x[2:]==(True,True) for x in live.trace if x[0]=='capture'))
        self.assertFalse(result['physical_headset_acceptance'])
        self.assertEqual(len(result['targets']),1)

    def test_other_actor_log_does_not_trigger_a_blind_mark(self):
        live,result=self.run_recon(acquisition=False)
        self.assertEqual(result['native_marked_count'],0)
        self.assertFalse(any(x[0]=='press' and x[1]=='binoculars.mark' for x in live.trace))
        self.assertFalse(result['targets'][0]['explicit_mark'])

    def test_physical_press_without_new_native_result_does_not_count_as_marked(self):
        live,result=self.run_recon(explicit=False)
        self.assertTrue(any(x[0]=='press' and x[1]=='binoculars.mark' for x in live.trace))
        self.assertEqual(result['native_marked_count'],0)

    def test_stow_and_smooth_return_happen_before_saved_tracking_cleanup_even_on_capture_failure(self):
        for failed in (False,True):
            live,result=self.run_recon(capture_failure=failed)
            self.assertEqual(result['status'],'failed' if failed else 'observed_physical_recon')
            stow=live.trace.index(('action','binoculars.stow'))
            restored=live.trace.index(('smooth_restore',)); exited=live.trace.index(('preserved_exit',))
            self.assertLess(stow,restored);self.assertLess(restored,exited)
            self.assertEqual(sum(x==('action','binoculars.stow') for x in live.trace),1)
            self.assertFalse(live.support)

    def test_unchanged_native_magnification_is_a_defect_and_still_stows(self):
        live,result=self.run_recon(zoom_changes=False)
        self.assertEqual(result['status'],'failed')
        self.assertIn('magnification did not change',result['error'])
        self.assertTrue(result['cleanup']['binocular_stowed'])

    def test_cleanup_can_stow_after_damage_or_detection_but_cannot_accept_another_owner(self):
        anchor=(30010,10,2,123,4)
        state={'activation':2,'now_ms':1000,'controls':{'context':'binoculars','rig_input':True,'travel_mode':1},
               'rendered':{'player_owner':123,'presentation_epoch':4,'presentation_focused':True,'sample_ms':990},
               'native':{'mission':30010,'location':10,'popup':False,'saving':False,
                         'player_vehicle_id':65535,'game_over':0,'not_alert':False}}
        with patch('gameplay_bot.recon.fresh_rig',return_value=(state,anchor)), \
             patch('gameplay_bot.recon._validate_xr_scene_state'), \
             patch('gameplay_bot.recon.needs_recovery',return_value=True):
            with self.assertRaises(BotFault): observed(None,anchor,7801)
            self.assertIs(observed(None,anchor,7801,cleanup=True),state)
            state['rendered']['player_owner']=999
            with self.assertRaises(BotFault):observed(None,anchor,7801,cleanup=True)


if __name__=='__main__': unittest.main()
