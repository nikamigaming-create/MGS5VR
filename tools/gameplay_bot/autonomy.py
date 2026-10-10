"""Reusable bounded field policy: approach, engage, verify, and recover.

The native atlas and enemy state are planning knowledge. Route, weapon, actor
and visual evidence remain distinct. Unknown actions never become capabilities
merely because an objective requests them.
"""
import math
import time

from .combat import admit, engage
from .commander import CommanderBattleChanged, ask_commander
from .core import BattleResponseNeeded, BotFault, atomic_json
from .locomotion import _position
from .perception import read_enemy_snapshot
from .route import navigate, set_posture
from .session import preserved_tracking_poses
from .health import critical_wound, needs_recovery, recover_health
from .stealth import prepare_loadout
from .navigation import NoConnectedNativeRoute
from .recon import inspect as inspect_recon
from .cover import flank_candidates, floor_candidates, probe as probe_cover, select as select_cover
from .tactics import TacticalRuntime


class NoImprovingVantage(BotFault):
    pass


def choose_vantage(nav, point, actor, standoff, *, threats=(), check_abort=None):
    if not 4<=standoff<=90:raise BotFault('Approach must retain a bounded engagement distance')
    destination=actor['position']
    # Perception owns actor records; NativeMap's cost field owns XYZ vectors.
    # Keep their boundary explicit instead of indexing an actor as a vector.
    threat_positions=[row['position'] for row in threats]
    route=nav.path(nav.nearest(point),nav.nearest(destination),threats=threat_positions,check_abort=check_abort)
    candidates=[node for node in route if math.dist(nav.positions[node],destination)>=standoff]
    if not candidates:raise NoImprovingVantage('No connected approach retains the requested standoff')
    chosen=candidates[-1]
    if math.dist(point,nav.positions[chosen])<4.:
        raise NoImprovingVantage('Mapped approach cannot improve the current vantage')
    return list(nav.positions[chosen]),{'route_nodes':len(route),'node':chosen,'standoff':standoff,
                                       'native_enemy_position':list(destination),'map_identity':nav.identity}


