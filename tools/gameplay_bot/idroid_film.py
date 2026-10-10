"""Film existing physical iDroid controls without certifying requested behavior.

Only the owning runner invokes this case. UI content/page identity, human hand
appearance and walking animation remain manual pixel-review requirements.
"""
import math
import time
from pathlib import Path

from .combat import fresh_rig, looking_head
from .core import BotFault, atomic_json
from .health import needs_recovery
from .live import authoritative_controls, channel, held_input_evidence
from .locomotion import _neutral_audit, _position
from .perception import read_enemy_snapshot
from .session import preserved_tracking_poses


SOURCE_LIMITS = {
    'field_pad_exclusion': 'src/native_actions.cpp:728-768 MGS5VR_iDroid/S_DISABLE_PLAYER_PAD',
    'menu_packet': 'src/native_controls.cpp:45-66 grips=shoulder tabs; left axis=menu; right axis=map',
    'existing_case': 'tools/gameplay_bot/idroid.py stationary-navigation, not walk/navigation switching',
    'requested_walk_while_open_source_supported': False,
    'grip_duration_navigation_lease_source_supported': False,
    'release_to_walk_source_supported': False,
    'direct_native_page_or_cursor_observer': False,
}


def bindings_contract(bindings, *, handheld=True):
    actions = bindings.get('actions', [])
    expected = {
        'system.idroid': ('tap', ['menu'], 550),
        'menus.previous_tab': ('level', ['left_grip'], 300),
        'menus.next_tab': ('level', ['right_grip'], 300),
        'menus.back': ('level', ['b'], 300),
    }
    result = {}
    for name, contract in expected.items():
        rows = [r for r in actions if r.get('name') == name]
        if len(rows) != 1 or len(rows[0].get('bindings', [])) != 1:
            raise BotFault('iDroid film requires one exact effective binding: '+name)
        b = rows[0]['bindings'][0]
        if (b.get('gesture'), b.get('inputs'), b.get('milliseconds')) != contract:
            raise BotFault('Current iDroid film binding differs: '+name)
        if name.startswith('menus.') and rows[0].get('contexts') != ['menus']:
            raise BotFault('iDroid menu binding owner differs')
        result[name] = rows[0]
    for name, source in [('axes.move', 'left_stick'), ('axes.menu', 'left_stick'), ('axes.map', 'right_stick')]:
        rows = [r for r in bindings.get('axes', []) if r.get('name') == name]
        if len(rows) != 1 or rows[0].get('source') != source:
            raise BotFault('Current iDroid film stick mapping differs: '+name)
        result[name] = rows[0]
    rows = [r for r in bindings.get('settings', []) if r.get('name') == 'settings.handheld_menus']
    if type(handheld) is not bool or len(rows) != 1 or rows[0].get('value') != int(handheld):
        raise BotFault('Film presentation differs from the effective handheld/spatial setting')
    return result


def nearby_guard_gate(snapshot, world, player):
    if (snapshot.get('mission'), snapshot.get('location')) != tuple(world[:2]):
        raise BotFault('Film enemy telemetry belongs to another world')
    if (snapshot.get('truncated') is not False or snapshot.get('unavailable_loaded') != 0 or
            str(snapshot.get('normal_life')) != '0' or not isinstance(snapshot.get('soldiers'), list)):
        raise BotFault('Film requires complete owned enemy telemetry')
    for actor in snapshot['soldiers']:
        p = actor.get('position')
        if (not isinstance(p, list) or len(p) != 3 or
                any(type(v) not in (int, float) or not math.isfinite(v) for v in p) or
                str(actor.get('life')) not in ('0', '1', '2', '3', '4')):
            raise BotFault('Film enemy position/life is unavailable')
        if str(actor['life']) == '0' and math.dist(player, p) <= 45.:
            raise BotFault('An active guard is within45m of the iDroid film case')


