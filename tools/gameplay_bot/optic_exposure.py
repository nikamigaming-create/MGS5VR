"""Matched physical-binocular lens sweeps with a stationary simulated head."""
from contextlib import ExitStack
import math
import time

from .core import BotFault
from .live import rotate
from .session import preserved_controller_pose


def multiply(a, b):
    x,y,z,w=a; X,Y,Z,W=b
    return [w*X+x*W+y*Z-z*Y,w*Y-x*Z+y*W+z*X,
            w*Z+x*Y-y*X+z*W,w*W-x*X-y*Y-z*Z]


def require_independent_luminance(exposure):
    """A distinct GPU ring is insufficient when its CPU destination is shared."""
    for head_name,lens_name in (('head_readbacks','lens_readbacks'),
                                ('head_luminance_textures','lens_luminance_textures')):
        head,lens=exposure.get(head_name),exposure.get(lens_name)
        try:
            head=[int(x,0) for x in head];lens=[int(x,0) for x in lens]
        except (TypeError,ValueError):
            raise BotFault('Native lens ownership diagnostic is incomplete: '+lens_name)
        if (len(head)!=4 or len(lens)!=4 or not all(head+lens)
            or len(set(head))!=4 or len(set(lens))!=4 or set(head)&set(lens)):
            raise BotFault('Head and lens share or lack luminance ownership: '+lens_name)


