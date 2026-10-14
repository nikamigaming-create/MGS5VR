"""Bounded physical camera/gallery/disposal checks in actual native scenes."""
import math
import time
from .core import BotFault, atomic_json
from .live import rotate
from .combat import pointing, mul
from .motion import _blend
from .session import preserved_tracking_poses, _controller_pose
from .cabin_workbench import compose, inverse_pose, shifted


def inspect_camera_manager(live, policy, seconds=120.):
    deadline=time.monotonic()+min(seconds,180.)
    result={'status':'running','phases':[],'scope':policy,'headset_acceptance':'not_run'}
    movie=None
    held=set()
    pairs={hand:compose(inverse_pose(_controller_pose(live,hand,'grip','local')),
        _controller_pose(live,hand,'aim','local')) for hand in ('left','right')}
    ready_until=min(deadline,time.monotonic()+30.)
    while True:
        ready=live.observe()
        if ready.get('scene')==policy['scene'] and ready.get('rendered',{}).get('studio',{}).get('available'):break
        if time.monotonic()>ready_until:raise BotFault('Selected native scene did not publish a ready camera')
        time.sleep(.10)
    def observe():
        if time.monotonic()>deadline:raise BotFault('Camera manager deadline')
        s=live.observe()
        if s['scene']!=policy['scene'] or s.get('menu') or s.get('loading'):
            raise BotFault('Camera manager lost the selected native scene')
        return s
    def studio():
        s=observe();v=(s.get('rendered') or {}).get('studio') or {}
        if not v.get('available'):raise BotFault('Current source-owned camera unavailable')
        return v
    def renew():
        if held:live.input([{'hand':hand,'component':'Grip','value':1.} for hand in sorted(held)],.03,lease_seconds=5.)
    def grip(hand,active=True):
        if active:held.add(hand)
        else:held.discard(hand)
        live.release(keep=tuple((h,'Grip',None) for h in held));renew();time.sleep(.12)
    def wait(predicate,label,timeout=5.):
        until=min(deadline,time.monotonic()+timeout)
        while time.monotonic()<until:
            renew();s=observe()
            if predicate(s):
                result['phases'].append({'label':label,'state':s})
                atomic_json(live.events.output/'camera-manager-progress.json',result);return s
            time.sleep(.05)
        raise BotFault('Camera manager outcome missing: '+label)
    def move(hand,target,seconds=.45):
        start=_controller_pose(live,hand,'grip','local');began=time.monotonic()
        for n in range(1,9):
            renew();pose=_blend(start,target,n/8)
            for kind,p in (('grip',pose),('aim',compose(pose,pairs[hand]))):
                live.call('set_controller_pose',{'hand':hand,'pose_type':kind,'base_space':'local',**p})
            time.sleep(max(0.,began+seconds*n/8-time.monotonic()))
        time.sleep(.15)
    def aim(hand,pose):move(hand,compose(pose,inverse_pose(pairs[hand])),.25)
    def trigger(hand):
        renew();live.input([{'hand':h,'component':'Grip','value':1.} for h in held]
            +[{'hand':hand,'component':'Trigger','value':1.}],.13,lease_seconds=5.)
        live.release(keep=tuple((h,'Grip',None) for h in held));time.sleep(.12)
    def checkpoint(label):
        renew();s=observe();eyes=live.capture(label,static_grips=bool(held));renew()
        result['phases'].append({'label':label,'state':s,'eyes':eyes})
        atomic_json(live.events.output/'camera-manager-progress.json',result)
    def button(index):
        camera=studio()['camera'];target=shifted(camera,[.029 if index%2 else -.029,-.025-.019*(index//2),.28])
        aim('left',target)
        wait(lambda s:s['rendered']['studio'].get('hover_control')==index,'button-hover-'+str(index),2.)
        trigger('left')
    def approach(target):
        start=live.text_result(live.call('get_head_pose',{'base_space':'local'}))['pose']
        position=[a+b for a,b in zip(target['position'],[0.,.55,.35])]
        end={'position':position,'orientation':pointing([a-b for a,b in zip(target['position'],position)])}
        delta=[a-b for a,b in zip(position,start['position'])]
        saved={h:_controller_pose(live,h,'grip','local') for h in ('left','right')}
        for n in range(1,9):
            live.call('set_head_pose',{'base_space':'local',**_blend(start,end,n/8)})
            for hand,p in saved.items():
                moved={**p,'position':[a+b*n/8 for a,b in zip(p['position'],delta)]}
                for kind,pose in (('grip',moved),('aim',compose(moved,pairs[hand]))):
                    live.call('set_controller_pose',{'hand':hand,'pose_type':kind,'base_space':'local',**pose})
            time.sleep(.03)
        time.sleep(.25)
    def key(index):
        v=studio();p=shifted(v['keyboard'],[(index%11-5)*.045,(.5-index//11)*.045,.25])
        aim('right',compose(v['room'],p));trigger('right')
    with preserved_tracking_poses(live):
        try:
            live.release();initial=live.observe(native=True)
            result['native_scene']=initial.get('native');mode=policy.get('mode','gallery')
            if mode=='reload':
                v=studio()
                if v['library']!=policy['library']:raise BotFault('Digital library changed after reload')
                if len(v['papers'])!=len(policy['papers']):raise BotFault('Physical collection changed after reload')
                for expected in policy['papers']:
                    if not any(all(p.get(k)==value for k,value in expected.items()) for p in v['papers']):
                        raise BotFault('Independent physical object missing after reload')
                checkpoint('independent-library-and-papers-restored');result['status']='observed_camera_reload';return result
            if mode=='binoculars':
                grip('right');checkpoint('ACC-right-grip-with-physical-binocular-stowed')
                state=live.observe(native=True)
                if (state['controls']['native_triggers']!=[0,0] or state['native']['status_BINOCLE']
                        or state['rendered'].get('binocular_held')):raise BotFault('Right grip leaked stock binocular/aim input in ACC')
                grip('right',False)
                live.execute({'op':'action','name':'gameplay.equip_binoculars','seconds':.6})
                wait(lambda s:s['controls']['context']=='binoculars' and s['rendered'].get('binocular_held'),
                    'ACC-physical-VR-binocular-equipped')
                checkpoint('ACC-physical-VR-binocular-only')
                live.execute({'op':'action','name':'binoculars.stow','seconds':.25})
                wait(lambda s:not s['rendered'].get('binocular_held'),'ACC-physical-binocular-stowed')
                result['status']='observed_ACC_binocular_routing';return result
            camera=studio()['camera']
            if initial['scene'] in ('title','cabin'):approach(camera);camera=studio()['camera']
            move('right',shifted(camera,[.076,-.007,.010]));grip('right')
            wait(lambda s:s['rendered']['studio']['camera_hand']==1,'camera-grabbed')
            head=live.text_result(live.call('get_head_pose',{'base_space':'local'}))['pose']
            # Upright, comfortable rear-control presentation, within arm reach.
            target=shifted(head,[.17,-.14,-.43]);target['orientation']=head['orientation']
            move('right',target);checkpoint('camera-rear-controls-live')
            if policy.get('film'):
                from .cabin_film import CabinFilm
                movie=CabinFilm(live)
                movie.begin('native-world-freeze-and-tracked-camera' if mode=='freeze' else 'camera-gallery-and-independent-print')
            if mode=='freeze':
                button(5)
                frozen=wait(lambda s:s['rendered']['studio']['freeze_state']==2
                    and s['rendered'].get('camera_world_frozen') and not s['camera_suspended'],'world-frozen-tracked-rig',5.)
                before=live.observe(native=True);result['freeze_native_before']=before['native']
                clock_before=live.native.read("return tostring(TppClock.GetTime('number'))")[0]
                time.sleep(.5);renew()
                for offset in ([.05,.02,0.],[-.05,.01,0.]):
                    new_head=shifted(head,offset)
                    live.call('set_head_pose',{'base_space':'local',**new_head})
                    move('right',shifted(target,[-offset[0],0.,0.]))
                    wait(lambda s:not s['camera_suspended'] and s['rendered'].get('right_palm_tracked')
                        and s['rendered'].get('camera_world_frozen'),'frozen-head-and-camera-motion')
                    checkpoint('frozen-motion-right' if offset[0]>0 else 'frozen-motion-left')
                after=live.observe(native=True);result['freeze_native_after']=after['native']
                clock_after=live.native.read("return tostring(TppClock.GetTime('number'))")[0]
                result['frozen_clock']=[clock_before,clock_after]
                if abs(clock_after-clock_before)>.001:raise BotFault('Native game clock advanced while frozen')
                if any(abs(after['native']['player_'+a]-before['native']['player_'+a])>.01 for a in 'xyz'):
                    raise BotFault('Frozen world player moved')
                original=studio()['photo'];button(4);trigger('right')
                wait(lambda s:s['rendered']['studio']['photo']>original,'frozen-native-photo',8.)
                checkpoint('camera-photo-with-world-frozen')
                if movie:movie.mark('shutter-world-frozen')
                button(5);wait(lambda s:not s['rendered'].get('camera_world_frozen')
                    and not s['camera_suspended'],'world-live-again',5.)
                clock_resume=live.native.read("return tostring(TppClock.GetTime('number'))")[0]
                renew();time.sleep(.6);clock_running=live.native.read("return tostring(TppClock.GetTime('number'))")[0]
                result['resumed_clock']=[clock_resume,clock_running]
                if initial['scene']=='gameplay' and clock_running<=clock_resume:
                    raise BotFault('Native game clock did not resume after camera Freeze')
                button(5);wait(lambda s:s['rendered'].get('camera_world_frozen'),'refrozen-before-stow',5.)
                grip('right',False)
                wait(lambda s:not s['rendered'].get('camera_world_frozen') and not s['camera_suspended'],'stow-releases-camera-freeze',5.)
                if movie:movie.mark('world-live-and-camera-stowed');movie.end();result['film']=movie.takes
                result['status']='observed_camera_freeze';return result
            first=studio()['photo'];trigger('right')
            one=wait(lambda s:s['rendered']['studio']['photo']>first,'first-live-photo',8.)['rendered']['studio']['photo']
            time.sleep(1.05);move('right',shifted(target,[.025,0.,0.]))
            trigger('right')
            two=wait(lambda s:s['rendered']['studio']['photo']>one,'second-live-photo',8.)['rendered']['studio']['photo']
            button(0);wait(lambda s:s['rendered']['studio']['selected_photo']==one,'browse-previous-photo')
            checkpoint('camera-gallery-previous');before_papers=len(studio()['papers'])
            if movie:movie.mark('browse-and-reprint')
            button(2);wait(lambda s:len(s['rendered']['studio']['papers'])==before_papers+1,'reprint-older-photo')
            button(1);wait(lambda s:s['rendered']['studio']['selected_photo']==two,'browse-next-photo')
            button(3);v=studio()
            if v['confirm_photo']!=two or two not in v['library']:raise BotFault('Delete did not ask before erasing')
            checkpoint('camera-confirm-delete');button(4)
            if movie:movie.mark('delete-confirmation-cancelled')
            if studio()['confirm_photo'] or two not in studio()['library']:raise BotFault('Live failed to cancel deletion')
            button(3);button(3)
            v=wait(lambda s:two not in s['rendered']['studio']['library'],'confirmed-digital-deletion-keeps-prints')['rendered']['studio']
            if not any(p['photo']==two for p in v['papers']):raise BotFault('Deleting a saved photo removed its physical print')
            checkpoint('camera-deleted-digital-photo-kept-physical-prints')
            if movie:movie.mark('digital-photo-deleted-print-retained')
            result['deleted_photo']=two;result['library']=v['library'];result['papers']=v['papers']
            if initial['scene']=='gameplay':
                offered=next((p for p in v['papers'] if p.get('field')),None)
                if not offered:raise BotFault('The reprint was not offered at the field camera')
                move('left',offered['field_pose']);grip('left')
                wait(lambda s:any(p['id']==offered['id'] and p['hand']==0 for p in s['rendered']['studio']['papers']),
                    'field-reprint-grabbed')
                move('left',shifted(head,[-.17,-.15,-.42]));checkpoint('field-independent-print-held')
                grip('left',False)
                wait(lambda s:any(p['id']==offered['id'] and p['hand']==-1 and not p['field']
                    for p in s['rendered']['studio']['papers']),'field-print-stowed-in-cabin-collection')
            grip('right',False)
            if initial['scene'] in ('title','cabin'):
                v=studio();note_home=compose(v['room'],v['notes_home']);approach(note_home)
                v=studio();note_home=compose(v['room'],v['notes_home']);move('left',note_home);grip('left')
                picked=wait(lambda s:any(p['hand']==0 and not p['photo'] for p in s['rendered']['studio']['papers']),'note-peeled')
                note=next(p for p in picked['rendered']['studio']['papers'] if p['hand']==0 and not p['photo']);nid=note['id']
                # Preserve the authored paper grip and use its visible face.
                v=studio();face=compose(v['room'],note['pose']);aim('right',shifted(face,[0.,0.,.25]));trigger('right')
                wait(lambda s:s['rendered']['studio']['editor']==nid,'note-editor-open')
                key(43);wait(lambda s:s['rendered']['studio']['confirm_paper']==nid,'note-disposal-confirmation')
                checkpoint('sticky-note-delete-confirmation');key(33)
                if studio()['confirm_paper']:raise BotFault('Cancel did not preserve the note')
                key(43);key(43)
                wait(lambda s:all(p['id']!=nid for p in s['rendered']['studio']['papers']),'confirmed-note-permanently-removed')
                grip('left',False);result['deleted_note']=nid
            if movie:movie.mark('gallery-complete-camera-stowed');movie.end();result['film']=movie.takes
            result['status']='observed_camera_manager'
            result['retained']={'library':studio()['library'],'papers':[{'id':p['id'],'photo':p['photo'],'text':p['text']} for p in studio()['papers']]}
            return result
        finally:
            if movie and movie.active:movie.end()
            held.clear();live.release()
            # Stow releases the owned freeze; cleanup never opens native Pause.
            until=time.monotonic()+3.
            while time.monotonic()<until:
                if not live.observe().get('rendered',{}).get('camera_world_frozen'):break
                time.sleep(.05)
            atomic_json(live.events.output/'camera-manager-progress.json',result)
