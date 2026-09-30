"""Leased file IPC for the isolated Elliott diagnostic runtime.

Requires its runtime-produced PID/pose readback and post-composition capture
adapter. Never connects to the shared Meta operator or injects desktop input.
"""
import base64
import ctypes
import io
import json
import math
import os
from pathlib import Path
import time

from .core import BotFault
from .live import rotate


def multiply(a, b):
    x,y,z,w=a; X,Y,Z,W=b
    return [w*X+x*W+y*Z-z*Y,w*Y-x*Z+y*W+z*X,w*Z+x*Y-y*X+z*W,w*W-x*X-y*Y-z*Z]


def inverse(q):
    return [-q[0],-q[1],-q[2],q[3]]


def compose(a, b):
    return {'position':[x+y for x,y in zip(a['position'],rotate(a['orientation'],b['position']))],
            'orientation':multiply(a['orientation'],b['orientation'])}


def relative(base, pose):
    q=inverse(base['orientation'])
    return {'position':rotate(q,[x-y for x,y in zip(pose['position'],base['position'])]),
            'orientation':multiply(q,pose['orientation'])}


def neutral():
    return dict(stickX=0.,stickY=0.,trigger=0.,squeeze=0.,primary=False,secondary=False,menu=False,thumbClick=False)


class ElliottOperator:
    def __init__(self, data, process_id):
        self.data=Path(data).resolve()
        self.pid=process_id
        self.inputs={'left':neutral(),'right':neutral()}
        self.expiry={'left':0.,'right':0.}
        self.tick=ctypes.windll.kernel32.GetTickCount64
        self.tick.restype=ctypes.c_ulonglong
        self.status()

    def status(self):
        deadline=time.monotonic()+2
        while True:
            try:
                result=json.loads((self.data/'pose_status.json').read_text())
                if result['pid']!=self.pid:
                    raise BotFault('Elliott readback belongs to a different process')
                age=self.tick()-result['tick_ms']
                if not 0<=age<=500:
                    raise BotFault('Elliott readback is stale or its clock changed')
                return result
            except (OSError,json.JSONDecodeError):
                if time.monotonic()>=deadline:raise BotFault('Elliott pose readback unavailable')
                time.sleep(.01)

    def issue(self, filename, payload, command, sequence=None, timeout=2.):
        self.status()
        target=self.data/filename
        if target.exists():
            raise BotFault('An earlier Elliott command is still pending')
        ack_path=self.data/'command_ack.json'
        ack_path.unlink(missing_ok=True)
        temporary=target.with_suffix('.'+str(os.getpid())+'.tmp')
        temporary.write_text(json.dumps(payload),encoding='utf-8')
        os.replace(temporary,target)
        deadline=time.monotonic()+timeout
        while time.monotonic()<deadline:
            try:
                ack=json.loads(ack_path.read_text())
                if ack.get('command')==command and (sequence is None or ack.get('sequence')==sequence):
                    if not ack.get('success'):raise BotFault('Elliott rejected command: '+str(ack))
                    return ack
            except (OSError,json.JSONDecodeError):pass
            time.sleep(.005)
        raise TimeoutError('Elliott command completion unknown: '+command)

    @staticmethod
    def response(value):
        return {'content':[{'type':'text','text':json.dumps(value)}]}

    def call(self, name, args, timeout=15.):
        name=name.removeprefix('openxr_')
        state=self.status()
        if name=='get_instance_info':
            return self.response({'application_name':'MGS5VR','runtime':'Elliott isolated diagnostic runtime','pid':self.pid})
        if name=='get_session_info':
            return self.response({'state':{'name':'XR_SESSION_STATE_FOCUSED'},'pid':self.pid,'frame':state['frame']})
        if name in ('get_head_pose','get_controller_pose'):
            pose=state['head' if name=='get_head_pose' else args['hand']]
            if args.get('base_space')=='view':pose=relative(state['head'],pose)
            return self.response(pose|{'flags':{'position_valid':True,'orientation_valid':True},
                                      'source':'runtime_readback','frame':state['frame']})
        if name=='set_controller_input':
            hand=args['hand']; now=time.monotonic()
            if now>=self.expiry[hand]:self.inputs[hand]=neutral()
            component=args['component']
            key={'A':'primary','X':'primary','B':'secondary','Y':'secondary','Menu':'menu',
                 'ThumbstickClick':'thumbClick','Grip':'squeeze','Trigger':'trigger'}.get(component)
            if component=='Thumbstick':key='stick'+args['sub_component']
            if key not in neutral():raise BotFault('Unsupported Elliott input '+str(component))
            value=args['value']
            if not math.isfinite(value) or not (-1 if key.startswith('stick') else 0)<=value<=1:
                raise BotFault('Invalid Elliott input value')
            lease=float(args.get('hold_duration',.5)) if args.get('auto_release') else .5
            if not 0<lease<=5:raise BotFault('Elliott inputs require a bounded lease')
            self.inputs[hand][key]=bool(value) if isinstance(neutral()[key],bool) else value
            seq=time.time_ns()//1000
            payload=self.inputs[hand]|{'hand':0 if hand=='left' else 1,'sequence':seq,'lease_ms':max(30,round(lease*1000))}
            answer=self.issue('controller_pose_command.json',payload,'controller_state',seq,timeout)
            self.expiry[hand]=now+lease
            return self.response(answer)
        if name=='set_controller_pose':
            hand=args['hand'];pose=state[hand]
            if args.get('base_space')=='view':pose=relative(state['head'],pose)
            pose={'position':args.get('position',pose['position']),'orientation':args.get('orientation',pose['orientation'])}
            if args.get('base_space')=='view':pose=compose(state['head'],pose)
            if any(not math.isfinite(v) for v in pose['position']+pose['orientation']) or abs(sum(x*x for x in pose['orientation'])-1)>.001:
                raise BotFault('Invalid controller pose')
            seq=time.time_ns()//1000
            payload={'hand':0 if hand=='left' else 1,'sequence':seq,'lease_ms':500,'neutral':True}
            payload.update(zip(('worldX','worldY','worldZ'),pose['position']))
            payload.update(zip(('qx','qy','qz','qw'),pose['orientation']))
            answer=self.issue('controller_pose_command.json',payload,'controller_state',seq,timeout)
            self.inputs[hand]=neutral();self.expiry[hand]=0
            time.sleep(.04)
            return self.response(answer)
        if name=='set_head_pose':
            pose=state['head']
            position=args.get('position',pose['position']);q=args.get('orientation',pose['orientation'])
            if args.get('base_space','local')!='local':raise BotFault('Elliott head commands require LOCAL space')
            if any(not math.isfinite(v) for v in position+q) or abs(sum(x*x for x in q)-1)>.001:
                raise BotFault('Invalid head pose')
            x,y,z,w=q
            payload=dict(zip(('x','y','z'),position))
            payload.update(pitch=math.asin(max(-1,min(1,2*(w*x-y*z)))),
                           yaw=math.atan2(2*(w*y+x*z),1-2*(x*x+y*y)),
                           roll=math.atan2(2*(w*z+x*y),1-2*(x*x+z*z)))
            answer=self.issue('head_pose_command.json',payload,'head_pose',timeout=timeout)
            time.sleep(.04)
            return self.response(answer)
        if name=='capture_composited_image':
            from PIL import Image
            target=self.data/'screenshot_request.json'
            if target.exists():raise BotFault('An earlier Elliott capture is still pending')
            output=self.data/'screenshot.bmp'
            prior=output.stat().st_mtime_ns if output.exists() else 0
            temporary=target.with_suffix('.tmp');temporary.write_text('{"eye":"both","layer":"all"}')
            os.replace(temporary,target)
            deadline=time.monotonic()+timeout
            while time.monotonic()<deadline:
                try:
                    if output.stat().st_mtime_ns>prior and not target.exists():
                        with Image.open(output) as frame:
                            frame.load();width,height=frame.size
                            if width%2:raise BotFault('Elliott SBS width is odd')
                            start=0 if args['eye']=='left' else width//2
                            stream=io.BytesIO();frame.crop((start,0,start+width//2,height)).save(stream,format='PNG')
                            return {'content':[{'type':'image','mimeType':'image/png','data':base64.b64encode(stream.getvalue()).decode()}]}
                except (OSError,ValueError):pass
                time.sleep(.01)
            raise TimeoutError('Elliott compositor capture completion unknown')
        raise BotFault('Unsupported Elliott operation: '+name)

    def close(self):
        pass
