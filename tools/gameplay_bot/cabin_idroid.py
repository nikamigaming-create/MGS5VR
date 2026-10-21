"""Exercise immediate ACC handset access through effective VR bindings."""
import math
import time
from .core import BotFault, atomic_json
from .idroid import device_orientation
from .session import preserved_tracking_poses
from .cabin_workbench import compose, inverse_pose


def inspect(live, behavior):
    result={'status':'running','phases':[],'captures':[],
            'visual_acceptance':'pending_both_eye_review','headset_acceptance':'not_run'}
    def record(label):
        state=live.observe(native=True)
        result['phases'].append({'label':label,'state':state})
        atomic_json(live.events.output/'cabin-idroid-progress.json',result)
        result['captures']+=live.capture(label)
        if state.get('idroid'):
            frame=state['rendered']
            target=compose(compose(frame['native_pose'],{'position':[0.,0.,0.],'orientation':[0.,1.,0.,0.]}),
                           compose(inverse_pose(frame['head_pose']),frame['left_grip']))
            a=target['orientation'];b=frame['left_palm']['orientation']
            cosine=abs(sum(x*y for x,y in zip(a,b)))/math.sqrt(sum(x*x for x in a)*sum(x*x for x in b))
            error=math.degrees(2*math.acos(min(1.,cosine)))
            result['phases'][-1]['palm_orientation_error_degrees']=error
            if error>2.:
                raise BotFault('iDroid rotated the palm away from its tracked grip: %.2f degrees'%error)
        return state
    def wait_hand(target):
        until=time.monotonic()+3.
        while time.monotonic()<until:
            state=live.observe();frame=state.get('rendered') or {}
            point=frame.get('left_grip',{}).get('position',[])
            if len(point)==3 and math.dist(point,target)<.015 and 0<=state['now_ms']-frame.get('sample_ms',0)<150:
                return
            time.sleep(.04)
        raise BotFault('ACC rendered hand did not consume the requested tracked pose')
    with preserved_tracking_poses(live):
        try:
            live.release()
            for cycle in range(2):
                live.execute({'op':'pose','hand':'left','position':[-.12,-.18,-.50],
                              'orientation':device_orientation()})
                live.execute({'op':'action','name':'system.idroid'})
                behavior.wait_for({'idroid':True,'idroid_menu_input_ready':True},8,'cabin-idroid-open')
                wait_hand([-.12,-.18,-.50])
                opened=record('cabin-idroid-open-'+str(cycle))
                if not opened.get('rendered',{}).get('idroid_device_tracked'):
                    raise BotFault('ACC iDroid opened without its tracked left-hand device')
                live.execute({'op':'pose','hand':'left','position':[-.20,-.12,-.45],
                              'orientation':device_orientation(20)})
                wait_hand([-.20,-.12,-.45])
                moved=record('cabin-idroid-hand-motion-'+str(cycle))
                a=opened['rendered']['idroid_device']['position'];b=moved['rendered']['idroid_device']['position']
                if math.dist(a,b)<.02:raise BotFault('ACC handset did not follow the left hand')
                live.execute({'op':'action','name':'system.idroid'})
                behavior.wait_for({'idroid':False,'scene':'cabin'},8,'cabin-idroid-stow')
                record('cabin-idroid-stowed-'+str(cycle))
            live.execute({'op':'action','name':'system.idroid'})
            behavior.wait_for({'idroid':True,'idroid_menu_input_ready':True},8,'cabin-idroid-walk-open')
            before=record('cabin-idroid-walk-before')
            axis=next(row['source'] for row in live.bindings['axes'] if row['name']=='axes.move')
            hand=axis.split('_')[0]
            live.input([{'hand':hand,'component':'Thumbstick','sub_component':'X','value':.65}],.5)
            # input() starts an expiring lease; it does not wait out the hold.
            # Releasing immediately measured RPC latency instead of movement.
            time.sleep(.5)
            live.release();time.sleep(.15)
            after=record('cabin-idroid-walk-after')
            a=before['rendered']['native_pose']['position'];b=after['rendered']['native_pose']['position']
            result['walk_distance']=math.dist(a,b)
            if result['walk_distance']<.015:raise BotFault('ACC locomotion is blocked while iDroid is open without focus grip')
            result['status']='observed_cabin_idroid'
        except Exception as error:
            result.update(status='failed',error=str(error))
        finally:
            live.release()
            if live.observe().get('idroid'):
                try:
                    live.execute({'op':'action','name':'system.idroid'})
                    behavior.wait_for({'idroid':False},8,'cabin-idroid-cleanup')
                except Exception as error:
                    result.update(status='failed',cleanup_error=str(error))
            atomic_json(live.events.output/'cabin-idroid-progress.json',result)
    return result
