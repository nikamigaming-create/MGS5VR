"""Acceptance accounting for handheld navigation; no simulated visual pass."""
import math
import pathlib
import sys
import unittest
from unittest.mock import Mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
from gameplay_bot.core import BotFault
from gameplay_bot.idroid import inspect_idroid, navigate_stationary, navigation_focus


class Clock:
    now = 0.
    def __call__(self):
        return self.now
    def sleep(self, duration):
        self.now += duration


class Live:
    def __init__(self, clock, *, source="left_stick", fault=None):
        self.clock, self.fault = clock, fault
        self.bindings = {"axes": [{"name": "axes.menu", "source": source}]}
        self.events = Mock()
        self.held = None
        self.ever_held = False
        self.menu = True
        self.actions, self.inputs, self.captures = [], [], []
        self.serial = 1000
        self.releases = 0
        self.palm = {"position": [0.,0.,0.], "orientation": [0.,0.,0.,1.]}

    def observe(self, native=False):
        self.serial += 1
        sample = 1001 if self.ever_held and self.fault == "duplicate" else self.serial
        sticks = [0., 0., 0., 0.]
        if self.held:
            index = (0 if self.held["hand"] == "left" else 2) + (self.held["sub_component"] == "Y")
            sticks[index] = self.held["value"]
        if self.fault == "no_input":
            sticks = [0., 0., 0., 0.]
        if self.fault == "neutral_held" and self.ever_held and not self.held:
            sticks[0] = .65
        pos = .01 if self.held and self.fault == "moves" else 0.
        yaw = 1. if self.held and self.fault == "turns" else 0.
        context = "gameplay" if self.held and self.fault == "ownership" else "menus"
        return {"scene": "menu" if self.menu else "gameplay", "idroid": self.menu, "activation": 1,
            "rendered": {"sample_ms": sample, "activation": 1, "rig_sequence": 0 if self.fault == "frozen" else self.serial,
                         "left_palm_tracked": True, "left_palm": self.palm},
            "idroid_menu_input_ready": self.menu, "now_ms": sample,
            "native": {"player_x": pos, "player_y": 2., "player_z": 3., "player_yaw": yaw},
            "controls": {"context": context, "sample_ms": sample, "age_ms": 0, "sticks": sticks}}

    def input(self, values, duration, *, lease_seconds):
        self.held = values[0]
        self.ever_held = True
        self.inputs.append(values[0])

    def release(self):
        self.held = None
        self.releases += 1

    def execute(self, step):
        self.actions.append(step)
        if step.get("op") == "pose":
            self.palm = {key: step[key] for key in ("position", "orientation")}
        if step.get("name") == "menus.back" and self.fault != "exit":
            self.menu = False

    def capture(self, label):
        if self.held:
            raise AssertionError("Blocking capture with held input")
        self.captures.append(label)
        return [label+"-left.png", label+"-right.png"]


class Behavior:
    def __init__(self, live):
        self.live = live
    def wait_for(self, expected, timeout, label):
        state = self.live.observe(native=True)
        if any(state.get(key) != value for key, value in expected.items()):
            raise BotFault("Observed state did not reach " + label)
        return state


class FocusLive(Live):
    """Separate grip/axis channels; unfocused phone retains horse controls."""
    def __init__(self, clock, *, focus_hand='left', source='left_stick', fault=None):
        super().__init__(clock, source=source)
        self.focus_hand, self.focus_fault, self.focus = focus_hand, fault, False
        self.bindings['actions']=[{'name':'system.idroid_focus','modifier':True,
            'bindings':[{'gesture':'level','inputs':[focus_hand+'_grip']}]}]

    def observe(self, native=False):
        state=super().observe(native=native)
        state.update(idroid_handheld=True,idroid_menu_control=2 if self.focus else 1,
            idroid_menu_routing_available=True,idroid_closing=False,idroid_tutorial_mode=0,
            pause=False,title=False,loading=False,demo=False,gamepad=False,transport={'seconds':0.},
            camera_active=True,camera_suspended=False)
        state['rendered'].update(player_owner=99,presentation_epoch=2)
        state['native'].update(mission=10020,sequence='Seq_Game_RescueMiller',popup=False,
            saving=False,game_over=0,tutorial_pause=False,status_ON_HORSE=True,status_NORMAL_ACTION=False)
        physical=[0]*11
        if self.focus:physical[7 if self.focus_hand=='left' else 8]=1.
        state['controls'].update(context='menus' if self.focus else 'horse',physical=physical,
            native_packet_source='xr_runtime_final_mapped_packet',native_buttons=0,
            native_axes=[0]*4,native_triggers=[0,0],rig_input=True,
            touches={k:0 for k in ('left_thumbrest','right_thumbrest','a_touch','b_touch','x_touch','y_touch',
                                  'left_stick_touch','right_stick_touch','left_trigger_touch','right_trigger_touch')})
        if self.focus and self.focus_fault=='ack':
            state['idroid_menu_control']=1;state['controls']['context']='horse'
        if self.focus and self.focus_fault=='owner':state['rendered']['player_owner']=100
        if self.focus and self.focus_fault=='popup':state['native']['popup']=True
        if self.held and self.focus_fault=='axis_owner':state['rendered']['presentation_epoch']=3
        return state

    def input(self, values, duration, *, lease_seconds):
        if values[0]['component']=='Grip':
            self.focus=True;self.inputs.append(values[0])
            if self.focus_fault=='timeout':raise TimeoutError('focus completion unknown')
        else:
            if not self.focus:raise AssertionError('Axis dispatched without physical grip focus')
            super().input(values,duration,lease_seconds=lease_seconds)

    def release(self, *, keep=()):
        super().release()
        if not keep:self.focus=False


