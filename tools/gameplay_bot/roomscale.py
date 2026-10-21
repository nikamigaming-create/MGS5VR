"""Bounded physical-space rig regression; no recenter or native warp."""
import math,time
from .core import BotFault,atomic_json
from .session import preserved_tracking_poses,bring_hands_into_view,_controller_pose
from .cabin_workbench import compose,inverse_pose as inverse
from .live import rotate
from .combat import pointing
from .locomotion import move_local

def inspect(live,seconds=180,policy=None):
    result={'status':'running','phases':[],'recenter_count':0,'physical_headset_acceptance':False}
    deadline=time.monotonic()+min(seconds,180)
    with preserved_tracking_poses(live):
        live.release();initial=live.observe(native=True)
        if initial['scene']!='gameplay' or initial['native']['mission']!=30010:raise BotFault('Ordinary isolated field required')
        bring_hands_into_view(live,initial['native'])
        head=live.text_result(live.call('get_head_pose',{'base_space':'local'}))['pose']
        hands=[_controller_pose(live,h,k,'local') for h in ('left','right') for k in ('grip','aim')]
        cases=[('neutral',0,0,0,0),('sit-half',0,-.25,0,0),('sit',0,-.5,0,0),('stand-half',0,-.25,0,0),('stand',0,0,0,0),('rise-from-chair',0,.25,0,0),('return-chair',0,0,0,0),('step-right',.25,0,0,0),('step-right-far',.5,0,0,0),('step-back',.5,0,.25,0),('walk-back',.5,0,0,0),('walk-return',.25,0,0,0),('walk-home',0,0,0,0)]
        cases += [('turn-'+str(y),0,0,0,y) for y in [45,90,135,170,179,181,190,225,270,315,360]]
        cases += [('reverse-'+str(y),0,0,0,y) for y in [315,270,225,190,181,179,170,135,90,45,0]]
        for label,x,y,z,angle in cases:
            if time.monotonic()>deadline:raise BotFault('Room-scale deadline')
            a=math.radians(angle)/2;turn={'position':[0,0,0],'orientation':[0,math.sin(a),0,math.cos(a)]}
            target=compose(head,turn);target['position']=[head['position'][i]+v for i,v in enumerate([x,y,z])]
            delta=compose(target,inverse(head))
            current=live.text_result(live.call('get_head_pose',{'base_space':'local'}))['pose']
            targets=[]
            for side in ('left','right'):
                poses=[compose(delta,{k:h[k] for k in ('position','orientation')}) for h in hands if h['hand']==side]
                targets.append({'hand':side,'base_space':'local',**poses[0],'aim_position':poses[1]['position'],'aim_orientation':poses[1]['orientation']})
            live.execute({'op':'motion','seconds':.7,'head':{'offset':[a-b for a,b in zip(target['position'],current['position'])],'orientation':target['orientation']},'hands':targets})
            time.sleep(.12);s=live.observe(native=True)
            if s['scene']!='gameplay' or s.get('menu') or s['native']['player_life']<initial['native']['player_life']:raise BotFault('Room-scale field owner/health changed')
            row={'label':label,'target':target,'state':s}
            if label in ['neutral','sit','stand','rise-from-chair','step-right-far','turn-179','turn-181','turn-270','turn-360','reverse-0']:
                row['eyes']=live.capture('roomscale-'+label)
                down=compose(target,{'position':[0,0,0],'orientation':pointing([0,-1,-.35])})
                live.execute({'op':'motion','seconds':.3,'head':{'offset':[0,0,0],'orientation':down['orientation']},'hands':targets})
                row['body_eyes']=live.capture('roomscale-body-'+label)
                live.execute({'op':'motion','seconds':.3,'head':{'offset':[0,0,0],'orientation':target['orientation']},'hands':targets})
            result['phases'].append(row);atomic_json(live.events.output/'roomscale.json',result)
        live.execute({'op':'action','name':'turn.right','seconds':.15});live.release()
        result['snap_right']=live.observe(native=True)
        live.execute({'op':'action','name':'turn.left','seconds':.15});live.release()
        result['snap_return']=live.observe(native=True)
        result['native_walk']=move_local(live,target_distance=.35)
        result['walk_eyes']=live.capture('roomscale-native-walk')
        result['status']='motion_sequence_completed'
        atomic_json(live.events.output/'roomscale.json',result)
    return result

