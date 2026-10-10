"""Execute handheld/spatial/ACC/tutorial and both-grip cases offline."""
import copy
import pathlib
import sys
import tempfile
import unittest
from contextlib import nullcontext
from unittest.mock import patch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from gameplay_bot.core import BotFault
from gameplay_bot.idroid_cases import compile_current_idroid, run_current_idroid_case
from gameplay_bot.idroid_film import film_idroid
from gameplay_bot.navigation import NativeMap
from gameplay_bot_idroid_film_tests import Clock, ENEMIES, OfflineLive


class ModeLive(OfflineLive):
    def __init__(self, output, clock, *, handheld=True, cabin=False, tutorial=0,
                 native_binocular_leak=False, start_open=False):
        super().__init__(output, clock)
        self.handheld=handheld; self.cabin=cabin; self.tutorial=tutorial
        self.leak=native_binocular_leak; self.idroid=start_open
        self.bindings['settings'][0]['value']=int(handheld)

    def observe(self, **kwargs):
        s=super().observe(**kwargs); p=s['controls']['physical']; n=s['native']
        s.update(idroid_handheld=self.handheld,
                 idroid_menu_input_ready=self.handheld and self.idroid,
                 idroid_tutorial_mode=self.tutorial if self.idroid else 0)
        n['helicopter_space']=self.cabin
        if self.cabin:n.update(mission=40010, sequence='Seq_Game_MainGame')
        n['tutorial_pause']=self.idroid and self.tutorial>0
        n['status_BINOCLE']=self.leak and bool(p[7] or p[8])
        s['controls']['native_buttons']=(0x100 if p[7] else 0)|(0x200 if p[8] else 0)
        return s


