"""Mission 18's observed enemy helicopter: real VR fire, read-only outcomes."""
import math
import time
from contextlib import ExitStack
from .core import BotFault, atomic_json
from .live import channel, held_input_evidence
from .session import preserved_tracking_poses, _controller_pose
from .recording import Recording
from .combat import (AimProbeUnavailable, admit, sight, arm, controller_pair,
    face_and_aim, effective_pitch, aim_error, facing_error, battle_checkpoint,
    paired_aim_position, inverse, rotate, one_hand_ready)

SCRIPT="""assert(vars.missionCode==10100 and TppSequence.GetCurrentSequenceName()=='Seq_Game_Escape')
local id=GameObject.GetGameObjectIdByIndex('TppEnemyHeli',0)
assert(id==7680 and type(svars.isEnemyHeliClear)=='boolean')
local p=TppMath.Vector3toTable(GameObject.SendCommand(id,{id='GetPosition'}))
return string.format('{"id":%d,"position":[%.6f,%.6f,%.6f],"cleared":%s}',id,p[1],p[2],p[3],tostring(svars.isEnemyHeliClear))"""

def joined_target(data, actor, *, require_clear=False):
    record=data.get('requested_marker') or {}
    if (actor.get('id')!=7680 or actor.get('cleared') is not False
        or record.get('id')!=7680 or record.get('found') is not True
        or record.get('kind')!=13 or type(record.get('flags')) is not int
        or record['flags']&5!=1):
        raise AimProbeUnavailable('Live typed helicopter/native marker identity changed')
    points=[actor.get('position'),record.get('position')]
    if any(not isinstance(p,list) or len(p)!=3 or any(type(v) not in (int,float) or not math.isfinite(v) for v in p) for p in points):
        raise AimProbeUnavailable('Helicopter position is unavailable')
    if math.dist(*points)>2.:
        raise AimProbeUnavailable('Typed helicopter moved beyond its fresh marker join')
    target=next((t for t in data.get('targets',[]) if t.get('id')==7680),None)
    if (not target or target.get('aim_kind')!=2 or target.get('query_valid') is not True
        or not .25<=target.get('distance',math.inf)<=180.
        or target.get('aim')!=record['position']
        or (require_clear and target.get('unoccluded') is not True)):
        raise AimProbeUnavailable('Exact helicopter marker/world ray is unavailable or obstructed')
    return target

def heli_sight(live,anchor):
    lease,_=live.native.read('diagnostics:combat-sight:3000:7680')
    if lease.get('armed') is not True:raise AimProbeUnavailable('Helicopter observation lease unavailable')
    # A newly armed lease can still expose the preceding soldier-only sample.
    # Wait for the marker job to publish this request; never reinterpret it.
    deadline=time.monotonic()+.75
    while True:
        fresh,_=live.native.read('inspect-combat-sight')
        admit(live,anchor)
        if (fresh.get('sample_ms',0)>lease['now_ms']
            and (fresh.get('requested_marker') or {}).get('id')==7680):break
        if time.monotonic()>=deadline:raise AimProbeUnavailable('Helicopter marker job did not publish the new observation lease')
        time.sleep(.015)
    data,state=sight(live,anchor)
    data,state=wait_support_publication(live,anchor,data,state)
    actor,_=live.native.read(SCRIPT)
    joined_target(data,actor)
    live.events.emit('helicopter_marker_join',geometry=data,actor=actor)
    return data,state

def wait_support_publication(live,anchor,data,state,*,clock=time.monotonic,sleep=time.sleep,read_sight=None):
    """Observe the actual socket after a head turn; never reuse its old pose."""
    deadline=clock()+.8;read_sight=read_sight or sight
    while True:
        frame=state.get('rendered') or {}
        if (frame.get('rig_sequence',0)>0 and frame.get('native_firearm') is True
            and frame.get('weapon_support_tracked') is True
            and isinstance(frame.get('weapon_support_grip'),dict)):
            return data,state
        if clock()>=deadline:
            raise AimProbeUnavailable('Fresh authored helicopter support contact did not return after head motion')
        live.events.emit('helicopter_support_publication_wait',state=state,input_replayed=False)
        sleep(.025)
        # sight() rechecks the exact world/player/activation and current muzzle.
        # No pose/grip/trigger RPC is dispatched while this publication is absent.
        data,state=read_sight(live,anchor)

