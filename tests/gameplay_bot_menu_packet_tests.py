"""All effective physical menu masks, native leakage and one-button transitions."""
import copy
import pathlib
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import patch

sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from gameplay_bot.core import BotFault
from gameplay_bot.idroid_cases import MENU_BUTTONS
from gameplay_bot.menu_packet_case import PHYSICAL_INDEX, menu_button_binding, menu_button_packet, run_menu_button_case

ANCHOR=(30010,10,2,99,4)
BINDINGS={'actions':[{'name':action,'contexts':['menus'],
    'bindings':[{'gesture':'level','inputs':[token]}]} for token,(action,mask) in MENU_BUTTONS.items()]}


def sampled(token=None,stamp=1001,*,popup=False,leak=False):
    p=[0]*11
    if token:p[PHYSICAL_INDEX[token]]=1
    mask=MENU_BUTTONS[token][1] if token else 0
    return {'now_ms':stamp,'gamepad':False,'menu':True,'idroid':True,'pause':False,'loading':False,
        'scene':'menu','native':{'mission':30010,'location':10,'player_life':7801,'popup':popup,
            'saving':False,'demo':False,'game_over':0,'tutorial_pause':False,'status_BINOCLE':leak},
        'controls':{'context':'menus','sample_ms':stamp,'age_ms':0,'physical':p,'sticks':[0]*4,
            'native_buttons':mask or 0,'native_axes':[0]*4,
            'native_triggers':[255 if token=='left_trigger' else 0,255 if token=='right_trigger' else 0]}}


class Clock:
    def __init__(self):self.now=0.
    def __call__(self):return self.now
    def sleep(self,seconds):self.now+=seconds


class FakeLive:
    def __init__(self,output,clock,*,transition=False,leak=False,stale=False):
        self.output=output;self.clock=clock;self.bindings=copy.deepcopy(BINDINGS);self.events=self
        self.held_action_recording=object();self.held={};self.inputs=[];self.cleanup_count=0
        self.transition=transition;self.leak=leak;self.stale=stale;self.sample_count=0
        self.popup=False;self.opened_menu=None
    def release(self):self.held={}
    def input(self,values,seconds,*,lease_seconds):
        self.inputs.append(values);self.held={v['component']:(v,self.clock()+lease_seconds) for v in values}
    def observe(self,**unused):
        self.clock.sleep(.01);self.held={k:v for k,v in self.held.items() if v[1]>self.clock()}
        token=None
        if self.held:
            v=next(iter(self.held.values()))[0]
            for t in MENU_BUTTONS:
                from gameplay_bot.live import channel
                if all(channel(t).get(k)==v.get(k) for k in ('hand','component','sub_component')):token=t;break
            self.sample_count+=1
            if self.transition and self.sample_count>=2:self.popup=True
        stamp=1000 if self.stale else 100000+round(self.clock()*1000)
        return sampled(token,stamp,popup=self.popup,leak=self.leak and bool(token))
    def capture(self,label):
        if self.held:raise AssertionError('Compositor RPC under held face button')
        return []
    def emit(self,*args,**kwargs):pass
    def cleanup_menus(self):self.cleanup_count+=1;self.opened_menu=None


class MenuPacketTests(unittest.TestCase):
    def test_all_ten_physical_buttons_have_exact_menu_native_packets(self):
        for token,(action,mask) in MENU_BUTTONS.items():
            with self.subTest(token=token):
                self.assertEqual(menu_button_binding(BINDINGS,token),(action,mask))
                self.assertIsNotNone(menu_button_packet(sampled(token),token,mask,after_sample_ms=1000))
                bad=sampled(token);bad['controls']['native_axes'][0]=1
                with self.assertRaises(BotFault):menu_button_packet(bad,token,mask,after_sample_ms=1000)

    def test_changed_effective_chord_cannot_authorize_a_physical_button(self):
        b=copy.deepcopy(BINDINGS);b['actions'][0]['bindings'][0]['inputs']=['a','menu']
        with self.assertRaises(BotFault):menu_button_binding(b,'a')
        b=copy.deepcopy(BINDINGS);b['actions'][0]['bindings'][0]['gesture']='hold'
        with self.assertRaises(BotFault):menu_button_binding(b,'a')

    def run_case(self,token='a',*,owns=True,changed_owner=False,**options):
        with tempfile.TemporaryDirectory() as tmp:
            clock=Clock();live=FakeLive(pathlib.Path(tmp),clock,**options)
            def fresh(adapter,*unused,**kwargs):
                current=adapter.observe()
                if changed_owner and adapter.held:
                    raise BotFault('Exact current world/VR owner changed')
                return current,ANCHOR
            with patch('gameplay_bot.menu_packet_case.fresh_rig',side_effect=fresh):
                result=run_menu_button_case(live,token=token,fixture_label='prepared-current-map',
                    output=tmp,own_existing_idroid=owns,clock=clock,sleep=clock.sleep)
            return result,live

    def test_each_physical_button_is_executable_once_and_cleanup_is_owned(self):
        for token in MENU_BUTTONS:
            with self.subTest(token=token):
                result,live=self.run_case(token)
                self.assertEqual(result['status'],'observed_native_menu_button_same_surface')
                self.assertEqual(result['input_attempts'],1);self.assertEqual(len(live.inputs),1)
                self.assertFalse(live.held);self.assertEqual(live.cleanup_count,1)
                self.assertFalse(result['page_or_cursor_effect_proven'])

    def test_new_popup_is_observed_and_cannot_authorize_a_second_confirm(self):
        result,live=self.run_case(transition=True)
        self.assertEqual(result['status'],'observed_native_menu_button_and_surface_transition')
        self.assertTrue(result['surface_after']['popup'])
        self.assertEqual(len(live.inputs),1);self.assertFalse(live.held)

    def test_native_binocular_leak_or_missing_sampler_cannot_become_packet_success_or_replay(self):
        for options in ({'leak':True},{'stale':True}):
            with self.subTest(options=options):
                result,live=self.run_case(**options)
                self.assertEqual(result['status'],'failed');self.assertEqual(len(live.inputs),1)
                self.assertFalse(live.held);self.assertEqual(live.cleanup_count,1)

    def test_unowned_current_menu_is_not_clicked_or_closed(self):
        result,live=self.run_case(owns=False)
        self.assertEqual(result['status'],'failed');self.assertEqual(live.inputs,[])
        self.assertEqual(live.cleanup_count,0)

    def test_changed_owner_during_button_releases_without_another_input(self):
        result,live=self.run_case(changed_owner=True)
        self.assertEqual(result['status'],'failed');self.assertIn('owner changed',result['error'])
        self.assertEqual(len(live.inputs),1);self.assertFalse(live.held)
        self.assertEqual(live.cleanup_count,1)


if __name__=='__main__':unittest.main()
