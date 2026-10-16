"""Commands must fail closed and release a partial or uncertain choice."""
import copy
import pathlib
import sys
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from gameplay_bot.commands import select, selection_inputs
from gameplay_bot.core import BotFault


def bindings():
    return {'actions': [
        {'name':'commands.keep_open','modifier':True,'contexts':['gameplay','commands'],
         'bindings':[{'gesture':'level','inputs':['x']}]},
        {'name':'commands.confirm','contexts':['commands'],
         'bindings':[{'gesture':'press','inputs':['a']}]}
    ], 'axes':[{'name':'axes.commands','source':'right_stick'}]}


GUARD = {'mission':10100,'sequence':'Seq_Game_Escape','popup':False,'saving':False}


def state(stamp=1000, phase=0, *, direction=-32767, confirm=True):
    physical=[0]*11;sticks=[0]*4
    if phase:physical[2]=1
    if phase>=2:sticks[2]=-1
    if phase>=3:physical[0]=1
    return {'now_ms':stamp,'activation':2,'scene':'gameplay','camera_active':True,
            'camera_available':True,'gamepad':False,'rendered':{'player_owner':99,'presentation_epoch':4},
            'camera_suspended':False,'menu':False,'idroid':False,'native':dict(GUARD),
            'transport':{'seconds':.001},'controls':{
                'context':'commands' if phase else 'gameplay','travel_mode':1,'rig_input':True,
                'sample_ms':stamp,'age_ms':0,'physical':physical,'sticks':sticks,
                'native_packet_source':'xr_runtime_final_mapped_packet',
                'native_buttons':0x100 | (0x80 if phase>=3 and confirm else 0),
                'native_axes':[0,0,direction if phase>=2 else 0,0]}}


class CommandsSelectionTests(unittest.TestCase):
    def test_axis_comes_from_effective_bindings(self):
        b=bindings()
        self.assertEqual(selection_inputs(b,'left')[2],'right_stick_left')
        b['axes'][0]['source']='left_stick'
        self.assertEqual(selection_inputs(b,'down')[2],'left_stick_down')

    def test_ambiguous_disabled_or_overlapping_inputs_refuse_before_live_reads(self):
        variants=[]
        for mutate in (
                lambda b:b['axes'].clear(),
                lambda b:b['axes'].append(copy.deepcopy(b['axes'][0])),
                lambda b:b['actions'][0].update(modifier=False),
                lambda b:b['actions'][0]['bindings'][0].update(gesture='tap'),
                lambda b:b['actions'][1]['bindings'][0].update(inputs=['x']),
                lambda b:b['actions'][0]['bindings'][0].update(inputs=['right_stick_left']),
                lambda b:b['actions'][1]['bindings'][0].update(inputs=['unknown'])):
            b=bindings();mutate(b);variants.append(b)
        for b in variants:
            with self.subTest(bindings=b):
                live=Mock(bindings=b)
                with self.assertRaises(BotFault):select(live,{'direction':'left','native_before':GUARD})
                live.observe.assert_not_called();live.input.assert_not_called()

    def test_no_native_prerequisites_no_action(self):
        for guard in ({},{'mission':10100},{**GUARD,'popup':True},{**GUARD,'saving':True},
                      {**GUARD,'sequence':'Seq_Demo_Test'}):
            live=Mock(bindings=bindings())
            with self.assertRaises(BotFault):select(live,{'direction':'left','native_before':guard})
            live.input.assert_not_called()

    def test_changed_owner_or_non_neutral_entry_does_not_dispatch(self):
        for key,value in (('activation',0),('scene','menu'),('menu',True),('idroid',True),
                          ('camera_suspended',True),('gamepad',True),('camera_available',False),
                          ('rendered',{})):
            current=state();current[key]=value
            live=Mock(bindings=bindings(),held={})
            with patch('gameplay_bot.commands.fresh_control_observation',return_value=(current,current['controls'])):
                with self.assertRaises(BotFault):select(live,{'direction':'left','native_before':GUARD})
            live.input.assert_not_called()
        for change in ('held','native','stick','horse'):
            current=state();live=Mock(bindings=bindings(),held={})
            if change=='held':live.held={'lease':True}
            elif change=='native':current['native']['mission']=10020
            elif change=='stick':current['controls']['sticks'][0]=1
            else:current['controls']['travel_mode']=2
            with patch('gameplay_bot.commands.fresh_control_observation',return_value=(current,current['controls'])):
                with self.assertRaises(BotFault):select(live,{'direction':'left','native_before':GUARD})
            live.input.assert_not_called()

    def execute_fixture(self, *, confirmation=True, direction=-32767, failure_phase=None, owner_change=False):
        live=Mock(bindings=bindings(),held={});phase=0;elapsed=0.;stamp=1000
        def clock():return elapsed
        def sleep(seconds):
            nonlocal elapsed
            elapsed+=seconds
        def send(*args,**kwargs):
            nonlocal phase
            phase+=1
            if failure_phase==phase:raise TimeoutError('completion unknown')
        def observe():
            nonlocal stamp
            stamp+=25
            sample=state(stamp,phase,direction=direction,confirm=confirmation)
            if owner_change and phase:sample['activation']=3
            return sample
        live.input.side_effect=send;live.observe.side_effect=observe
        initial=state()
        with patch('gameplay_bot.commands.fresh_control_observation',return_value=(initial,initial['controls'])):
            try:
                result=select(live,{'direction':'left','native_before':GUARD},clock=clock,sleep=sleep)
            except BaseException as error:result=error
        return live,result

    def test_confirmation_requires_native_r3_and_matching_direction(self):
        live,result=self.execute_fixture()
        self.assertIsInstance(result,dict)
        self.assertEqual(live.input.call_count,3);live.release.assert_called_once()
        self.assertTrue(any(call.args[0]=='commands_choice_delivered' for call in live.events.emit.call_args_list))
        for confirmation,direction in ((False,-32767),(True,32767),(True,0)):
            live,result=self.execute_fixture(confirmation=confirmation,direction=direction)
            self.assertIsInstance(result,BotFault)
            self.assertEqual(live.input.call_count,3);live.release.assert_called_once()

    def test_unknown_input_completion_is_released_never_replayed(self):
        for phase in (1,2,3):
            live,result=self.execute_fixture(failure_phase=phase)
            self.assertIsInstance(result,TimeoutError)
            self.assertEqual(live.input.call_count,phase);live.release.assert_called_once()

    def test_owner_change_releases_before_any_selection(self):
        live,result=self.execute_fixture(owner_change=True)
        self.assertIsInstance(result,BotFault)
        self.assertEqual(live.input.call_count,1);live.release.assert_called_once()


if __name__=='__main__':unittest.main()
