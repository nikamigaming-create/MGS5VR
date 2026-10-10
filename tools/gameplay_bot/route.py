"""Follow native navigation edges through ordinary configured VR inputs."""
import math
import time

from .core import BotFault, atomic_json
from .live import authoritative_controls, channel, fresh_control_observation
from .locomotion import _axis_source, _position, _validate_field_state, _validate_xr_field_state
from .navigation import ObstacleMemory, RouteProgress, arrived, movement_mode, posture, stance_press


def set_posture(live, desired, *, sleep=time.sleep):
    live.release()
    for _ in range(3):
        before=live.observe(native=True)
        current = posture(before['native'])
        seconds = stance_press(current, desired)
        if seconds is None:
            return
        try:live.execute({'op': 'action', 'name': 'gameplay.stance', 'seconds': seconds})
        except BotFault as error:
            if str(error)!='Requested physical input was never observed held by the native XR control sampler':raise
            # Actual 191402 ended here although STAND had become SQUAT.
            # A completed short pulse can fall between observer reads. Its
            # native posture outcome is useful; do not replay the toggle or
            # claim that an unseen held packet was observed.
            after,_=fresh_control_observation(live.observe,native=True)
            _validate_xr_field_state(after)
            owner=lambda s:tuple(s.get('native',{}).get(k) for k in ('mission','location','sequence'))+(s.get('activation'),)
            if (owner(before)!=owner(after) or after['native'].get('status_CARRY') is not False
                or after['native'].get('player_vehicle_id')!=65535 or posture(after['native'])!=desired):raise
            live.events.emit('posture_outcome_without_held_sample',before=before,after=after,
                desired=desired,physical_held_sample_observed=False,input_replayed=False)
            return
        sleep(.45)
    if posture(live.observe(native=True)['native']) != desired:
        raise BotFault('Native posture did not acknowledge the VR stance action')


def finish_navigation(live, head, result, output):
    """Keep failed-run evidence even when a dead operator cannot release input."""
    errors = []
    for label, action in (
        ('release', live.release),
        ('restore_head', lambda: live.call('set_head_pose', {'base_space':'local', **head})),
    ):
        try:
            action()
        except Exception as error:
            errors.append(label + ': ' + str(error))
    try:
        result['after'] = live.observe(native=True)
        if not errors and result['status']=='needs_battle_response':
            live.events.emit('navigation_battle_checkpoint',state=result['after'],
                             final_compositor_captured=False,reason=result.get('reason'))
        elif not errors:
            result['captures'] += live.capture('navigation-finished')
    except Exception as error:
        errors.append('final_observation: ' + str(error))
    if errors:
        result['cleanup_errors'] = errors
        result['observed_status_before_cleanup'] = result['status']
        result['status'] = 'failed'
        result.setdefault('error', errors[0])
    atomic_json(output/'result.json', result)


def battle_reason(native, baseline_life=None, *, known_battle=False):
    if not known_battle and native.get('not_alert') is False:return 'detected'
    if baseline_life is not None and native['player_life']<baseline_life:return 'damage'
    if any(native.get('status_'+name) is True for name in ('INJURY_LOWER','INJURY_UPPER')):
        return 'critical_wound'
    return None


def body_heading_error(native, point, target):
    yaw=native.get('player_yaw')
    if type(yaw) not in (int,float) or not math.isfinite(yaw):return None
    dx,dz=target[0]-point[0],target[2]-point[2]
    if math.hypot(dx,dz)<.05:return 0.
    desired=math.degrees(math.atan2(dx,dz))
    return abs((desired-yaw+180.)%360.-180.)


