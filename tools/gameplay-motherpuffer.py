"""Perform bounded field actions on one connection without a proof-case wrapper."""
import argparse
import json
import math
import subprocess
import time
from pathlib import Path

from gameplay_bot.core import BotFault, Events, atomic_json
from gameplay_bot.live import InputLease, Live, ROOT, channel, fresh_control_observation, rotate
from gameplay_bot.combat import (fresh_rig, looking_head, normalized, inverse, mul,
    controller_pair, comfortable_grip, paired_aim_position, arm, sight, aim_quaternion,
    corrected_aim, place_support_hand)
from gameplay_bot.session import _controller_pose
from gameplay_bot.recording import Recording


def level_view(live):
    live.release()
    state, anchor = fresh_rig(live)
    frame = state['rendered']
    direction = rotate(frame['native_pose']['orientation'], [0., 0., 1.])
    length = math.hypot(direction[0], direction[2])
    if length < .1:
        raise BotFault('Current view has no stable horizontal heading')
    eye = frame['native_pose']['position']
    target = [eye[0] + 30.*direction[0]/length, eye[1], eye[2] + 30.*direction[2]/length]
    head = live.text_result(live.call('get_head_pose', {'base_space':'local'}))['pose']
    head['orientation'] = looking_head(head['orientation'], frame['native_pose'], target)
    live.call('set_head_pose', {'base_space':'local', **head})
    until = time.monotonic()+1.
    while time.monotonic()<until:
        state, _ = fresh_rig(live, anchor)
        view = rotate(state['rendered']['native_pose']['orientation'], [0.,0.,1.])
        if abs(view[1]) < .08:
            live.events.emit('motherpuffer_view_leveled', direction=view)
            return state, anchor
        time.sleep(.025)
    raise BotFault('Requested level view did not reach the rendered camera')


def action(live, name, seconds=.20, **extra):
    state, controls = fresh_control_observation(live.observe, native=True)
    n=state['native']
    if n.get('popup') is not False or n.get('saving') is not False or n.get('player_life',0)<=0:
        raise BotFault('Field action lost live player ownership')
    return live.execute({'op':'action','name':name,'seconds':seconds,
        'native_before':{'mission':n['mission'],'location':n['location'],'popup':False,'saving':False}, **extra})


def vehicle(live, args, result):
    state, anchor=level_view(live)
    before=state['native']; result['before']=state
    if before['player_vehicle_id']==65535:
        action(live,'gameplay.interact',.12)
    until=time.monotonic()+3.
    while time.monotonic()<until:
        state,controls=fresh_control_observation(live.observe,native=True)
        if state['native']['player_vehicle_id']==args.vehicle_id and controls['context']=='vehicle':break
        time.sleep(.025)
    else:
        result['after']=state
        raise BotFault('Y did not enter the exact nearby truck')
    result['entered']=state; result['entered_eyes']=live.capture('motherpuffer-truck-entered')
    point=lambda s:[s['native']['player_'+a] for a in 'xyz']
    result['drive']=[]
    for name,seconds in [('vehicle.accelerate',.8),('vehicle.brake_reverse',.3),('vehicle.brake_reverse',.8)]:
        before=live.observe(native=True)
        action(live,name,seconds)
        after=live.observe(native=True)
        displacement=math.dist(point(before),point(after))
        result['drive'].append({'action':name,'distance':displacement,'before':before,'after':after})
        print(json.dumps({'action':name,'distance':displacement}),flush=True)
    result['driving_eyes']=live.capture('motherpuffer-truck-driven')
    action(live,'vehicle.interact',.12)
    until=time.monotonic()+3.
    while time.monotonic()<until:
        state=live.observe(native=True)
        if state['native']['player_vehicle_id']==65535:break
        time.sleep(.025)
    else:raise BotFault('Truck exit did not restore on-foot ownership')
    result['after']=state;result['after_eyes']=live.capture('motherpuffer-truck-exited')
    result['status']='driven_and_exited' if result['drive'][0]['distance']>.2 else 'entered_but_no_driving_displacement'