class IdroidFocusTests(unittest.TestCase):
    def test_both_sticks_wait_for_effective_focus_and_release_it(self):
        for focus_hand in ('left','right'):
            for source in ('left_stick','right_stick'):
                clock=Clock();live=FocusLive(clock,focus_hand=focus_hand,source=source);result={}
                navigate_stationary(live,result,clock=clock,sleep=clock.sleep)
                self.assertEqual(live.inputs[0]['component'],'Grip')
                self.assertEqual(live.inputs[0]['hand'],focus_hand)
                self.assertEqual(live.inputs[1]['hand'],source.split('_')[0])
                self.assertEqual(len(live.inputs),3)
                self.assertEqual(result['max_displacement'],0.)
                self.assertFalse(live.focus);self.assertFalse(live.held)

    def test_unavailable_or_wrong_focus_binding_dispatches_nothing(self):
        for bindings in ([],[{'name':'system.idroid_focus','modifier':False,'bindings':[]}],
                         [{'name':'system.idroid_focus','modifier':True,
                           'bindings':[{'gesture':'level','inputs':['a']}]}]):
            clock=Clock();live=FocusLive(clock);live.bindings['actions']=bindings
            with self.assertRaises(BotFault):navigate_stationary(live,{},clock=clock,sleep=clock.sleep)
            self.assertFalse(live.inputs)

    def test_missing_ack_changed_owner_or_popup_never_sends_an_axis(self):
        for fault in ('ack','owner','popup','timeout'):
            clock=Clock();live=FocusLive(clock,fault=fault)
            with self.assertRaises((BotFault,TimeoutError)):
                navigate_stationary(live,{},clock=clock,sleep=clock.sleep)
            self.assertEqual(len(live.inputs),1)
            self.assertEqual(live.inputs[0]['component'],'Grip')
            self.assertFalse(live.focus);self.assertFalse(live.held)
            self.assertLess(clock.now,1.)

    def test_owner_change_during_axis_stops_before_the_other_stick(self):
        clock=Clock();live=FocusLive(clock,fault='axis_owner')
        with self.assertRaisesRegex(BotFault,'owner'):
            navigate_stationary(live,{},clock=clock,sleep=clock.sleep)
        self.assertEqual(len(live.inputs),2)
        self.assertFalse(live.focus);self.assertFalse(live.held)

    def test_expired_focus_never_dispatches_another_axis(self):
        clock=Clock();live=FocusLive(clock);initial=live.observe(native=True)
        with self.assertRaisesRegex(BotFault,'expired'):
            with navigation_focus(live,initial,clock=clock,sleep=clock.sleep) as (_,guard):
                clock.sleep(4.6);guard(live.observe(native=True))
        self.assertEqual(len(live.inputs),1);self.assertFalse(live.focus)

    def test_tutorial_loading_or_unidentified_owner_refuses_before_focus(self):
        for key,value in (('pause',True),('loading',True),('idroid_closing',True),
                          ('camera_active',False),('camera_suspended',True),('activation',2),
                          ('idroid_tutorial_mode',11),('rendered',{})):
            clock=Clock();live=FocusLive(clock);initial=live.observe(native=True);initial[key]=value
            with self.assertRaises(BotFault):
                with navigation_focus(live,initial,clock=clock,sleep=clock.sleep):
                    self.fail('Unknown owner admitted')
            self.assertFalse(live.inputs)


