"""Field queue preemption, stale state and route invalidation contracts."""
import copy
import pathlib
import sys
import unittest
from unittest.mock import Mock

sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from gameplay_bot.core import BotFault
from gameplay_bot.tactics import TacticalRuntime


def state():
    return {'now_ms':1000,'activation':3,'rendered':{'sample_ms':990,'player_owner':4,'presentation_epoch':5},
            'native':{'mission':30010,'location':10,'not_alert':True,'player_life':7801,
                      'status_INJURY_LOWER':False,'status_INJURY_UPPER':False}}


class TacticalTests(unittest.TestCase):
    def setup_runtime(self):
        now=[0.];r=TacticalRuntime(Mock(),7801,clock=lambda:now[0]);r.publish(state());return r,now

    def test_priority_interrupts_preserve_the_objective_until_actual_completion(self):
        r,now=self.setup_runtime();camp=r.enqueue('secure_camp',20);r.enqueue('body_case',10)
        self.assertEqual(r.intent(),'secure_camp')
        detected=state();detected['native']['not_alert']=False;r.publish(detected)
        self.assertEqual(r.intent(),'counterattack');self.assertIn(camp,r.pending)
        detected['native']['player_life']=4033;r.publish(detected)
        self.assertEqual(r.intent(known_battle=True),'recover_health')
        detected['native']['status_INJURY_UPPER']=True;r.publish(detected)
        self.assertEqual(r.intent(),'recover_health')
        recovered=state();r.publish(recovered);r.finish(camp,{'status':'observed_native_camp_incapacitated'})
        self.assertEqual(r.intent(),'body_case')

    def test_owner_change_cancels_frozen_tasks_and_prevents_reusing_a_route(self):
        r,now=self.setup_runtime();job=r.enqueue('secure_camp');token=r.plan_token()
        changed=state();changed['rendered']['player_owner']=6;r.publish(changed)
        self.assertFalse(r.pending);self.assertEqual(r.intent(),'observe')
        with self.assertRaisesRegex(BotFault,'cancelled'):r.finish(job,{'status':'pass'})
        with self.assertRaisesRegex(BotFault,'invalidated'):r.accept_plan(token)

    def test_damage_invalidates_a_computed_route_even_with_same_owner(self):
        r,now=self.setup_runtime();token=r.plan_token();changed=state();changed['native']['player_life']=7800
        r.publish(changed)
        with self.assertRaisesRegex(BotFault,'invalidated'):r.accept_plan(token)

    def test_stale_or_unjoined_state_cannot_authorize_queue_execution(self):
        r,now=self.setup_runtime();r.enqueue('secure_camp');now[0]=.251
        with self.assertRaisesRegex(BotFault,'stale'):r.intent()
        for change in ('old_frame','missing_alert','missing_wound','unknown_owner'):
            s=state()
            if change=='old_frame':s['rendered']['sample_ms']=849
            elif change=='missing_alert':del s['native']['not_alert']
            elif change=='missing_wound':del s['native']['status_INJURY_UPPER']
            else:s['activation']=False
            with self.subTest(change=change),self.assertRaises(BotFault):r.publish(s)

    def test_hot_history_is_bounded_and_immutable_without_claiming_decoded_pixels(self):
        r,now=self.setup_runtime();s=state();r.publish(s);s['native']['player_life']=1
        self.assertEqual(r.state['native']['player_life'],7801)
        for _ in range(130):r.publish(state())
        self.assertEqual(len(r.history),120)
        self.assertFalse(r.events.emit.call_args.kwargs['decoded_pixels_in_blackboard'])


if __name__=='__main__':unittest.main()
