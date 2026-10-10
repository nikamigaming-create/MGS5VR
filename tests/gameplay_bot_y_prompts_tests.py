"""Owned held-out eye glyphs and current-capture lifecycle, with no native calls."""
import copy
import pathlib
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from gameplay_bot.core import BotFault
from gameplay_bot.y_prompts import classify_y_image, current_y_lifecycle, observe_current_y

ROOT=pathlib.Path(__file__).resolve().parents[1]
CATALOG=ROOT/'private/current-y-prompts/catalog.json'


def state(stamp):
    return {'now_ms':stamp,'native':{'mission':30010,'location':10},'activation':2,
            'rendered':{'sample_ms':stamp-1,'player_owner':99,'presentation_epoch':4,
                'presentation_focused':True,'native_pose':{'position':[0.,1.,0.],
                                                          'orientation':[0.,0.,0.,1.]}},
            'controls':{'physical':[0.]*11,'sticks':[0.]*4}}


class LifecycleTests(unittest.TestCase):
    def test_same_fresh_owner_and_pose_has_capture_interval_freshness_only(self):
        result=current_y_lifecycle(state(1000),state(1800))
        self.assertTrue(result['freshness_verified'])
        self.assertFalse(result['exact_native_target_link_verified'])
        self.assertEqual(result['milliseconds'],800)

    def test_changed_stale_owner_focus_pose_and_interval_cannot_join_visible_prompt(self):
        candidates=[]
        s=state(1800);s['rendered']['player_owner']=100;candidates.append(s)
        s=state(1800);s['rendered']['sample_ms']=1000;candidates.append(s)
        s=state(1800);s['rendered']['presentation_focused']=False;candidates.append(s)
        s=state(1800);s['rendered']['native_pose']['position'][0]=.051;candidates.append(s)
        s=state(1800);s['rendered']['native_pose']['orientation']=[0,.035,0,.999387];candidates.append(s)
        s=state(1800);s['rendered']['native_pose']['orientation'][0]=float('nan');candidates.append(s)
        candidates.extend([state(1000),state(3001)])
        for s in candidates:
            with self.subTest(s=s),self.assertRaises(BotFault):current_y_lifecycle(state(1000),s)

    def test_held_input_cannot_invoke_current_capture(self):
        live=Mock();live.held={'test':1}
        with self.assertRaises(BotFault):observe_current_y(live,catalog_path=CATALOG)
        live.capture.assert_not_called();live.input.assert_not_called()

    def test_neutral_capture_adapter_joins_the_fresh_owner_without_normal_or_vehicle_guess(self):
        live=Mock();live.held={};live.capture.return_value=[{'path':'owned-left.png'},{'path':'owned-right.png'}]
        glyph={'classes':['mortar_exit_arrow'],'native_action_success_proven':False}
        with patch('gameplay_bot.combat.fresh_rig',side_effect=[(state(1000),(30010,10,2,99,4)),
                    (state(1800),(30010,10,2,99,4))]), \
             patch('gameplay_bot.y_prompts.classify_y_image',return_value=glyph):
            result=observe_current_y(live,catalog_path=CATALOG)
        self.assertTrue(result['freshness_verified']);self.assertFalse(result['native_action_success_proven'])
        live.input.assert_not_called();live.release.assert_not_called()


@unittest.skipUnless(CATALOG.exists(), 'private owned final-eye catalog is not part of public source')
class OwnedPixelTests(unittest.TestCase):
    def image(self,run,pattern):
        path=next((ROOT/'artifacts/bot/runs'/run).glob(pattern),None)
        if path is None:self.skipTest('exact retained owned eye is unavailable')
        return path

    def test_entry_and_exit_arrow_are_distinct_and_entry_has_fulton_slot_ambiguity(self):
        entry=classify_y_image(self.image('20261004T205036455995Z','mortar-interaction*-left.png'),CATALOG)
        exit=classify_y_image(self.image('20261004T205036455995Z','mortar-failed*-left.png'),CATALOG)
        self.assertEqual(set(entry['classes']),{'mortar_use','fulton_hold'})
        self.assertTrue(entry['fulton_and_emplacement_slots_visible'])
        self.assertEqual(exit['classes'],['mortar_exit_arrow'])
        self.assertFalse(exit['fulton_and_emplacement_slots_visible'])
        self.assertFalse(exit['native_action_success_proven']);self.assertFalse(exit['freshness_verified'])
        self.assertIsNone(exit['exact_native_target_id'])

    def test_held_out_separate_take_has_use_and_never_exit(self):
        result=classify_y_image(self.image('20261004T203332954328Z','mortar-failed*-left.png'),CATALOG)
        self.assertIn('mortar_use',result['classes']);self.assertNotIn('mortar_exit_arrow',result['classes'])

    def test_unrelated_approach_pixels_and_blank_pixels_do_not_authorize_known_y(self):
        result=classify_y_image(self.image('20261004T205036455995Z','navigation-start*-left.png'),CATALOG)
        self.assertEqual(result['classes'],[])
        from PIL import Image
        with tempfile.TemporaryDirectory() as tmp:
            for color in ('white','black'):
                p=pathlib.Path(tmp)/(color+'.png');Image.new('RGB',(840,880),color).save(p)
                self.assertEqual(classify_y_image(p,CATALOG)['classes'],[])

    def test_unsealed_template_source_cannot_classify(self):
        import json
        with tempfile.TemporaryDirectory() as tmp:
            data=json.loads(CATALOG.read_text());data['templates'][0]['source_sha256']='0'*64
            p=pathlib.Path(tmp)/'catalog.json';p.write_text(json.dumps(data))
            with self.assertRaises(BotFault):
                classify_y_image(self.image('20261004T205036455995Z','navigation-start*-left.png'),p)


if __name__=='__main__':unittest.main()
