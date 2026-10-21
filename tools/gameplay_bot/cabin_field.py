"""Bounded isolated-field check for physical binocular markers and the camera."""
import json
import math
import subprocess
import time

from .binocular_presentation import BinocularPresentation, inspection_poses, _paired
from .cabin_workbench import shifted
from .combat import looking_head, mul, controller_pair
from .core import Behaviors, BotFault, atomic_json
from .live import ROOT, channel
from .perception import read_enemy_snapshot
from .recon import MarkerLog, marked_lines
from .session import preserved_tracking_poses, _controller_pose


def inspect_studio_hands(live, seconds=60.):
    """Review native left-hand handset contact at reachable, paired poses."""
    initial=live.observe(native=True)
    if (initial.get('native',{}).get('mission')!=10195 or initial.get('scene')!='gameplay'
            or initial.get('menu') or initial.get('loading') or initial['native'].get('saving')):
        raise BotFault('Hand contact inspection requires safe isolated 10195 gameplay')
    deadline=time.monotonic()+min(seconds,90.)
    behavior=Behaviors(live,live.events);captures=[];observations=[];opened=False
    with preserved_tracking_poses(live):
        try:
            live.release()
            pairs={hand:controller_pair(_controller_pose(live,hand,'grip','local'),
                _controller_pose(live,hand,'aim','local')) for hand in ('left','right')}
            def place(hand,point,yaw=0.):
                half=math.radians(yaw)/2
                grip={'position':point,'orientation':[0.,math.sin(half),0.,math.cos(half)]}
                for kind,pose in zip(('grip','aim'),_paired(grip,pairs[hand])):
                    live.call('set_controller_pose',{'hand':hand,'pose_type':kind,'base_space':'view',**pose})
                time.sleep(.25)
            def checkpoint(label):
                if time.monotonic()>deadline:raise BotFault('Hand contact inspection deadline')
                state=live.observe(native=True);frame=state.get('rendered') or {}
                if (state['native'].get('mission')!=10195 or state.get('pause') or state.get('loading')
                        or state['native'].get('player_life',0)<=0 or not frame.get('left_palm_tracked')
                        or not frame.get('right_palm_tracked') or not 0<=state['now_ms']-frame.get('sample_ms',0)<=150):
                    raise BotFault('Native hand contact lost its fresh live owner')
                observations.append({'label':label,'state':state})
                captures.extend(live.capture(label))
            place('right',[.24,-.35,-.40]);place('left',[-.20,-.18,-.46])
            live.execute({'op':'action','name':'system.idroid'})
            behavior.wait_for({'idroid':True,'rendered.idroid_device_tracked':True},5,'left-handset-open')
            opened=True;checkpoint('left-bionic-handset-normal-grip')
            place('left',[-.12,-.12,-.42],20);checkpoint('left-bionic-handset-translated-and-turned')
            live.execute({'op':'action','name':'menus.back'})
            behavior.wait_for({'idroid':False,'scene':'gameplay'},5,'left-handset-stowed')
            opened=False;checkpoint('native-hands-after-handset-stow')
            live.execute({'op':'action','name':'system.idroid'})
            behavior.wait_for({'idroid':True,'rendered.idroid_device_tracked':True},5,'left-handset-reopened')
            opened=True;checkpoint('left-bionic-handset-immediate-reopen')
            return {'status':'observed_hand_contact','observations':observations,'captures':captures,
                    'visual_acceptance':'pending_both_eye_review','headset_acceptance':'not_run'}
        finally:
            live.release()
            if opened and live.observe().get('idroid'):
                live.execute({'op':'action','name':'menus.back'})
                behavior.wait_for({'idroid':False,'scene':'gameplay'},5,'inspection-handset-cleanup')