class IdroidAcceptanceTests(unittest.TestCase):
    def test_distinct_native_samples_from_both_sticks_with_neutral_exit(self):
        clock = Clock(); live = Live(clock)
        result = inspect_idroid(live, Behavior(live), clock=clock, sleep=clock.sleep)
        case = result["cases"][0]
        self.assertEqual(case["status"], "observed_pass")
        self.assertEqual(case["visual_acceptance"], "pending")
        self.assertEqual(case["headset_acceptance"], "not_run")
        self.assertFalse(live.held)
        self.assertFalse(live.menu)
        self.assertEqual({v["hand"] for v in live.inputs}, {"left", "right"})
        self.assertGreater(clock.now, 1.8)
        self.assertEqual(case["max_displacement"], 0.)
        self.assertGreater(len({tuple(p["orientation"]) for p in case["declared_poses"]}), 1)

    def test_remapped_menu_axis_is_respected(self):
        clock = Clock(); live = Live(clock, source="right_stick")
        navigate_stationary(live, {}, clock=clock, sleep=clock.sleep)
        self.assertEqual(live.inputs[0]["hand"], "right")
        self.assertEqual(live.inputs[1]["hand"], "left")

    def test_duplicate_input_sample_is_not_two_observations(self):
        clock = Clock(); live = Live(clock, fault="duplicate")
        with self.assertRaisesRegex(BotFault, "distinct"):
            navigate_stationary(live, {}, clock=clock, sleep=clock.sleep)
        self.assertFalse(live.held)

    def test_motion_turn_and_lost_ownership_fail_and_release(self):
        for fault in ("moves", "turns", "ownership", "no_input", "neutral_held"):
            with self.subTest(fault=fault):
                clock = Clock(); live = Live(clock, fault=fault)
                result = inspect_idroid(live, Behavior(live), clock=clock, sleep=clock.sleep)
                self.assertEqual(result["status"], "failed")
                self.assertFalse(live.held)
                self.assertGreater(live.releases, 1)

    def test_unknown_exit_never_guesses_confirm(self):
        clock = Clock(); live = Live(clock, fault="exit")
        result = inspect_idroid(live, Behavior(live), clock=clock, sleep=clock.sleep)
        self.assertEqual(result["status"], "failed")
        self.assertIn("exit_error", result["cases"][0])
        self.assertFalse(any(a.get("name") == "menus.confirm" for a in live.actions))

    def test_stationary_player_cannot_pass_a_frozen_native_hand(self):
        clock = Clock(); live = Live(clock, fault="frozen")
        result = inspect_idroid(live, Behavior(live), clock=clock, sleep=clock.sleep)
        case = result["cases"][0]
        self.assertEqual(case["stationary_navigation"], "observed_pass")
        self.assertEqual(case["native_palm_motion"], "failed")
        self.assertEqual(result["status"], "failed")
        self.assertFalse(live.held)

    def test_missing_position_is_not_stationary_proof(self):
        clock = Clock(); live = Live(clock)
        observe = live.observe
        def broken(native=False):
            state = observe(native=native)
            state["native"]["player_x"] = math.nan
            return state
        live.observe = broken
        result = inspect_idroid(live, Behavior(live), clock=clock, sleep=clock.sleep)
        self.assertEqual(result["status"], "failed")
        self.assertFalse(live.inputs)

    def test_one_missing_publication_reobserves_without_replaying_input(self):
        clock=Clock();live=Live(clock);observe=live.observe;missed=[]
        def intermittent(native=False):
            state=observe(native=native)
            if live.held and live.held['hand']=='right' and not missed:
                state['controls']=None;missed.append(True)
            return state
        live.observe=intermittent
        result={}
        navigate_stationary(live,result,clock=clock,sleep=clock.sleep)
        self.assertEqual(len(live.inputs),2)
        self.assertEqual(len(result['unavailable_control_samples']),1)
        self.assertFalse(live.held)
        self.assertEqual(result['max_displacement'],0.)

    def test_missing_control_sample_still_checks_native_player_motion(self):
        clock=Clock();live=Live(clock);observe=live.observe
        def missing_and_moving(native=False):
            state=observe(native=native)
            if live.held:
                state['controls']=None
                state['native']['player_x']=.01
            return state
        live.observe=missing_and_moving
        with self.assertRaisesRegex(BotFault,'moved Snake'):
            navigate_stationary(live,{},clock=clock,sleep=clock.sleep)
        self.assertFalse(live.held)

    def test_continuously_missing_held_samples_fail_in_bounded_time(self):
        clock=Clock();live=Live(clock);observe=live.observe
        def missing(native=False):
            state=observe(native=native)
            if live.held:state['controls']=None
            return state
        live.observe=missing
        with self.assertRaises(BotFault):
            navigate_stationary(live,{},clock=clock,sleep=clock.sleep)
        self.assertLess(clock.now,.4)
        self.assertEqual(len(live.inputs),1)
        self.assertFalse(live.held)


if __name__ == "__main__":
    unittest.main()
