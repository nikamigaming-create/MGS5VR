"""Operate one visually reviewed native hospital profile decision through VR."""
import math
import time

from .core import BotFault, matches
from .session import preserved_tracking_poses, _controller_pose
from .cabin_workbench import compose, inverse_pose, shifted
from .combat import mul


def inspect_profile(live, timeout, policy):
    # This menu publishes no stable row identity. Record actual input and both
    # final eyes; a neutral packet alone cannot certify the intended UI result.
    if not 0 < timeout <= 45 or not isinstance(policy, dict):
        raise BotFault('Profile observation requires a bounded policy')
    if 'keys' in policy:
        return type_profile(live, timeout, policy)
    avatar = policy.get('reviewed_screen') in ('hospital-avatar-preset','hospital-avatar-editor')
    head_turn = policy.get('head_turn_degrees')
    if head_turn is not None and (not avatar or type(head_turn) not in (int,float)
                                 or not math.isfinite(head_turn) or not 0 < head_turn <= 25):
        raise BotFault('Avatar inspection permits at most 25 degrees of bounded physical head turn')
    steps = policy.get('steps')
    if ((not avatar and policy.get('reviewed_screen') not in ('hospital-name-form', 'hospital-name-editor'))
            or not isinstance(steps, list) or not 1 <= len(steps) <= 4
            or any(step not in ('up', 'down', 'left', 'right', 'confirm', 'back') for step in steps)
            or sum(step in ('confirm', 'back') for step in steps) > 1
            or any(step in ('confirm', 'back') for step in steps[:-1])):
        raise BotFault('Profile policy permits up to three cursor edges and one final reviewed button')
    guard = {'mission':10010, 'sequence':'Seq_Game_AvatarEdit' if avatar else 'Seq_Game_FewDaysLater2',
             'story':0, 'popup':False, 'saving':False}
    fast = {'menu':not avatar, 'pause':False, 'idroid':False, 'title':False,
            'loading':False, 'demo':not avatar, 'gamepad':False, 'controls.context':'menus',
            'controls.native_buttons':0}
    if not avatar:
        fast['rendered.presentation_focused']=True
    deadline = time.monotonic() + timeout
    live.release()
    before = live.observe(native=True)
    if not matches(before, fast) or not matches(before.get('native', {}), guard):
        raise BotFault('Profile decision requires the current native name/birthday form')
    result = {'status':'observed_profile_input', 'before':before,
              'input_scope':'physical_input_and_release_only', 'ui_outcome_proven':False,
              'visual_acceptance':'pending_two_eye_review', 'physical_headset_acceptance':'not_run',
              'captures':live.capture('hospital-profile-before'), 'steps':[]}
    try:
        for step in steps:
            if time.monotonic() >= deadline:
                raise BotFault('Profile input deadline expired')
            current = live.observe(native=True)
            if not matches(current, fast) or not matches(current.get('native', {}), guard):
                raise BotFault('Native profile owner changed before the selected edge')
            if step in ('confirm', 'back'):
                live.execute({'op':'action', 'name':'menus.'+step, 'seconds':.15,
                              'state_before':fast, 'native_before':guard})
            else:
                live.execute({'op':'menu_navigate', 'owner':'hospital_avatar' if avatar else 'hospital_profile',
                              'direction':step, 'state_before':fast, 'native_before':guard})
            result['steps'].append({'input':step, 'state':live.observe(native=True)})
        result['after'] = live.observe(native=True)
        result['captures'] += live.capture('hospital-profile-after')
        if head_turn is not None:
            result['head_motion'] = []
            with preserved_tracking_poses(live):
                head = live.text_result(live.call('get_head_pose', {'base_space':'local'}))['pose']
                hands = [_controller_pose(live, hand, kind, 'local')
                         for hand in ('left','right') for kind in ('grip','aim')]
                previous_degrees = 0
                for degrees in (head_turn,-head_turn,0):
                    for ramp in range(1,5):
                        current = live.observe(native=True)
                        if (time.monotonic() >= deadline or not matches(current,fast)
                                or not matches(current.get('native',{}),guard)):
                            raise BotFault('Avatar head inspection lost its native editor owner')
                        angle = math.radians(previous_degrees+(degrees-previous_degrees)*ramp/4)/2
                        target = {'position':head['position'], 'orientation':mul(head['orientation'],
                                  [0.,math.sin(angle),0.,math.cos(angle)])}
                        live.call('set_head_pose', {'base_space':'local',**target})
                        for hand in hands:
                            live.call('set_controller_pose',hand)
                        time.sleep(.075)
                    observed = live.text_result(live.call('get_head_pose',{'base_space':'local'}))
                    if (not observed.get('flags',{}).get('orientation_valid')
                            or abs(sum(a*b for a,b in zip(observed['pose']['orientation'],target['orientation']))) < .9999):
                        raise BotFault('Avatar physical head turn was not acknowledged by the runtime')
                    live.release()
                    result['head_motion'].append({'degrees':degrees,'runtime_pose':observed,
                        'state':live.observe(native=True),'eyes':live.capture('avatar-head-'+str(degrees))})
                    previous_degrees = degrees
        return result
    finally:
        live.release()


