"""Follow witnessed hospital routes using ordinary physical XR movement.

The retail rail owns the route and collision. This executor reads its active
state and Snake's heading; free following reads the real Ishmael actor and
explicit candidate waypoints. Neither path enables rails or writes transforms.
"""
import math
import time

from .combat import RigPublicationDelay, fresh_rig, looking_head
from .core import BotFault
from .locomotion import _axis_source, _distance, _position, _validate_field_state
from .locomotion import _validate_xr_field_state


RAIL_STATE = """return string.format(
    '{"mission":%d,"location":%d,"rail_active":%s}',
    vars.missionCode, vars.locationCode,
    tostring(mvars.isNotFinishedRailAction==true))"""


ISHMAEL_STATE = """local id=GameObject.GetGameObjectId("ishmael")
local p=GameObject.SendCommand(id,{id="GetPosition"})
return string.format('{"mission":%d,"location":%d,"exists":%s,"id":%d,"position":[%.6f,%.6f,%.6f],"movement_locked":%s}',
    vars.missionCode,vars.locationCode,tostring(mvars.ishmaelExists==true),id,p:GetX(),p:GetY(),p:GetZ(),
    tostring(mvars.prohibitMoveEnabled==true))"""


def _ishmael_state(live):
    actor,_=live.native.read(ISHMAEL_STATE)
    point=actor.get('position')
    if (actor.get('mission')!=10010 or actor.get('location')!=30 or actor.get('exists') is not True
            or type(actor.get('id')) is not int or not 0<actor['id']<65535
            or not isinstance(point,list) or len(point)!=3
            or any(type(v) not in (int,float) or not math.isfinite(v) for v in point)):
        raise BotFault('The owned native Ishmael actor is unavailable; no movement renewed')
    if type(actor.get('movement_locked')) is not bool:
        raise BotFault('The native hospital movement lock is unavailable')
    return actor


def _hospital_movement_lock(live):
    state,_=live.native.read("""return string.format(
        '{"mission":%d,"location":%d,"movement_locked":%s}',
        vars.missionCode,vars.locationCode,tostring(mvars.prohibitMoveEnabled==true))""")
    if (state.get('mission')!=10010 or state.get('location')!=30
            or type(state.get('movement_locked')) is not bool):
        raise BotFault('The native hospital movement owner is unavailable')
    return state


def probe_stick(live, axis, value, seconds=.6):
    """Measure one bounded physical stick response in the witnessed stair action."""
    if (axis not in ('X','Y') or type(value) not in (int,float) or not math.isfinite(value)
            or not .6<=abs(value)<=.8 or type(seconds) not in (int,float)
            or not math.isfinite(seconds) or not .2<=seconds<=.8):
        raise BotFault('Hospital motor probe requires one bounded physical stick axis')
    live.release()
    before,anchor=fresh_rig(live)
    native=before['native']
    if (native.get('mission')!=10010 or native.get('location')!=30 or native.get('story')!=0
            or native.get('sequence') not in ('Seq_Game_Stairway','Seq_Game_AfterCorridor')
            or before['controls'].get('travel_mode')!=4):
        raise BotFault('Hospital motor probe requires the actual scripted standing action')
    _validate_field_state(before,witnessed_hospital_route=True)
    if _ishmael_state(live)['movement_locked']:
        raise BotFault('Native movement lock prevents the motor probe')
    source=_axis_source(live.bindings,'axes.move')
    hand='left' if source=='left_stick' else 'right'
    channels=[{'hand':hand,'component':'Thumbstick','sub_component':a,
               'value':value if a==axis else 0.} for a in ('X','Y')]
    started=time.monotonic()
    try:
        live.input(channels,seconds,lease_seconds=1.)
        while time.monotonic()-started<seconds:
            after,_=fresh_rig(live,anchor)
            if after['native']['sequence']!=native['sequence']:
                raise BotFault('Hospital motor probe reached a new native scene; input released')
            if after['native'].get('status_NORMAL_ACTION') is True:
                # The same scene can leave its scripted action during this
                # pulse. Release before comparing a new Lua state with the
                # previous XR travel-mode sample; never replay the pulse.
                break
            _validate_field_state(after,witnessed_hospital_route=True)
            time.sleep(.025)
    finally:
        live.release()
    time.sleep(.15)
    after=live.observe(native=True)
    result={'axis':axis,'value':value,'seconds':seconds,
            'before_position':_position(before),'after_position':_position(after),
            'before_yaw':native.get('player_yaw'),'after_yaw':after['native'].get('player_yaw'),
            'before_camera_yaw':native.get('player_camera_yaw'),
            'after_camera_yaw':after['native'].get('player_camera_yaw'),
            'displacement':_distance(_position(before),_position(after))}
    live.events.emit('hospital_motor_probe',**result)
    return result