def inspect_optic_exposure(live, seconds=90.):
    """No firing, locomotion, menu navigation, progression edits or native aiming."""
    initial=live.observe(native=True)
    if (initial.get('scene')!='gameplay' or initial.get('camera_active') is not True
        or initial.get('menu') is not False
        or initial.get('controls',{}).get('context')!='gameplay'
        or initial.get('rendered',{}).get('binocular_held') is not False):
        raise BotFault('Optic exposure inspection requires neutral on-foot gameplay without binoculars')
    head=live.text_result(live.call('get_head_pose',{'base_space':'local'}))
    if not all(head.get('flags',{}).get(k) is True for k in ('position_valid','orientation_valid')):
        raise BotFault('Cannot establish the stationary head for exposure comparison')
    exposure,_=live.native.read('inspect-native-exposure')
    original_mode=exposure['experimental_enabled']
    original_lens_rendering=exposure['lens_render_enabled']
    deadline=time.monotonic()+min(seconds,150.)
    samples=[]
    def observe_until(predicate, timeout=5., cleanup=False):
        end=time.monotonic()+timeout
        if not cleanup:end=min(deadline,end)
        while True:
            state=live.observe()
            if predicate(state):return state
            if time.monotonic()>=end:raise BotFault('Physical optic publication did not reach its guarded outcome')
            time.sleep(.04)
    def mode(enabled):
        answer,_=live.native.read('diagnostics:scope-exposure-isolation:'+('on' if enabled else 'off'))
        if answer.get('experimental_enabled') is not enabled:
            raise BotFault('Native exposure diagnostic mode did not apply')
        live.events.emit('optic_exposure_mode',enabled=enabled,state=answer)
        return answer
    def lens_rendering(render):
        answer,_=live.native.read('diagnostics:optic-lens-render:'+('on' if render else 'off'))
        if answer.get('lens_render_enabled') is not render:
            raise BotFault('Native lens render diagnostic mode did not apply')
        live.events.emit('optic_lens_render_mode',enabled=render,state=answer)
    try:
        with ExitStack() as saved:
            for kind in ('grip','aim'):
                saved.enter_context(preserved_controller_pose(live,'right',kind,'local'))
            live.execute({'op':'pose','hand':'right','position':[.149788,-.007438,-.11512],
                          'orientation':[.5,0,0,.8660254],
                          'aim_orientation':[.70710678,0,0,.70710678]})
            live.execute({'op':'action','name':'gameplay.equip_binoculars'})
            observe_until(lambda s:s.get('rendered',{}).get('binocular_held') is True)
            for phase,isolated,render_lens in (('without_lens',False,False),('shared',False,True),('isolated',True,True)):
                mode(isolated)
                lens_rendering(render_lens)
                for pitch in (-25.,25.,-25.,25.):
                    if time.monotonic()>=deadline:raise BotFault('Optic exposure wall-clock limit')
                    angle=math.radians(pitch)/2
                    aim=multiply([math.sin(angle),0,0,math.cos(angle)],[.70710678,0,0,.70710678])
                    position=[.149788,-.007438,-.11512]
                    for correction in range(3):
                        live.execute({'op':'pose','hand':'right','position':position,
                                      'orientation':[.5,0,0,.8660254],'aim_orientation':aim})
                        time.sleep(.18)
                        state=live.observe()
                        ocular=state.get('rendered',{}).get('binocular_ocular',{}).get('position')
                        if not ocular:raise BotFault('Binocular ocular publication is absent')
                        # Keep the real ocular near the right eye while the hand
                        # rotates. Only controller pose is changed, never head,
                        # camera matrices or the native optic aiming controls.
                        h=head['pose']; hq=h['orientation']
                        eye=[a+b for a,b in zip(h['position'],rotate(hq,[.032,0,0]))]
                        target=[a+b for a,b in zip(eye,rotate(hq,[0,0,-.06]))]
                        delta=rotate([-hq[0],-hq[1],-hq[2],hq[3]],[a-b for a,b in zip(target,ocular)])
                        if math.sqrt(sum(v*v for v in delta))<.01:break
                        position=[a+b for a,b in zip(position,delta)]
                        if any(abs(v)>1. for v in position):raise BotFault('Optic feedback left the local hand envelope')
                    before,_=live.native.read('inspect-native-exposure')
                    time.sleep(2.)
                    current_head=live.text_result(live.call('get_head_pose',{'base_space':'local'}))
                    if current_head.get('pose')!=head['pose']:
                        raise BotFault('Head moved during optic-only exposure comparison')
                    captures=live.capture('optic-'+phase.replace('_','-')
                                          +'-look-'+('up' if pitch>0 else 'down')+'-'+str(len(samples)))
                    after,_=live.native.read('inspect-native-exposure')
                    if render_lens and after.get('observed_lens_passes',0)<=before.get('observed_lens_passes',0):
                        raise BotFault('No extra native lens render occurred at the inspected physical ocular')
                    if not render_lens and (after.get('observed_lens_passes')!=before.get('observed_lens_passes')
                                            or after.get('head_passes',0)<=before.get('head_passes',0)):
                        raise BotFault('No-lens reference did not render only advancing head passes')
                    if isolated and (after.get('isolated') is not True or after.get('head_restored') is not True):
                        raise BotFault('Lens adaptation did not restore the head owner exactly')
                    if isolated:require_independent_luminance(after)
                    row={'phase':phase,'isolated':isolated,'lens_rendering':render_lens,
                         'pitch_degrees':pitch,'controller_position':position,
                         'head':current_head,'state':state,'exposure_before':before,
                         'exposure_after':after,'captures':captures}
                    samples.append(row);live.events.emit('optic_exposure_sample',**row)
            # Keep both controller poses in LOCAL space while changing only
            # the simulated head. This checks that isolation has not frozen
            # the ordinary head adaptation or redirected it to the lens.
            for kind in ('grip','aim'):
                fixed=live.text_result(live.call('get_controller_pose',{
                    'hand':'right','pose_type':kind,'base_space':'local'}))
                live.call('set_controller_pose',{'hand':'right','pose_type':kind,
                    'base_space':'local',**fixed['pose']})
            for pitch in (-35.,35.,-35.,35.):
                if time.monotonic()>=deadline:raise BotFault('Head adaptation wall-clock limit')
                angle=math.radians(pitch)/2
                orientation=multiply(head['pose']['orientation'],[math.sin(angle),0,0,math.cos(angle)])
                live.call('set_head_pose',{'base_space':'local',
                    'position':head['pose']['position'],'orientation':orientation})
                time.sleep(3.)
                captures=live.capture('head-isolated-look-'+('up' if pitch>0 else 'down')+'-'+str(len(samples)))
                after,_=live.native.read('inspect-native-exposure')
                row={'isolated':True,'motion':'head_only','head_pitch_degrees':pitch,
                     'head':live.text_result(live.call('get_head_pose',{'base_space':'local'})),
                     'exposure_after':after,'captures':captures}
                samples.append(row);live.events.emit('optic_exposure_sample',**row)
    finally:
        live.release()
        live.call('set_head_pose',{'base_space':'local',**head['pose']})
        restored_head=live.text_result(live.call('get_head_pose',{'base_space':'local'}))
        if restored_head.get('pose')!=head['pose']:
            raise BotFault('Head pose restoration did not match its saved readback')
        live.events.emit('head_pose_restored',pose=head['pose'],verified=True)
        state=live.observe()
        if state.get('rendered',{}).get('binocular_held') is True:
            live.execute({'op':'action','name':'binoculars.stow'})
            observe_until(lambda s:s.get('rendered',{}).get('binocular_held') is False,cleanup=True)
        mode(original_mode)
        lens_rendering(original_lens_rendering)
    return {'status':'observed_optic_exposure','optic':'physical_binoculars',
            'head_stationary_during_optic_sweeps':True,'samples':samples,'visual_acceptance':'pending',
            'scope_firearm_acceptance':False,'physical_headset_acceptance':False}
