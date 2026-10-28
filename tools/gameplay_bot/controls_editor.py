"""Bounded in-game spatial remapping, context and neutral-handoff inspection."""
import time
from .core import BotFault,atomic_json
from .live import channel
from .combat import pointing
from .motion import _blend
from .session import preserved_tracking_poses,_controller_pose
from .cabin_workbench import compose,inverse_pose,shifted

def inspect_controls_editor(live,policy,seconds=180.):
    deadline=time.monotonic()+min(seconds,180.)
    result={'status':'running','scope':policy,'phases':[],'headset_acceptance':'not_run'}
    settings_path=live.game/'mgs5vr-controls.ini'
    saved_settings=settings_path.read_bytes()
    applied_settings=saved_settings
    pairs={h:compose(inverse_pose(_controller_pose(live,h,'grip','local')),_controller_pose(live,h,'aim','local')) for h in ('left','right')}
    def observe():
        if time.monotonic()>deadline:raise BotFault('Controls editor deadline')
        s=live.observe()
        cabin_screen=(policy['scene']=='cabin' and s.get('cabin') and not s.get('camera_active')
                      and (s.get('controls_editor') or {}).get('visible'))
        if (s['scene']!=policy['scene'] and not cabin_screen) or s.get('loading'):raise BotFault('Controls editor lost selected native scene')
        if s.get('controls_editor'):
            s['rendered']=s.get('rendered') or {};s['rendered']['controls_editor']=s['controls_editor']
        return s
    def editor():return observe().get('rendered',{}).get('controls_editor',{})
    def wait(predicate,label,timeout=6.):
        until=min(deadline,time.monotonic()+timeout)
        while time.monotonic()<until:
            s=observe()
            if predicate(s):
                result['phases'].append({'label':label,'state':s});return s
            time.sleep(.04)
        raise BotFault('Controls editor missing '+label)
    def move_aim(target):
        start=_controller_pose(live,'right','grip','local');finish=compose(target,inverse_pose(pairs['right']))
        for n in range(1,7):
            grip=_blend(start,finish,n/6)
            for kind,p in (('grip',grip),('aim',compose(grip,pairs['right']))):
                live.call('set_controller_pose',{'hand':'right','pose_type':kind,'base_space':'local',**p})
            time.sleep(.025)
        time.sleep(.1)
    def click(control):
        nonlocal applied_settings
        if control==113 and settings_path.read_bytes()!=applied_settings:
            raise BotFault('Personal controls changed outside this editor take')
        f=editor();hit=next(h for h in f['layout'] if h['id']==control)
        local=[(hit['x']+hit['width']/2-.5)*1.2,(.5-hit['y']-hit['height']/2)*.75,0.]
        target=shifted(f['panel'],local)['position']
        head=live.text_result(live.call('get_head_pose',{'base_space':'local'}))['pose']
        origin=shifted(head,[.22,-.18,-.33])['position']
        move_aim({'position':origin,'orientation':pointing([a-b for a,b in zip(target,origin)])})
        wait(lambda s:s['rendered']['controls_editor']['hover']==control,'hover-'+str(control),3.)
        live.input([channel('right_trigger')],.14);time.sleep(.12);live.release();time.sleep(.16)
        if control==113:applied_settings=settings_path.read_bytes()
    def select(action):
        f=editor();index=next(n for n,r in enumerate(f['rows']) if r['action']==action)
        while editor()['page']<index//11:click(105)
        while editor()['page']>index//11:click(104)
        click(index%11);wait(lambda s:s['rendered']['controls_editor']['action']==action,'select-'+action)
    def capture(label,static_grips=False):
        s=observe();eyes=live.capture(label.replace('.','-'),static_grips=static_grips)
        result['phases'].append({'label':label,'state':s,'eyes':eyes});atomic_json(live.events.output/'controls-editor-progress.json',result)
    def open_panel(mode):
        live.execute({'op':'action','name':'system.controls','seconds':.9});live.release()
        wait(lambda s:s['rendered']['controls_editor']['visible'] and s['rendered']['controls_editor']['live_mode']==mode,'panel-open-'+mode,8.)
        time.sleep(.25)
    def record_chord(show_live=False):
        click(106);wait(lambda s:s['rendered']['controls_editor']['phase']==2,'capture-ready')
        live.input([channel('left_grip'),channel('y')],.65,lease_seconds=5.)
        time.sleep(.65)
        if show_live:
            s=wait(lambda s:s['rendered']['controls_editor']['live_inputs']&(1<<3|1<<7)==(1<<3|1<<7),'two-controller-inputs-live')
            if s['controls']['native_buttons'] or any(s['controls']['native_triggers']):raise BotFault('Mapping input leaked into the native game')
            capture('live-3D-controller-chord-highlight',static_grips=True)
        live.release();wait(lambda s:s['rendered']['controls_editor']['phase']==3,'chord-proposal')
    with preserved_tracking_poses(live):
        try:
            live.release();wait(lambda s:s['rendered'].get('controls_editor') is not None,'editor-runtime-ready',15.)
            initial=observe();result['initial_native']=initial.get('native')
            if policy.get('restore_vr_only'):
                if not editor().get('vr_enabled'):
                    select('system.toggle_vr');click(120);click(113)
                    wait(lambda s:s['rendered']['controls_editor'].get('vr_enabled') and s.get('camera_active'),
                         'immersive-renderer-restored',8.)
                click(100);wait(lambda s:not s['rendered']['controls_editor']['visible'],'editor-closed')
                result['status']='restored_vr_and_closed_editor';return result
            if initial.get('rendered',{}).get('binocular_held'):
                live.execute({'op':'action','name':'binoculars.stow','seconds':.25});live.release()
                wait(lambda s:not s['rendered'].get('binocular_held'),'initial-binocular-stowed')
            open_panel('MENUS' if policy['scene']=='menu' else 'ACC' if policy['scene']=='cabin' else 'ON FOOT' if policy['scene']=='gameplay' else 'LAUNCH CABIN')
            capture('context-controls-panel-live')
            if policy.get('vr_only'):
                click(117);wait(lambda s:s['rendered']['controls_editor'].get('settings'),'settings-tab')
                select('system.toggle_vr');was_enabled=editor().get('vr_enabled') is True
                if not was_enabled:raise BotFault('Original-renderer comparison requires immersive VR initially enabled')
                try:
                    click(120);click(113)
                    wait(lambda s:not s['rendered']['controls_editor'].get('vr_enabled') and not s.get('camera_active'),
                         'original-renderer-live',8.)
                    time.sleep(1.);capture('mission-end-original-renderer')
                finally:
                    if not editor().get('vr_enabled'):
                        click(120);click(113)
                        wait(lambda s:s['rendered']['controls_editor'].get('vr_enabled'),'immersive-renderer-restored',8.)
                click(100);wait(lambda s:not (s.get('controls_editor') or
                    (s.get('rendered') or {}).get('controls_editor') or {}).get('visible'),'editor-closed')
                result['status']='observed_original_renderer_comparison';return result
            head=live.text_result(live.call('get_head_pose',{'base_space':'local'}))['pose']
            for offset,label in (([.06,.02,0.],'panel-head-motion-right'),([-.06,-.01,0.],'panel-head-motion-left')):
                moved=shifted(head,offset);live.call('set_head_pose',{'base_space':'local',**moved});time.sleep(.2);capture(label)
            live.call('set_head_pose',{'base_space':'local',**head})
            if policy.get('inspect_only'):
                click(100);wait(lambda s:not s['rendered']['controls_editor']['visible'] and not s['rendered']['controls_editor']['owns_input'],'editor-close-neutral')
                result['status']='observed_controls_panel';return result
            if policy.get('settings'):
                if not editor().get('settings'):click(117)
                wait(lambda s:s['rendered']['controls_editor'].get('settings'),'live-settings-tab')
                configured=live.bindings.get('settings',[])
                if len(editor()['rows'])!=len(configured)+1:raise BotFault('The headset omitted a supported gameplay setting')
                selected=(('settings.binocular_auto_mark',120),('settings.player_height_offset_cm',119),('settings.turn_mode',118))
                if policy.get('body_switches'):
                    selected=(('settings.full_body',120),('settings.eyepatch',120))
                def show_effect(name,label):
                    if not policy.get('body_switches'):return
                    click(100)
                    wait(lambda s:not s['rendered']['controls_editor']['visible'],'effect-panel-closed')
                    current_head=live.text_result(live.call('get_head_pose',{'base_space':'local'}))['pose']
                    try:
                        if name=='settings.full_body':
                            target=compose(current_head,{'position':[0.,0.,0.],'orientation':pointing([0.,-1.,-.45])})
                            live.call('set_head_pose',{'base_space':'local',**target,'duration_seconds':.7})
                        time.sleep(2.);capture(label,static_grips=True)
                    finally:
                        live.call('set_head_pose',{'base_space':'local',**current_head,'duration_seconds':.7})
                    open_panel('ON FOOT' if policy['scene']=='gameplay' else 'ACC')
                    if not editor().get('settings'):click(117)
                    select(name)
                for name,control in selected:
                    select(name);original=editor()['binding'];click(control);capture('setting-proposed-'+name)
                    click(112);wait(lambda s:s['rendered']['controls_editor']['binding']==original and not s['rendered']['controls_editor']['proposed'],'setting-cancel-'+name)
                    click(control);proposed=editor()['proposed'];click(113)
                    wait(lambda s:s['rendered']['controls_editor']['phase']==0 and s['rendered']['controls_editor']['binding']==proposed,'setting-applied-'+name,8.)
                    capture('setting-live-'+name)
                    show_effect(name,'effect-live-'+name)
                    click(control if control==120 else 118 if control==119 else 119);click(113)
                    wait(lambda s:s['rendered']['controls_editor']['phase']==0 and s['rendered']['controls_editor']['binding']==original,'setting-restored-'+name,8.)
                    show_effect(name,'effect-restored-'+name)
                if policy.get('body_switches'):
                    click(100);wait(lambda s:not s['rendered']['controls_editor']['visible'],'body-switch-editor-closed')
                    result['status']='observed_body_switches';return result
                select('system.toggle_vr');click(120);click(113)
                wait(lambda s:s['rendered']['controls_editor']['visible'] and s['rendered']['controls_editor']['binding']=='0' and not s.get('camera_active'),'vr-off-panel-stays-available',8.)
                capture('live-vr-off-spatial-screen-and-settings')
                click(120);click(113)
                wait(lambda s:s['rendered']['controls_editor']['binding']=='1' and s.get('camera_active'),'vr-on-immersive-return',8.)
                capture('live-vr-on-immersive-return');click(116)
                click(102)
            select('gameplay.equip_binoculars');click(115)
            record_chord(show_live=True);f=editor()
            if f['proposed']!='hold(y + left_grip,400)':raise BotFault('Captured chord/timing disagrees with physical input')
            capture('physical-chord-proposed-check-or-cancel')
            click(112);wait(lambda s:s['rendered']['controls_editor']['phase']==0 and not s['rendered']['controls_editor']['proposed'],'cancel-keeps-binding')
            click(115);record_chord();click(113)
            wait(lambda s:s['rendered']['controls_editor']['phase']==0 and s['rendered']['controls_editor']['binding']=='hold(y + left_grip,400)','binding-applied-live',8.)
            capture('confirmed-binding-applied-live')
            click(106);wait(lambda s:s['rendered']['controls_editor']['phase']==2,'conflict-capture-ready')
            live.input([channel('y')],.2);time.sleep(.15);live.release()
            wait(lambda s:s['rendered']['controls_editor']['phase']==3 and 'conflict' in s['rendered']['controls_editor']['status'],'mode-conflict-rejected')
            capture('same-mode-conflict-keeps-working-binding');click(112)
            # Inspect driving without representing it as the current native mode.
            for _ in range(2 if policy['scene']=='cabin' else 3):click(103)
            f=editor()
            if f['mode']!='DRIVING' or not any(r['action']=='vehicle.accelerate' for r in f['rows']):raise BotFault('Driving inspection omitted its native controls')
            if any(r['action']=='gameplay.dive' for r in f['rows']):raise BotFault('Driving inspection included on-foot Dive')
            capture('driving-bindings-inspection-current-mode-explicit');click(102)
            click(100);wait(lambda s:not s['rendered']['controls_editor']['visible'] and not s['rendered']['controls_editor']['owns_input'] and not s['rendered'].get('camera_world_frozen'),'close-restores-native-world',6.)
            # Same physical chord with the just-confirmed threshold in the game.
            live.input([channel('left_grip'),channel('y')],.8,lease_seconds=1.2);time.sleep(.8);live.release()
            wait(lambda s:s['rendered'].get('binocular_held'),'new-binding-equipped-physical-binocular',8.)
            open_panel('BINOCULARS');f=editor()
            if not any(r['action']=='binoculars.mark' for r in f['rows']):raise BotFault('Live binocular mode omitted explicit Mark')
            capture('live-binocular-mode-controls');click(100)
            wait(lambda s:not s['rendered']['controls_editor']['visible'] and not s['rendered']['controls_editor']['owns_input'],'binocular-editor-closed')
            live.execute({'op':'action','name':'binoculars.stow','seconds':.25});live.release()
            wait(lambda s:not s['rendered'].get('binocular_held'),'physical-binocular-stowed')
            result['status']='observed_controls_editor'
            return result
        finally:
            try:
                live.release()
                current=live.observe()
                if (current.get('rendered') or {}).get('controls_editor',{}).get('visible'):
                    if current.get('scene')==policy['scene'] or (policy['scene']=='cabin' and current.get('cabin')):click(100)
                    else:result['cleanup']='Scene changed; runtime closes editor on scene loss.'
                live.release()
            finally:
                if not policy.get('restore_vr_only'):
                    if settings_path.read_bytes()==applied_settings:
                        settings_path.write_bytes(saved_settings)
                        result['personal_controls_restored']=True
                    else:
                        result['personal_controls_restored']=False
                        result['controls_restore_error']='External edit preserved; original controls retained in run output.'
                        (live.events.output/'original-controls.ini').write_bytes(saved_settings)
                atomic_json(live.events.output/'controls-editor-progress.json',result)
