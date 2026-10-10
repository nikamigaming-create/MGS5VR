"""Escalate an exhausted field policy to its supervising worker, with evidence.

The worker supplies one mapped waypoint, not arbitrary input or native writes.
Successful guidance is retained as a learning candidate; promoting a reusable
rule requires its real action outcome and a separate regression check.
"""
import json
import math
import time
import uuid

from .combat import admit
from .core import BotFault, atomic_json
from .live import digest
from .locomotion import _position


class CommanderBattleChanged(BotFault):
    pass


def validate_guidance(decision, request, nav, state, *, now_ns=None):
    if (decision.get('request_id') != request['request_id']
        or decision.get('kind') != 'mapped_waypoint'
        or decision.get('map_identity') != nav.identity):
        raise BotFault('Commander clue belongs to another request, action or map')
    age=((time.time_ns() if now_ns is None else now_ns)-request['issued_unix_ns'])/1e9
    if not 0<=age<=30:
        raise BotFault('Commander clue needs a fresh two-eye observation')
    expected=[row['sha256'] for row in request['captures']]
    reviewed=decision.get('reviewed_both_capture_sha256')
    if (len(expected)!=2 or len(set(expected))!=2 or not isinstance(reviewed,list)
        or sorted(reviewed)!=sorted(expected)):
        raise BotFault('Commander clue must cite both current final-eye images')
    native=state['native'];frame=state['rendered']
    current=[native['mission'],native['location'],state['activation'],
             frame['player_owner'],frame['presentation_epoch']]
    if current!=request['world_owner']:
        raise BotFault('Commander clue lost its native world or presentation owner')
    if native.get('not_alert') is not True or native['player_life']<=0:
        raise BotFault('Waiting for a commander clue is unsafe in the current battle state')
    goal=decision.get('goal');lesson=decision.get('lesson')
    if (not isinstance(goal,list) or len(goal)!=3
        or any(type(v) not in (int,float) or not math.isfinite(v) for v in goal)
        or not isinstance(lesson,str) or not lesson.strip() or len(lesson)>1000):
        raise BotFault('Commander clue needs a finite waypoint and an explicit lesson')
    point=_position(state)
    if math.dist(point,goal)>40:
        raise BotFault('Commander waypoint exceeds the bounded local recovery range')
    start=nav.nearest(point);end=nav.nearest(goal)
    route=nav.path(start,end)
    return list(nav.positions[end]),{'node':end,'route_nodes':len(route),'lesson':lesson,
                                    'request_id':request['request_id'],'scope':'experimental_guidance'}


def ask_commander(live, nav, *, output, anchor, reason, stages, deadline,
                  wait_seconds=10., clock=time.monotonic, sleep=time.sleep):
    """Publish fresh neutral eyes and watch the battle while waiting for a clue."""
    live.release();state,_=admit(live,anchor)
    if state['native'].get('not_alert') is not True:
        raise CommanderBattleChanged('Detected before commander wait; resume battle policy')
    captures=live.capture('commander-stuck')
    state,_=admit(live,anchor)
    request={'schema':1,'request_id':uuid.uuid4().hex,'issued_unix_ns':time.time_ns(),
             'reason':reason,'world_owner':list(anchor),'map_identity':nav.identity,
             'state':state,'attempted_stages':stages,
             'captures':[{'path':str(p),'sha256':digest(p)} for p in captures],
             'allowed_clue':'mapped_waypoint within 40 m, with a lesson and both reviewed hashes',
             'response_file':str(output/'commander'/'decision.json'),
             'status':'needs_commander'}
    atomic_json(output/'commander'/'request.json',request)
    live.events.emit('commander_attention_required',request_id=request['request_id'],reason=reason,
                     request_file=str(output/'commander'/'request.json'))
    decision_path=output/'commander'/'decision.json'
    until=min(deadline,clock()+min(90.,max(0.,wait_seconds)))
    refresh_at=clock()+20.;waiting_life=state['native']['player_life']
    while clock()<until:
        if (output/'STOP').exists():raise BotFault('Stopped while awaiting commander')
        state,_=admit(live,anchor)
        if state['native'].get('not_alert') is not True:
            raise CommanderBattleChanged('Detected during commander wait; resume battle policy')
        if state['native']['player_life']<waiting_life:
            raise CommanderBattleChanged('Damage during commander wait; resume health priority')
        if decision_path.is_file():
            decision=json.loads(decision_path.read_text(encoding='utf-8'))
            goal,plan=validate_guidance(decision,request,nav,state)
            atomic_json(output/'commander'/'accepted.json',{'request':request,'decision':decision,'plan':plan})
            live.events.emit('commander_clue_accepted',**plan,goal=goal)
            return goal,plan
        if clock()>=refresh_at:
            captures=live.capture('commander-refreshed');state,_=admit(live,anchor)
            request.update(request_id=uuid.uuid4().hex,issued_unix_ns=time.time_ns(),state=state,
                           captures=[{'path':str(p),'sha256':digest(p)} for p in captures])
            atomic_json(output/'commander'/'request.json',request)
            live.events.emit('commander_attention_refreshed',request_id=request['request_id'],
                             request_file=str(output/'commander'/'request.json'))
            refresh_at=clock()+20.
        sleep(.1)
    request['status']='commander_deadline'
    atomic_json(output/'commander'/'request.json',request)
    raise BotFault('Commander clue deadline; retained fresh evidence instead of repeating the failed approach')
