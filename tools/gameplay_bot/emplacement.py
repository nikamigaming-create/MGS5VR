"""Use one authored mortar after verified native guard incapacitation."""
import json
import math
import time

from .combat import admit, fresh_rig, looking_head
from .core import BotFault, atomic_json
from .live import authoritative_controls, fresh_control_observation
from .locomotion import _position, _validate_xr_scene_state
from .perception import read_enemy_snapshot
from .route import navigate, set_posture
from .session import preserved_tracking_poses


def verify_fixture(observed, policy):
    expected = {'id': policy['id'], 'type': policy['type'],
                'declared_type': policy['type'], 'registry_kind': policy['registry_kind'],
                'lookup_status': True, 'broken': False}
    if not isinstance(observed, dict) or any(
            type(observed.get(key)) is not type(value) or observed.get(key) != value
            for key, value in expected.items()):
        raise BotFault(f'Native emplacement registry/lookup differs from reviewed policy: {observed}')
    return observed


def _observe_fixture(live, policy):
    # GetGameObjectId returns status THEN ID. pcall's flag is not the ID.
    key=json.dumps(policy['key']);locator=json.dumps(policy['locator']);dataset=json.dumps(policy['dataset'])
    script=f'''local row=mvars.gim_identifierParamTable[{key}]
assert(type(row)=='table' and row.locatorName=={locator} and row.dataSetName=={dataset},'authored emplacement changed')
local status,id=TppGimmick.GetGameObjectId({key})
assert(status==true and type(id)=='number' and id~=GameObject.NULL_ID,'native emplacement lookup unavailable')
local kind=GameObject.GetTypeIndex(id)
assert(type(row.type)=='number' and type(row.gimmickType)=='number','native registry categories unavailable')
local broken=TppGimmick.IsBroken{{gimmickId={key}}}
assert(type(broken)=='boolean','native emplacement broken state unavailable')
return '{{"id":'..id..',"type":'..kind..',"declared_type":'..row.type..',"registry_kind":'..row.gimmickType..',"lookup_status":true,"broken":'..tostring(broken)..'}}' '''
    observed,timing=live.native.read(script)
    live.events.emit('emplacement_fixture',policy=policy,observed=observed,transport=timing)
    return observed


def bind_fixture(live, policy):
    """Resolve the owned locator once, then freeze its ID for this live run.

    FOX's streamed object registry can assign a new ID to the same authored
    emplacement. The key, dataset, locator and categories remain the contract;
    subsequent interaction checks still reject an ID changing within the run.
    """
    state, anchor = fresh_rig(live)
    native = state['native']
    if native['mission'] != policy['mission'] or native['location'] != policy['location']:
        raise BotFault('Emplacement binding requires its reviewed native world')
    observed = _observe_fixture(live, policy)
    current_id = observed.get('id') if isinstance(observed, dict) else None
    if type(current_id) is not int or not 0 < current_id < 65535:
        raise BotFault('Authored emplacement has no current native registry ID')
    bound = {**policy, 'id': current_id}
    verify_fixture(observed, bound)
    # The read and its authorization must belong to the same live field owner.
    fresh_rig(live, anchor)
    bound['registry_binding'] = {'reviewed_id': policy['id'], 'current_id': current_id,
        'process_id': live.identity['process']['ProcessId'], 'field_owner': list(anchor),
        'source': 'exact_native_key_dataset_locator_and_categories'}
    live.events.emit('emplacement_registry_bound', policy=bound, observed=observed)
    return bound


def fixture(live, policy):
    return verify_fixture(_observe_fixture(live, policy), policy)


def check_guards(live, script, camp, point, *, diagnostic_active_camp=False):
    enemies=read_enemy_snapshot(live.native.read,script)
    roster=[r for r in enemies['soldiers'] if r.get('camp')==camp]
    active=[r for r in enemies['soldiers'] if r['life']==enemies['normal_life']
            and (r.get('camp')==camp or math.dist(point,r['position'])<45.)]
    if type(diagnostic_active_camp) is not bool:raise BotFault('Emplacement diagnostic choice must be explicit')
    if not roster or (active and not diagnostic_active_camp):raise BotFault('Active native guards remain; secure them before using the mortar')
    if diagnostic_active_camp:
        live.events.emit('emplacement_active_camp_diagnostic',active_guards=active,
            camp_secured=False,does_not_certify_camp_clearance=True)
    return enemies


def mount_identity(native, policy):
    vehicle=native.get('player_vehicle_id');ridden=native.get('player_ridden_game_object_id')
    kind=native.get('player_ridden_game_object_type')
    if type(ridden) is int and ridden==policy['id'] and type(kind) is int and kind==policy['type']:
        if type(vehicle) is int and vehicle not in (65535,policy['id']):return None
        return 'player_ridden_game_object_id'
    if type(vehicle) is int and vehicle==policy['id']:
        if type(ridden) is int and ridden not in (65535,policy['id']):return None
        return 'player_vehicle_id'
    return None