class ModeTests(unittest.TestCase):
    def run_case(self, *, grip='right_grip', own=False, **options):
        with tempfile.TemporaryDirectory() as tmp:
            output=pathlib.Path(tmp); clock=Clock(); live=ModeLive(output,clock,**options)
            nav=NativeMap([{'source':'owned-synthetic', 'sha256':'a'*64, 'groups':[{
                'id':0,'nodes':[[0,0,0],[0,0,-3]],'edges':[{'from':0,'to':1,'flags':0}]}]}])
            def fresh(adapter,*unused,**kw):
                s=adapter.observe(native=True)
                return s,(s['native']['mission'],10,1,s['rendered']['player_owner'],1)
            enemies=copy.deepcopy(ENEMIES)
            if options.get('cabin'):enemies['mission']=40010
            with patch('gameplay_bot.combat.fresh_rig',side_effect=fresh), \
                 patch('gameplay_bot.idroid_film.fresh_rig',side_effect=fresh), \
                 patch('gameplay_bot.idroid_film.read_enemy_snapshot',return_value=enemies), \
                 patch('gameplay_bot.idroid_film.preserved_tracking_poses',return_value=nullcontext()):
                before=live.observe(native=True)
                result=run_current_idroid_case(live,nav,state=before,enemy_script='owned',
                    output=output,goal_node=None if options.get('cabin') else 1,
                    navigation_grip=grip,own_existing_menu=own,clock=clock,sleep=clock.sleep)
            return result,live

    def test_both_grips_have_distinct_native_tab_packets_and_right_grip_release(self):
        result,live=self.run_case()
        self.assertEqual(result['status'],'observed_current_control_probe')
        self.assertEqual(len(result['phases']),4)
        byname={p['name']:p for p in result['phases']}
        for token,mask in [('right_grip',0x200),('left_grip',0x100)]:
            phase=byname[token+'-page-pulse']
            self.assertEqual(phase['held_native_checkpoint']['controls']['native_buttons'],mask)
            self.assertFalse(phase['held_native_checkpoint']['native']['status_BINOCLE'])
        self.assertTrue(all(s['state']['controls']['physical'][8]==0
            for s in result['phases'][-1]['grip_released_same_axis_samples']))
        self.assertFalse(live.idroid); self.assertFalse(live.held)

    def test_spatial_uses_current_spatial_presentation_without_faking_handheld_ready(self):
        result,live=self.run_case(handheld=False)
        self.assertEqual(result['status'],'observed_current_control_probe')
        self.assertFalse(result['declared_current_mode']['handheld'])
        self.assertTrue(all(p['held_native_checkpoint']['idroid_menu_input_ready'] is False
                            for p in result['phases'][:-1]))
        self.assertEqual(live.bindings['settings'][0]['value'],0)

    def test_acc_exercises_both_grips_and_menu_cleanup_with_no_walking_axis(self):
        for handheld in (False,True):
            with self.subTest(handheld=handheld):
                result,live=self.run_case(cabin=True,handheld=handheld)
                self.assertEqual(result['status'],'observed_current_control_probe')
                self.assertFalse(result['declared_current_mode']['exercise_walk'])
                self.assertFalse(result['requested_walk_while_open_native_forward_displacement_observed'])
                self.assertTrue(all(v['component']!='Thumbstick' for row in live.inputs for v in row))
                self.assertFalse(live.idroid)

    def test_actual_forced_tutorial_fixture_is_observed_and_never_mutated_into_a_fixture(self):
        result,live=self.run_case(tutorial=10,start_open=True,own=True)
        self.assertEqual(result['status'],'observed_current_control_probe')
        self.assertEqual(result['declared_current_mode']['tutorial_mode'],10)
        self.assertTrue(result['before']['native']['tutorial_pause'])
        self.assertFalse(live.idroid); self.assertFalse(result['requested_behavior_acceptance'])
        self.assertFalse(any('Tutorial' in str(args) for name,args in live.calls))

    def test_transient_native_binocular_leak_under_grip_stops_without_success_or_replay(self):
        result,live=self.run_case(native_binocular_leak=True)
        self.assertEqual(result['status'],'failed')
        self.assertFalse(live.idroid); self.assertFalse(live.held)
        self.assertLessEqual(len(live.inputs),2)
        self.assertFalse(result['requested_behavior_acceptance'])

    def test_current_user_menu_without_explicit_case_ownership_is_not_stowed(self):
        with tempfile.TemporaryDirectory() as tmp:
            live=ModeLive(pathlib.Path(tmp),Clock(),start_open=True)
            with self.assertRaises(BotFault):
                compile_current_idroid(live.observe(),live.bindings,goal_node=1)
            self.assertTrue(live.idroid); self.assertEqual(live.calls,[])

    def test_stale_owner_or_mode_cannot_authorize_compiled_input(self):
        with tempfile.TemporaryDirectory() as tmp:
            live=ModeLive(pathlib.Path(tmp),Clock()); s=live.observe()
            current=copy.deepcopy(s);current['idroid_handheld']=False
            with patch('gameplay_bot.combat.fresh_rig',return_value=(current,(30010,10,1,1,1))),self.assertRaises(BotFault):
                run_current_idroid_case(live,None,state=s,enemy_script='owned',output=tmp,goal_node=1)
            self.assertEqual(live.inputs,[]); self.assertEqual(live.calls,[])

    def test_remaining_button_recipes_are_explicit_unexecuted_and_not_full_game_pass(self):
        with tempfile.TemporaryDirectory() as tmp:
            live=ModeLive(pathlib.Path(tmp),Clock()); case=compile_current_idroid(live.observe(),live.bindings,goal_node=1)
            self.assertEqual(set(case['other_button_recipes']),
                {'a','x','y','left_stick_click','right_stick_click','left_trigger','right_trigger'})
            self.assertTrue(all(v['executed'] is False for v in case['other_button_recipes'].values()))
            self.assertFalse(case['field_walking_expected_from_source'])
            self.assertFalse(case['spatial_native_pixels_or_full_game_acceptance'])


if __name__=='__main__':unittest.main()