def corridor_cue(live, sequence, seconds=.6):
    """Advance an ended retail corridor segment through its actual VR stick cue."""
    allowed={'Seq_Demo_Corridor'+str(i) for i in range(1,8)}
    if sequence not in allowed or type(seconds) not in (int,float) or not .2<=seconds<=1.:
        raise BotFault('Corridor cue needs one witnessed segment and bounded input')
    live.release()
    before=live.observe(native=True)
    native=before['native']
    _validate_xr_field_state(before)
    if (native.get('mission')!=10010 or native.get('location')!=30 or native.get('story')!=0
            or native.get('sequence')!=sequence or native.get('demo') is not False
            or native.get('demo_nonplayable') is not False
            or native.get('status_NORMAL_ACTION') is not True or native.get('status_PARTS_ACTIVE') is not True
            or native.get('game_over')!=0 or native.get('popup') is not False or native.get('saving') is not False):
        raise BotFault('The native corridor scene has not returned player control')
    gate,_=live.native.read('return string.format("{\\"can_skip\\":%s}",tostring(mvars.canSkip==true))')
    if gate.get('can_skip') is not True:
        raise BotFault('The owned corridor movement gate is not ready')
    source=_axis_source(live.bindings,'axes.move')
    move={'hand':'left' if source=='left_stick' else 'right','component':'Thumbstick','sub_component':'Y','value':.8}
    started=time.monotonic()
    try:
        live.input([move],seconds,lease_seconds=1.2)
        while time.monotonic()-started<seconds:
            after=live.observe(native=True)
            if after['native'].get('sequence')!=sequence:
                break
            _validate_xr_field_state(after)
            time.sleep(.025)
    finally:
        live.release()
    live.events.emit('hospital_corridor_move_cue',sequence=sequence,seconds=time.monotonic()-started,
                     native_sequence_written=False)