def mounted_state(live, policy):
    state,controls=fresh_control_observation(live.observe,native=True)
    _validate_xr_scene_state(state)
    native=state['native']
    if (native['mission']!=policy['mission'] or native['location']!=policy['location']
        or not mount_identity(native,policy) or native['player_life']<=0
        or native['popup'] is not False or native['saving'] is not False
        or native['demo'] is not False or native['game_over']!=0
        or controls['context']!='gameplay' or controls['rig_input'] is not True):
        raise BotFault('Fresh mounted mortar/camera/control owner changed')
    return state,controls


def require_unburdened_field(state, minimum_life):
    native=state['native']
    if (native.get('status_CARRY') is not False or native.get('status_NORMAL_ACTION') is not True
            or native.get('not_alert') is not True or native.get('player_life',0)<minimum_life):
        raise BotFault('Emplacement requires a fresh released, undetected field state without carry or health loss')


def look_at_mount(live, anchor, target, *, clock=time.monotonic, sleep=time.sleep):
    state,_=fresh_rig(live,anchor)
    head=live.text_result(live.call('get_head_pose',{'base_space':'local'}))['pose']
    head['orientation']=looking_head(head['orientation'],state['rendered']['native_pose'],target)
    requested=list(head['orientation']);floor=state['rendered']['sample_ms']
    live.call('set_head_pose',{'base_space':'local',**head})
    until=clock()+1.5
    while True:
        state,_=fresh_rig(live,anchor)
        q=state['rendered'].get('head_pose',{}).get('orientation')
        matched=(isinstance(q,list) and len(q)==4 and all(type(v) in (int,float) and math.isfinite(v) for v in q)
            and abs(sum(a*b for a,b in zip(q,requested)))>=.999)
        if state['rendered']['sample_ms']>floor and matched:
            live.events.emit('emplacement_facing_published',requested_head=head,state=state)
            return state
        if clock()>=until:raise BotFault('Mortar-facing HMD pose did not reach a fresh same-owner rig publication')
        sleep(.025)


