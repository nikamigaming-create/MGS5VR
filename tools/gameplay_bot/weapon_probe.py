"""Inspect the loaded precision weapon through existing physical selection."""
import math
import time
from .core import BotFault, atomic_json, matches
from .session import preserved_tracking_poses, _controller_pose
from .combat import controller_pair, comfortable_grip, effective_pitch, mul, fresh_rig
from .locomotion import _validate_xr_scene_state
from .arsenal import guard, human_hand, human_support, static_ready
from .arsenal import inventory, discharge
from .live import channel, held_input_evidence, rotate, static_grip_capture_allowed


def held_tracking_loss(live,plan,anchor,health):
    """Observe existing rifle contact through a bounded right-controller loss."""
    rows=[]
    def grips():
        live.input([channel('left_grip'),channel('right_grip')],.03,lease_seconds=3.)
    try:
        grips();time.sleep(.5)
        before,_=guard(live,plan,anchor,health)
        if not before['rendered'].get('weapon_support_attached'):
            raise BotFault('Held tracking probe requires acquired rifle support')
        rows.append({'label':'before','state':before})
        live.execute({'op':'tracking_loss','mask':12,'milliseconds':8000,'static_grips':True})
        start=time.monotonic()
        loss_captured=False
        while time.monotonic()-start<9.:
            grips()
            # This is observation with static grips, not firearm admission:
            # combat.guard correctly rejects the very tracking loss under test.
            state,_=fresh_rig(live,anchor)
            _validate_xr_scene_state(state,activation=anchor[2])
            if not matches(state['native'],plan['world']) or state['native']['player_life']<health:
                raise BotFault('Held tracking probe lost its field owner or health')
            rows.append({'label':'loss_or_recovery','elapsed':time.monotonic()-start,'state':state})
            atomic_json(live.events.output/'sniper-tracking.json',rows)
            if not loss_captured and time.monotonic()-start>=5. and state.get('tracking_fault_mask')==12:
                grips()
                rows[-1]['eyes']=live.capture('sniper-tracking-loss',static_grips=True)
                loss_captured=True
            time.sleep(.2)
        grips()
        rows[-1]['eyes']=live.capture('sniper-tracking-recovered',static_grips=True)
        return rows
    finally:
        live.release()
        live.native.read('diagnostics:controller-tracking-loss:off')
        atomic_json(live.events.output/'sniper-tracking.json',rows)


def check_recovery_attack(state, ammunition, *, held):
    controls=state.get('controls') or {}
    physical=controls.get('physical') or []
    triggers=controls.get('native_triggers') or []
    if (len(physical)!=11 or len(triggers)!=2
            or physical[10] != (1 if held else 0) or triggers[1]!=0):
        raise BotFault('Recovery safety lacks sampled trigger suppression/release')
    if inventory(state['native'])!=ammunition:
        raise BotFault('Native ammunition changed during suppressed tracking recovery')