def follow_rail(live, seconds=30., *, ishmael=False, waypoints=None,
                sequence='Seq_Game_EnterSmokeRoom', stop_at_last_waypoint=False,
                event_manifest=None, event=None,
                clock=time.monotonic, sleep=time.sleep):
    limit = 120. if event_manifest is not None and event is not None else 45.
    if type(seconds) not in (int, float) or not math.isfinite(seconds) or not 1. <= seconds <= limit:
        raise BotFault(f'Hospital route needs a bounded 1..{int(limit)} second duration')
    live.release()
    initial, anchor = fresh_rig(live, clock=clock, sleep=sleep)
    native = initial['native']
    waypoints=[] if waypoints is None else waypoints
    if (not isinstance(waypoints,list) or len(waypoints)>4 or (waypoints and not ishmael)
            or any(not isinstance(point,list) or len(point)!=3
                   or any(type(v) not in (int,float) or not math.isfinite(v) for v in point)
                   or _distance(_position(initial),point)>10 for point in waypoints)):
        raise BotFault('Hospital route needs at most four bounded, explicitly witnessed waypoints')
    waypoint_index=0
    if type(stop_at_last_waypoint) is not bool or (stop_at_last_waypoint and not waypoints):
        raise BotFault('Stopping at the authored endpoint requires an explicit waypoint route')
    identity = tuple(native.get(k) for k in ('mission', 'location', 'sequence', 'story'))
    if (identity[:2] != (10010, 30) or identity[3] != 0
            or not isinstance(identity[2],str) or not identity[2].startswith('Seq_Game_')):
        raise BotFault('Hospital rail follow requires the witnessed campaign owner')
    if ishmael:
        prone_required = sequence in ('Seq_Game_EnterSmokeRoom','Seq_Game_EscapeFromSmokeRoom')
        if (not isinstance(sequence,str) or not sequence.startswith('Seq_Game_')
                or identity[2]!=sequence
                or not any(native.get('status_'+posture) is True for posture in ('CRAWL','SQUAT','STAND'))
                or (prone_required and native.get('status_CRAWL') is not True)):
            raise BotFault('Free hospital following requires its exact witnessed movement owner and posture')
        corpse_crawl = (sequence=='Seq_Game_AfterCurtainRoom'
                        and native.get('status_CRAWL') is True
                        and native.get('status_NORMAL_ACTION') is True
                        and native.get('status_PARTS_ACTIVE') is True)
        if native.get('status_INTRUDE') is True and not corpse_crawl:
            raise BotFault('Clear the native constrained crawl before following a free hospital route')
        # The entrance legitimately hides Ishmael while the player sneaks.
        # A route through owned NAV2 still obeys the native movement lock;
        # it does not depend on an actor it is not following.
        independent_path = event_manifest is not None or (waypoints and stop_at_last_waypoint)
        _hospital_movement_lock(live) if independent_path else _ishmael_state(live)
    else:
        rail, _ = live.native.read(RAIL_STATE)
        if rail != {'mission': 10010, 'location': 30, 'rail_active': True}:
            raise BotFault('The retail hospital rail is not active; no movement dispatched')
    _validate_field_state(initial,witnessed_hospital_rail=not ishmael,witnessed_smoke_corridor=ishmael,
                          witnessed_hospital_route=ishmael)

    route = None
    if event_manifest is not None or event is not None:
        if (not ishmael or waypoints or stop_at_last_waypoint
                or not isinstance(event_manifest, str) or not isinstance(event, str)):
            raise BotFault('Hospital A* routing requires one exact owned event, without manual waypoints')
        from .hospital_navigation import HospitalEventRoute
        route = HospitalEventRoute(event_manifest, event, sequence, native)
        live.events.emit('hospital_event_route_planned', **route.evidence())

    head = live.text_result(live.call('get_head_pose', {'base_space': 'local'}))['pose']
    hands = []
    for side in ('left', 'right'):
        for kind in ('grip', 'aim'):
            args = {'hand': side, 'pose_type': kind}
            local = live.text_result(live.call('get_controller_pose', {**args, 'base_space': 'local'}))['pose']
            view = live.text_result(live.call('get_controller_pose', {**args, 'base_space': 'view'}))['pose']
            hands.append((args, local, view))
    current_head = dict(head)
    source = _axis_source(live.bindings, 'axes.move')
    move = {'hand': 'left' if source == 'left_stick' else 'right',
            'component': 'Thumbstick', 'sub_component': 'Y', 'value': .8}
    move_x = {**move, 'sub_component': 'X', 'value': 0.}
    started = checkpoint_time = clock()
    checkpoint = _position(initial)
    renew = report = 0.
    samples = recoveries = posture_requests = 0
    native_lock_started = None
    actor_wait_started = None
    progress_target = None
    best_target_distance = math.inf
    travel_join_started = None
    crawl_turn_started = None
    crawl_turns = 0
    crawl_turn_key = None
    crawl_turn_target = None
    reason = 'bounded_segment_complete'
    latest = initial
    live.events.emit('hospital_rail_started', state=initial, seconds=seconds,
                     route_owner=('owned_native_event_route' if route else 'authored_waypoints' if waypoints
                                  else 'native_ishmael_position' if ishmael else 'retail_hospital_rail'),
                     transform_writes=False)
    try:
        while clock() - started < seconds:
            latest = live.observe(native=True)
            now = clock()
            observed = latest.get('native', {})
            current_identity = tuple(observed.get(k) for k in ('mission', 'location', 'sequence', 'story'))
            if current_identity[:2] != identity[:2] or current_identity[3] != identity[3]:
                raise BotFault('Hospital mission/location/story changed; movement released')
            if (current_identity[2] != identity[2] or observed.get('demo') is True
                    or latest.get('demo') is True):
                reason = 'native_scene_transition'
                break
            try:
                latest, _ = fresh_rig(live, anchor, clock=clock, sleep=sleep)
            except RigPublicationDelay:
                live.release()
                recoveries += 1
                live.events.emit('hospital_rail_publication_delay', state=latest,
                                 recoveries=recoveries, input_replayed=False)
                if recoveries > 2:
                    raise
                # Reacquire an actually fresh frame before renewing movement.
                # Focus/owner/mission faults never enter this delay recovery.
                sleep(.35)
                checkpoint_time = clock()
                renew = 0.
                continue
            observed = latest['native']
            # A retail scene can start during fresh-rig acquisition. Release
            # normally at that handoff instead of rejecting its new owner as
            # an invalid gameplay frame.
            current_identity = tuple(observed.get(k) for k in ('mission','location','sequence','story'))
            if current_identity[:2] != identity[:2] or current_identity[3] != identity[3]:
                raise BotFault('Hospital mission/location/story changed; movement released')
            if current_identity[2] != identity[2] or observed.get('demo') is True or latest.get('demo') is True:
                reason = 'native_scene_transition'
                break
            if (identity[2] in ('Seq_Game_Stairway','Seq_Game_AfterCorridor') and ishmael
                    and observed.get('status_NORMAL_ACTION') is True
                    and observed.get('status_STAND') is True
                    and latest.get('controls',{}).get('travel_mode')==4):
                # Native standing actions finish between the Lua observation
                # and XR audit. Release until the same owner publishes its
                # new on-foot mode; do not steer with the preceding mode.
                live.release()
                if travel_join_started is None:
                    travel_join_started=now
                    live.events.emit('hospital_travel_mode_join_wait',input_replayed=False)
                if now-travel_join_started>1.:
                    raise BotFault('Hospital standing action did not publish its on-foot mode')
                renew=0.
                checkpoint_time=now
                sleep(.05)
                continue
            travel_join_started=None
            _validate_field_state(latest,witnessed_hospital_rail=not ishmael,witnessed_smoke_corridor=ishmael,
                                  witnessed_hospital_route=ishmael)
            actor = (_hospital_movement_lock(live) if route or (waypoints and stop_at_last_waypoint)
                     else _ishmael_state(live)) if ishmael else None
            if actor and actor['movement_locked']:
                if native_lock_started is None:
                    live.release()
                    native_lock_started = now
                    live.events.emit('hospital_native_movement_lock',position=_position(latest),input_released=True)
                if now-native_lock_started > 15.:
                    raise BotFault('Native hospital movement lock exceeded its bounded wait')
                checkpoint, checkpoint_time = _position(latest), now
                sleep(.1)
                continue
            if native_lock_started is not None:
                live.events.emit('hospital_native_movement_resumed',seconds=now-native_lock_started)
                native_lock_started = None
                renew = 0.
                checkpoint, checkpoint_time = _position(latest), now
            if ishmael and prone_required and observed.get('status_CRAWL') is False and observed.get('status_SQUAT') is True:
                live.release()
                posture_requests+=1
                if posture_requests>2:
                    raise BotFault('Hospital prone guide repeated; movement released for review')
                live.events.emit('hospital_route_prone_requested',state=latest,requests=posture_requests)
                live.execute({'op':'action','name':'gameplay.stance','seconds':1.2,
                              'native_before':{'mission':10010,'location':30,'story':0,
                                  'sequence':identity[2],'status_SQUAT':True,
                                  'status_CRAWL':False,'popup':False,'saving':False}})
                live.release()
                renew=0.
                checkpoint_time=clock()
                continue
            yaw = observed.get('player_yaw')
            camera = latest.get('rendered', {}).get('native_pose')
            if type(yaw) not in (int, float) or not math.isfinite(yaw) or not camera:
                raise BotFault('Hospital body heading or published camera is unavailable')
            # Keep the bot looking along the actual authored crawl direction.
            # Recompose from the published camera, which already includes HMD
            # motion, rather than accumulating a fixed turn at every corner.
            if ishmael:
                point=None if route or waypoint_index<len(waypoints) else actor['position']
                player=_position(latest)
                if route:
                    route.observe_posture(observed)
                    if route.inside(player):
                        live.release()
                        reason='owned_event_trap_entered'
                        break
                    point=route.target(player)
                    distance=math.dist([player[0],player[1]-route.root_offset,player[2]],point)
                    if progress_target!=tuple(point) or distance<best_target_distance-.08:
                        progress_target=tuple(point)
                        best_target_distance=distance
                        checkpoint_time=now
                    # Keep the complete native path at doorways. Slow near
                    # each node so an overshoot cannot cut the next corner.
                    move['value']=.8 if latest['controls'].get('travel_mode')==4 else min(.8,max(.4,distance*.65))
                elif waypoint_index<len(waypoints):
                    point=waypoints[waypoint_index]
                    if math.hypot(point[0]-player[0],point[2]-player[2])<.45:
                        waypoint_index+=1
                        live.events.emit('hospital_route_waypoint_reached',index=waypoint_index,position=player)
                        if waypoint_index==len(waypoints) and stop_at_last_waypoint:
                            reason='authored_waypoints_reached'
                            break
                        continue
                elif (math.hypot(point[0]-player[0],point[2]-player[2])<.7
                      and abs(point[1]-player[1])<1.2):
                    live.release()
                    if actor_wait_started is None:
                        actor_wait_started=now
                        live.events.emit('hospital_actor_dialogue_wait',position=player,input_released=True)
                    if now-actor_wait_started>=8.:
                        reason='native_actor_reached'
                        break
                    checkpoint_time=now
                    renew=0.
                    sleep(.1)
                    continue
                if actor_wait_started is not None:
                    live.events.emit('hospital_actor_follow_resumed',seconds=now-actor_wait_started)
                    actor_wait_started=None
                    checkpoint_time=now
                if not route:
                    distance=math.hypot(point[0]-player[0],point[2]-player[2])
                    target_key=('waypoint',waypoint_index) if waypoint_index<len(waypoints) else ('ishmael',actor['id'])
                    if progress_target!=target_key or distance<best_target_distance-.08:
                        progress_target=target_key
                        best_target_distance=distance
                        checkpoint_time=now
                # The observed corpse-corridor stall retained a backwards
                # prone animation despite the VR view facing the target.
                # Use the existing native crouched-turn recovery, then return
                # to crawl. All posture and motion still use VR bindings.
                if identity[2]=='Seq_Game_AfterCurtainRoom':
                    from .route import body_heading_error, set_posture
                    facing=body_heading_error(observed,player,point)
                    if crawl_turn_started is not None:
                        if facing is None:
                            raise BotFault('Hospital crawl-turn heading became unavailable')
                        if facing<=45. or (route and route.index!=crawl_turn_target):
                            live.release()
                            set_posture(live,'crawl',sleep=sleep)
                            live.events.emit('hospital_crawl_turn_finished',error_degrees=facing,
                                             seconds=clock()-crawl_turn_started)
                            crawl_turn_started=None
                            checkpoint_time=clock()
                            renew=0.
                            continue
                        if now-crawl_turn_started>2.:
                            raise BotFault('Hospital crouched turn did not align the native body')
                    elif (observed.get('status_CRAWL') is True and facing is not None and facing>100.
                          and observed.get('status_INTRUDE') is False):
                        live.release()
                        key=('node',route.index) if route else ('waypoint',waypoint_index)
                        if key!=crawl_turn_key:
                            crawl_turn_key,crawl_turns=key,0
                        crawl_turns+=1
                        if crawl_turns>2:
                            raise BotFault('Hospital waypoint exceeded two native crawl-turn recoveries')
                        set_posture(live,'crouch',sleep=sleep)
                        crawl_turn_started=clock()
                        crawl_turn_target=route.index if route else None
                        checkpoint_time=crawl_turn_started
                        renew=0.
                        live.events.emit('hospital_crawl_turn_started',error_degrees=facing,target=point)
                        continue
                # The eye sits ahead of the native actor root while crawling
                # or crouching. Looking AT a nearby floor waypoint reverses
                # steering as soon as the eye passes it, before the body has
                # arrived. Look along the root-to-waypoint travel direction.
                dx,dz=point[0]-player[0],point[2]-player[2]
                length=math.hypot(dx,dz)
                if length<1e-6:
                    raise BotFault('Hospital route has no horizontal travel direction')
                target=[camera['position'][0]+dx/length*5.,camera['position'][1],
                        camera['position'][2]+dz/length*5.]
            else:
                angle = math.radians(yaw)
                target = [camera['position'][0] + math.sin(angle), camera['position'][1],
                          camera['position'][2] + math.cos(angle)]
            # The camera includes the HMD pose from its own publication. The
            # preceding operator request may not have reached that frame yet.
            # Pair these poses instead of accumulating turns against old pixels.
            published_head = latest.get('rendered', {}).get('head_pose')
            if not published_head or not published_head.get('orientation'):
                raise BotFault('Hospital steering lacks its camera-associated head pose')
            current_head['orientation'] = looking_head(published_head['orientation'], camera, target)
            live.call('set_head_pose', {'base_space': 'local', **current_head})
            # This is a simulated body turn, so carry both tracked hands with
            # it. Independent head-look tests continue to keep hands in LOCAL.
            for args, _, view in hands:
                live.call('set_controller_pose', {**args, 'base_space': 'view', **view})
            if now >= renew:
                if not ishmael:
                    rail, _ = live.native.read(RAIL_STATE)
                    if rail != {'mission': 10010, 'location': 30, 'rail_active': True}:
                        reason='native_rail_finished'
                        break
                channels=[move]
                if route:
                    channels=[{**move_x,'value':0.},move]
                live.input(channels, 1., lease_seconds=1.)
                renew = now + .5
            samples += 1
            point = _position(latest)
            if not ishmael and _distance(checkpoint, point) >= .12:
                checkpoint, checkpoint_time = point, now
            elif now - checkpoint_time >= 4.:
                live.release()
                recoveries += 1
                live.events.emit('hospital_rail_no_progress', state=latest, recoveries=recoveries)
                if route:
                    try:
                        edge=route.obstructed(point)
                    except BotFault:
                        live.capture('hospital-route-blocked-facing-target')
                        raise
                    live.events.emit('hospital_event_route_replanned',blocked_edge=list(edge),
                                     position=point,**route.evidence())
                    progress_target=None
                    best_target_distance=math.inf
                    checkpoint_time=now
                    renew=0.
                    sleep(.15)
                    continue
                if ishmael:
                    live.events.emit('hospital_route_blocked',position=point,waypoint_index=waypoint_index,
                                     predicted_clearance='unknown',reason='no_native_progress')
                    live.capture('hospital-route-blocked-facing-target')
                    raise BotFault('Hospital route blocked; movement released for replanning')
                if recoveries > 2:
                    live.capture('hospital-rail-blocked')
                    raise BotFault('Hospital rail made no progress after two released recoveries')
                sleep(.35)
                checkpoint_time = clock()
                renew = 0.
            if now >= report:
                live.events.emit('hospital_rail_progress', position=point, heading=yaw,
                                 seconds=now-started, samples=samples, recoveries=recoveries)
                report = now + 2.
            sleep(.05)
    finally:
        try:
            live.release()
        finally:
            live.call('set_head_pose', {'base_space': 'local', **head})
            for args, local, _ in hands:
                live.call('set_controller_pose', {**args, 'base_space': 'local', **local})
    result = {'reason': reason, 'seconds': clock()-started, 'samples': samples,
              'recoveries': recoveries, 'posture_requests':posture_requests,
              'native_displacement': _distance(_position(initial), _position(latest)),
              'before_sequence': identity[2], 'after_sequence': latest['native']['sequence']}
    if route:
        result['event_route']=route.evidence()
    live.events.emit('hospital_rail_completed', **result)
    return result


