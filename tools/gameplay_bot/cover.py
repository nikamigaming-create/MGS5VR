"""Choose mapped cover from fresh native terrain rays, then recheck on arrival.

Obstruction of observed person-marker rays is planning evidence. It does not
identify enemy eyes, measure shadows, or certify the game's concealment rules.
"""
import math
import time
from collections import deque

from .combat import admit
from .core import BattleResponseNeeded, BotFault
from .locomotion import _position
from .navigation import NoConnectedNativeRoute


def floor_candidates(nav, point, goal, *, limit=8):
    """A small connected native neighbourhood, retaining actual floor heights."""
    centre=nav.nearest(goal);seen={centre};queue=deque([centre]);nodes=[]
    while queue and len(seen)<=256:
        node=queue.popleft();position=nav.positions[node]
        if math.dist(position,point)<=59.5 and abs(position[1]-goal[1])<=2.5:
            nodes.append(node)
        for other in nav.adj[node]:
            if other not in seen and math.dist(nav.positions[other],goal)<=8.:
                seen.add(other);queue.append(other)
    if len(nodes)<=limit:return nodes
    # Start near the route's intended stop, then spread probes along actual
    # connected ground rather than spending all eight rays in one tiny patch.
    selected=[min(nodes,key=lambda n:math.dist(nav.positions[n],goal))]
    while len(selected)<limit:
        remaining=[n for n in nodes if n not in selected]
        selected.append(max(remaining,key=lambda n:(
            min(math.dist(nav.positions[n],nav.positions[s]) for s in selected),-n)))
    return selected


def flank_candidates(nav, point, actor, *, radius=12., check_abort=None):
    """Ground routes around an obstructed actor, including elevated guards.

    Circle points are search hints only. Returned XYZ comes from connected
    native NAV nodes on the player's supporting floor, never a guessed floor.
    """
    start=nav.nearest(point);nodes=[]
    for step in range(8):
        if check_abort is not None:check_abort()
        angle=step*math.pi/4.;hint=[actor['position'][0]+radius*math.cos(angle),point[1],
                                  actor['position'][2]+radius*math.sin(angle)]
        try:
            node=nav.nearest(hint,max_distance=6.,max_height=2.5)
            if node in nodes or not 3<=math.dist(point,nav.positions[node])<=40.:continue
            nav.path(start,node,check_abort=check_abort)
            nodes.append(node)
        except BattleResponseNeeded:raise
        except BotFault:continue
    return nodes


def verified_candidates(data, nav, nodes, state, nonce, *, transport_seconds=0.):
    frame=state['rendered'];now=data.get('now_ms');owner=(state['activation'],frame['player_owner'],frame['presentation_epoch'])
    if (data.get('schema')!=1 or data.get('source')!='native_marker_job_terrain_obstruction'
        or data.get('available') is not True or data.get('nonce')!=nonce
        or tuple(data.get(k) for k in ('activation','player_owner','presentation_epoch'))!=owner):
        raise BotFault('Cover ray result has no fresh matching native owner/nonce')
    if (type(now) is not int or not math.isfinite(transport_seconds) or transport_seconds<0
        or any(type(data.get(key)) is not int or not 0<=now-data[key]+transport_seconds*1000<=age
               for key,age in (('sample_ms',150),('frame_sample_ms',150),('observed_player_ms',250)))
        or not data.get('issued_ms',now+1)<=now<data.get('expires_ms',0)):
        raise BotFault('Cover ray result is stale or outside its native lease')
    point=_position(state);root=data.get('observed_player');offset=data.get('eye_offset')
    finite=lambda v:isinstance(v,list) and len(v)==3 and all(type(x) in (int,float) and math.isfinite(x) for x in v)
    if not finite(root) or not finite(offset) or math.dist(root,point)>.002 or math.dist(offset,[0,0,0])>3.001:
        raise BotFault('Cover ray result lost the observed root/eye join')
    rows=data.get('candidates');persons=data.get('persons')
    if (not isinstance(rows,list) or not isinstance(persons,list) or not 1<=len(persons)<=4
        or len(rows)!=len(nodes) or len({p.get('id') for p in persons})!=len(persons)):
        raise BotFault('Cover ray candidate/person set is incomplete')
    ids={p['id'] for p in persons};accepted=[]
    for row in rows:
        node=row.get('node');position=row.get('point');eye=row.get('eye');rays=row.get('rays')
        if (type(node) is not int or node not in nodes or not finite(position) or not finite(eye)
            or math.dist(position,nav.positions[node])>.002
            or math.dist(eye,[a+b for a,b in zip(position,offset)])>.002
            or not isinstance(rays,list) or len(rays)!=len(persons)
            or {r.get('actor_id') for r in rays}!=ids):
            raise BotFault('Cover ray points do not match the requested native floor')
        for ray in rays:
            values=[ray.get(k) for k in ('distance','raw_along','raw_lateral_squared')]
            if any(type(v) not in (int,float) or not math.isfinite(v) for v in values):
                raise BotFault('Nonfinite native cover ray')
            length,along,lateral=values
            blocked=(ray.get('query_valid') is True and ray.get('query_reason')==9
                     and type(ray.get('ray_result')) is int and ray['ray_result']>0
                     and 1<=length<=300 and .05<=along<length-.35 and 0<=lateral<=.04)
            if ray.get('terrain_obstructed') is not blocked:
                raise BotFault('Native cover classification disagrees with its retained terrain ray')
        accepted.append(row)
    if len({r['node'] for r in accepted})!=len(nodes):raise BotFault('Duplicate native cover result')
    return accepted


