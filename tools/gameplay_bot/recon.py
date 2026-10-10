"""Physical binocular reconnaissance, with native acquisition before mark input."""
import re
import time

from .binocular_presentation import BinocularPresentation
from .combat import fresh_rig
from .core import BotFault, atomic_json
from .health import HealthRecoveryNeeded, needs_recovery
from .locomotion import _validate_xr_scene_state
from .session import preserved_tracking_poses


class MarkerLog:
    """Read only new evidence from this process's already-owned game log."""
    def __init__(self,path):
        self.path=path
        self.offset=path.stat().st_size
        with path.open('rb') as stream:
            stream.seek(max(0,self.offset-128))
            self.anchor=stream.read(self.offset-max(0,self.offset-128))

    def read(self):
        with self.path.open('rb') as stream:
            stream.seek(max(0,self.offset-len(self.anchor)))
            if stream.read(len(self.anchor))!=self.anchor:
                raise BotFault('Marker log rotated or changed; current acquisition evidence unavailable')
            return stream.read(2*1024*1024).decode('utf-8',errors='replace')


def marked_lines(text,actor_id):
    return [line for line in text.splitlines() if re.search(
        r'Physical optic native person mark id='+str(actor_id)+r' result=\d+ acquired=(?:true|1)\b',line)]


def observed(live,anchor,healthy_life,*,cleanup=False):
    state,_=fresh_rig(live,anchor)
    _validate_xr_scene_state(state,activation=anchor[2])
    native=state['native'];frame=state.get('rendered') or {};controls=state['controls']
    if ((native.get('mission'),native.get('location'),state['activation'],
         frame.get('player_owner'),frame.get('presentation_epoch'))!=anchor
        or controls.get('context') not in ('gameplay','binoculars')
        or controls.get('rig_input') is not True or controls.get('travel_mode')!=1
        or frame.get('presentation_focused') is not True
        or type(frame.get('sample_ms')) is not int or not 0<=state['now_ms']-frame['sample_ms']<=150
        or native.get('popup') is not False or native.get('saving') is not False
        or native.get('player_vehicle_id')!=65535 or native.get('game_over')!=0):
        raise BotFault('Reconnaissance lost its native field or binocular owner')
    if not cleanup:
        if needs_recovery(native,healthy_life):raise HealthRecoveryNeeded('Damage takes priority over reconnaissance')
        if native.get('not_alert') is not True:raise BotFault('Detection interrupted the stealth reconnaissance')
    return state


def inspect(live,enemies,*,output,anchor,camp,healthy_life,max_seconds=28.):
    output.mkdir(parents=True,exist_ok=True)
    result={'status':'running','targets':[],'captures':[],
            'visual_acceptance':'pending_both_eye_and_motion_review','optic':'physical_handheld_binoculars',
            'binocular_presentation':'measured_controller_pairs_and_authored_support',
            'physical_headset_acceptance':False}
    started=time.monotonic()
    automatic=next((row['value'] for row in live.bindings['settings']
                    if row['name']=='settings.binocular_auto_mark'),None)
    if type(automatic) not in (int,float) or automatic<.5:
        raise BotFault('Current recon requires the established automatic visible-target acquisition setting')
    targets=[row for row in enemies['soldiers'] if row.get('camp')==camp
             and row['life']==enemies['normal_life']]
    presentation=None
    try:
        with preserved_tracking_poses(live):
            try:
                live.release();observed(live,anchor,healthy_life)
                presentation=BinocularPresentation(live,lambda:observed(live,anchor,healthy_life))
                presentation.raise_to_eye()
                live.execute({'op':'action','name':'gameplay.equip_binoculars','seconds':.6})
                deadline=time.monotonic()+3.
                while True:
                    state=observed(live,anchor,healthy_life)
                    if state['rendered'].get('binocular_held') is True and state['controls']['context']=='binoculars':break
                    if time.monotonic()>=deadline:raise BotFault('Physical binoculars did not equip')
                    time.sleep(.04)
                result['before']=state
                presentation.align_to_ocular()
                presentation.hold_support()
                for actor in targets[:6]:
                    if time.monotonic()-started>=max_seconds:break
                    state=observed(live,anchor,healthy_life)
                    cursor=MarkerLog(live.game/'mgs5vr.log')
                    # Actor root + eye-height remains a planning hypothesis.
                    # The native physical optic ray owns visibility/acquisition.
                    target=[actor['position'][0],actor['position'][1]+1.35,actor['position'][2]]
                    presentation.look_at(target)
                    presentation.align_to_ocular()
                    until=min(started+max_seconds,time.monotonic()+1.3);acquired=[]
                    while time.monotonic()<until:
                        presentation.support_observation()
                        state=observed(live,anchor,healthy_life)
                        acquired=marked_lines(cursor.read(),actor['id'])
                        if acquired:break
                        time.sleep(.05)
                    row={'id':actor['id'],'name':actor['name'],'planning_point':target,
                         'native_acquisition_lines':acquired,'state':state,'explicit_mark':False,
                         'support':presentation.support_observation()}
                    if acquired:
                        explicit=MarkerLog(live.game/'mgs5vr.log')
                        presentation.press('binoculars.mark',acquired=True,seconds=.25)
                        time.sleep(.08)
                        row['native_explicit_mark_lines']=marked_lines(explicit.read(),actor['id'])
                        row['explicit_mark']=bool(row['native_explicit_mark_lines'])
                    else:row['scope']='no fresh acquisition; no blind terrain marker input'
                    presentation.support_observation()
                    row['captures']=live.capture('recon-person-'+str(actor['id']),static_grips=True)
                    result['captures']+=row['captures'];result['targets'].append(row)
                    live.events.emit('autonomous_recon_target',**row)
                state=observed(live,anchor,healthy_life)
                before=state['rendered']['binocular_magnification']
                presentation.press('binoculars.zoom',seconds=.25)
                time.sleep(.1);state=observed(live,anchor,healthy_life)
                result['zoom']={'before':before,'after':state['rendered']['binocular_magnification']}
                if result['zoom']['after']==before:
                    raise BotFault('Physical binocular zoom input was sampled but magnification did not change')
                presentation.support_observation()
                result['captures']+=live.capture('recon-zoom',static_grips=True)
                result['status']='observed_physical_recon'
                result['native_marked_count']=sum(row['explicit_mark'] for row in result['targets'])
            finally:
                # Stow while the saved inspection head/hands still own the
                # device. Restoring the HMD first caused a 136–174 degree snap.
                live.release()
                state=observed(live,anchor,healthy_life,cleanup=True)
                if state['controls']['context']=='binoculars':
                    live.execute({'op':'action','name':'binoculars.stow','seconds':.25})
                    until=time.monotonic()+3.
                    while True:
                        state=observed(live,anchor,healthy_life,cleanup=True)
                        if state['rendered'].get('binocular_held') is False:break
                        if time.monotonic()>=until:raise BotFault('Recon cleanup did not stow the physical binoculars')
                        time.sleep(.04)
                if presentation is not None:
                    presentation.observe=lambda:observed(live,anchor,healthy_life,cleanup=True)
                    presentation.restore()
                result['cleanup']={'binocular_stowed':state['rendered'].get('binocular_held') is False,
                                   'head_and_controller_pairs_restored_smoothly':presentation is not None}
    except Exception as error:
        result['status']='failed';result['error']=str(error)
    finally:
        live.release()
        result['seconds']=time.monotonic()-started
        atomic_json(output/'result.json',result)
    return result