def type_profile(live, timeout, policy):
    keys=policy.get('keys')
    if (policy.get('reviewed_screen')!='hospital-name-editor'
            or not isinstance(keys,list) or not 1<=len(keys)<=16
            or any(type(key)!=int or not 0<=key<44 or key in (35,43) for key in keys)):
        raise BotFault('Profile typing requires 1..16 reviewed physical keys')
    guard={'mission':10010,'sequence':'Seq_Game_FewDaysLater2','story':0,'popup':False,'saving':False}
    fast={'menu':True,'pause':False,'idroid':False,'title':False,'loading':False,'demo':True,
          'gamepad':False,'rendered.presentation_focused':True,'rendered.studio.keyboard_only':True,
          'text_input.ready':True,'text_input.active':True,'text_input.visible':True}
    deadline=time.monotonic()+timeout
    def observe():
        state=live.observe(native=True)
        if not matches(state,fast) or not matches(state.get('native',{}),guard):
            raise BotFault('VR typing lost the current native hospital profile owner')
        return state
    def wait(predicate):
        until=min(deadline,time.monotonic()+3.)
        while time.monotonic()<until:
            state=observe()
            if predicate(state):return state
            time.sleep(.04)
        raise BotFault('Native profile did not acknowledge the physical text key')
    result={'status':'observed_profile_keys','input_scope':'physical_keyboard_and_native_publication',
            'ui_outcome_proven':False,'visual_acceptance':'pending_two_eye_review',
            'physical_headset_acceptance':'not_run','keys':[]}
    with preserved_tracking_poses(live):
        try:
            live.release();result['before']=observe()
            result['captures']=live.capture('hospital-keyboard-before')
            grip=_controller_pose(live,'right','grip','local')
            aim=_controller_pose(live,'right','aim','local')
            gripToAim=compose(inverse_pose(grip),aim)
            for key in keys:
                state=observe();studio=state['rendered']['studio'];audit=state['text_input']
                target=compose(studio['room'],shifted(studio['keyboard'],[(key%11-5)*.045,(.5-key//11)*.045,.25]))
                palm=compose(target,inverse_pose(gripToAim))
                for kind,pose in (('grip',palm),('aim',target)):
                    live.call('set_controller_pose',{'hand':'right','pose_type':kind,'base_space':'local',**pose})
                wait(lambda s:s['rendered']['studio']['hover_key']==key)
                live.input([{'hand':'right','component':'Trigger','value':1.}],.15)
                time.sleep(.16);live.release()
                observed=wait(lambda s:s['text_input']['down']==audit['down']+1
                              and s['text_input']['up']==audit['up']+1
                              and s['text_input']['converted']>=audit['converted']+2)
                result['keys'].append({'key':key,'native':observed['text_input']})
            result['after']=observe()
            result['captures']+=live.capture('hospital-keyboard-after')
            return result
        finally:
            live.release()