def probe(live,nav,nodes,anchor,*,clock=time.monotonic,sleep=time.sleep,check_abort=None):
    """Observation only. Reissue stale requests with a new root and nonce."""
    if not nodes:return None
    for attempt in range(2):
        if check_abort is not None:check_abort()
        state,_=admit(live,anchor);point=_position(state);nonce=time.time_ns() & ((1<<63)-1)
        numeric=lambda values:','.join(format(value,'.9g') for value in values)
        frame=state['rendered']
        command='diagnostics:cover-probe:'+':'.join(map(str,[nonce,1500,state['activation'],
            frame['player_owner'],frame['presentation_epoch'],state['now_ms']]))+':'+numeric(point)+':'
        command+=';'.join(str(node)+','+numeric(nav.positions[node]) for node in nodes)
        data,timing=live.native.read(command);accepted=data.get('accepted') is True;deadline=clock()+.20
        while accepted and not data.get('available') and data.get('reason')=='awaiting_native_marker_job' and clock()<deadline:
            if check_abort is not None:check_abort()
            sleep(.015);data,timing=live.native.read('inspect-cover-probe:'+str(nonce))
        if check_abort is not None:check_abort()
        live.events.emit('native_cover_probe',snapshot=data,transport=timing,attempt=attempt+1,
                         map_identity=nav.identity,input_replayed=False)
        if not accepted or not data.get('available'):continue
        try:
            rows=verified_candidates(data,nav,nodes,state,nonce,transport_seconds=timing.get('seconds',0.))
            return {'snapshot':data,'rows':rows,'native_posture':{
                k:state['native'][k] for k in ('status_CRAWL','status_SQUAT','status_STAND')}}
        except BotFault as error:
            live.events.emit('native_cover_probe_rejected',reason=str(error),input_replayed=False)
    return None


def select(rows, active_ids, point, intended, *, expose_id=None, max_peek_distance=25.):
    """Prefer obstruction; a peek exposes only the selected guard's ray."""
    ranked=[]
    for row in rows:
        rays={r['actor_id']:r for r in row['rays'] if r['actor_id'] in active_ids}
        if not rays or any(r['query_valid'] is not True for r in rays.values()):continue
        missing=sorted(active_ids-set(rays));blocked=sum(r['terrain_obstructed'] is True for r in rays.values())
        if expose_id is not None:
            target=rays.get(expose_id)
            if (not target or target['terrain_obstructed'] is not False
                or target['distance']>max_peek_distance):continue
        elif not blocked:continue
        distance=math.dist(row['point'],point)
        if distance<2.:continue
        # Missing guard rays are explicit; never label partial cover as safe.
        score=(not missing,blocked,-math.dist(row['point'],intended),-distance)
        ranked.append((score,row,missing))
    if not ranked:return None
    _,row,missing=max(ranked,key=lambda item:item[0])
    return {'node':row['node'],'goal':row['point'],'rays':row['rays'],
            'missing_active_guard_ids':missing,'observed_marker_cover':not missing and all(
                r['terrain_obstructed'] for r in row['rays'] if r['actor_id'] in active_ids),
            'enemy_ai_concealment_proven':False,'shadows_proven':False}