def tracking_trigger_recovery(live,plan,anchor,health):
    """Press only after confirmed loss; retain an explicit no-discharge claim."""
    rows=[]
    def hold(trigger=False):
        tokens=['left_grip','right_grip']+(['right_trigger'] if trigger else [])
        live.input([channel(t) for t in tokens],.03,lease_seconds=1.)
    def observe():
        state,_=fresh_rig(live,anchor)
        _validate_xr_scene_state(state,activation=anchor[2])
        if (not matches(state['native'],plan['world'])
                or state['native']['player_life']<health
                or any(state.get(k) for k in ('menu','idroid','demo','loading'))):
            raise BotFault('Trigger recovery lost its field owner or health')
        return state
    try:
        actions={a['name']:a for a in live.bindings['actions']}
        if (actions['gameplay.fire_or_cqc']['bindings'][0]['inputs']!=['right_trigger']
                or {t for a in ('gameplay.ready_weapon','gameplay.support_grip')
                    for t in actions[a]['bindings'][0]['inputs']}!={'left_grip','right_grip'}):
            raise BotFault('Trigger recovery requires the audited effective grip/trigger bindings')
        until=time.monotonic()+1.5
        while True:
            hold();time.sleep(.1)
            before,_=guard(live,plan,anchor,health)
            if before['rendered'].get('weapon_support_attached'):break
            if time.monotonic()>until:
                raise BotFault('Trigger recovery requires an acquired supported firearm')
        ammunition=inventory(before['native'])
        rows.append({'label':'before','state':before})
        live.execute({'op':'tracking_loss','mask':12,'milliseconds':8000,'static_grips':True})
        deadline=time.monotonic()+1.
        while True:
            hold();state=observe()
            if state.get('tracking_fault_mask')==12:break
            if time.monotonic()>deadline:raise BotFault('Loss was not confirmed before trigger dispatch')
            time.sleep(.025)
        # Confirmed loss is a required precondition, not just a requested fault.
        rows.append({'label':'confirmed_loss_before_trigger','state':state})
        started=time.monotonic();recovered=None
        while time.monotonic()-started<11.:
            hold(True);time.sleep(.05);state=observe()
            check_recovery_attack(state,ammunition,held=True)
            elapsed=time.monotonic()-started
            rows.append({'label':'held_trigger','elapsed':elapsed,'state':state})
            atomic_json(live.events.output/'tracking-trigger-recovery.json',rows)
            if state.get('tracking_fault_mask')==0:
                recovered=time.monotonic() if recovered is None else recovered
                if time.monotonic()-recovered>=1.:break
            elif state.get('tracking_fault_mask')!=12:
                raise BotFault('Unexpected tracking fault during trigger recovery')
            time.sleep(.1)
        else:raise BotFault('Tracking did not recover with held trigger before deadline')
        live.release();time.sleep(.2)
        state=observe();check_recovery_attack(state,ammunition,held=False)
        rows.append({'label':'released','state':state,'eyes':live.capture('tracking-trigger-released')})
        return {'status':'observed_no_discharge_through_recovery','samples':len(rows),
                'positive_trigger_rearm':'not_tested'}
    finally:
        live.release()
        live.native.read('diagnostics:controller-tracking-loss:off')
        atomic_json(live.events.output/'tracking-trigger-recovery.json',rows)


def require_supported_shots(shots):
    if len(shots)!=3:
        raise BotFault('Supported firing requires three completed trigger edges')
    for index,row in enumerate(shots):
        if (row.get('discharge',{}).get('status')!='pass'
                or row.get('support_retained') is not True
                or row.get('trigger_sampled') is not True):
            raise BotFault('Supported firing failed discharge, contact or input evidence at shot '+str(index))


def repeated_shots(live, plan, anchor, health, moving=False, expected_slot='PRIMARY_BACK'):
    """Keep both physical grips held across real trigger edges and recoil."""
    actions={a['name']:a for a in live.bindings['actions']}
    tokens=[]
    for name in ('gameplay.ready_weapon','gameplay.support_grip'):
        binding=actions[name]['bindings'][0]
        if binding['gesture']!='level':raise BotFault('Grip binding changed')
        tokens.extend(binding['inputs'])
    tokens=list(dict.fromkeys(tokens))
    if set(tokens)!={'left_grip','right_grip'}:raise BotFault('Grip inputs changed')
    if actions['gameplay.fire_or_cqc']['bindings'][0]['inputs']!=['right_trigger']:
        raise BotFault('Trigger binding changed')
    shots=[]
    poses=[_controller_pose(live,h,k,'local') for h in ('left','right') for k in ('grip','aim')]
    pivot=poses[2]['position']
    live.input([channel(t) for t in tokens],.03,lease_seconds=3.)
    time.sleep(.5)
    for index in range(3):
        if moving:
            motion=[]
            for part in range(1,7):
                angle=math.radians((12,-12,0)[index])*part/6
                q=[0.,math.sin(angle/2),0.,math.cos(angle/2)]
                live.input([channel(t) for t in tokens],.03,lease_seconds=3.)
                for pose in poses:
                    offset=rotate(q,[a-b for a,b in zip(pose['position'],pivot)])
                    live.call('set_controller_pose',{**pose,
                        'position':[a+b for a,b in zip(pivot,offset)],
                        'orientation':mul(q,pose['orientation'])})
                state,_=guard(live,plan,anchor,health)
                motion.append(state)
                atomic_json(live.events.output/('sniper-motion-'+str(index)+'.json'),motion)
                if not state['rendered'].get('weapon_support_attached'):
                    raise BotFault('Support lost during moving aim before shot '+str(index))
            atomic_json(live.events.output/('sniper-motion-'+str(index)+'.json'),motion)
        before,_=guard(live,plan,anchor,health)
        if not before['rendered'].get('weapon_support_attached'):
            raise BotFault('Support lost before shot '+str(index))
        live.input([channel(t) for t in tokens],.03,lease_seconds=3.)
        samples=[]
        try:
            live.input([channel('right_trigger')],.15,lease_seconds=.27)
            until=time.monotonic()+.22
            while time.monotonic()<until:
                state,_=guard(live,plan,anchor,health)
                samples.append(state)
        finally:
            live.call('set_controller_input',{**channel('right_trigger'),'value':0,'auto_release':False})
            live.held.pop(('right','Trigger',None),None)
        until=time.monotonic()+1.1
        while time.monotonic()<until:
            state,_=guard(live,plan,anchor,health)
            samples.append(state)
            time.sleep(.025)
        after=samples[-1]
        row={'shot':index,'discharge':discharge(inventory(before['native']),inventory(after['native']),expected_slot),
             'samples':samples,'support_retained':all(s['rendered'].get('weapon_support_attached') for s in samples),
             'trigger_sampled':any(held_input_evidence(s,tokens+['right_trigger'],after_sample_ms=before['now_ms']) for s in samples)}
        live.input([channel(t) for t in tokens],.03,lease_seconds=3.)
        shots.append(row)
        atomic_json(live.events.output/'sniper-shots.json',shots)
        row['eyes']=live.capture('sniper-shot-'+str(index),static_grips=True)
        atomic_json(live.events.output/'sniper-shots.json',shots)
        if row['discharge']['status']!='pass' or not row['support_retained']:
            break
    require_supported_shots(shots)
    return shots