def inspect_field_studio(live, seconds=90.):
    initial=live.observe(native=True)
    if (initial.get('native',{}).get('mission')!=10195 or initial.get('scene')!='gameplay'
            or initial.get('menu') or initial.get('loading') or initial['native'].get('saving')):
        raise BotFault('Field studio requires the isolated on-foot 10195 test save')
    deadline=time.monotonic()+min(seconds,150.)
    result={'status':'running','phases':[],
            'scope':'Isolated mission 10195; native soldier fixture, physical binocular clear/mark/zoom, real camera shutter'}
    presentation=None
    output=live.events.output
    names=('settings.god_mode','settings.enemy_world_markers')
    previous={row['name']:row['value'] for row in live.bindings['settings'] if row['name'] in names}
    if set(previous)!=set(names):raise BotFault('Effective combat preferences are unavailable')
    def settings(changes):
        request=output/'field-settings-request.json'
        def bridge(method,**values):
            atomic_json(request,{'method':method,'gameExe':str(live.game/'mgsvtpp.exe'),**values})
            return json.loads(subprocess.check_output(['powershell.exe','-NoProfile','-NonInteractive','-File',
                str(ROOT/'tools/launcher-bridge.ps1'),'-CheckerPath',str(ROOT/'build/dev/mgs5vr_controls.exe'),
                '-RequestPath',str(request)],text=True,encoding='utf-8-sig'))
        loaded=bridge('load')
        answer=bridge('saveControls',revision=loaded['controlsRevision'],changes={k:str(v) for k,v in changes.items()})
        if not answer.get('ok',True):raise BotFault('Temporary combat preferences were rejected')
        live.events.emit('field_studio_preferences',changes=changes)
    native_cache=initial['native'];native_checked=time.monotonic()
    def observe():
        nonlocal native_cache,native_checked
        if time.monotonic()>deadline:raise BotFault('Field studio wall-clock deadline')
        if time.monotonic()-native_checked>.75:
            state=live.observe(native=True);native_cache=state['native'];native_checked=time.monotonic()
        else:state=live.observe();state['native']=native_cache
        if (state['native'].get('mission')!=10195 or state.get('loading') or state.get('menu')
                or state['native'].get('player_life',0)<=0):
            raise BotFault('Field studio lost the safe native mission owner')
        # Serialized capture/pose RPCs can leave a briefly aged publication.
        # Wait for the same owner; never replay input or accept the stale pose.
        frame=state.get('rendered') or {}
        owner=(state.get('activation'),frame.get('player_owner'),frame.get('presentation_epoch'))
        until=min(deadline,time.monotonic()+1.)
        while not (type(frame.get('sample_ms')) is int
                and 0<=state['now_ms']-frame['sample_ms']<=150):
            if time.monotonic()>=until:raise BotFault('Field studio did not receive a fresh tracked publication')
            time.sleep(.02);state=live.observe();state['native']=native_cache;frame=state.get('rendered') or {}
            if ((state.get('activation'),frame.get('player_owner'),frame.get('presentation_epoch'))!=owner
                    or state.get('loading') or state.get('menu')):
                raise BotFault('Field studio owner changed while waiting for a fresh pose')
        return state
    def read_marker_log(cursor):
        try:return cursor.read()
        except BotFault:
            rotated=cursor.path.with_name(cursor.path.name+'.1')
            with rotated.open('rb') as stream:
                stream.seek(max(0,cursor.offset-len(cursor.anchor)))
                if stream.read(len(cursor.anchor))!=cursor.anchor:raise
                tail=stream.read(2*1024*1024)
            current=cursor.path.read_bytes()
            if len(tail)+len(current)>2*1024*1024:raise BotFault('Rotated marker evidence exceeded its read bound')
            return (tail+current).decode('utf-8',errors='replace')
    try:
        settings(dict(zip(names,(1,1))));time.sleep(2.4)
        enemies=read_enemy_snapshot(live.native.read,(ROOT/'tools/navigation-state.lua').read_text())
        player=[initial['native']['player_'+axis] for axis in 'xyz']
        nearest=sorted((e for e in enemies['soldiers'] if e['life']==enemies['normal_life']),
                       key=lambda e:math.dist(e['position'],player))[:2]
        if len(nearest)!=2:raise BotFault('No loaded native soldier pair for the isolated fixture')
        actor=nearest[1];result['native_fixture']=nearest
        live.native.read("assert(vars.missionCode==10195 and not TppMission.IsGameOver()); TppClock.SetTime('12:00:00'); "
            +"TppPlayer.Warp{pos={"+','.join(str(v) for v in nearest[0]['position'])+"},rotY=0}; return tostring(true)")
        time.sleep(1.5);native_checked=0
        presentation=BinocularPresentation(live,observe)
        def target():
            return live.native.read("local p=GameObject.SendCommand("+str(actor['id'])
                +",{id='GetPosition'}); return string.format('[%.6f,%.6f,%.6f]',p:GetX(),p:GetY()+1.35,p:GetZ())")[0]
        def aim(point):
            state=live.observe(native=True)
            head=live.text_result(live.call('get_head_pose',{'base_space':'local'}))['pose']
            head['orientation']=looking_head(head['orientation'],state['rendered']['native_pose'],point)
            live.call('set_head_pose',{'base_space':'local',**head,'duration_seconds':.25});time.sleep(.35)
            head=live.text_result(live.call('get_head_pose',{'base_space':'local'}))['pose']
            for hand,poses in inspection_poses(head,presentation.pairs['right'],presentation.pairs['left'],presentation.fit).items():
                for kind,pose in zip(('grip','aim'),poses):
                    live.call('set_controller_pose',{'hand':hand,'pose_type':kind,'base_space':'local',**pose})
            time.sleep(.35)
        def settle(duration):
            until=time.monotonic()+duration
            while time.monotonic()<until:presentation.support_observation();time.sleep(.05)
        def capture(label):
            presentation.support_observation()
            markers=live.native.read('inspect-optic-markers')[0]
            if not markers.get('available'):raise BotFault('Acquired native-marker snapshot is stale')
            live.release(keep=(('left','Grip',None),));presentation.hold_support();state=observe()
            if state['controls']['context']!='binoculars':raise BotFault('Released capture lost the binocular latch')
            result['phases'].append({'label':label,'state':state,'markers':markers,'eyes':live.capture(label,static_grips=True)})
            presentation.hold_support();atomic_json(output/'field-studio-progress.json',result)
        aim(target());cursor=MarkerLog(live.game/'mgs5vr.log')
        live.execute({'op':'action','name':'gameplay.equip_binoculars','seconds':.6})
        Behaviors(live,live.events).wait_for({'controls.context':'binoculars'},3,'physical-binocular-equipped',native=True)
        presentation.hold_support();aim(target());settle(1.)
        result['initial_acquisition']=marked_lines(read_marker_log(cursor),actor['id'])
        if not result['initial_acquisition']:raise BotFault('Physical sight did not acquire the intended native actor')
        capture('marked-before-clear')
        zoom=observe()['rendered']['binocular_magnification']
        presentation.press('binoculars.zoom');settle(.4);capture('physical-binocular-zoom')
        result['zoom_before']=zoom;result['zoom_after']=observe()['rendered']['binocular_magnification']
        for _ in range(3):
            if observe()['rendered']['binocular_magnification']==zoom:break
            presentation.press('binoculars.zoom');settle(.15)
        if observe()['rendered']['binocular_magnification']!=zoom:
            raise BotFault('Physical zoom cycle did not return to the initial inspection magnification')
        cursor=MarkerLog(live.game/'mgs5vr.log');aim(target())
        live.input([channel('a')],.25,lease_seconds=.3);time.sleep(.3)
        live.release(keep=(('left','Grip',None),));settle(.7)
        result['clear_log']=read_marker_log(cursor);capture('cleared-still-looking')
        point=target();aim([point[0],point[1]+6,point[2]]);settle(.25);capture('looked-away')
        cursor=MarkerLog(live.game/'mgs5vr.log');aim(target());settle(1.3)
        result['automatic_return_lines']=marked_lines(read_marker_log(cursor),actor['id']);capture('returned-after-clear')
        aim(target())
        cursor=MarkerLog(live.game/'mgs5vr.log');presentation.press('binoculars.mark',acquired=True);settle(.1)
        result['explicit_remark_lines']=marked_lines(read_marker_log(cursor),actor['id']);capture('explicit-remark')
        live.release();live.execute({'op':'action','name':'binoculars.stow','seconds':.25})
        Behaviors(live,live.events).wait_for({'controls.context':'gameplay'},3,'physical-binocular-stowed',native=True)
        # A released side-cup hand is empty. Rest it at ordinary reach rather
        # than photographing an open bionic palm six centimetres from the eye.
        head=live.text_result(live.call('get_head_pose',{'base_space':'local'}))['pose']
        left=shifted(head,[-.24,-.35,-.35]);left['orientation']=mul(head['orientation'],[.5,0.,0.,.8660254])
        for kind in ('grip','aim'):
            live.call('set_controller_pose',{'hand':'left','pose_type':kind,'base_space':'local',**left})
        time.sleep(.35)
        result['world_markers']={'state':observe(),'eyes':live.capture('world-marker-view')}
        camera=observe()['rendered'].get('studio') or {}
        if not camera.get('available') or camera.get('cabin'):raise BotFault('Field camera is unavailable')
        previous_photo=camera['photo']
        for kind in ('grip','aim'):
            live.call('set_controller_pose',{'hand':'right','pose_type':kind,'base_space':'local',
                **shifted(camera['camera'],[.05,-.025,0.])})
        live.input([channel('right_grip')],.15,lease_seconds=5.);time.sleep(.3)
        if observe()['rendered']['studio']['camera_hand']!=1:raise BotFault('Physical camera did not attach to the grip')
        head=live.text_result(live.call('get_head_pose',{'base_space':'local'}))['pose']
        camera_pose=shifted(head,[.16,-.15,-.32])
        for kind in ('grip','aim'):
            live.call('set_controller_pose',{'hand':'right','pose_type':kind,'base_space':'local',**camera_pose})
        live.input([channel('right_grip'),channel('right_trigger')],.15,lease_seconds=5.)
        photo_until=time.monotonic()+8.
        while time.monotonic()<photo_until:
            state=observe()
            if state['rendered']['studio']['photo']>previous_photo:break
            time.sleep(.05)
        else:raise BotFault('Physical camera did not publish a new native-scene photo')
        if state['controls']['native_triggers']!=[0,0]:raise BotFault('Camera shutter leaked native firearm input')
        live.release(keep=(('right','Grip',None),));time.sleep(.6)
        result['field_camera']={'photo':state['rendered']['studio']['photo'],'state':state,
                               'eyes':live.capture('field-camera-real-photo',static_grips=True)}
        phases={p['label']:p for p in result['phases']}
        def marked(label):return any(p['actor']==actor['id'] for p in phases[label]['markers']['points'])
        result['outcomes']={'initially_marked':marked('marked-before-clear'),
            'cleared':not marked('cleared-still-looking'),
            'clear_survives_look_away_return':not marked('returned-after-clear') and not result['automatic_return_lines'],
            'explicit_remark_restored':marked('explicit-remark') and bool(result['explicit_remark_lines']),
            'zoom_changes':result['zoom_before']!=result['zoom_after']}
        atomic_json(output/'field-studio-progress.json',result)
        if not all(result['outcomes'].values()):raise BotFault('A physical binocular outcome did not match its contract')
        result.update(status='observed_field_studio',visual_acceptance='pending_both_eye_review')
        return result
    finally:
        try:
            live.release()
            if presentation and live.observe().get('controls',{}).get('context')=='binoculars':
                live.execute({'op':'action','name':'binoculars.stow','seconds':.25})
        finally:settings(previous)