def firearm(live,args,result):
    state,anchor=level_view(live)
    live.equipment_select({'category':'primary','card':'left'})
    pair=controller_pair(_controller_pose(live,'right','grip','local'),_controller_pose(live,'right','aim','local'))
    support=controller_pair(_controller_pose(live,'left','grip','local'),_controller_pose(live,'left','aim','local'))
    state,_=fresh_rig(live,anchor)
    gp=comfortable_grip(state['native']);gp[1]=-.12
    actions={a['name']:a for a in live.bindings['actions']}
    ready=actions['gameplay.ready_weapon']['bindings'][0]['inputs']
    live.execute({'op':'pose','hand':'right','position':gp,'orientation':normalized(pair['orientation']),
        'aim_position':paired_aim_position(gp,[0.,0.,0.,1.],pair),'aim_orientation':[0.,0.,0.,1.]})
    try:
        arm(live,ready,anchor)
        attitude=None
        for _ in range(5):
            data,state=sight(live,anchor)
            view=rotate(state['rendered']['native_pose']['orientation'],[0.,0.,1.])
            length=math.hypot(view[0],view[2]);p=data['muzzle']['position']
            target=[p[0]+30.*view[0]/length,p[1],p[2]+30.*view[2]/length]
            pitch=next((s['value'] for s in live.bindings.get('settings',[]) if s['name']=='weapon_aim_pitch'),0.)
            attitude=(aim_quaternion(state['rendered']['native_pose'],p,target,pitch) if attitude is None
                else corrected_aim(state['rendered']['native_pose'],data['muzzle'],target,attitude))
            live.execute({'op':'pose','hand':'right','position':gp,'orientation':normalized(mul(attitude,pair['orientation'])),
                'aim_position':paired_aim_position(gp,attitude,pair),'aim_orientation':attitude})
            live.input([channel(t) for t in ready],.1,lease_seconds=3.)
            time.sleep(.05)
            state,_=fresh_rig(live,anchor)
            if state['rendered'].get('weapon_support_tracked'):
                place_support_hand(live,state['rendered'],support)
            data,state=sight(live,anchor)
            direction=rotate(data['muzzle']['orientation'],[0.,0.,1.])
            result.setdefault('aim_samples',[]).append({'direction':direction,'muzzle':data['muzzle'],'state':state})
            print(json.dumps({'muzzle_direction':direction}),flush=True)
            if abs(direction[1])<.08:break
        else:raise BotFault('Authored muzzle did not converge to horizontal hand aim')
    finally:live.release()
    state=live.observe(native=True)
    result['raised']=live.execute({'op':'action','name':'gameplay.ready_weapon','seconds':.5,
        'include_support':True,'static_grip_capture':True,'capture_while_held':'motherpuffer-forward-grip',
        'while_held':{'rendered.native_firearm':True,'rendered.weapon_support_attached':True}})
    result['before_fire']=live.observe(native=True)
    action(live,'gameplay.fire_or_cqc',.15,include_ready=True)
    result['after_fire']=live.observe(native=True)
    result['after_eyes']=live.capture('motherpuffer-forward-fired')
    result['status']='forward_hand_aim_and_trigger_observed'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode',choices=('vehicle','firearm'))
    for name in ('game-dir','proxy','output'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--vehicle-id',type=int,default=25603)
    parser.add_argument('--seconds',type=float,default=60.)
    parser.add_argument('--record',action='store_true',required=True)
    args=parser.parse_args()
    if not math.isfinite(args.seconds) or not 15. <= args.seconds <= 120.:
        parser.error('--seconds must be between 15 and 120')
    from gameplay_bot.live import game_identity
    with InputLease():
        identity=game_identity(args.game_dir)
        bindings=json.loads(subprocess.check_output([str(args.game_dir/'mgs5vr_controls.exe'),'--bindings-json',str(args.game_dir/'mgs5vr-controls.ini')],text=True))
        events=Events(args.output,identity);atomic_json(args.output/'bindings.json',bindings)
        live=Live(args.proxy,args.game_dir,bindings,events);rec=None
        result={'bot':'motherpuffer','status':'running','mode':args.mode,'visual_acceptance':'pending'}
        try:
            live.ready();live.release()
            rec=Recording(args.game_dir,events,identity['process']['ProcessId'],segment_seconds=25.)
            rec.start()
            deadline=time.monotonic()+args.seconds
            def check():
                if time.monotonic()>=deadline:
                    raise BotFault('Motherpuffer action deadline exceeded')
                rec.check()
            live.background_check=check;live.held_action_recording=rec
            (vehicle if args.mode=='vehicle' else firearm)(live,args,result)
        except Exception as error:
            result.update(status='failed',error=str(error))
            live.background_check=None
            try:
                live.release();result['failure_state']=live.observe(native=True)
                result['failure_eyes']=live.capture('motherpuffer-failed-released')
            except Exception as cleanup:result['cleanup_error']=str(cleanup)
        finally:
            live.background_check=None;live.held_action_recording=None
            try:
                if rec:result['recording']=rec.close()
            finally:
                live.close();atomic_json(args.output/'result.json',result)
        print(json.dumps({'bot':'motherpuffer','status':result['status'],'error':result.get('error')}),flush=True)
        return int(result['status']=='failed')

if __name__=='__main__':raise SystemExit(main())
