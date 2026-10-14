"""Feedback-driven firearm use through the effective VR hand/input route.

Native marker capsules and terrain visibility help plan. Actual actor life and
ammunition establish outcomes. Both-eye visual review is a separate acceptance
gate; neither a visibility query nor an input acknowledgement proves pixels.
"""
import math
import re
import time

from .core import BotFault, atomic_json
from .live import channel, fresh_control_observation, held_input_evidence, rotate
from .locomotion import _position, _validate_field_state
from .perception import read_enemy_snapshot
from .session import preserved_tracking_poses
from .health import HealthRecoveryNeeded, needs_recovery
from .motion import _pose


class AimProbeUnavailable(BotFault):
    """One target cannot currently be inspected; other targets remain eligible."""


class RigPublicationDelay(BotFault):
    """The same live owner has not produced a fresh rig within the deadline."""


def mul(a, b):
    x,y,z,w=a; u,v,t,s=b
    return [w*u+x*s+y*t-z*v, w*v-x*t+y*s+z*u,
            w*t+x*v-y*u+z*s, w*s-x*u-y*v-z*t]


def normalized(values):
    if any(type(v) not in (int,float) or not math.isfinite(v) for v in values):
        raise BotFault('Aim geometry is nonfinite')
    length=math.sqrt(sum(v*v for v in values))
    if length<1e-8:raise BotFault('Aim geometry is degenerate')
    return [v/length for v in values]


def inverse(q):
    q=normalized(q);return [-q[0],-q[1],-q[2],q[3]]


def pointing(direction):
    """OpenXR aim -Z with a gravity-up yaw/pitch convention, xyzw."""
    x,y,z=normalized(direction)
    yaw=math.atan2(-x,-z);pitch=math.asin(max(-1.,min(1.,y)))
    return mul([0.,math.sin(yaw/2),0.,math.cos(yaw/2)],
               [math.sin(pitch/2),0.,0.,math.cos(pitch/2)])


def effective_pitch(bindings):
    rows=[r['value'] for r in bindings.get('settings',[])
          if r.get('name')=='settings.weapon_aim_pitch_degrees']
    if len(rows)!=1 or type(rows[0]) not in (int,float) or not math.isfinite(rows[0]) or abs(rows[0])>90:
        raise BotFault('Effective firearm aim calibration is unavailable')
    return rows[0]


def aim_quaternion(native_camera, muzzle, target, pitch_degrees):
    # nativeTrackedPose = nativeHead * Y180 * inverse(sourceHead) * tracked.
    # For a VIEW-space pose, sourceHead cancels. Native muzzle forward is +Z;
    # XR aim forward is -Z. Remove only the exported firearm pitch calibration.
    basis=mul(normalized(native_camera['orientation']),[0.,1.,0.,0.])
    direction=rotate(inverse(basis),[t-m for t,m in zip(target,muzzle)])
    angle=math.radians(pitch_degrees)/2
    return normalized(mul(pointing(direction),[-math.sin(angle),0.,0.,math.cos(angle)]))


def aim_error(muzzle, target):
    ray=normalized(rotate(normalized(muzzle['orientation']),[0.,0.,1.]))
    delta=[t-m for t,m in zip(target,muzzle['position'])]
    along=sum(a*b for a,b in zip(ray,delta))
    lateral=math.sqrt(sum((d-r*along)**2 for d,r in zip(delta,ray)))
    return along,lateral