def use_emplacement(live, nav, *, policy, camp, enemy_script, output, max_seconds=0., minimum_life=None,
                    diagnostic_active_camp=False):
    output.mkdir(parents=True,exist_ok=True)
    result={'status':'running','objective':'mount_aim_fire_dismount','policy':policy,'axes':[],
            'visual_acceptance':'pending_both_eye_barrel_hands_and_shell_review','all_emplacements_acceptance':False}
    result['diagnostic_active_camp']=diagnostic_active_camp
    started=time.monotonic()
    try:
        if policy['type'] not in (81,82) or policy['map_identity']!=nav.identity:
            raise BotFault('This selected case requires a reviewed native emplacement and map')
        with preserved_tracking_poses(live):
            live.release();state,anchor=admit(live)
            if minimum_life is None:minimum_life=state['native']['player_life']
            if type(minimum_life) not in (int,float) or not math.isfinite(minimum_life) or minimum_life<=0:
                raise BotFault('Emplacement health boundary requires a positive native baseline')
            require_unburdened_field(state,minimum_life)
            fixture(live,policy)
            result['guards_before']=check_guards(live,enemy_script,camp,_position(state),diagnostic_active_camp=diagnostic_active_camp)
            route_output=output/'approach';route_output.mkdir()
            route=navigate(live,nav,policy['goal'],output=route_output,
                enemy_reader=lambda:read_enemy_snapshot(live.native.read,enemy_script),mode='stealth',
                max_seconds=max_seconds,expected_location=policy['location'],arrival_radius=policy.get('arrival_radius',1.25),recenter=False,
                interrupt_on_danger=diagnostic_active_camp)
            result['route']=str(route_output/'result.json')
            if route['status']!='observed_native_route':raise BotFault(route.get('error','Mortar approach did not arrive'))
            state,_=admit(live,anchor)
            require_unburdened_field(state,minimum_life)
            result['guards_at_mortar']=check_guards(live,enemy_script,camp,_position(state),diagnostic_active_camp=diagnostic_active_camp)
            mount_posture=policy.get('mount_posture','crouch')
            if mount_posture not in ('crouch','stand'):raise BotFault('Mortar mount posture must be an explicit ordinary stance')
            set_posture(live,mount_posture)
            state,_=admit(live,anchor)
            require_unburdened_field(state,minimum_life)
            target=[policy['goal'][0],policy['goal'][1]+.8,policy['goal'][2]]
            mount_view=policy.get('mount_view','physical_target')
            if mount_view=='physical_target':
                result['before_mount']=look_at_mount(live,anchor,target)
            elif mount_view=='current':
                result['before_mount'],_=fresh_rig(live,anchor)
            else:
                raise BotFault('Mortar view must be physical_target or current')
            result['mount_view']=mount_view
            result['mount_posture']=mount_posture
            result['mount_observer_before']={key:result['before_mount']['native'].get(key) for key in
                ('player_vehicle_id','player_ridden_game_object_id','player_ridden_game_object_type')}
            result['mount_captures']=live.capture('mortar-interaction-distance')
            fixture(live,policy)
            state,_=admit(live,anchor)
            require_unburdened_field(state,minimum_life)
            mount_seconds=policy.get('mount_seconds',.12)
            if type(mount_seconds) not in (int,float) or not .03<=mount_seconds<=.30:
                raise BotFault('Mortar interaction needs a bounded 0.03..0.30 second reviewed pulse')
            live.execute({'op':'action','name':'gameplay.interact','seconds':mount_seconds,
                'native_before':{'mission':policy['mission'],'location':policy['location'],
                                 'player_vehicle_id':65535,'popup':False,'demo':False}})
            deadline=time.monotonic()+4.
            while time.monotonic()<deadline:
                state=live.observe(native=True)
                live.events.emit('emplacement_mount_observation',state=state,
                    mount_identity_source=mount_identity(state['native'],policy))
                if mount_identity(state['native'],policy):break
                if state['native']['player_life']<=0:raise BotFault('Player died before mortar mounting')
                time.sleep(.04)
            else:raise BotFault('One interaction did not mount the exact native mortar; no ambiguous replay')
            state,_=mounted_state(live,policy);fixture(live,policy)
            mount_field=mount_identity(state['native'],policy)
            result['mount_identity_source']=mount_field
            result['mounted']=state;result['mounted_captures']=live.capture('native-mortar-mounted')
            # Physical sampling and native packets are evidence separate from
            # actual barrel motion. Each isolated pulse expires independently.
            for axis,sign in (('X',1),('X',-1),('Y',1),('Y',-1)):
                before,controls=mounted_state(live,policy)
                samples=[];floor=controls['sample_ms'];duration=.35;end=time.monotonic()+duration
                try:
                    live.input([{'hand':'right','component':'Thumbstick','sub_component':axis,'value':sign*.45}],
                               duration,lease_seconds=duration)
                    while time.monotonic()<end:
                        current=live.observe(native=False)
                        try:actual=authoritative_controls(current)
                        except BotFault:continue
                        if actual['sample_ms']>floor:
                            floor=actual['sample_ms']
                            samples.append({'now_ms':current['now_ms'],'controls':actual,'rendered':current.get('rendered')})
                        time.sleep(.015)
                finally:live.release()
                after,_=mounted_state(live,policy)
                result['axes'].append({'axis':axis,'sign':sign,'before':before,'after':after,'samples':samples,
                    'captures':live.capture('mortar-'+axis.lower()+('plus' if sign>0 else 'minus'))})
                atomic_json(output/'progress.json',result)
                if max_seconds and time.monotonic()-started>=max_seconds:raise BotFault('Mortar case reached its selected deadline')
            mounted_state(live,policy)
            # Mounted mortar owns RT; the handheld ready modifier is irrelevant.
            result['fire']=live.execute({'op':'action','name':'gameplay.fire_or_cqc','seconds':.12,
                'native_before':{'mission':policy['mission'],'location':policy['location'],mount_field:policy['id']},
                'while_held':{'native.'+mount_field:policy['id'],'native.status_GUN_FIRE':True},
                'capture_while_held':'mortar-native-fire'})
            result['fire_captures']=live.capture('mortar-after-native-fire')
            mounted_state(live,policy)
            live.execute({'op':'action','name':'gameplay.interact','seconds':.30,
                'native_before':{'mission':policy['mission'],'location':policy['location'],mount_field:policy['id']}})
            deadline=time.monotonic()+4.
            while time.monotonic()<deadline:
                state=live.observe(native=True)
                native=state['native']
                if native.get('player_vehicle_id')==65535 and native.get(mount_field)==65535:break
                time.sleep(.04)
            else:raise BotFault('Mortar dismount did not return native on-foot ownership')
            result['after']=state;result['after_captures']=live.capture('mortar-dismounted')
            result['status']='observed_native_mortar_mount_fire_dismount'
    except Exception as error:
        result['status']='failed';result['error']=str(error)
        try:
            live.release();result['failure_state']=live.observe(native=True)
            result['failure_captures']=live.capture('mortar-failed-released')
        except Exception as cleanup:result['cleanup_error']=str(cleanup)
    finally:
        live.release();result['seconds']=time.monotonic()-started
        atomic_json(output/'result.json',result)
    return result