def wait_hover(live,anchor,*,max_seconds=8.,clock=time.monotonic,sleep=time.sleep):
    """Wait for observed slow flight; never alter the helicopter's route/AI."""
    deadline=clock()+max_seconds;previous=None;stable=None
    while clock()<deadline:
        admit(live,anchor)
        actor,_=live.native.read(SCRIPT);now=clock()
        if actor.get('cleared') is True:return actor
        p=actor.get('position')
        if (actor.get('id')!=7680 or actor.get('cleared') is not False
            or not isinstance(p,list) or len(p)!=3
            or any(type(v) not in (int,float) or not math.isfinite(v) for v in p)):
            raise AimProbeUnavailable('Typed helicopter motion observation unavailable')
        if previous and now>previous[0] and math.dist(p,previous[1])/(now-previous[0])<=1.5:
            stable=previous[0] if stable is None else stable
            if now-stable>=.35:
                live.events.emit('helicopter_slow_flight_observed',actor=actor,stable_seconds=now-stable)
                return actor
        else:stable=None
        previous=(now,list(p));sleep(.1)
    raise AimProbeUnavailable('Helicopter stayed in fast flight; no aiming or trigger dispatched')

def prepare_rifle_hold(live,pair,support_pair):
    if live.held:raise BotFault('Rifle hand preparation requires released inputs')
    # Captured ALM48 failure requested86cm from a54cm support arm. Keep the
    # firing grip close to the shoulder; acquire support only at its real socket.
    right=[.14,-.18,-.20]
    for hand,position,relationship in [('right',right,pair),('left',[-.18,-.22,-.20],support_pair)]:
        live.execute({'op':'pose','hand':hand,'position':position,
                      'orientation':relationship['orientation'],
                      'aim_position':paired_aim_position(position,[0.,0.,0.,1.],relationship),
                      'aim_orientation':[0.,0.,0.,1.]})
    return right

def support_contact_error(frame):
    if frame.get('left_palm_tracked') is not True or frame.get('weapon_support_attached') is not True:
        raise AimProbeUnavailable('Rendered left palm/support contact unavailable')
    try:
        head=frame['head_pose'];native=frame['native_pose']
        local=frame['weapon_support_grip']['position']
        relative=rotate(inverse(head['orientation']),[x-y for x,y in zip(local,head['position'])])
        world=[x+y for x,y in zip(native['position'],rotate(native['orientation'],[-relative[0],relative[1],-relative[2]]))]
        error=math.dist(world,frame['left_palm']['position'])
    except (KeyError,TypeError,ValueError,IndexError) as exc:
        raise AimProbeUnavailable('Rendered support contact transform unavailable') from exc
    if not math.isfinite(error):raise AimProbeUnavailable('Rendered support contact is nonfinite')
    return error