def shoot_extinguisher(live):
    """Shoot the retail tutorial prop with the tracked hand and actual barrel.

    Its owned Locator supplies the target. Native ammunition establishes a discharge. The marker is an aiming prior,
    not a prop collision query; this does not certify a hit or modify a gimmick.
    """
    from .combat import (admit, arm, sight, controller_pair, comfortable_grip,
                         effective_pitch, aim_quaternion, corrected_aim, aim_error,
                         facing_error, mul, normalized, paired_aim_position)
    from .motion import _read
    live.release()
    state,anchor=admit(live)
    native=state['native']
    identity={'mission':10010,'location':30,'story':0,'sequence':'Seq_Game_GunTutorial',
              'weapon_SECONDARY':3,'popup':False,'saving':False}
    if any(native.get(k)!=v for k,v in identity.items()):
        raise BotFault('Extinguisher shot requires the actual hospital pistol lesson')
    ready=next((a for a in live.bindings['actions'] if a['name']=='gameplay.ready_weapon'),None)
    if (not ready or len(ready.get('bindings',[]))!=1 or not ready.get('modifier')
            or ready['bindings'][0]['gesture']!='level'
            or not set(ready['bindings'][0]['inputs']).issubset({'left_grip','right_grip'})):
        raise BotFault('Hospital shot requires the effective physical grip-ready binding')
    inputs=ready['bindings'][0]['inputs']
    target,_=live.native.read("""local b=DataIdentifier.GetDataBodyWithIdentifier(
        's10010_l01_sequence_DataIdentifier','extinguisher') local p=b.data.transform.translation
        return string.format('{"position":[%.6f,%.6f,%.6f]}',p:GetX(),p:GetY(),p:GetZ())""")
    target=target.get('position')
    if (not isinstance(target,list) or len(target)!=3
            or any(type(v) not in (int,float) or not math.isfinite(v) for v in target)
            or not 1.<_distance(_position(state),target)<20.):
        raise BotFault('The authored tutorial target is outside the observed local scene')
    grip=_read(live,'get_controller_pose',{'hand':'right','pose_type':'grip','base_space':'view'})
    aim=_read(live,'get_controller_pose',{'hand':'right','pose_type':'aim','base_space':'view'})
    pair=controller_pair(grip,aim)
    head=_read(live,'get_head_pose',{'base_space':'local'},local_head=True)
    pitch=effective_pitch(live.bindings)
    position=comfortable_grip(native)
    before_ammo=native['ammo_in_weapon_SECONDARY']
    rounds=0
    try:
        for attempt in range(1):
            arm(live,inputs,anchor)
            deadline=time.monotonic()+4.;stable=0;attitude=None
            while time.monotonic()<deadline:
                geometry,current=sight(live,anchor)
                frame=current['rendered']
                if facing_error(frame['native_pose'],target)>5.:
                    head['orientation']=looking_head(frame['head_pose']['orientation'],frame['native_pose'],target)
                    live.call('set_head_pose',{'base_space':'local',**head})
                    time.sleep(.04)
                    attitude=None
                    continue
                attitude=(aim_quaternion(frame['native_pose'],geometry['muzzle']['position'],target,pitch)
                          if attitude is None else corrected_aim(frame['native_pose'],geometry['muzzle'],target,attitude))
                live.execute({'op':'pose','hand':'right','position':position,
                    'orientation':normalized(mul(attitude,pair['orientation'])),
                    'aim_position':paired_aim_position(position,attitude,pair),'aim_orientation':attitude})
                live.input([{'hand':'left' if t=='left_grip' else 'right','component':'Grip','value':1.} for t in inputs],
                           .03,lease_seconds=3.)
                time.sleep(.06)
                checked,current=sight(live,anchor)
                along,lateral=aim_error(checked['muzzle'],target)
                stable=stable+1 if along>1. and lateral<.06 else 0
                live.events.emit('hospital_extinguisher_aim',target=target,along=along,lateral=lateral,
                                 muzzle=checked['muzzle'],attempt=attempt,stable=stable)
                if stable>=2:break
            if stable<2:
                raise BotFault('The actual hospital pistol barrel did not converge on the authored extinguisher')
            # Trigger input has no capture/pose RPC while held. The existing
            # action proves native readiness first and independently releases.
            current,_=admit(live,anchor)
            ammo=current['native']['ammo_in_weapon_SECONDARY']
            live.execute({'op':'action','name':'gameplay.fire_or_cqc','seconds':.12,
                          'include_ready':True,'native_before':{**identity,'ammo_in_weapon_SECONDARY':ammo}})
            after=live.observe(native=True)
            spent=ammo-after['native']['ammo_in_weapon_SECONDARY']
            if spent!=1:
                raise BotFault('Hospital shot did not establish exactly one native pistol discharge')
            rounds+=spent
            result={'rounds':rounds,'ammo_before':before_ammo,
                    'ammo_after':after['native']['ammo_in_weapon_SECONDARY'],
                    'target':target,'transform_writes':False,'prop_hit_proven':False,
                    'evidence_limit':'Authored locator and native discharge; no prop collision observation'}
            live.events.emit('hospital_pistol_aimed_discharge',**result)
            return result
    finally:
        live.release()