def motion_measure(before, after, direction):
    a, b = _position(before), _position(after)
    x, y, z = [v-u for u, v in zip(a, b)]
    return {'before_native_root': a, 'after_native_root': b,
            'xz_displacement_m': math.hypot(x, z), 'root_y_delta_m': y,
            'distance_3d_m': math.dist(a, b), 'forward_displacement_m': x*direction[0]+z*direction[1],
            'lateral_displacement_m': abs(x*direction[1]-z*direction[0]),
            'walking_animation_visual_acceptance': 'pending',
            'scope': 'native endpoint displacement; input alone is not movement or animation proof'}


def film_idroid(live, nav, *, enemy_script, world, goal_node, output, max_seconds=90.,
                handheld=True, navigation_grip='left_grip', tutorial_mode=0,
                helicopter_space=False, exercise_walk=True, exercise_both_grips=False,
                start_open=False, own_existing_menu=False,
                clock=time.monotonic, sleep=time.sleep):
    """One bounded current-control take, with a continuous stick/grip release.

The destination is a caller-selected nearby connected NAV node. This is a short
motion experiment, not native collision/cover or mission acceptance.
"""
    output = Path(output); output.mkdir(parents=True, exist_ok=True)
    result = {'status': 'running', 'case': 'current-idroid-grip-walk-film', 'phases': [], 'captures': [],
              'source_limits': SOURCE_LIMITS, 'visual_acceptance': 'pending_actual_both_eye_and_motion_review',
              'requested_behavior_acceptance': False, 'physical_headset_acceptance': False,
              'full_game_acceptance': False, 'map_identity': nav.identity}
    start = clock(); owner = None; baseline_life = None; origin = None; opened = False
    try:
        if (not isinstance(world, (list, tuple)) or len(world) != 3 or
                any(type(v) is not int or v <= 0 for v in world[:2]) or
                not isinstance(world[2], str) or not world[2] or
                type(max_seconds) not in (int, float) or not math.isfinite(max_seconds) or not 15 <= max_seconds <= 120):
            raise BotFault('iDroid film requires an exact field world and a15..120s bound')
        if (any(type(v) is not bool for v in (handheld, helicopter_space, exercise_walk,
                    exercise_both_grips, start_open, own_existing_menu)) or
                navigation_grip not in ('left_grip', 'right_grip') or
                type(tutorial_mode) is not int or not 0 <= tutorial_mode <= 255 or
                (helicopter_space and exercise_walk) or (start_open and not own_existing_menu)):
            raise BotFault('Film needs explicit current mode, grip and pre-existing menu ownership')
        if getattr(live, 'held_action_recording', None) is None:
            raise BotFault('iDroid motion film requires continuous native source-eye recording')
        result['effective_bindings'] = bindings_contract(live.bindings, handheld=handheld)
        result['declared_current_mode'] = {'handheld': handheld, 'navigation_grip': navigation_grip,
            'tutorial_mode': tutorial_mode, 'helicopter_space': helicopter_space,
            'exercise_walk': exercise_walk, 'exercise_both_grips': exercise_both_grips,
            'start_open': start_open, 'own_existing_menu': own_existing_menu}

        def fast_guard(state, expected):
            if clock()-start >= max_seconds or (output/'STOP').exists() or (live.events.output/'STOP').exists():
                raise BotFault('iDroid film stopped or reached its deadline')
            frame = state.get('rendered') or {}
            if (state.get('gamepad') is not False or state.get('camera_active') is not True or
                    state.get('camera_available') is not True or state.get('loading') is not False or
                    state.get('title') is not False or state.get('demo') is not False or state.get('pause') is not False or
                    state.get('head_tracking_fault_mask') != 0 or state.get('tracking_fault_mask') != 0 or
                    frame.get('presentation_focused') is not True or
                    (state.get('activation'), frame.get('player_owner'), frame.get('presentation_epoch')) != owner):
                raise BotFault('iDroid film lost its focused VR field owner')
            controls = authoritative_controls(state)
            if expected is not None:
                if (state.get('idroid') is not expected or state.get('menu') is not expected or
                        state.get('scene') != ('menu' if expected else 'gameplay') or
                        controls.get('context') != ('menus' if expected else 'gameplay')):
                    raise BotFault('iDroid film input owner changed')
                if expected and (state.get('idroid_handheld') is not handheld or
                        state.get('idroid_menu_input_ready') is not handheld or
                        (handheld and frame.get('idroid_device_tracked') is not True)):
                    raise BotFault('Physical iDroid device or native menu readiness is unavailable')
            return controls

        def checkpoint(expected=None, *, guards=True):
            nonlocal owner, baseline_life, origin
            state, identity = fresh_rig(live)
            if owner is None:owner = tuple(identity[2:])
            controls = fast_guard(state, expected)
            native = state['native']
            if ((native.get('mission'), native.get('location'), native.get('sequence')) != tuple(world) or
                    native.get('helicopter_space') is not helicopter_space or native.get('player_vehicle_id') != 65535 or
                    any(native.get(k) is not False for k in ('popup','saving','demo','demo_nonplayable','title')) or
                    native.get('game_over') != 0 or native.get('not_alert') is not True or
                    native.get('status_CARRY') is not False or native.get('status_BINOCLE') is not False or
                    (native.get('tutorial_pause') is not False and not
                     (tutorial_mode > 0 and state.get('idroid') is True and native.get('tutorial_pause') is True)) or
                    (state.get('idroid') is True and state.get('idroid_tutorial_mode') != tutorial_mode)):
                raise BotFault('iDroid film lost ordinary undetected field gameplay')
            if baseline_life is None:baseline_life = native['player_life']
            if needs_recovery(native, baseline_life):
                raise BotFault('Health recovery takes priority over iDroid filming')
            point = _position(state)
            if origin is None:origin = point
            if math.hypot(point[0]-origin[0], point[2]-origin[2]) > 3.:
                raise BotFault('iDroid film moved beyond its3m native motion bound')
            if not helicopter_space:
                nav.nearest(point)
                if guards:nearby_guard_gate(read_enemy_snapshot(live.native.read, enemy_script), world, point)
            live.events.emit('idroid_film_checkpoint', state=state, input_neutral=not bool(live.held))
            return state

        def wait_owner(expected):
            end = clock()+5.
            while clock() < end:
                state = checkpoint()
                if (state.get('idroid') is expected and state.get('menu') is expected and
                        (not expected or (state.get('idroid_menu_input_ready') is handheld and
                         (not handheld or state['rendered'].get('idroid_device_tracked') is True)))):
                    fast_guard(state, expected); return state
                sleep(.05)
            raise BotFault('Physical iDroid ownership transition did not complete; no replay')

        def samples(seconds, tokens, *, without_grip=False, newer_than=-1):
            end = clock()+seconds; rows = []; stamps = set()
            while clock() < end:
                state = live.observe(native=False); controls = fast_guard(state, True)
                audit = held_input_evidence(state, tokens, after_sample_ms=newer_than)
                if not tokens and controls['sample_ms']>newer_than:
                    _neutral_audit(controls)
                    audit={'sample_ms':controls['sample_ms'],'requested_inputs':[],
                           'neutral_observation':True,'sampled_context':controls['context']}
                if audit:
                    if without_grip and any(v > .09 for v in controls['physical'][7:9]):
                        raise BotFault('Grip remained held after the explicit release')
                    allowed = {i for i, token in ((7, 'left_grip'), (8, 'right_grip')) if token in tokens}
                    if any(abs(v) > .09 for i,v in enumerate(controls['physical'][:11]) if i not in allowed):
                        raise BotFault('Unexpected face/trigger/modifier in iDroid probe')
                    axis_expected = .55 if 'left_stick_up' in tokens else 0.
                    if (any(abs(v) > .09 for i,v in enumerate(controls['sticks']) if i!=1) or
                            abs(controls['sticks'][1]-axis_expected)>.08 or
                            controls.get('native_buttons') != sum(0x100 if i == 7 else 0x200 for i in allowed) or
                            controls.get('native_triggers') != [0,0]):
                        raise BotFault('Current iDroid menu packet or stick chord differs')
                    rows.append({'state': state, 'audit': audit, 'native_axes': controls.get('native_axes'),
                                 'native_triggers': controls.get('native_triggers')})
                    stamps.add(audit['sample_ms'])
                sleep(.025)
            if len(stamps) < 2:
                raise BotFault('Two distinct physical iDroid control samples were not observed')
            return rows

        def released_capture(label):
            if live.held:raise BotFault('Held iDroid motion must use video, not a compositor capture')
            state = checkpoint(); _neutral_audit(state['controls'])
            result['captures'] += live.capture(label)
            return checkpoint()

        def phase(name, seconds, tokens):
            before = checkpoint(True); floor = before['controls']['sample_ms']
            begun=clock()
            try:
                live.input([channel(t) if t in ('left_grip','right_grip') else {**channel(t),'value':.55} for t in tokens],
                           seconds, lease_seconds=seconds+.25)
                held = samples(seconds*.5, tokens, newer_than=floor)
                # A released native read cannot rule out transient native
                # binocular mode during a grip. Read under this same short
                # independent lease and require an actual held XR publication.
                held_native=checkpoint(True,guards=False)
                if held_input_evidence(held_native,tokens,after_sample_ms=floor) is None:
                    raise BotFault('iDroid phase lacked a held native binocular-leakage observation; no replay')
                remaining=seconds-(clock()-begun)
                if remaining>.06:
                    held+=samples(remaining,tokens,newer_than=held_native['controls']['sample_ms'])
            finally:live.release()
            after = checkpoint(True)
            entry = {'name':name,'before':before,'after':after,'sampled_inputs':held,
                     'held_native_checkpoint':held_native,'native_binocular_mode_during_held_input':False,
                     'motion':motion_measure(before,after,direction),'page_and_cursor_change':'pending_pixel_review'}
            result['phases'].append(entry);live.events.emit('idroid_film_phase', **entry)
            released_capture('idroid-film-'+name+'-released')
            return entry

        def stow():
            nonlocal opened
            for _ in range(3):
                live.release(); state = checkpoint()
                if state.get('idroid') is False:
                    fast_guard(state,False);opened=False;return state
                fast_guard(state,True)
                live.execute({'op':'action','name':'menus.back','seconds':.12,
                              'state_before':{'idroid':True,'menu':True,'pause':False},
                              'native_before':{'mission':world[0],'location':world[1],'sequence':world[2],
                                               'popup':False,'saving':False,
                                               'tutorial_pause':state['native']['tutorial_pause']}})
                end=clock()+2.
                while clock()<end:
                    state=checkpoint()
                    if state.get('idroid') is False:
                        fast_guard(state,False);opened=False;return state
                    if state.get('idroid_closing') is not True:break
                    sleep(.05)
            raise BotFault('Ordinary iDroid Back did not complete stow; no unknown dialog confirmed')

        with preserved_tracking_poses(live):
            live.release(); before = checkpoint(start_open);result['before']=before
            player=_position(before);direction=[0.,0.]
            if exercise_walk:
                if type(goal_node) is not int or not 0 <= goal_node < len(nav.positions):
                    raise BotFault('Film requires a caller-selected exact nearby NAV node')
                goal = list(nav.positions[goal_node])
                length=math.hypot(goal[0]-player[0],goal[2]-player[2])
                route=nav.path(nav.nearest(player),goal_node)
                if not 1.5 <= length <= 4. or len(route)>8 or abs(goal[1]-player[1])>2.5:
                    raise BotFault('Film requires a1.5..4m nearby connected NAV destination, no more than8 nodes')
                direction=[(goal[0]-player[0])/length,(goal[2]-player[2])/length]
                result['declared_short_motion_path']={'goal':goal,'node':goal_node,'nodes':route,
                  'scope':'connected native floor and bounded motion experiment; collision traversal not certified'}
                head=live.text_result(live.call('get_head_pose',{'base_space':'local'}))['pose']
                camera=before['rendered']['native_pose']
                head['orientation']=looking_head(head['orientation'],camera,[goal[0],camera['position'][1],goal[2]])
                live.call('set_head_pose',{'base_space':'local',**head})
            released_capture('idroid-film-before-open')
            # Own this attempted opening for observed, bounded cleanup even
            # when its operator completion becomes unknown. Never replay it.
            opened=True;live.opened_menu='idroid'
            if not start_open:live.execute({'op':'action','name':'system.idroid','state_before':{'scene':'gameplay','idroid':False,'menu':False},
                          'native_before':{'mission':world[0],'location':world[1],'sequence':world[2],
                                           'popup':False,'saving':False,'tutorial_pause':False}})
            wait_owner(True);released_capture('idroid-film-open')
            if exercise_walk:phase('open-walk-request',.55,['left_stick_up'])
            phase(navigation_grip+'-page-pulse',.18,[navigation_grip])
            if exercise_both_grips:
                other_grip='right_grip' if navigation_grip=='left_grip' else 'left_grip'
                phase(other_grip+'-page-pulse',.18,[other_grip])
            before=checkpoint(True);floor=before['controls']['sample_ms'];axis={**channel('left_stick_up'),'value':.55}
            try:
                # One axis lease spans the grip release. Do not stop/reapply it
                # and mislabel that as release-to-walk behavior.
                tokens=[navigation_grip]+(['left_stick_up'] if exercise_walk else [])
                live.input([channel(navigation_grip)]+([axis] if exercise_walk else []),1.6,lease_seconds=1.6)
                held=samples(.5,tokens,newer_than=floor)
                middle=checkpoint(True,guards=False)
                if held_input_evidence(middle,tokens,after_sample_ms=floor) is None:
                    raise BotFault('Grip release lacks a fresh native checkpoint under its original lease; no replay')
                live.release(keep={(axis['hand'],axis['component'],axis.get('sub_component'))} if exercise_walk else ())
                released=samples(.45,['left_stick_up'] if exercise_walk else [],without_grip=True,newer_than=middle['controls']['sample_ms'])
            finally:live.release()
            after=checkpoint(True)
            entry={'name':'held-grip-navigate-then-release-same-stick','before':before,'grip_release_native_checkpoint':middle,
                   'after':after,'held_samples':held,'grip_released_same_axis_samples':released,
                   'navigation_motion':motion_measure(before,middle,direction),
                   'release_motion':motion_measure(middle,after,direction),
                   'axis_stopped_and_reapplied':False,'page_and_cursor_change':'pending_pixel_review'}
            result['phases'].append(entry);live.events.emit('idroid_film_phase',**entry)
            released_capture('idroid-film-grip-released')
            stow();released_capture('idroid-film-stowed')
            opened=True;live.opened_menu='idroid'
            live.execute({'op':'action','name':'system.idroid','state_before':{'scene':'gameplay','idroid':False,'menu':False},
                          'native_before':{'mission':world[0],'location':world[1],'sequence':world[2],
                                           'popup':False,'saving':False,'tutorial_pause':False}})
            wait_owner(True);released_capture('idroid-film-immediate-reopen')
            result['after_reopen']=checkpoint(True)
            stow();result['after']=released_capture('idroid-film-final-stowed')
            result['status']='observed_current_control_probe'
            result['requested_walk_while_open_native_forward_displacement_observed']=exercise_walk and result['phases'][0]['motion']['forward_displacement_m']>=.03
            result['requested_release_to_walk_native_forward_displacement_observed']=exercise_walk and entry['release_motion']['forward_displacement_m']>=.03
    except Exception as error:
        result['status']='failed';result['error']=str(error)
    finally:
        try:
            live.release()
            if opened:
                # The existing cleanup has its own bounded Back/owner checks;
                # a case deadline/STOP/health loss must not prohibit cleanup.
                live.cleanup_menus();result['cleanup_stowed_test_opened_idroid']=True
        except Exception as error:
            result['cleanup_error']=str(error);result['status']='failed'
        result['seconds']=clock()-start
        atomic_json(output/'result.json',result)
    return result
