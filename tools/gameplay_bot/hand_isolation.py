"""Observe right-wrist influence with the HMD and opposite controller fixed."""
import math
import time
from .core import BotFault, atomic_json, matches
from .session import preserved_tracking_poses, _controller_pose
from .combat import controller_pair, mul
from .arsenal import human_hand
from .live import rotate


def inspect(live,seconds,policy):
    deadline=time.monotonic()+min(seconds,60.)
    result={'status':'running','phases':[], 'physical_headset_acceptance':False}
    live.release()
    def observe():
        s=live.observe(native=True)
        if (time.monotonic()>deadline or not matches(s['native'],policy['world'])
                or any(s.get(k) for k in ('menu','demo','loading','idroid'))
                or not s.get('camera_active') or s.get('camera_suspended')
                or any(s['controls']['physical']) or any(s['controls']['native_triggers'])
                or s['rendered'].get('binocular_held')):
            raise BotFault('Wrist isolation lost its neutral field owner')
        if not s['rendered']['full_body']['valid']:
            raise BotFault('Wrist isolation requires an admitted full-body frame')
        return s
    try:
        observe()
        with preserved_tracking_poses(live):
            pairs={h:controller_pair(_controller_pose(live,h,'grip','local'),
                                    _controller_pose(live,h,'aim','local')) for h in ('left','right')}
            human_hand(live,'left',[-.3,-.2,-.5],pairs['left']['orientation'],pairs['left'])
            for angle in (0,30,-30,0):
                yaw=math.radians(angle)/2
                human_hand(live,'right',[.3,-.2,-.5],
                           mul([0.,math.sin(yaw),0.,math.cos(yaw)],pairs['right']['orientation']),pairs['right'])
                samples=[]
                end=time.monotonic()+2.
                while time.monotonic()<end:
                    samples.append(observe());time.sleep(.1)
                state=samples[-1];f=state['rendered']
                forward=rotate(f['body_chest']['orientation'],[0,0,1])
                row={'right_wrist_yaw_degrees':angle,'state':state,'samples':samples,
                     'chest_yaw_degrees':math.degrees(math.atan2(forward[0],forward[2])),
                     'eyes':live.capture('right-wrist-only-'+str(len(result['phases'])))}
                result['phases'].append(row)
                atomic_json(live.events.output/'hand-isolation.json',result)
            baseline=result['phases'][0]['state']['rendered']
            for row in result['phases']:
                f=row['state']['rendered']
                for label in ('head_pose','left_grip','left_palm'):
                    row[label+'_position_delta']=math.dist(f[label]['position'],baseline[label]['position'])
                if row['head_pose_position_delta']>.001 or row['left_grip_position_delta']>.001:
                    raise BotFault('Independent wrist test moved the HMD or opposite input')
            result['status']='observed_right_wrist_isolation'
    except Exception as error:
        result.update(status='failed',error=str(error));raise
    finally:
        live.release();atomic_json(live.events.output/'hand-isolation.json',result)
    return result
