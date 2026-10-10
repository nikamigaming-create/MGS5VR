"""Current physical contexts, menu shoulder allowance and forbidden native optics."""
import copy
import pathlib
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent))
from gameplay_bot.core import BotFault
from gameplay_bot.grip_leakage_case import grip_packet, grip_roles, run_current_grip_leakage
from gameplay_bot.live import channel
from gameplay_bot_menu_packet_tests import Clock, FakeLive, sampled

ANCHOR=(30010,10,2,99,4)


def bindings(context):
    mapping={'gameplay':{'gameplay.support_grip':'left_grip','gameplay.ready_weapon':'right_grip'},
             'binoculars':{'binoculars.support_grip':'left_grip'},
             'menus':{'menus.previous_tab':'left_grip','menus.next_tab':'right_grip'}}[context]
    return {'actions':[{'name':name,'contexts':[context],
                       'bindings':[{'gesture':'level','inputs':[token]}]} for name,token in mapping.items()]}


def grip_state(context,tokens=(),stamp=1001,leak=False):
    s=sampled(stamp=stamp,leak=leak);c=s['controls'];n=s['native']
    c['context']=context
    c['physical'][7]=int('left_grip' in tokens);c['physical'][8]=int('right_grip' in tokens)
    c['native_buttons']=((0x100 if 'left_grip' in tokens else 0)|
                         (0x200 if 'right_grip' in tokens else 0)) if context=='menus' else 0
    c['native_triggers']=[255 if context=='gameplay' and 'right_grip' in tokens else 0,0]
    s['menu']=s['idroid']=context=='menus'
    n.update(player_x=0.,player_y=0.,player_z=0.,status_CARRY=False,not_alert=True,
             status_INJURY_LOWER=False,status_INJURY_UPPER=False,status_NORMAL_ACTION=True)
    s['rendered']={'binocular_held':context=='binoculars'}
    return s


class GripLive(FakeLive):
    def __init__(self,output,clock,context,**options):
        super().__init__(output,clock,**options);self.context=context;self.bindings=bindings(context)
    def input(self,values,seconds,*,lease_seconds):
        self.inputs.append(values)
        self.held={(v['hand'],v['component'],v.get('sub_component')):(v,self.clock()+lease_seconds) for v in values}
    def observe(self,**unused):
        self.clock.sleep(.01);self.held={k:v for k,v in self.held.items() if v[1]>self.clock()}
        tokens=[]
        for token in ('left_grip','right_grip'):
            if any(all(channel(token).get(k)==v.get(k) for k in ('hand','component','sub_component'))
                   for v,_ in self.held.values()):tokens.append(token)
        return grip_state(self.context,tokens,100000+round(self.clock()*1000),self.leak and bool(tokens))


class GripTests(unittest.TestCase):
    def test_three_contexts_have_distinct_allowed_native_grip_roles(self):
        for context in ('gameplay','binoculars','menus'):
            roles=grip_roles(bindings(context),context);self.assertTrue(roles)
            for tokens in (['left_grip'],['right_grip'],['left_grip','right_grip']):
                self.assertIsNotNone(grip_packet(grip_state(context,tokens),context,tokens,after_sample_ms=1000))

    def test_shoulders_or_trigger_bleed_into_physical_binoculars_fail(self):
        for field,value in [('native_buttons',0x200),('native_triggers',[255,0])]:
            s=grip_state('binoculars',['left_grip']);s['controls'][field]=value
            with self.assertRaises(BotFault):grip_packet(s,'binoculars',['left_grip'],after_sample_ms=1000)
        with self.assertRaises(BotFault):
            grip_packet(grip_state('menus',['left_grip'],leak=True),'menus',['left_grip'],after_sample_ms=1000)

    def test_effective_extra_grip_action_or_chord_prevents_dispatch(self):
        b=bindings('binoculars');b['actions'].append({'name':'binoculars.native_right_shoulder',
            'contexts':['binoculars'],'bindings':[{'gesture':'level','inputs':['right_grip']}]})
        with self.assertRaises(BotFault):grip_roles(b,'binoculars')
        b=bindings('gameplay');b['actions'][0]['bindings'][0]['inputs']=['left_grip','right_grip']
        with self.assertRaises(BotFault):grip_roles(b,'gameplay')

    def run_case(self,context,**options):
        with tempfile.TemporaryDirectory() as tmp:
            clock=Clock();live=GripLive(pathlib.Path(tmp),clock,context,**options)
            def fresh(adapter,*unused,**kw):return adapter.observe(),ANCHOR
            with patch('gameplay_bot.grip_leakage_case.fresh_rig',side_effect=fresh):
                result=run_current_grip_leakage(live,output=tmp,clock=clock,sleep=clock.sleep)
            return result,live

    def test_executes_left_right_and_both_under_each_current_context_without_fixture_setup(self):
        for context in ('gameplay','binoculars','menus'):
            with self.subTest(context=context):
                result,live=self.run_case(context)
                self.assertEqual(result['status'],'observed_current_grip_packets_without_native_binocular_mode')
                self.assertEqual(len(result['phases']),3);self.assertEqual(len(live.inputs),3)
                self.assertTrue(all(len(phase['samples'])>=2 for phase in result['phases']))
                self.assertFalse(result['fixture_setup_changed_by_case'])
                self.assertFalse(result['visual_grain_absence_proven']);self.assertFalse(live.held)

    def test_transient_native_binocular_mode_stops_and_releases_without_replay(self):
        result,live=self.run_case('gameplay',leak=True)
        self.assertEqual(result['status'],'failed');self.assertEqual(len(live.inputs),1)
        self.assertFalse(live.held);self.assertFalse(result['visual_grain_absence_proven'])


if __name__=='__main__':unittest.main()
