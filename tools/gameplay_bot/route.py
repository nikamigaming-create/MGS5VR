"""Follow native navigation edges through ordinary configured VR inputs."""
import math
import time

from .core import BotFault, atomic_json
from .live import authoritative_controls, channel
from .locomotion import _axis_source, _position, _validate_field_state
from .navigation import ObstacleMemory, RouteProgress, arrived, movement_mode, posture, stance_press


def set_posture(live, desired, *, sleep=time.sleep):
    live.release()
    for _ in range(3):
        current = posture(live.observe(native=True)['native'])
        seconds = stance_press(current, desired)
        if seconds is None:
            return
        live.execute({'op': 'action', 'name': 'gameplay.stance', 'seconds': seconds})
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
        if not errors:
            result['captures'] += live.capture('navigation-finished')
    except Exception as error:
        errors.append('final_observation: ' + str(error))
    if errors:
        result['cleanup_errors'] = errors
        result['observed_status_before_cleanup'] = result['status']
        result['status'] = 'failed'
        result.setdefault('error', errors[0])
    atomic_json(output/'result.json', result)


def navigate(live, nav, goal, *, output, enemy_reader, mode='auto', max_seconds=600,
             obstacle_file=None, stop_at_bridge=True, expected_location=None,
             clock=time.monotonic, sleep=time.sleep):
    if mode not in ('auto', 'walk', 'run', 'crouch', 'crawl'):
        raise BotFault('Unknown movement mode')
    admitted_mission = None
    if expected_location is not None:
        preflight = live.observe(native=True)
        observed = preflight.get('native', {}).get('location')
        if type(expected_location) is not int or type(observed) is not int or observed != expected_location:
            raise BotFault('Native location changed after selecting its navigation graph; no movement dispatched')
        _validate_field_state(preflight)
        admitted_mission = (preflight['native']['mission'], observed)
    source = _axis_source(live.bindings, 'axes.move')
    move = {'hand': source.split('_')[0], 'component': 'Thumbstick', 'sub_component': 'Y'}
    run = next(a for a in live.bindings['actions'] if a['name'] == 'gameplay.run')
    sprint = [channel(t) for t in run['bindings'][0]['inputs']]
    head = live.text_result(live.call('get_head_pose', {'base_space': 'local'}))['pose']
    result = {'status': 'running', 'map_identity': nav.identity, 'goal': goal,
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
    try:
        live.release()
        live.call('set_head_pose', {'base_space': 'local', 'position': head['position'], 'orientation': [0,0,0,1]})
        live.execute({'op': 'action', 'name': 'system.recenter'})
        sleep(.25)
        result['captures'] += live.capture('navigation-start')
        while clock() - started < max_seconds:
            if (output/'STOP').exists():
                result['status'] = 'stopped'
                break
            state = live.observe(native=True)
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
            if stop_at_bridge and native.get('mission6_bridge_demo') is True:
                result['status'] = 'observed_native_bridge_trigger'
                break
            if native.get('demo') is True and native.get('demo_nonplayable') is not False:
                raise BotFault('Non-playable native cinematic owns movement')
            if (state['scene'] != 'gameplay' or not state['camera_active'] or state['menu'] or
                    state['idroid'] or native['player_vehicle_id'] != 65535 or native['player_life'] <= 0):
                raise BotFault('Native scene/camera/menu/travel/life changed; movement released')
            controls = authoritative_controls(state)
            if controls['context'] != 'gameplay' or controls['travel_mode'] != 1 or not controls['rig_input']:
                raise BotFault('Fresh ordinary on-foot VR controls are required')
            if now >= next_enemies:
                snapshot = enemy_reader()
                if (snapshot['mission'], snapshot['location']) != mission:
                    raise BotFault('Enemy telemetry belongs to another mission')
                result['enemy_snapshots'].append(snapshot)
                enemies = [e['position'] for e in snapshot['soldiers']
                           if e.get('position') and e.get('life') == snapshot['normal_life']]
                next_enemies = now + 2.
            selected = movement_mode(native, enemies, last_life, current_mode) if mode == 'auto' else mode
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
            if posture(native) != desired:
                set_posture(live, desired, sleep=sleep)
                result['postures'].append({'seconds': now-started, 'posture': desired})
                progress = RouteProgress()
                renew = 0.
                continue
            target_goal = retreat_goal or goal
            if arrived(point, target_goal, radius=3.5):
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
            while index < len(route) and arrived(point, nav.positions[route[index]]):
                history.append(route[index])
                index += 1
            if index == len(route):
                raise BotFault('Graph endpoint reached but physical goal is outside its arrival radius')
            target = nav.positions[route[index]]
            distance = math.dist(point, target)
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
            dx, dz = target[0]-point[0], target[2]-point[2]
            turn = (math.degrees(math.atan2(dx,dz))-native['player_camera_yaw']+180)%360-180
            half = math.radians(turn)/2
            live.call('set_head_pose', {'base_space':'local', 'position':head['position'],
                                       'orientation':[0,math.sin(half),0,math.cos(half)]})
            # Brake near graph corners; never cut a corner by skipping an edge.
            magnitude = .9 if selected in ('crawl','crouch') else (.70 if distance < 3. else 1.)
            use_sprint = selected in ('run','evade') and distance > (3. if selected == 'evade' else 6.)
            values = [{**move,'value':magnitude}] + (sprint if use_sprint else [])
            if now >= renew or values != current_channels:
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