def engage(live,*,output,max_seconds=40.,max_rounds=12,one_handed=False):
    if not 1<=max_seconds<=60 or type(max_rounds) is not int or not 3<=max_rounds<=60:
        raise BotFault('Helicopter test requires 1..60 seconds and 3..60 rounds')
    if type(one_handed) is not bool:raise BotFault('Explicit grip mode must be boolean')
    result={'status':'running','bursts':[],'captures':[],'rounds':0,
            'grip_mode':'one_hand' if one_handed else 'two_hand',
            'visual_acceptance':'pending_both_eye_and_temporal_review',
            'outcome_scope':'native mission helicopter-clear flag; no inferred hit or health value'}
    started=time.monotonic();rec=None
    try:
        with preserved_tracking_poses(live), ExitStack() as cleanup:
            state,anchor=admit(live)
            if state['native'].get('mission')!=10100 or state['native'].get('weapon_PRIMARY_BACK')!=873:
                raise BotFault('Helicopter case requires the reviewed Mission18 ALM48 fixture')
            actions={a['name']:a for a in live.bindings['actions']}
            ready=actions['gameplay.ready_weapon']['bindings'][0]
            support=actions['gameplay.support_grip']['bindings'][0]
            trigger=actions['gameplay.fire_or_cqc']['bindings'][0]
            ready_inputs=list(dict.fromkeys(ready['inputs']+support['inputs']))
            if (ready['gesture']!='level' or support['gesture']!='level'
                or set(ready_inputs)!={'left_grip','right_grip'}
                or trigger['gesture']!='level' or trigger['inputs']!=['right_trigger']):
                raise BotFault('Effective physical grip/trigger bindings are unavailable')
            if one_handed:ready_inputs=list(ready['inputs'])
            if one_handed and ready_inputs!=['right_grip']:raise BotFault('One-hand fixture requires effective right grip')
            rec=Recording(live.game,live.events,live.identity['process']['ProcessId'],segment_seconds=25.)
            rec.start()
            def check():
                if time.monotonic()-started>=max_seconds:raise BotFault('Helicopter input deadline expired')
                rec.check()
            live.background_check=check;live.held_action_recording=rec
            # Deadline/capture checks must not obstruct saved-pose restoration.
            cleanup.callback(setattr,live,'background_check',None)
            cleanup.callback(setattr,live,'held_action_recording',None)
            pair=controller_pair(_controller_pose(live,'right','grip','local'),_controller_pose(live,'right','aim','local'))
            support_pair=controller_pair(_controller_pose(live,'left','grip','local'),_controller_pose(live,'left','aim','local'))
            result['captures']+=battle_checkpoint(live,'helicopter-before',state)
            while result['rounds']<=max_rounds-3:
                if (output/'STOP').exists():result['status']='stopped';break
                actor,_=live.native.read(SCRIPT)
                if actor['cleared'] is True:result['status']='native_helicopter_clear';break
                actor=wait_hover(live,anchor)
                if actor['cleared'] is True:result['status']='native_helicopter_clear';break
                grip_position=prepare_rifle_hold(live,pair,support_pair)
                arm(live,ready['inputs'],anchor)
                data,state=heli_sight(live,anchor);actor,_=live.native.read(SCRIPT)
                target=joined_target(data,actor)
                def restore_view_hands():
                    # The simulator head RPC replaces controller poses too.
                    # Reapply paired VIEW hand targets before asking for a
                    # muzzle/socket publication; a missing rig cannot guide it.
                    live.release()
                    prepare_rifle_hold(live,pair,support_pair)
                    arm(live,ready['inputs'],anchor)
                face_and_aim(live,{'id':7680,'sight':target},anchor,effective_pitch(live.bindings),
                    pair=pair,support_pair=support_pair,ready_inputs=ready_inputs,geometry_reader=heli_sight,
                    grip_position=grip_position,after_head_turn=restore_view_hands,support_required=not one_handed,
                    # Actual moving-helicopter take111900 exhausted the generic
                    # 2.8s probe with two isolated good samples. Give this
                    # explicitly selected case more tracking time; retain two
                    # consecutive<=18cm samples and every fresh trigger gate.
                    max_seconds=6.)
                result['captures']+=battle_checkpoint(live,'helicopter-aimed',state)
                data,state=heli_sight(live,anchor);actor,_=live.native.read(SCRIPT)
                target=joined_target(data,actor,require_clear=True)
                along,lateral=aim_error(data['muzzle'],target['aim'])
                contact_error=None if one_handed else support_contact_error(state['rendered'])
                grip_valid=(one_hand_ready(state) if one_handed else
                            state['rendered'].get('weapon_support_attached') is True and contact_error<=.025)
                if (not math.isfinite(lateral) or along<.25 or lateral>.18
                    or facing_error(state['rendered']['native_pose'],target['aim'])>15.
                    or not grip_valid):
                    raise AimProbeUnavailable('Helicopter barrel/view/support changed before trigger')
                before={k:state['native']['ammo_in_weapon_'+k] for k in ['PRIMARY_HIP','PRIMARY_BACK','SECONDARY']}
                sampled=False;stamp=state['now_ms'];begun=time.monotonic()
                try:
                    live.input([channel('right_trigger')],.12,lease_seconds=.24)
                    while time.monotonic()-begun<.24:
                        current,_=admit(live,anchor)
                        audit=held_input_evidence(current,ready_inputs+['right_trigger'],after_sample_ms=stamp)
                        if audit and current['controls']['native_triggers']==[255,255]:sampled=True
                        if sampled and time.monotonic()-begun>=.12:break
                        time.sleep(.015)
                finally:
                    live.call('set_controller_input',{**channel('right_trigger'),'value':0,'auto_release':False})
                    live.held.pop(('right','Trigger',None),None)
                if not sampled:raise BotFault('Helicopter trigger delivery ambiguous; no replay')
                after,_=admit(live,anchor)
                deltas={k:before[k]-after['native']['ammo_in_weapon_'+k] for k in before}
                if not 1<=deltas['PRIMARY_BACK']<=3 or any(deltas[k]!=0 for k in ['PRIMARY_HIP','SECONDARY']):
                    raise BotFault('Discharge did not match the reviewed ALM48 slot/burst bound')
                outcome,_=live.native.read(SCRIPT)
                burst={'rounds':deltas['PRIMARY_BACK'],'barrel_lateral_error':lateral,'geometry':data,
                       'rendered_support_contact_error':contact_error,
                       'actor_before':actor,'native_outcome_after':outcome,'ammo_before':before,'ammo_deltas':deltas}
                result['bursts'].append(burst);result['rounds']+=burst['rounds']
                live.events.emit('helicopter_burst',**burst)
                live.release()
            else:result['status']='round_budget_reached'
    except AimProbeUnavailable as error:result.update(status='needs_visible_aligned_helicopter',reason=str(error))
    except Exception as error:result.update(status='failed',error=str(error))
    finally:
        live.background_check=None;live.held_action_recording=None
        try:
            live.release();live.native.read('diagnostics:combat-sight:0')
            result['after']=live.observe(native=True)
            result['captures']+=live.capture('helicopter-released')
        except Exception as error:result.update(cleanup_error=str(error),status='failed')
        if rec:
            try:result['recording']=rec.close()
            except Exception as error:result.update(recording_error=str(error),status='failed')
        result['seconds']=time.monotonic()-started
        atomic_json(output/'result.json',result)
    return result