def horse_chase(live, seconds=90.):
    """Operate the real scripted shotgun without leaving a chase idle.

    The retail chase camera supplies a screen-center aiming prior. Actual
    barrel geometry closes the hand aim loop; native ammo proves discharge
    and reload. Screen center is not an enemy position or a hit observation.
    """
    from .combat import (admit, arm, sight, controller_pair, comfortable_grip,
                         effective_pitch, aim_quaternion, corrected_aim, aim_error,
                         normalized, mul, rotate, paired_aim_position)
    from .motion import _read
    from .session import preserved_tracking_poses
    if type(seconds) not in (int,float) or not math.isfinite(seconds) or not 1<=seconds<=120:
        raise BotFault('Horse chase needs a bounded 1..120 second duration')
    ready=next(a for a in live.bindings['actions'] if a['name']=='gameplay.ready_weapon')
    if (len(ready.get('bindings',[]))!=1 or not ready.get('modifier')
            or ready['bindings'][0]['gesture']!='level'
            or not set(ready['bindings'][0]['inputs']).issubset({'left_grip','right_grip'})):
        raise BotFault('Scripted chase requires the effective physical grip-ready binding')
    owner={'mission':10010,'location':30,'story':0,'sequence':'Seq_Game_VolginRide',
           'game_over':0,'popup':False,'saving':False}
    rounds=reloads=attempts=reload_attempts=0
    deadline=time.monotonic()+seconds
    with preserved_tracking_poses(live):
        live.release()
        initial,anchor=admit(live,witnessed_hospital_ride=True)
        if any(initial['native'].get(k)!=v for k,v in owner.items()):
            raise BotFault('Scripted chase requires the actual live VolginRide owner')
        grip=_read(live,'get_controller_pose',{'hand':'right','pose_type':'grip','base_space':'view'})
        aim=_read(live,'get_controller_pose',{'hand':'right','pose_type':'aim','base_space':'view'})
        pair=controller_pair(grip,aim);pitch=effective_pitch(live.bindings)
        from .live import channel, held_input_evidence
        ready_inputs=ready['bindings'][0]['inputs']
        ready_channels=[channel(t) for t in ready_inputs]
        keep={(v['hand'],v['component'],v.get('sub_component')) for v in ready_channels}
        def aiming_prior(current):
            frame=current['rendered'];camera=frame['native_pose']
            # +0x140 is an observed native focus point, not an enemy position.
            # The live chase placed it inside the handoff's near-field space.
            # Keep it diagnostic; it cannot certify an enemy aiming prior.
            forward=rotate(camera['orientation'],[0.,0.,20.])
            return [a+b for a,b in zip(camera['position'],forward)],'native_chase_view_center'
        # The native horse handoff owns an animated draw. A sampled LT packet
        # and solved muzzle exist before that animation finishes. Keep the
        # physical grip up; do not repeatedly restart the draw between pulls.
        arm(live,ready_inputs,anchor,witnessed_hospital_ride=True)
        draw_until=min(deadline,time.monotonic()+3.)
        while time.monotonic()<draw_until:
            admitted,_=admit(live,anchor,witnessed_hospital_ride=True)
            live.input(ready_channels,.2,lease_seconds=3.)
            time.sleep(.04)
        while time.monotonic()<deadline:
            state=live.observe(native=True);native=state['native']
            if native.get('game_over')!=0:
                raise BotFault('Native horse chase reached Game Over; no further action sent')
            if native.get('sequence')!=owner['sequence']:
                if (native.get('mission'),native.get('location'),native.get('sequence'))!=(10010,30,'Seq_Demo_Bridge'):
                    raise BotFault('Horse chase left its witnessed native bridge handoff')
                live.release()
                result={'rounds':rounds,'reloads':reloads,'trigger_pulls':attempts,
                        'reload_attempts':reload_attempts,'next_sequence':native.get('sequence'),
                        'hit_proven':False,'transform_writes':False}
                live.events.emit('hospital_horse_native_transition',**result)
                return result
            if tuple(native.get(k) for k in ('mission','location','story'))!=(10010,30,0):
                raise BotFault('Horse chase mission/location/story changed')
            state,_=admit(live,anchor,witnessed_hospital_ride=True)
            ammo=state['native']['ammo_in_weapon_PRIMARY_HIP']
            # The live shell reload reached seven including the chamber.
            if type(ammo) is not int or not 0<=ammo<=7:
                raise BotFault('Native scripted shotgun magazine is unavailable')
            if ammo<=4 or (attempts and attempts%6==0 and reload_attempts<attempts//6):
                live.release()
                live.execute({'op':'action','name':'gameplay.reload','seconds':.15,'native_before':owner})
                reload_attempts+=1
                until=min(deadline,time.monotonic()+2.5)
                while True:
                    current=live.observe(native=True);n=current['native']
                    if n.get('sequence')!=owner['sequence'] or n.get('game_over')!=0:
                        break
                    if n.get('ammo_in_weapon_PRIMARY_HIP',-1)>ammo:
                        reloads+=1
                        live.events.emit('hospital_horse_native_reload',ammo_before=ammo,
                            ammo_after=n['ammo_in_weapon_PRIMARY_HIP'],state=current)
                        break
                    if time.monotonic()>=until:
                        live.events.emit('hospital_horse_reload_unverified',ammo_before=ammo,
                            ammo_after=n.get('ammo_in_weapon_PRIMARY_HIP'),state=current)
                        break
                    time.sleep(.04)
                continue
            arm(live,ready_inputs,anchor,witnessed_hospital_ride=True)
            position=comfortable_grip(state['native']);attitude=None;converged=False
            for _ in range(4):
                geometry,current=sight(live,anchor,witnessed_hospital_ride=True);camera=current['rendered']['native_pose']
                target,prior=aiming_prior(current)
                attitude=(aim_quaternion(camera,geometry['muzzle']['position'],target,pitch)
                    if attitude is None else corrected_aim(camera,geometry['muzzle'],target,attitude))
                live.execute({'op':'pose','hand':'right','position':position,
                    'orientation':normalized(mul(attitude,pair['orientation'])),
                    'aim_position':paired_aim_position(position,attitude,pair),'aim_orientation':attitude})
                time.sleep(.04)
                checked,current=sight(live,anchor,witnessed_hospital_ride=True)
                # The horse moved during pose publication. This prior is the
                # current chase view, not a fixed point left behind in space.
                view=current['rendered']['native_pose']
                target,prior=aiming_prior(current)
                along,lateral=aim_error(checked['muzzle'],target)
                live.events.emit('hospital_horse_barrel_aim',target_prior=prior,
                    target=target,along=along,lateral=lateral,muzzle=checked['muzzle'],
                    muzzle_sample_ms=checked['sample_ms'],view_sample_ms=current['rendered']['sample_ms'])
                if along>1. and lateral/along<math.tan(math.radians(1.)):
                    converged=True;break
            if not converged:
                raise BotFault('Scripted shotgun barrel did not align with the native chase view')
            before=live.observe(native=True)['native']['ammo_in_weapon_PRIMARY_HIP']
            live.input(ready_channels,.2,lease_seconds=3.)
            admitted,_=admit(live,anchor,witnessed_hospital_ride=True)
            live.events.emit('semantic_action',action='gameplay.fire_or_cqc',
                binding={'gesture':'level','inputs':ready_inputs+['right_trigger']},
                duration=.25,retained_ready_grip=True)
            live.input([channel('right_trigger')],.25,lease_seconds=.55)
            pull_until=min(deadline,time.monotonic()+.5);first=None
            while True:
                current=live.observe(native=True);n=current['native']
                if not all(n.get(k)==v for k,v in owner.items()):
                    live.release();break
                audit=held_input_evidence(current,ready_inputs+['right_trigger'],
                    after_sample_ms=admitted['now_ms'])
                if audit and current['controls'].get('native_triggers')==[255,255]:
                    first=audit['sample_ms'] if first is None else first
                    if audit['sample_ms']-first>=200:break
                if time.monotonic()>=pull_until:
                    raise BotFault('Retained-grip chase pull was not sampled before trigger expiry')
                time.sleep(.015)
            live.release(keep=keep)
            attempts+=1
            until=min(deadline,time.monotonic()+.75)
            while True:
                current=live.observe(native=True);n=current['native']
                if (n.get('sequence')!=owner['sequence'] or n.get('game_over')!=0
                        or n.get('ammo_in_weapon_PRIMARY_HIP')!=before or time.monotonic()>=until):
                    break
                time.sleep(.025)
            if n.get('sequence')!=owner['sequence'] or n.get('game_over')!=0:
                continue
            spent=before-n['ammo_in_weapon_PRIMARY_HIP']
            if spent>0:
                rounds+=spent
                live.events.emit('hospital_horse_native_discharge',rounds=rounds,
                    ammo_before=before,ammo_after=n['ammo_in_weapon_PRIMARY_HIP'],state=current)
            else:
                live.events.emit('hospital_horse_discharge_unverified',trigger_pulls=attempts,
                    ammo_before=before,ammo_after=n['ammo_in_weapon_PRIMARY_HIP'],state=current)
    raise BotFault('Horse chase bounded deadline ended before a native scene transition')