def navigate(live, nav, goal, *, output, enemy_reader, mode='auto', max_seconds=0,
             obstacle_file=None, stop_at_bridge=True, expected_location=None,
             arrival_radius=3.5, carry_reader=None, recenter=True, interrupt_on_danger=False, known_battle=False,
             avoid_threats=True, clock=time.monotonic, sleep=time.sleep):
    if mode not in ('auto', 'stealth', 'walk', 'run', 'crouch', 'crawl'):
        raise BotFault('Unknown movement mode')
    if type(max_seconds) not in (int, float) or not math.isfinite(max_seconds) or max_seconds < 0:
        raise BotFault('Navigation duration must be finite and nonnegative; zero uses progress guards')
    if type(recenter) is not bool:
        raise BotFault('Navigation recenter policy must be an explicit boolean')
    if type(interrupt_on_danger) is not bool:
        raise BotFault('Navigation danger policy must be an explicit boolean')
    if type(known_battle) is not bool:
        raise BotFault('Navigation battle acknowledgement must be an explicit boolean')
    if type(avoid_threats) is not bool:
        raise BotFault('Navigation threat avoidance must be an explicit boolean')
    if carry_reader is not None and (not callable(carry_reader) or mode!='walk'):
        raise BotFault('Witnessed body carry requires an exact-actor reader and walking mode')
    if type(arrival_radius) not in (int,float) or not math.isfinite(arrival_radius) or not .75<=arrival_radius<=3.5:
        raise BotFault('Arrival radius must be 0.75..3.5 native coordinate units')
    admitted_mission = None
    if expected_location is not None:
        preflight = live.observe(native=True)
        observed = preflight.get('native', {}).get('location')
        if type(expected_location) is not int or type(observed) is not int or observed != expected_location:
            raise BotFault('Native location changed after selecting its navigation graph; no movement dispatched')
        witnessed=carry_reader() is True if carry_reader is not None else False
        if carry_reader is not None and not witnessed:raise BotFault('Exact body carry was not witnessed before navigation')
        _validate_field_state(preflight,witnessed_carry=witnessed)
        admitted_mission = (preflight['native']['mission'], observed)
    source = _axis_source(live.bindings, 'axes.move')
    move = {'hand': source.split('_')[0], 'component': 'Thumbstick', 'sub_component': 'Y'}
    run = next(a for a in live.bindings['actions'] if a['name'] == 'gameplay.run')
    sprint = [channel(t) for t in run['bindings'][0]['inputs']]
    head = live.text_result(live.call('get_head_pose', {'base_space': 'local'}))['pose']
    result = {'status': 'running', 'map_identity': nav.identity, 'goal': goal, 'arrival_radius':arrival_radius,
              'samples': [], 'replans': [], 'captures': [], 'postures': [], 'enemy_snapshots': []}
    blocked, history = set(), []
    route, index, last_life, mission = [], 0, None, admitted_mission
    progress = RouteProgress()
    started = clock()
    next_enemies = renew = next_report = 0.
    enemies = []
    memory = None
    current_mode = None
    current_channels = None
    retreat_goal = None
    retreat_until = 0.
    crawl_turn_since=None;crawl_turns=0
    nav_owner=None
    try:
        live.release()
        nav_head=dict(head)
        if carry_reader is None and recenter:
            nav_head['orientation']=[0,0,0,1]
            live.call('set_head_pose', {'base_space':'local',**nav_head})
            live.execute({'op': 'action', 'name': 'system.recenter'})
            sleep(.25)
        from .combat import fresh_rig
        baseline,nav_owner=fresh_rig(live)
        baseline_life=baseline['native']['player_life']
        if not interrupt_on_danger or battle_reason(baseline['native'],known_battle=known_battle) is None:
            result['captures'] += live.capture('navigation-start')
        # Long routes remain live while native progress advances. Per-node
        # stall detection, bounded replans, input leases and STOP still apply.
        while not max_seconds or clock() - started < max_seconds:
            if (output/'STOP').exists():
                result['status'] = 'stopped'
                break
            from .combat import fresh_rig
            state,nav_owner=fresh_rig(live,nav_owner)
            native = state['native']
            now = clock()
            point = _position(state)
            if memory is None:
                mission = mission or (native['mission'], native['location'])
                memory = ObstacleMemory(obstacle_file, nav.identity, mission, time.time())
                blocked = memory.active(time.time())
            if (native['mission'], native['location']) != mission:
                raise BotFault('Mission/location changed during native navigation')
            result['samples'].append({**{k:native.get(k) for k in
                ('player_x','player_y','player_z','player_life','not_alert','mission6_event_sequence','mission6_bridge_demo')},
                'seconds': now-started, 'mode': current_mode, 'route_index': index})
            if interrupt_on_danger:
                reason=battle_reason(native,baseline_life,known_battle=known_battle)
                if reason:
                    live.release();result['status']='needs_battle_response';result['reason']=reason
                    live.events.emit('navigation_danger_interrupt',reason=reason,state=state,
                                     seconds=now-started,no_further_movement=True)
                    break
            if stop_at_bridge and native.get('mission6_bridge_demo') is True:
                result['status'] = 'observed_native_bridge_trigger'
                break
            if native.get('demo') is True and native.get('demo_nonplayable') is not False:
                raise BotFault('Non-playable native cinematic owns movement')
            if (state['scene'] != 'gameplay' or not state['camera_active'] or state['menu'] or
                    state['idroid'] or native['player_vehicle_id'] != 65535 or native['player_life'] <= 0):
                raise BotFault('Native scene/camera/menu/travel/life changed; movement released')
            controls = authoritative_controls(state)
            if controls.get('travel_mode') == 4:
                # A native fence vault can temporarily own the standing action.
                # Release movement while it finishes; never drive an action as
                # ordinary locomotion or manufacture an on-foot acknowledgement.
                from .locomotion import _validate_xr_scene_state
                live.release()
                until = min(started + max_seconds, clock() + 2.) if max_seconds else clock() + 2.
                while True:
                    state, nav_owner = fresh_rig(live, nav_owner, clock=clock, sleep=sleep)
                    _validate_xr_scene_state(state)
                    native = state['native']
                    controls = authoritative_controls(state)
                    if ((native['mission'], native['location']) != mission
                            or native.get('popup') is not False or native.get('saving') is not False
                            or native.get('player_vehicle_id') != 65535
                            or native.get('demo') is True or native.get('status_CARRY') is not False
                            or controls.get('context') != 'gameplay' or controls.get('rig_input') is not True):
                        raise BotFault('Native navigation owner changed during action recovery')
                    if controls.get('travel_mode') == 1 and native.get('status_NORMAL_ACTION') is True:
                        break
                    if controls.get('travel_mode') not in (1, 4) or clock() >= until:
                        raise BotFault('Native navigation action did not return to ordinary foot controls')
                    live.events.emit('navigation_action_recovery_wait', state=state, movement_released=True)
                    sleep(.025)
                progress = RouteProgress()
                renew = 0.
                continue
            if controls['context'] != 'gameplay' or controls['travel_mode'] != 1 or not controls['rig_input']:
                raise BotFault('Fresh ordinary on-foot VR controls are required')
            if carry_reader is not None:
                if carry_reader() is not True:raise BotFault('Exact body carry ended during the walking route')
                _validate_field_state(state,witnessed_carry=True)
            if now >= next_enemies:
                snapshot = enemy_reader()
                if (snapshot['mission'], snapshot['location']) != mission:
                    raise BotFault('Enemy telemetry belongs to another mission')
                if snapshot.get('truncated') is not False or snapshot.get('unavailable_loaded') != 0:
                    raise BotFault('Loaded enemy telemetry is incomplete; movement released')
                result['enemy_snapshots'].append(snapshot)
                enemies = [e['position'] for e in snapshot['soldiers']
                           if avoid_threats and e.get('position') and e.get('life') == snapshot['normal_life']]
                next_enemies = now + 2.
            selected = movement_mode(native,enemies,last_life,current_mode,stealth=mode=='stealth') if mode in ('auto','stealth') else mode
            if crawl_turn_since is not None and selected in ('crawl','crouch'):selected='crouch'
            last_life = native['player_life']
            if selected == 'retreat':
                selected = 'evade'
                if history and now >= retreat_until:
                    # Backtrack over actual visited graph nodes, not a blind
                    # dodge into unmapped ground. Replan with current enemies.
                    choices = [n for n in reversed(history) if math.dist(point, nav.positions[n]) >= 12.]
                    if choices:
                        retreat_goal = nav.positions[choices[0]]
                        route = []
                        retreat_until = now + 15.
                        live.events.emit('navigation_retreat', position=point, goal=retreat_goal)
            if now < retreat_until:
                selected = 'evade'
            desired = selected if selected in ('crawl','crouch') else 'stand'
            if carry_reader is None and posture(native) != desired:
                set_posture(live, desired, sleep=sleep)
                result['postures'].append({'seconds': now-started, 'posture': desired})
                progress = RouteProgress()
                renew = 0.
                continue
            target_goal = retreat_goal or goal
            if arrived(point, target_goal, radius=3.5 if retreat_goal else arrival_radius):
                if carry_reader is not None and carry_reader() is not True:
                    raise BotFault('Exact body carry was lost before route arrival')
                live.release()
                if retreat_goal:
                    retreat_goal = None
                    route = []
                    continue
                result['status'] = 'observed_native_route'
                break
            if not route:
                live.release()
                start_node = nav.nearest(point)
                end_node = nav.nearest(target_goal)
                route = nav.path(start_node, end_node, blocked=blocked, threats=enemies)
                index = 0
                result['replans'].append({'seconds': now-started, 'route': route, 'blocked': sorted(blocked)})
                if len(result['replans']) > 16:
                    raise BotFault('Route exceeded its bounded collision/retreat replans')
                live.events.emit('navigation_plan', nodes=len(route), blocked=len(blocked), start=point, goal=target_goal)
                progress = RouteProgress()
                renew = 0.
            while index < len(route) and arrived(point, nav.positions[route[index]],
                    radius=min(1.4,arrival_radius*.5) if index==len(route)-1 and not retreat_goal else 1.4):
                history.append(route[index])
                index += 1
            if index == len(route):
                raise BotFault('Graph endpoint reached but physical goal is outside its arrival radius')
            target = nav.positions[route[index]]
            distance = math.dist(point, target)
            facing=body_heading_error(native,point,target)
            if crawl_turn_since is not None:
                if facing is None:raise BotFault('Native body facing became unavailable during the crouched turn')
                if facing<=45.:
                    live.release();set_posture(live,'crawl',sleep=sleep)
                    live.events.emit('navigation_crawl_turn_finished',state=state,error_degrees=facing,
                        elapsed_seconds=now-crawl_turn_since)
                    crawl_turn_since=None;progress=RouteProgress();renew=0.;continue
                if now-crawl_turn_since>2.:
                    raise BotFault('Crouched turn did not align native body before resuming the crawl')
            elif native.get('status_CRAWL') is True and facing is not None and facing>100.:
                # FOX can preserve a backwards prone animation when only the
                # VR view changes. Turn through a real crouched movement phase
                # on this same NAV edge; never rotate/write Snake's transform.
                live.release();set_posture(live,'crouch',sleep=sleep)
                crawl_turn_since=clock();crawl_turns+=1
                if crawl_turns>20:raise BotFault('Crawl route exceeded its bounded native turn corrections')
                live.events.emit('navigation_crawl_turn_started',state=state,error_degrees=facing,target=target)
                progress=RouteProgress();renew=0.;continue
            if progress.stalled(route[index], distance, now):
                live.release()
                if index == 0:
                    raise BotFault('Cannot reach the mapped starting floor; no traversed edge to replan')
                a = route[max(0,index-1)]
                b = route[index]
                blocked.add((a,b))
                blocked.add((b,a))
                memory.remember(a,b,time.time())
                result['captures'] += live.capture('navigation-blocked-'+str(len(blocked)//2))
                atomic_json(output/'obstacles.json', {'map_identity':nav.identity,'mission':mission,'edges':sorted(blocked)})
                live.events.emit('navigation_blocked', position=point, edge=[a,b])
                route = []
                continue
            # Native camera yaw already includes the current simulated head.
            # Replacing its absolute pose with only the remaining yaw error
            # repeatedly undid the previous turn. Compose the correction onto
            # the current head using this same published native camera.
            from .combat import looking_head
            camera=state['rendered']['native_pose']
            level_target=[target[0],camera['position'][1],target[2]]
            nav_head['orientation']=looking_head(nav_head['orientation'],camera,level_target)
            live.call('set_head_pose', {'base_space':'local',**nav_head})
            # Brake near graph corners; never cut a corner by skipping an edge.
            magnitude = .9 if selected in ('crawl','crouch') else (.70 if distance < 3. else 1.)
            use_sprint = selected in ('run','evade') and distance > (3. if selected == 'evade' else 6.)
            values = [{**move,'value':magnitude}] + (sprint if use_sprint else [])
            if now >= renew or values != current_channels:
                if carry_reader is not None and carry_reader() is not True:
                    raise BotFault('Exact body carry was lost before movement dispatch')
                if values != current_channels:
                    keys = {(v['hand'],v['component'],v.get('sub_component')) for v in values}
                    for old in current_channels or []:
                        if (old['hand'],old['component'],old.get('sub_component')) not in keys:
                            live.call('set_controller_input',{**old,'value':0,'auto_release':False})
                live.input(values, 1., lease_seconds=1.)
                current_channels = values
                renew = now + .55
            current_mode = selected
            if now >= next_report:
                atomic_json(output/'progress.json', result)
                live.events.emit('navigation_progress', position=point, remaining_nodes=len(route)-index,
                                 mode=selected, life=native['player_life'], enemies=len(enemies))
                print({'position':[round(v,1) for v in point], 'mode':selected,
                       'remaining_nodes':len(route)-index, 'life':native['player_life']}, flush=True)
                next_report = now + 10.
            sleep(.08)
        else:
            raise BotFault('Native navigation deadline reached')
    except Exception as error:
        result['status'] = 'failed'
        result['error'] = str(error)
    finally:
        finish_navigation(live, head, result, output)
    return result