def supported_reload(live,plan,anchor,health,expected_slot):
    actions={a['name']:a for a in live.bindings['actions']}
    binding=actions['gameplay.reload']['bindings'][0]
    if (binding['gesture']!='tap' or binding['milliseconds']!=300
            or set(binding['inputs'])!={'left_grip','b'}):
        raise BotFault('Supported reload requires audited effective left-grip+B tap')
    rows=[]
    def grips():
        live.input([channel('left_grip'),channel('right_grip')],.03,lease_seconds=3.)
    try:
        grips();time.sleep(.25)
        before,_=guard(live,plan,anchor,health)
        if not before['rendered'].get('weapon_support_attached'):
            raise BotFault('Reload requires an already acquired support contact')
        ammunition=inventory(before['native']);manipulated=False;captured_manipulation=False;completed=False
        # input() installs expiring channels; its duration argument does not
        # sleep. Observe an actual held chord before releasing the tap.
        live.input([channel('left_grip'),channel('right_grip'),channel('b')],.12,lease_seconds=.27)
        time.sleep(.10)
        pressed,_=guard(live,plan,anchor,health)
        rows.append({'label':'sampled_reload_chord','native_manipulation':False,'state':pressed})
        if not held_input_evidence(pressed,['left_grip','right_grip','b'],after_sample_ms=before['now_ms']):
            raise BotFault('Reload chord was not sampled before tap release')
        button=channel('b')
        live.call('set_controller_input',{**button,'value':0,'auto_release':False})
        live.held.pop((button['hand'],button['component'],button.get('sub_component')),None)
        start=time.monotonic()
        while time.monotonic()-start<10.:
            grips();state,_=guard(live,plan,anchor,health)
            observer=state['rendered'].get('weapon_native_observer') or {}
            flags,mode=observer.get('flags',0),observer.get('mode',0)
            native_manipulation=(flags&0x1c0)==0x40 and bool(mode&0x04000000)
            row={'elapsed':time.monotonic()-start,'native_manipulation':native_manipulation,'state':state}
            rows.append(row);atomic_json(live.events.output/'supported-reload.json',rows)
            if not state['rendered'].get('weapon_support_attached'):
                raise BotFault('Acquired support detached during native reload')
            if (native_manipulation and not captured_manipulation
                    and static_grip_capture_allowed(live.held,state,live.bindings)):
                row['eyes']=live.capture('supported-native-reload',static_grips=True)
                captured_manipulation=True
            manipulated=manipulated or native_manipulation
            current=inventory(state['native'])
            if (manipulated and not native_manipulation
                    and current[expected_slot]['ammo']>ammunition[expected_slot]['ammo']):
                row['eyes']=live.capture('supported-after-reload',static_grips=True)
                completed=True;break
            time.sleep(.06)
        if not completed:
            raise BotFault('Native reload manipulation and replenished magazine were not both observed')
        return {'status':'observed_supported_native_reload_and_return','samples':len(rows),
                'slot':expected_slot,'ammunition_before':ammunition,'ammunition_after':current}
    finally:
        live.release();atomic_json(live.events.output/'supported-reload.json',rows)