def corrected_aim(native_camera, muzzle, target, current_aim):
    """Close the loop on the actual authored barrel, including its own offsets."""
    actual=normalized(rotate(normalized(muzzle['orientation']),[0.,0.,1.]))
    desired=normalized([t-p for t,p in zip(target,muzzle['position'])])
    dot=max(-1.,min(1.,sum(a*b for a,b in zip(actual,desired))))
    cross=lambda a,b:[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
    if dot<-.999999:
        axis=min(([1.,0.,0.],[0.,1.,0.],[0.,0.,1.]),key=lambda v:abs(sum(a*b for a,b in zip(actual,v))))
        delta=normalized(cross(actual,axis)+[0.])
    else:delta=normalized(cross(actual,desired)+[1.+dot])
    basis=mul(normalized(native_camera['orientation']),[0.,1.,0.,0.])
    view_delta=mul(mul(inverse(basis),delta),basis)
    return normalized(mul(view_delta,normalized(current_aim)))


def facing_error(native_camera, target):
    """Degrees between the published player view (+Z) and the target."""
    forward=normalized(rotate(normalized(native_camera['orientation']),[0.,0.,1.]))
    direction=normalized([t-p for t,p in zip(target,native_camera['position'])])
    return math.degrees(math.acos(max(-1.,min(1.,sum(a*b for a,b in zip(forward,direction))))))


def controller_pair(grip, aim):
    """Keep the measured grip transform in the aim controller's own frame."""
    inverse_aim=inverse(aim['orientation'])
    delta=[g-a for g,a in zip(grip['position'],aim['position'])]
    if len(delta)!=3 or any(not math.isfinite(v) for v in delta):raise BotFault('Controller pair is unavailable')
    return {'orientation':normalized(mul(inverse_aim,grip['orientation'])),
            'position':rotate(inverse_aim,delta)}


def paired_aim_position(grip_position, aim_orientation, pair):
    delta=rotate(aim_orientation,pair['position'])
    return [g-d for g,d in zip(grip_position,delta)]


def support_poses(frame, pair):
    """Authored support contact is LOCAL, with the measured grip/aim join."""
    grip=frame.get('weapon_support_grip')
    if frame.get('weapon_support_tracked') is not True or not isinstance(grip,dict):
        raise AimProbeUnavailable('The current weapon has no tracked authored support contact')
    point=grip.get('position');attitude=grip.get('orientation')
    if (not isinstance(point,list) or len(point)!=3 or not isinstance(attitude,list) or len(attitude)!=4
        or any(type(v) not in (int,float) or not math.isfinite(v) for v in point+attitude)):
        raise BotFault('The authored support contact is invalid')
    attitude=normalized(attitude)
    aim_orientation=normalized(mul(attitude,inverse(pair['orientation'])))
    return {'position':list(point),'orientation':attitude}, {
        'position':paired_aim_position(point,aim_orientation,pair),'orientation':aim_orientation}


def place_support_hand(live, frame, pair):
    grip,aim=support_poses(frame,pair)
    # Validate both transforms before either mutation. No guessed weapon offset
    # or VIEW-space conversion: the rig exports this exact LOCAL contact.
    for kind,pose in (('grip',grip),('aim',aim)):
        live.call('set_controller_pose',{'hand':'left','pose_type':kind,'base_space':'local',**pose})
    live.events.emit('combat_support_pose_set',base_space='local',grip=grip,aim=aim,
                     source_sample_ms=frame.get('sample_ms'),
                     presentation_epoch=frame.get('presentation_epoch'))


def aimed_support_frame(frame, grip_position, aim_orientation, pair):
    """Move the measured LOCAL contact with the same requested primary turn.

    A rifle's attached support hand guides the barrel. Sampling its socket
    after turning only the primary hand follows that still-constrained old
    barrel, so the two simulated hands oppose each other. Prepare both poses
    from one publication before dispatch, preserving the authored contact.
    """
    head=frame.get('head_pose') or {};current=frame.get('right_grip') or {}
    head=_pose(head.get('position'),head.get('orientation'),local_head=True)
    current=_pose(current.get('position'),current.get('orientation'))
    contact,_=support_poses(frame,{'position':[0.,0.,0.],'orientation':[0.,0.,0.,1.]})
    view=_pose(grip_position,normalized(mul(aim_orientation,pair['orientation'])))
    target={'position':[h+d for h,d in zip(head['position'],rotate(head['orientation'],view['position']))],
            'orientation':normalized(mul(head['orientation'],view['orientation']))}
    target=_pose(**target)
    turn=normalized(mul(target['orientation'],inverse(current['orientation'])))
    offset=rotate(turn,[c-p for c,p in zip(contact['position'],current['position'])])
    contact=_pose([p+d for p,d in zip(target['position'],offset)],
                  normalized(mul(turn,contact['orientation'])))
    return {**frame,'weapon_support_grip':contact}


def comfortable_grip(native):
    # A standing chest pose puts the pistol into the ground from the actual
    # prone eye. These are bot hand poses; player rig/calibration stay intact.
    if native.get('status_CRAWL') is True:return [.18,-.06,-.30]
    if native.get('status_SQUAT') is True:return [.22,-.12,-.40]
    if native.get('status_STAND') is True:return [.22,-.25,-.45]
    raise BotFault('A natural ready-hand pose requires a known native posture')


def looking_head(head_orientation,native_camera,target):
    # HeadCamera publishes native * Y180 * relativeHMD * inverse(Y180).
    # Turn the existing HMD in its own frame, including target elevation.
    local=rotate(inverse(native_camera['orientation']),
                 [t-p for t,p in zip(target,native_camera['position'])])
    return normalized(mul(head_orientation,pointing(rotate([0.,1.,0.,0.],local))))


def fresh_rig(live, anchor=None, *, clock=time.monotonic, sleep=time.sleep):
    """Wait for a current same-owner pose; never accept old gameplay pixels."""
    until=clock()+.75;expected=anchor
    while True:
        state,_=fresh_control_observation(live.observe,native=True)
        native=state['native'];frame=state.get('rendered') or {}
        identity=(native.get('mission'),native.get('location'),state.get('activation'),
                  frame.get('player_owner'),frame.get('presentation_epoch'))
        if (any(type(v) is not int or v<=0 for v in identity)
            or (expected is not None and identity!=expected)
            or frame.get('presentation_focused') is not True
            or type(native.get('player_life')) not in (int,float) or native['player_life']<=0):
            raise BotFault('Field world, life, focus or VR rig owner changed')
        expected=identity
        stamp=frame.get('sample_ms')
        if type(stamp) is int and 0<=state['now_ms']-stamp<=150:return state,identity
        live.events.emit('field_publication_wait',state=state,anchor=identity,input_replayed=False)
        if clock()>=until:raise RigPublicationDelay('The same-owner VR rig did not publish a fresh pose before the bounded deadline')
        sleep(.025)


def admit(live, anchor=None, *, witnessed_hospital_ride=False, clock=time.monotonic, sleep=time.sleep):
    # A released sprint/hit animation can briefly clear NORMAL_ACTION. Wait
    # with observation only; no gameplay action may use that transition state.
    until=clock()+1.5;expected=anchor
    while True:
        state,identity=fresh_rig(live,expected,clock=clock,sleep=sleep)
        expected=identity
        ride = (witnessed_hospital_ride is True
            and tuple(state['native'].get(k) for k in ('mission','location','story','sequence'))
                == (10010,30,0,'Seq_Game_VolginRide')
            and state['native'].get('hospital_volgin_ride') is True
            and state['native'].get('weapon_PRIMARY_HIP') == 70)
        if witnessed_hospital_ride and not ride:
            raise BotFault('Scripted horse combat lost its native ride and shotgun owner')
        try:_validate_field_state(state,witnessed_hospital_route=ride)
        except BotFault:
            native=state['native']
            waiting=(native.get('status_NORMAL_ACTION') is False and native.get('status_CARRY') is False
                and native.get('status_CQC_HOLD') is False and native.get('status_CQC_CONTINUOUS') is False
                and native.get('saving') is False and native.get('popup') is False
                and native.get('game_over')==0 and type(native.get('game_over')) is int
                and native.get('player_vehicle_id')==65535 and native.get('title') is False
                and native.get('sequence','').startswith('Seq_Game_')
                and any(native.get('status_'+p) is True for p in ('STAND','SQUAT','CRAWL')))
            if not waiting or clock()>=until:raise
            # This second check retains every XR/scene guard during the wait.
            from .locomotion import _validate_xr_field_state
            _validate_xr_field_state(state)
            live.events.emit('field_action_transition_wait',state=state,input_replayed=False)
            sleep(.025);continue
        break
    native=state['native'];frame=state.get('rendered') or {}
    if (type(native.get('player_life')) not in (int,float) or native['player_life']<=0
        or any(type(v) is not int or v<=0 for v in identity)
        or frame.get('presentation_focused') is not True
        or type(frame.get('sample_ms')) is not int or not 0<=state['now_ms']-frame['sample_ms']<=150
        or any(frame.get('raw_right_'+kind+'_tracked') is not True for kind in ('grip','aim'))
        or (anchor is not None and identity!=anchor)):
        raise BotFault('Combat world, life, focus or VR rig owner changed')
    return state,identity


def sight(live, anchor, *, witnessed_hospital_ride=False, clock=time.monotonic):
    # Actual 20261005-truck-active-driver-69cf fired the WU correctly, then
    # its native bolt animation withdrew the muzzle after arm() had sampled
    # readiness. The same-owner publication remained fresh throughout. Allow
    # the already observed native cycle to finish within the independent 3 s
    # grip lease; this does not relax pose freshness or admit another trigger.
    deadline=clock()+2.8;renewed=False;newer_than=-1
    while True:
        started=clock()
        data,timing=live.native.read('inspect-combat-sight')
        state,_=admit(live,anchor,**({'witnessed_hospital_ride':True} if witnessed_hospital_ride else {}))
        frame=state['rendered']
        owner_valid=(data.get('activation')==anchor[2] and data.get('player_owner')==anchor[3]
                     and data.get('presentation_epoch')==anchor[4])
        fresh=(data.get('muzzle_tracked') is True
               and type(data.get('sample_ms')) is int and type(data.get('now_ms')) is int
               and 0<=data['now_ms']-data['sample_ms']+(clock()-started)*1000<=250
               and abs(frame['sample_ms']-data['sample_ms'])<=150
               and data['sample_ms']>newer_than)
        if data.get('enabled') is False and not renewed and clock()<deadline:
            # Actual run 161525 reached a valid same-owner attached muzzle
            # 58 ms after its observation-only lease expired. Renew only that
            # diagnostic lease and require a NEW native marker-job sample.
            # An unarmed observer starts with zero owner fields. The fresh rig
            # above already establishes authority to arm its read-only lease;
            # the resulting sample must still join the exact current owner.
            lease,_=live.native.read('diagnostics:combat-sight:3000')
            if lease.get('armed') is not True:raise BotFault('Native sight observation lease could not renew')
            newer_than=lease['now_ms'];renewed=True
            live.events.emit('combat_sight_lease_renewed',previous=data,newer_than=newer_than,
                             input_replayed=False)
            time.sleep(.025)
            continue
        if owner_valid and data.get('enabled') is True and fresh:
            live.events.emit('combat_sight',snapshot=data,transport=timing,native_tick=state['now_ms'])
            return data,state
        live.events.emit('combat_sight_unavailable',snapshot=data,transport=timing,
                         anchor=anchor,state=state,read_seconds=clock()-started)
        if not owner_valid or data.get('enabled') is not True or clock()>=deadline:
            raise BotFault('Native muzzle/visibility publication is unavailable or stale')
        # A same-owner publication gap gets observation only. No trigger,
        # pose or action is replayed, and stale geometry never admits fire.
        time.sleep(.025)


def read_actor(live, actor):
    name=actor['name'];object_id=actor['id']
    if not re.fullmatch(r'sol_[A-Za-z0-9_]+',name) or type(object_id) is not int or not 0<object_id<65535:
        raise BotFault('Combat requires an authored soldier identity')
    script=f'''local id=GameObject.GetGameObjectId('TppSoldier2','{name}')
assert(id=={object_id},'native soldier identity changed')
local life=GameObject.SendCommand(id,{{id='GetLifeStatus'}})
assert(type(life)=='number','native soldier life unavailable')
return '{{"id":'..tostring(id)..',"life":'..tostring(life)..'}}' '''
    data,timing=live.native.read(script)
    if data.get('id')!=object_id or type(data.get('life')) is not int:
        raise BotFault('Actor outcome is ambiguous')
    return data,timing


def active_targets(enemies, geometry, *, camp=None, radius=80, require_clear=True, target_name=None):
    if target_name is not None and (not isinstance(target_name, str)
            or not re.fullmatch(r'sol_[A-Za-z0-9_]+', target_name)):
        raise BotFault('Combat target must be one exact native soldier name')
    if enemies.get('truncated') is not False or enemies.get('unavailable_loaded')!=0:
        raise BotFault('Combat cannot plan from incomplete enemy state')
    actors={r['id']:r for r in enemies['soldiers'] if r['life']==enemies['normal_life']
            and re.fullmatch(r'sol_[A-Za-z0-9_]+',r['name']) and (camp is None or r.get('camp')==camp)
            and (target_name is None or r['name']==target_name)}
    candidates=[]
    for target in geometry['targets']:
        if (target.get('query_valid') is True and (not require_clear or target.get('unoccluded') is True)
            and 1<=target.get('distance',math.inf)<=radius and target['id'] in actors):
            candidates.append({**actors[target['id']], 'sight':target})
    # Native phase is retained for evidence, not guessed into an aggression enum.
    return sorted(candidates,key=lambda row:(row['sight']['distance'],row['id']))


def arm(live, ready_inputs, anchor, *, healthy_life=None, witnessed_hospital_ride=False, clock=time.monotonic, sleep=time.sleep):
    state,_=admit(live,anchor,**({'witnessed_hospital_ride':True} if witnessed_hospital_ride else {}))
    if healthy_life is not None and needs_recovery(state['native'],healthy_life):
        raise HealthRecoveryNeeded('Damage takes priority before weapon readiness')
    already=held_input_evidence(state,ready_inputs)
    if (already and state['rendered'].get('native_firearm') is True
        and state['controls'].get('native_triggers')==[255,0]
        and state['rendered'].get('weapon_native_observer',{}).get('muzzle_solved') is True):
        # Repeated arm() calls used to impose 200ms even when the current
        # sampled weapon was already ready. Renew only its grip lease.
        live.input([channel(t) for t in ready_inputs],.03,lease_seconds=3.)
        live.events.emit('combat_ready_lease_renewed',state=state,readiness_wait_seconds=0.)
        return state
    live.input([channel(t) for t in ready_inputs],.25,lease_seconds=3.)
    # The real WU bolt cycle in 20261004T152244356757Z suppressed its solved
    # muzzle beyond 1.3 s, then returned. Keep the native manipulation gate;
    # wait within the existing independent 3 s grip lease instead of aborting.
    deadline=clock()+2.8;since=None
    while clock()<deadline:
        raised,_=admit(live,anchor,**({'witnessed_hospital_ride':True} if witnessed_hospital_ride else {}))
        if healthy_life is not None and needs_recovery(raised['native'],healthy_life):
            raise HealthRecoveryNeeded('Damage takes priority while waiting for the native weapon cycle')
        audit=held_input_evidence(raised,ready_inputs,after_sample_ms=state['now_ms'])
        if (audit and raised['rendered'].get('native_firearm') is True
            and raised['controls'].get('native_triggers')==[255,0]
            and raised['rendered'].get('weapon_native_observer',{}).get('muzzle_solved') is True):
            since=audit['sample_ms'] if since is None else since
            if audit['sample_ms']-since>=200:
                # A long native bolt cycle can consume most of the first grip
                # lease. Renew this modifier before starting the aim/capture
                # phase; there is still no trigger input or unknown replay.
                live.input([channel(t) for t in ready_inputs],.25,lease_seconds=3.)
                return raised
        else:since=None
        sleep(.025)
    raise BotFault('Ready input did not establish the native firearm and authored muzzle')


def face_and_aim(live, actor, anchor, pitch, *, pair, support_pair, ready_inputs, max_seconds=2.8):
    """Look and raise the barrel before judging its current terrain visibility.

    A lowered muzzle can be inside terrain while the player can raise it.
    This read-only probe may aim at an obstructed capsule; fire() still requires
    a fresh clear ray and solved barrel alignment before any trigger input.
    """
    state,_=admit(live,anchor)
    target=actor['sight']['aim']
    head=live.text_result(live.call('get_head_pose',{'base_space':'local'}))['pose']
    head['orientation']=looking_head(head['orientation'],state['rendered']['native_pose'],target)
    live.call('set_head_pose',{'base_space':'local',**head})
    # The head RPC moves simulated controllers too. Re-establish the existing
    # separate grip/aim relationship at a comfortable VIEW-space palm position.
    grip_position=comfortable_grip(state['native'])
    deadline=time.monotonic()+max_seconds;stable=0;attitude=None
    while time.monotonic()<deadline:
        data,state=sight(live,anchor)
        target=next((t for t in data['targets'] if t['id']==actor['id']),None)
        if not target or not target['query_valid']:
            raise AimProbeUnavailable('Target lost its verified native geometry during the aim probe')
        if facing_error(state['rendered']['native_pose'],target['aim'])>5.:
            head['orientation']=looking_head(head['orientation'],state['rendered']['native_pose'],target['aim'])
            live.call('set_head_pose',{'base_space':'local',**head})
            time.sleep(.04)
            data,state=sight(live,anchor)
            target=next((t for t in data['targets'] if t['id']==actor['id']),None)
            if not target or not target['query_valid']:raise AimProbeUnavailable('Target lost geometry after looking')
            attitude=None
        attitude=(aim_quaternion(state['rendered']['native_pose'],data['muzzle']['position'],target['aim'],pitch)
                  if attitude is None else corrected_aim(state['rendered']['native_pose'],data['muzzle'],target['aim'],attitude))
        support_frame=aimed_support_frame(state['rendered'],grip_position,attitude,pair)
        # Prepare both from the old publication, then move them together.
        # Never resample a socket constrained by a left hand we have not moved.
        live.execute({'op':'pose','hand':'right','position':grip_position,
                      'orientation':normalized(mul(attitude,pair['orientation'])),
                      'aim_position':paired_aim_position(grip_position,attitude,pair),'aim_orientation':attitude})
        place_support_hand(live,support_frame,support_pair)
        live.input([channel(t) for t in ready_inputs],.25,lease_seconds=3.)
        time.sleep(.04)
        checked,checked_state=sight(live,anchor)
        latest=next((t for t in checked['targets'] if t['id']==actor['id']),None)
        if latest is None or not latest['query_valid']:raise AimProbeUnavailable('Target geometry became unavailable during aim')
        along,lateral=aim_error(checked['muzzle'],latest['aim'])
        facing=facing_error(checked_state['rendered']['native_pose'],latest['aim'])
        live.events.emit('combat_aim_sample',target_id=actor['id'],along=along,lateral=lateral,
                         muzzle=checked['muzzle'],target=latest['aim'],view_target_angle_degrees=facing,
                         native_view=checked_state['rendered']['native_pose'],
                         native_body_yaw=checked_state['native']['player_yaw'],
                         support_attached=checked_state['rendered'].get('weapon_support_attached'),
                         support_grip=checked_state['rendered'].get('weapon_support_grip'))
        stable=stable+1 if (along>1 and lateral<=.18 and facing<=15.
                           and checked_state['rendered'].get('weapon_support_attached') is True) else 0
        if stable>=2:return checked,latest
    raise AimProbeUnavailable('The actual VR view and barrel did not converge on the native target capsule')


def fire(live, actor, anchor, ready_inputs, *, seconds=.20, healthy_life=None, expected_slot=None):
    state,_=admit(live,anchor)
    if healthy_life is not None and needs_recovery(state['native'],healthy_life):
        raise HealthRecoveryNeeded('Damage or a critical wound takes priority over firing')
    alive,_=read_actor(live,actor)
    if alive['life']!=0:return {'status':'already_incapacitated','actor_life_after':alive['life'],
                              'target_id':actor['id'],'target_name':actor['name'],'rounds':0}
    geometry,state=sight(live,anchor)
    target=next((row for row in geometry['targets'] if row['id']==actor['id']),None)
    if target is None or not target['query_valid'] or not target['unoccluded']:
        raise AimProbeUnavailable('Target lost visibility at the final trigger admission')
    along,lateral=aim_error(geometry['muzzle'],target['aim'])
    if along<=1 or lateral>.18:
        raise AimProbeUnavailable('Actual barrel/target alignment changed before trigger dispatch')
    facing=facing_error(state['rendered']['native_pose'],target['aim'])
    if facing>15.:
        raise BotFault('Player view faces away from the actual firing target; trigger refused')
    if state['rendered'].get('weapon_support_attached') is not True:
        raise AimProbeUnavailable('Authored support hand detached before trigger dispatch')
    before={slot:state['native']['ammo_in_weapon_'+slot]
            for slot in ('PRIMARY_HIP','PRIMARY_BACK','SECONDARY')}
    if any(type(v) not in (int,float) or v<0 for v in before.values()):raise BotFault('Native ammunition is unavailable')
    # Keep the already sampled 3-second ready/support leases independent.
    # Reissuing grips with the trigger's short expiry dropped both hands
    # during the subsequent native bolt/tranquilizer wait.
    values=[channel('right_trigger')]
    sampled=False;started=time.monotonic();deadline=started+seconds+.12
    try:
        live.input(values,seconds,lease_seconds=seconds+.12)
        while time.monotonic()<deadline:
            held,_=admit(live,anchor)
            audit=held_input_evidence(held,ready_inputs+['right_trigger'],after_sample_ms=state['now_ms'])
            if audit and held['controls'].get('native_triggers')==[255,255]:sampled=True
            if sampled and time.monotonic()-started>=seconds:break
            time.sleep(.015)
    finally:
        # No pose/capture RPC is allowed while a fire trigger is held.
        live.call('set_controller_input', {**channel('right_trigger'),'value':0,'auto_release':False})
        live.held.pop(('right','Trigger',None),None)
    if not sampled:raise BotFault('VR trigger/ready input was not sampled; action is ambiguous')
    live.input([channel(t) for t in ready_inputs],.03,lease_seconds=3.)
    after,_=admit(live,anchor)
    deltas={slot:before[slot]-after['native']['ammo_in_weapon_'+slot] for slot in before}
    changed=[slot for slot,value in deltas.items() if value>0]
    if len(changed)!=1 or any(v<0 for v in deltas.values()):
        raise BotFault('Fire input did not establish one native ammunition discharge')
    if expected_slot is not None and changed!=[expected_slot]:
        raise BotFault('Actual discharge belongs to a different slot than the reviewed stealth weapon')
    outcome,_=read_actor(live,actor)
    if expected_slot=='SECONDARY' and outcome['life']==0:
        # Let a real tranquilizer result settle before spending another round.
        until=time.monotonic()+2.
        while time.monotonic()<until:
            resting,_=admit(live,anchor)
            if healthy_life is not None and needs_recovery(resting['native'],healthy_life):break
            outcome,_=read_actor(live,actor)
            if outcome['life']!=0:break
            time.sleep(.05)
    result={'status':'observed_discharge','target_id':actor['id'],'target_name':actor['name'],
            'slot':changed[0],'rounds':deltas[changed[0]],'before':before,'after':{
                slot:after['native']['ammo_in_weapon_'+slot] for slot in before},
            'actor_life_after':outcome['life'],'player_life':after['native']['player_life'],
            'view_target_angle_degrees':facing,'barrel_lateral_error':lateral,
            'native_body_yaw':state['native']['player_yaw'],
            'support_attached_before_fire':state['rendered']['weapon_support_attached'],
            'support_grip_before_fire':state['rendered'].get('weapon_support_grip')}
    live.events.emit('combat_burst',**result)
    return result


def track_and_fire(live, actor, anchor, pitch, *, pair, support_pair, ready_inputs,
                   healthy_life=None, expected_slot=None, attempts=3):
    """Reacquire a walking guard before dispatch; never replay a trigger."""
    for attempt in range(attempts):
        face_and_aim(live,actor,anchor,pitch,pair=pair,
                     support_pair=support_pair,ready_inputs=ready_inputs)
        try:
            return fire(live,actor,anchor,ready_inputs,
                        seconds=.12 if expected_slot=='SECONDARY' else .20,
                        healthy_life=healthy_life,expected_slot=expected_slot)
        except AimProbeUnavailable as error:
            live.events.emit('combat_reacquire_before_trigger',target_id=actor['id'],
                             attempt=attempt+1,error=str(error),input_replayed=False)
            if attempt+1==attempts:raise


def battle_checkpoint(live, label, state):
    recording=getattr(live,'held_action_recording',None)
    if state.get('native',{}).get('not_alert') is False and recording is not None:
        recording.check()
        live.events.emit('battle_video_checkpoint',label=label,state=state,
                         source='native_source_eye_before_runtime_crop_and_composition',
                         final_compositor_captured=False,visual_acceptance='pending_video_review')
        return []
    return live.capture(label)


def engage(live, *, output, enemy_script, camp=None, max_seconds=30., max_rounds=24, radius=80.,
           healthy_life=None, expected_slot=None, target_name=None):
    """A bounded autonomous combat slice; no blanket game/visual acceptance."""
    if not 1<=max_seconds<=120 or type(max_rounds) is not int or not 1<=max_rounds<=100 or not 1<=radius<=180:
        raise BotFault('Combat requires bounded time, rounds and range')
    if target_name is not None and (not isinstance(target_name, str)
            or not re.fullmatch(r'sol_[A-Za-z0-9_]+', target_name)):
        raise BotFault('Combat target must be one exact native soldier name')
    pitch=effective_pitch(live.bindings)
    ready=next((a for a in live.bindings['actions'] if a['name']=='gameplay.ready_weapon'),None)
    support=next((a for a in live.bindings['actions'] if a['name']=='gameplay.support_grip'),None)
    trigger=next((a for a in live.bindings['actions'] if a['name']=='gameplay.fire_or_cqc'),None)
    if (not ready or not ready.get('modifier') or not ready.get('bindings') or not trigger or not trigger.get('bindings')
        or ready['bindings'][0]['gesture']!='level' or not set(ready['bindings'][0]['inputs']).issubset({'left_grip','right_grip'})
        or trigger['bindings'][0]['inputs']!=['right_trigger'] or trigger['bindings'][0]['gesture']!='level'):
        raise BotFault('Combat requires effective grip-ready and right-trigger bindings')
    if (not support or not support.get('modifier') or not support.get('bindings')
        or support['bindings'][0]['gesture']!='level'
        or set(ready['bindings'][0]['inputs']+support['bindings'][0]['inputs'])!={'left_grip','right_grip'}):
        raise BotFault('Two-hand combat requires the effective ready and support grip bindings')
    ready_inputs=list(dict.fromkeys(ready['bindings'][0]['inputs']+support['bindings'][0]['inputs']))
    result={'status':'running','bursts':[],'outcomes':[],'captures':[],
            'visual_acceptance':'pending_both_eye_review','max_rounds':max_rounds,'camp':camp,'target_name':target_name}
    started=time.monotonic();rounds=0;attempts={};last_target=None
    try:
        with preserved_tracking_poses(live):
            live.release();state,anchor=admit(live)
            from .session import _controller_pose
            pair=controller_pair(_controller_pose(live,'right','grip','local'),
                                 _controller_pose(live,'right','aim','local'))
            support_pair=controller_pair(_controller_pose(live,'left','grip','local'),
                                         _controller_pose(live,'left','aim','local'))
            result['controller_pair']=pair
            result['support_controller_pair']=support_pair
            result['before']=state
            result['captures']+=battle_checkpoint(live,'combat-before',state)
            while time.monotonic()-started<max_seconds and rounds<max_rounds:
                if (output/'STOP').exists():result['status']='stopped';break
                state,_=admit(live,anchor)
                if healthy_life is not None and needs_recovery(state['native'],healthy_life):
                    raise HealthRecoveryNeeded('Damage or a critical wound takes priority over combat')
                arm(live,ready_inputs,anchor,healthy_life=healthy_life)
                lease,_=live.native.read('diagnostics:combat-sight:3000')
                if lease.get('armed') is not True:raise BotFault('Native visibility lease was not armed')
                time.sleep(.08)
                enemies=read_enemy_snapshot(live.native.read,enemy_script)
                if (enemies['mission'],enemies['location'])!=anchor[:2]:raise BotFault('Enemy world changed')
                data,state=sight(live,anchor)
                candidates=active_targets(enemies,data,camp=camp,radius=radius,require_clear=False,target_name=target_name)
                if not candidates:
                    result['status']='no_visible_active_target';break
                actor=None
                for candidate in candidates:
                    if attempts.get(candidate['id'],0)>=4:raise BotFault('Target survived four verified bursts; retain aim/hit failure')
                    arm(live,ready_inputs,anchor,healthy_life=healthy_life)
                    lease,_=live.native.read('diagnostics:combat-sight:3000')
                    if lease.get('armed') is not True:raise BotFault('Native aim-probe visibility lease was not armed')
                    try:
                        checked,target=face_and_aim(live,candidate,anchor,pitch,pair=pair,
                            support_pair=support_pair,ready_inputs=ready_inputs)
                    except AimProbeUnavailable as error:
                        # Retain the actual attempted view/gun pose before
                        # preserved_tracking_poses restores the caller's pose.
                        arm(live,ready_inputs,anchor,healthy_life=healthy_life)
                        result['captures']+=live.capture('combat-aim-failed-'+str(candidate['id']),static_grips=True)
                        live.events.emit('combat_probe_unavailable',target_id=candidate['id'],error=str(error))
                        continue
                    arm(live,ready_inputs,anchor,healthy_life=healthy_life)
                    result['captures']+=live.capture('combat-aim-probe-'+str(candidate['id']),static_grips=True)
                    if target['unoccluded'] is True:
                        actor=candidate;break
                    live.events.emit('combat_probe_obstructed',target_id=candidate['id'],
                                     muzzle=checked['muzzle'],target=target)
                if actor is None:
                    result['status']='no_visible_active_target';break
                lease,_=live.native.read('diagnostics:combat-sight:3000')
                if lease.get('armed') is not True:raise BotFault('Post-capture visibility lease was not armed')
                # Actual run 165246 lost a walking guard during a redundant
                # 200 ms readiness wait after aiming. face_and_aim renews the
                # grip lease and checks attached hands; reacquire immediately
                # before fire instead of waiting on that already-ready state.
                try:
                    burst=track_and_fire(live,actor,anchor,pitch,pair=pair,
                        support_pair=support_pair,ready_inputs=ready_inputs,
                        healthy_life=healthy_life,expected_slot=expected_slot)
                except AimProbeUnavailable as error:
                    live.events.emit('combat_probe_unavailable',target_id=actor['id'],
                                     error=str(error),before_trigger=True,input_replayed=False)
                    result['status']='no_visible_active_target';break
                result['bursts'].append(burst)
                burst=result['bursts'][-1];rounds+=burst.get('rounds',0)
                attempts[actor['id']]=attempts.get(actor['id'],0)+1
                last_target=actor
                # Confirm a life transition on two separate correlated reads.
                first,_=read_actor(live,actor);time.sleep(.10);second,_=read_actor(live,actor)
                if first['life']==second['life'] and second['life'] in (1,2,3,4):
                    preceding=sum(b.get('rounds',0) for b in result['bursts'] if b['target_id']==actor['id'])
                    result['outcomes'].append({'target_id':actor['id'],'name':actor['name'],
                        'life':second['life'],'preceding_rounds':preceding,
                        'scope':('native_incapacitation_after_vr_discharge; buddy attribution not excluded'
                                 if preceding else 'native_incapacitation_without_bot_discharge')})
                live.events.emit('combat_progress',rounds=rounds,outcomes=len(result['outcomes']),seconds=time.monotonic()-started)
                atomic_json(output/'progress.json',result)
            else:result['status']='budget_reached'
            live.release();live.native.read('diagnostics:combat-sight:0')
            result['after']=live.observe(native=True)
            result['captures']+=battle_checkpoint(live,'combat-after',result['after'])
    except HealthRecoveryNeeded as error:
        result['status']='needs_health_recovery';result['reason']=str(error)
    except Exception as error:
        result['status']='failed';result['error']=str(error)
    finally:
        try:live.release();live.native.read('diagnostics:combat-sight:0')
        except Exception as error:result['cleanup_error']=str(error);result['status']='failed'
        if result['status']=='failed' and 'cleanup_error' not in result:
            try:
                result['after']=live.observe(native=True)
                result['captures']+=live.capture('combat-failed-released')
            except Exception as error:result['failure_capture_error']=str(error)
        result['seconds']=time.monotonic()-started;result['rounds']=rounds
        atomic_json(output/'result.json',result)
    return result