def secure_camp(live, nav, *, output, enemy_script, camp, max_seconds=180., obstacle_file=None,
                stealth_loadout=None):
    if not isinstance(camp,str) or not camp or not 10<=max_seconds<=600:
        raise BotFault('Secure-camp needs one named native camp and bounded duration')
    output.mkdir(parents=True,exist_ok=True)
    result={'status':'running','objective':'secure-camp','camp':camp,'stages':[],
            'visual_acceptance':'pending_both_eye_and_action_review','map_identity':nav.identity,
            'capabilities_not_proven':['carry','conceal_body','interrogate','vehicle','binoculars','supply_drop']}
    started=time.monotonic();anchor=None;attempt=0;cycle=0;recoveries=0;visited=[];recons=0
    standoffs=(20.,10.,4.);guided=False;overt=False;cover_home=None;peek_attempts=0;flanks=set();cqc_attempts=set()
    try:
        with preserved_tracking_poses(live):
            live.release();state,anchor=admit(live)
            result['before']=state
            if stealth_loadout is None:raise BotFault('Autonomous stealth requires a reviewed loadout policy')
            healthy_life=state['native']['player_life']
            runtime=TacticalRuntime(live.events,healthy_life);runtime.publish(state)
            camp_job=runtime.enqueue('secure_camp',20,camp=camp,map_identity=nav.identity)
            live.tactical_runtime=runtime
            def watch_battle():
                current,_=admit(live,anchor)
                runtime.publish(current)
                if ((not overt and current['native'].get('not_alert') is False)
                        or needs_recovery(current['native'],healthy_life)):
                    live.events.emit('planning_battle_interrupt',state=current,no_action_dispatched=True)
                    raise BattleResponseNeeded('Current detection or damage preempts neutral planning')
            visited.append(state['native'])
            result['loadout']=prepare_loadout(live,stealth_loadout,output=output,anchor=anchor)
            shot_slot=stealth_loadout['shot_slot']
            result['policy']='stealth_first; reviewed secondary; interrupt on detection; bounded defense then mapped health recovery'
            enemies=read_enemy_snapshot(live.native.read,enemy_script)
            recon=inspect_recon(live,enemies,output=output/'recon-00',anchor=anchor,
                               camp=camp,healthy_life=healthy_life)
            result['stages'].append({'kind':'reconnaissance','status':recon['status'],
                                    'result':str(output/'recon-00'/'result.json'),
                                    'native_marked_count':recon.get('native_marked_count',0)})
            if recon['status']=='failed':raise BotFault(recon['error'])
            recons=1
            while time.monotonic()-started<max_seconds:
                if (output/'STOP').exists():result['status']='stopped';break
                state,_=admit(live,anchor)
                runtime.publish(state);intent=runtime.intent(known_battle=overt)
                if intent=='counterattack':
                    fallback=stealth_loadout.get('detection_fallback')
                    if not fallback:raise BotFault('Detected without a reviewed heavy-weapon fallback')
                    detected_at=time.monotonic()
                    live.events.emit('autonomous_detection_response_started',state=state,
                                     interrupt_stealth_planning=True)
                    result['detected_loadout']=prepare_loadout(live,fallback,output=output,anchor=anchor,nonlethal=False)
                    shot_slot=fallback['shot_slot'];overt=True
                    state,_=admit(live,anchor)
                    live.events.emit('autonomous_detection_fallback',slot=shot_slot,policy=fallback,
                                     selection_seconds=time.monotonic()-detected_at,state=state)
                    if not critical_wound(state['native']) and state['native']['player_life']>=healthy_life*.35:
                        if state['native'].get('status_CRAWL') is True:set_posture(live,'crouch')
                        defense_output=output/'detection-response'
                        defense=engage(live,output=defense_output,enemy_script=enemy_script,camp=None,
                            max_seconds=min(8.,max_seconds-(time.monotonic()-started)),max_rounds=4,radius=60.,
                            healthy_life=None,expected_slot=shot_slot)
                        result['stages'].append({'kind':'immediate_detection_response','status':defense['status'],
                            'rounds':defense['rounds'],'outcomes':defense['outcomes'],
                            'result':str(defense_output/'result.json')})
                        if defense['status']=='failed':raise BotFault(defense['error'])
                    state,_=admit(live,anchor)
                if needs_recovery(state['native'],healthy_life):
                    if recoveries>=2:raise BotFault('Repeated damage exhausted this bounded recovery test')
                    recovery_output=output/f'recovery-{recoveries:02d}';recoveries+=1
                    recovery=recover_health(live,nav,output=recovery_output,anchor=anchor,
                        healthy_life=healthy_life,visited=visited,
                        enemy_reader=lambda:read_enemy_snapshot(live.native.read,enemy_script),
                        deadline=started+max_seconds,obstacle_file=obstacle_file)
                    result['stages'].append({'kind':'health_recovery','result':str(recovery_output/'result.json'),
                                            'status':recovery['status']})
                    if recovery['status']!='observed_native_health_recovered':raise BotFault(recovery['error'])
                    state,new_anchor=admit(live)
                    if new_anchor[:2]!=anchor[:2] or new_anchor[3:]!=anchor[3:]:raise BotFault('Recovery changed native world/owner')
                    anchor=new_anchor
                healthy_life=max(healthy_life,state['native']['player_life'])
                enemies=read_enemy_snapshot(live.native.read,enemy_script)
                state,_=admit(live,anchor);runtime.publish(state);runtime.publish_enemies(enemies)
                if (enemies['mission'],enemies['location'])!=anchor[:2]:raise BotFault('Native enemy world changed')
                roster=[r for r in enemies['soldiers'] if r.get('camp')==camp]
                if not roster:raise BotFault('Requested camp has no observed native soldier roster')
                active=[r for r in roster if r['life']==enemies['normal_life']]
                point=_position(state)
                nearby=[r for r in enemies['soldiers'] if r['life']==enemies['normal_life'] and math.dist(point,r['position'])<45.]
                result['current_roster']=[{k:r[k] for k in ('name','id','life','phase','position')} for r in roster]
                live.events.emit('autonomous_guard_decision',roster=result['current_roster'],
                                 nearby_active=nearby,position=point)
                if not active and not nearby:
                    # A new snapshot verifies the native camp outcome. This is
                    # temporary incapacitation, never permanent map completion.
                    time.sleep(.2)
                    confirm=read_enemy_snapshot(live.native.read,enemy_script)
                    confirmed=[r for r in confirm['soldiers'] if r.get('camp')==camp]
                    live.events.emit('autonomous_guard_confirmation',roster=confirmed,
                                     normal_life=confirm['normal_life'],initial_count=len(roster))
                    if (len(confirmed)==len(roster) and all(r['life']!=confirm['normal_life'] for r in confirmed)):
                        state,_=admit(live,anchor);runtime.publish(state)
                        runtime.finish(camp_job,{'status':'observed_native_camp_incapacitated','roster':confirmed})
                        result['last_cover_goal']=cover_home
                        result['status']='observed_native_camp_incapacitated';break
                remaining=max_seconds-(time.monotonic()-started)
                if cycle>=20:raise BotFault('Camp policy exceeded its bounded action cycles')
                # A nearby isolated guard has a real lowered-trigger CQC path.
                # Approach on the native floor, interrupt on detection, then
                # independently revalidate proximity before the first press.
                close=sorted((r for r in active if r['id'] not in cqc_attempts
                    and math.hypot(r['position'][0]-point[0],r['position'][2]-point[2])<=7.
                    and abs(r['position'][1]-point[1])<=1.5),
                    key=lambda r:math.dist(point,r['position']))
                if close and not overt:
                    actor=close[0];cqc_attempts.add(actor['id'])
                    isolated=all(r['id']==actor['id'] or r['life']!=enemies['normal_life']
                        or math.hypot(r['position'][0]-actor['position'][0],r['position'][2]-actor['position'][2])>2.5
                        or abs(r['position'][1]-actor['position'][1])>1.5 for r in enemies['soldiers'])
                    if isolated:
                        try:
                            watch_battle();end=nav.nearest(actor['position'])
                            goal=list(nav.positions[end]);nav.path(nav.nearest(point),end,check_abort=watch_battle)
                            if math.hypot(goal[0]-actor['position'][0],goal[2]-actor['position'][2])>1.:
                                raise BotFault('Native floor does not support the close CQC destination')
                        except BattleResponseNeeded:continue
                        except BotFault as error:
                            live.events.emit('cqc_approach_unavailable',target_id=actor['id'],reason=str(error))
                        else:
                            stage=output/f'cqc-{actor["id"]}';stage.mkdir(exist_ok=True)
                            if math.hypot(point[0]-actor['position'][0],point[2]-actor['position'][2])>1.25:
                                route=navigate(live,nav,goal,output=stage/'approach',
                                    enemy_reader=lambda:read_enemy_snapshot(live.native.read,enemy_script),
                                    mode='stealth',max_seconds=min(15.,remaining),expected_location=anchor[1],
                                    obstacle_file=obstacle_file,arrival_radius=.75,recenter=False,
                                    interrupt_on_danger=True,known_battle=False)
                                visited.extend(route['samples'])
                                result['stages'].append({'kind':'cqc_approach','target_id':actor['id'],
                                    'status':route['status'],'result':str(stage/'approach'/'result.json')})
                                if route['status']=='needs_battle_response':continue
                                if route['status']!='observed_native_route':
                                    live.events.emit('cqc_approach_blocked',target_id=actor['id'],result=route)
                                    continue
                            set_posture(live,'crouch')
                            from .cqc import attempt_cqc
                            action=attempt_cqc(live,actor=actor,anchor=anchor,enemy_script=enemy_script,
                                output=stage,healthy_life=healthy_life,pulses=5,pulse_seconds=.20)
                            result['stages'].append({'kind':'close_quarters','target_id':actor['id'],
                                'status':action['status'],'result':str(stage/'result.json')})
                            atomic_json(output/'progress.json',result)
                            if action.get('ordinary_field_resumed') is False:
                                raise BotFault('Native CQC release remains pending; ordinary field input withheld')
                            state,_=admit(live,anchor)
                            # A safe neutral failed experiment is retained and
                            # the firearm policy may continue. Never repeat CQC
                            # or proceed while a native restraint still owns it.
                            if action['status']=='native_cqc_release_pending':raise BotFault('Native CQC release remains pending')
                            continue
                if state['native'].get('status_CRAWL') is True:
                    # A crawl route keeps the approach low. Raise only into a
                    # real crouch for the short weapon/visibility inspection.
                    set_posture(live,'crouch')
                stage=output/f'combat-{cycle:02d}';cycle+=1
                combat=engage(live,output=stage,enemy_script=enemy_script,camp=None,
                              max_seconds=min(30.,remaining),max_rounds=12 if overt else 4,radius=60. if overt else 25.,
                              healthy_life=healthy_life,expected_slot=shot_slot)
                result['stages'].append({'kind':'combat','result':str(stage/'result.json'),
                                        'status':combat['status'],'rounds':combat['rounds'],'outcomes':combat['outcomes']})
                atomic_json(output/'progress.json',result)
                if combat['status']=='failed':raise BotFault(combat['error'])
                if combat['status']=='needs_health_recovery':continue
                if combat['outcomes']:
                    if cover_home is not None and not overt:
                        state,_=admit(live,anchor)
                        if math.dist(_position(state),cover_home)>2.:
                            return_output=output/f'cover-return-{cycle:02d}'
                            return_output.mkdir()
                            back=navigate(live,nav,cover_home,output=return_output,
                                enemy_reader=lambda:read_enemy_snapshot(live.native.read,enemy_script),
                                mode='stealth',max_seconds=min(40.,max_seconds-(time.monotonic()-started)),
                                obstacle_file=obstacle_file,expected_location=anchor[1],arrival_radius=1.5,recenter=False,
                                interrupt_on_danger=True,known_battle=overt)
                            result['stages'].append({'kind':'return_to_cover','status':back['status'],
                                                    'result':str(return_output/'result.json')})
                            if back['status'] not in ('observed_native_route','needs_battle_response'):
                                raise BotFault(back.get('error','Cover return did not arrive'))
                            visited.extend(back['samples']);state,new_anchor=admit(live)
                            if new_anchor[:2]!=anchor[:2] or new_anchor[3:]!=anchor[3:]:raise BotFault('Cover return changed native owner')
                            anchor=new_anchor
                    # Replan from the new roster, without an agent round trip.
                    continue
                if combat['status']!='no_visible_active_target':
                    raise BotFault('Combat budget ended without a verified incapacitation')
                enemies=read_enemy_snapshot(live.native.read,enemy_script)
                state,_=admit(live,anchor);point=_position(state)
                active=[r for r in enemies['soldiers'] if r.get('camp')==camp and r['life']==enemies['normal_life']]
                if not active:
                    active=[r for r in enemies['soldiers'] if r['life']==enemies['normal_life']
                            and math.dist(point,r['position'])<45.]
                if not active:continue
                active_ids={r['id'] for r in active}
                active += [r for r in enemies['soldiers'] if r['life']==enemies['normal_life']
                           and r['id'] not in active_ids and math.dist(point,r['position'])<45.]
                approachable=sorted(active,key=lambda r:math.dist(point,r['position']))
                unreachable=set()
                threats=[r for r in enemies['soldiers'] if r['life']==enemies['normal_life']]
                found=False;planning_battle_changed=False
                if cover_home is not None and not overt and peek_attempts<3:
                    # Same native floor neighbourhood, this time with the
                    # actual crouched eye. A clear marker ray suggests a peek;
                    # fresh muzzle terrain rays still decide whether to fire.
                    peek_attempts+=1;set_posture(live,'crouch');state,_=admit(live,anchor);point=_position(state)
                    nodes=floor_candidates(nav,point,cover_home)
                    try:tested=probe_cover(live,nav,nodes,anchor,check_abort=watch_battle)
                    except BattleResponseNeeded:continue
                    active_ids={r['id'] for r in threats if math.dist(point,r['position'])<=180.}
                    if tested:
                        for actor in approachable:
                            peek=select_cover(tested['rows'],active_ids,point,cover_home,expose_id=actor['id'])
                            if peek:
                                goal=peek['goal'];plan={'node':peek['node'],'cover_peek':peek,
                                    'target_id':actor['id'],'map_identity':nav.identity,'cover_home':cover_home}
                                found=True;break
                while not found and attempt<len(standoffs):
                    for candidate in approachable:
                        if candidate['id'] in unreachable:continue
                        # Planning can consume time too. Recheck the real
                        # player between searches instead of idling in battle.
                        state,_=admit(live,anchor)
                        if needs_recovery(state['native'],healthy_life):break
                        try:
                            goal,plan=choose_vantage(nav,point,candidate,standoffs[attempt],threats=threats,
                                                    check_abort=watch_battle)
                            plan['target_id']=candidate['id']
                            if not overt:
                                set_posture(live,'crouch' if math.dist(point,candidate['position'])>55. else 'crawl')
                                state,_=admit(live,anchor);point=_position(state)
                                nodes=floor_candidates(nav,point,goal)
                                tested=probe_cover(live,nav,nodes,anchor,check_abort=watch_battle)
                                active_ids={r['id'] for r in threats if math.dist(point,r['position'])<=180.}
                                choice=select_cover(tested['rows'],active_ids,point,goal) if tested else None
                                plan['cover_probe_status']='observed_marker_obstruction' if choice else 'no_verified_cover_at_candidates'
                                if choice:
                                    goal=choice['goal'];plan['node']=choice['node'];plan['cover']=choice
                                    cover_home=list(goal);peek_attempts=0
                            found=True;break
                        except BattleResponseNeeded:
                            planning_battle_changed=True;break
                        except NoConnectedNativeRoute as error:
                            unreachable.add(candidate['id'])
                            live.events.emit('autonomous_actor_route_unavailable',target_id=candidate['id'],
                                position=candidate['position'],reason=str(error))
                        except NoImprovingVantage as error:
                            live.events.emit('autonomous_standoff_exhausted',target_id=candidate['id'],
                                standoff=standoffs[attempt],reason=str(error))
                    if found or planning_battle_changed or needs_recovery(state['native'],healthy_life):break
                    attempt+=1
                if planning_battle_changed or needs_recovery(state['native'],healthy_life):continue
                if not found and not overt:
                    for actor in approachable:
                        if actor['id'] in flanks:continue
                        flanks.add(actor['id']);set_posture(live,'crouch')
                        state,_=admit(live,anchor);point=_position(state)
                        try:
                            nodes=flank_candidates(nav,point,actor,check_abort=watch_battle)
                            tested=probe_cover(live,nav,nodes,anchor,check_abort=watch_battle)
                        except BattleResponseNeeded:
                            planning_battle_changed=True;break
                        active_ids={r['id'] for r in threats if math.dist(point,r['position'])<=180.}
                        choice=select_cover(tested['rows'],active_ids,point,point,expose_id=actor['id']) if tested else None
                        if choice:
                            goal=choice['goal'];plan={'node':choice['node'],'target_id':actor['id'],
                                'native_flank':choice,'map_identity':nav.identity,
                                'lesson':'Blocked centre approach requires a connected side angle with fresh native rays'}
                            found=True;break
                if planning_battle_changed:continue
                try:watch_battle()
                except BattleResponseNeeded:continue
                if not found:
                    if guided:raise BotFault('Commander waypoint did not resolve combat; retain a new learning failure')
                    try:
                        goal,plan=ask_commander(live,nav,output=output,anchor=anchor,
                            reason='All mapped approaches and available aim points remained obstructed',
                            stages=result['stages'],deadline=started+max_seconds)
                    except CommanderBattleChanged as error:
                        live.events.emit('commander_wait_interrupted',reason=str(error),
                                         response='resume_health_and_battle_policy')
                        continue
                    guided=True
                live.events.emit('autonomous_approach_plan',**plan,goal=goal)
                route_output=output/f"approach-{len(result['stages']):02d}";route_output.mkdir()
                with preserved_tracking_poses(live):
                    route=navigate(live,nav,goal,output=route_output,
                        enemy_reader=lambda:read_enemy_snapshot(live.native.read,enemy_script),
                        mode='stealth',max_seconds=min(150.,max_seconds-(time.monotonic()-started)),
                        obstacle_file=obstacle_file,expected_location=anchor[1],arrival_radius=1.5,recenter=False,
                        interrupt_on_danger=True,known_battle=overt)
                result['stages'].append({'kind':'approach','result':str(route_output/'result.json'),
                                        'status':route['status'],'goal':goal,'plan':plan})
                if route['status'] not in ('observed_native_route','needs_battle_response'):
                    raise BotFault(route.get('error','Native approach did not arrive'))
                visited.extend(route['samples'])
                if guided:
                    atomic_json(output/'commander'/'learning-candidate.json',{
                        'plan':plan,'route_result':str(route_output/'result.json'),
                        'route_status':route['status'],'action_outcome':'not_yet_proven',
                        'promotion':'requires subsequent combat outcome and regression check'})
                # Recenter is an established VR interaction used by navigation.
                # Read the new camera generation; keep mission/location/owner.
                state,new_anchor=admit(live)
                if new_anchor[:2]!=anchor[:2] or new_anchor[3:]!=anchor[3:]:raise BotFault('Approach changed native world/owner')
                anchor=new_anchor
                if route['status']=='needs_battle_response':continue
                if 'cover_peek' not in plan:attempt+=1
                if recons<2 and not overt:
                    enemies=read_enemy_snapshot(live.native.read,enemy_script)
                    recon_output=output/f'recon-{recons:02d}';recons+=1
                    recon=inspect_recon(live,enemies,output=recon_output,anchor=anchor,
                                       camp=camp,healthy_life=healthy_life)
                    result['stages'].append({'kind':'reconnaissance','status':recon['status'],
                        'result':str(recon_output/'result.json'),'native_marked_count':recon.get('native_marked_count',0)})
                    if recon['status']=='failed':raise BotFault(recon['error'])
                atomic_json(output/'progress.json',result)
            else:raise BotFault('Autonomous field policy reached its time deadline')
            result['after']=live.observe(native=True)
            result['captures']=live.capture('autonomy-finished-released')
    except Exception as error:
        result['status']='failed';result['error']=str(error)
    finally:
        try:
            live.release();live.native.read('diagnostics:combat-sight:0')
            live.native.read('diagnostics:cover-probe:off')
        except Exception as error:result['cleanup_error']=str(error);result['status']='failed'
        result['seconds']=time.monotonic()-started
        runtime=getattr(live,'tactical_runtime',None)
        if runtime is not None:
            result['tactical_runtime']={'revision':runtime.revision,'interrupt_epoch':runtime.interrupt_epoch,
                'pending':[job.name for job in runtime.pending],'completed':runtime.completed,
                'cancelled':runtime.cancelled,'pixel_scope':'rendered metadata and recorded frames; no decoded pixel cache'}
        atomic_json(output/'result.json',result)
    return result