def inspect(live, seconds, policy):
    result={'status':'running','phases':[], 'visual_acceptance':'pending',
            'firing_tested':False,'physical_headset_acceptance':False,
            'requested_primary_card':policy.get('card','right'),
            'actual_selected_weapon':'unproven_by_inventory_or_support_flags'}
    plan={'world':policy.get('world',{'mission':30050,'location':50,'sequence':'Seq_Game_MainGame'})}
    try:
        live.release()
        state,anchor=guard(live,plan)
        health=state['native']['player_life']
        result['disable_actions'],_=live.native.read('''local rows={}
for _,k in ipairs({'NONE','SUBJECTIVE_CAMERA','LOOK_CAMERA_DIR','HELISPACE_CASSETTE','TIME_CIGARETTE'}) do
 local v=PlayerDisableAction[k]
 if type(v)=='number' then
  rows[#rows+1]=string.format('%q:%d',k,v)
 end
end
return '{'..table.concat(rows,',')..'}' ''')
        if policy.get('native_audit_only'):
            result['state']=state
            result['status']='observed_native_weapon_context'
            return result
        if state['native'].get('customized_weapon_1')!=799:
            raise BotFault('Reviewed loaded sniper base is no longer equipped')
        with preserved_tracking_poses(live):
            pair=controller_pair(_controller_pose(live,'right','grip','local'),
                                 _controller_pose(live,'right','aim','local'))
            support_pair=controller_pair(_controller_pose(live,'left','grip','local'),
                                         _controller_pose(live,'left','aim','local'))
            gp=comfortable_grip(state['native'])
            half=-math.radians(effective_pitch(live.bindings))/2
            go=mul([math.sin(half),0.,0.,math.cos(half)],pair['orientation'])
            human_hand(live,'right',gp,go,pair)
            human_hand(live,'left',[-.04,-.16,-.55],[0.,.70710678,0.,.70710678],support_pair)
            live.equipment_select({'case_id':'field-sniper','category':'primary','card':policy.get('card','right')})
            human_hand(live,'left',[-.35,-.25,-.4],[0.,0.,0.,1.],support_pair)
            state,_=guard(live,plan,anchor,health)
            result['selected']=state
            result['selection_eyes']=live.capture('sniper-selected')
            one=static_ready(live,plan,'sniper-one-hand',state)
            result['phases'].append({'label':'one-hand',**one})
            live.release()
            human_support(live,one['state']['rendered'],support_pair)
            for index in range(3):
                state,_=guard(live,plan,anchor,health)
                two=static_ready(live,plan,'sniper-acquire-'+str(index),state,True)
                result['phases'].append({'label':'acquire-'+str(index),**two})
                live.release();time.sleep(.3)
            result['status']='observed_support_telemetry'
            if policy.get('tracking_loss') is True:
                result['tracking']=held_tracking_loss(live,plan,anchor,health)
            if policy.get('tracking_trigger_recovery') is True:
                result['trigger_recovery']=tracking_trigger_recovery(live,plan,anchor,health)
            if policy.get('fire') is True:
                expected_slot='PRIMARY_HIP' if policy.get('card','right')=='left' else 'PRIMARY_BACK'
                result['shots']=repeated_shots(live,plan,anchor,health,policy.get('moving',False),expected_slot)
                result['firing_tested']=True
                if result.get('trigger_recovery'):
                    proven=bool(result['shots']) and all(r['discharge']['status']=='pass'
                        and r['trigger_sampled'] for r in result['shots'])
                    result['trigger_recovery']['positive_trigger_rearm']='observed_native_discharge' if proven else 'unproven'
                if policy.get('reload') is True:
                    result['reload']=supported_reload(live,plan,anchor,health,expected_slot)
    except Exception as error:
        result.update(status='failed',error=str(error))
        raise
    finally:
        live.release()
        atomic_json(live.events.output/'weapon-probe.json',result)
    return result
