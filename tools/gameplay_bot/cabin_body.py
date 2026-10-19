"""Bounded cabin body inspection using tracked head/controller motion."""
import math
import time
from .core import BotFault, atomic_json
from .combat import pointing, mul
from .session import preserved_tracking_poses
from .cabin_workbench import compose


def inspect(live, seconds=120):
    result={'status':'running','phases':[],'headset_acceptance':'not_run',
            'visual_acceptance':'pending_both_eye_review','recenter_count':0}
    deadline=time.monotonic()+min(seconds,180.)
    with preserved_tracking_poses(live):
        try:
            live.release()
            if live.observe()['scene']!='cabin':raise BotFault('Cabin body inspection requires a closed-menu ACC')
            head=live.text_result(live.call('get_head_pose',{'base_space':'local'}))['pose']
            cases=[('seated',0.,0.,0.,0.),('standing',0.,.45,0.,0.),
                   ('walk-right',.25,.45,0.,0.),('walk-back',.25,.45,.25,0.),
                   ('walk-home',0.,.45,0.,0.)]
            cases += [('turn-'+str(angle),0.,.45,0.,math.radians(angle))
                      for angle in (90,179,181,270,360)]
            cases += [('reverse-'+str(angle),0.,.45,0.,math.radians(angle))
                      for angle in (270,181,179,90,0)]
            cases += [('seated-return',0.,0.,0.,0.)]
            for label,x,height,z,yaw in cases:
                if time.monotonic()>=deadline:raise BotFault('Cabin body inspection deadline')
                target={'position':[head['position'][0]+x,head['position'][1]+height,head['position'][2]+z],
                        'orientation':mul(head['orientation'],mul([0.,math.sin(yaw/2),0.,math.cos(yaw/2)],pointing([0.,-1.,-.35])))}
                live.call('set_head_pose',{'base_space':'local',**target})
                for hand,x in [('left',-.24),('right',.24)]:
                    grip=compose({'position':target['position'],'orientation':mul(head['orientation'],[0.,math.sin(yaw/2),0.,math.cos(yaw/2)])},
                                 {'position':[x,-.32,-.34],'orientation':[.5,0.,0.,.8660254]})
                    for kind in ('grip','aim'):live.call('set_controller_pose',{'hand':hand,'pose_type':kind,'base_space':'local',**grip})
                until=min(deadline,time.monotonic()+4.)
                while True:
                    s=live.observe();f=s.get('rendered') or {};h=f.get('head_pose') or {}
                    if s['scene']!='cabin':raise BotFault('Cabin body inspection lost its scene')
                    if (len(h.get('position',[]))==3 and math.dist(h['position'],target['position'])<.01
                            and abs(sum(a*b for a,b in zip(h.get('orientation',[]),target['orientation'])))>.999
                            and 0<=s['now_ms']-f.get('sample_ms',0)<150
                            and f.get('full_body',{}).get('valid') is True
                            and f.get('left_palm_tracked') is True
                            and f.get('right_palm_tracked') is True):break
                    if time.monotonic()>until:raise BotFault('Cabin body/head/hands did not settle together: '+label)
                    time.sleep(.05)
                result['phases'].append({'label':label,'state':s,'eyes':live.capture('cabin-body-'+label)})
                atomic_json(live.events.output/'cabin-body-progress.json',result)
            result['status']='observed_cabin_body'
            result['full_body_admitted']=all(p['state']['rendered'].get('full_body',{}).get('valid') for p in result['phases'])
        except Exception as error:
            result.update(status='failed',error=str(error))
            raise
        finally:
            live.release()
            atomic_json(live.events.output/'cabin-body-progress.json',result)
    return result
