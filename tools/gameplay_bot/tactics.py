"""Fresh field blackboard and preemptible objective queue; no input ownership.

Rendered metadata identifies pixels. It is not a decoded image, enemy vision,
or proof that every game action is deterministic. Executors retain native,
physical-input and visual guards for their selected action.
"""
import copy
import math
import time
from collections import deque
from dataclasses import dataclass

from .core import BotFault
from .health import critical_wound, needs_recovery


@dataclass(frozen=True)
class Objective:
    name: str
    priority: int
    order: int
    owner: tuple
    payload: dict


class TacticalRuntime:
    def __init__(self, events, healthy_life, *, clock=time.monotonic, max_age=.25):
        if type(healthy_life) not in (int,float) or not math.isfinite(healthy_life) or healthy_life<=0:
            raise BotFault('Tactical runtime needs a finite positive observed health baseline')
        self.events=events;self.healthy_life=healthy_life;self.clock=clock;self.max_age=max_age
        self.state=None;self.owner=None;self.received=None;self.revision=0;self.interrupt_epoch=0
        self.history=deque(maxlen=120);self.pending=[];self.completed=[];self.cancelled=[];self.order=0
        self.enemy_snapshot=None;self.enemy_received=None

    def publish(self, state):
        native=state.get('native',{});frame=state.get('rendered',{})
        owner=(native.get('mission'),native.get('location'),state.get('activation'),
               frame.get('player_owner'),frame.get('presentation_epoch'))
        if (any(type(v) is not int or v<=0 for v in owner)
                or type(native.get('not_alert')) is not bool
                or type(native.get('player_life')) not in (int,float)
                or not math.isfinite(native['player_life']) or native['player_life']<=0):
            raise BotFault('Blackboard cannot admit unknown native world, owner, alert or health')
        critical_wound(native)
        now_ms=state.get('now_ms');sample=frame.get('sample_ms')
        if type(now_ms) is not int or type(sample) is not int or not 0<=now_ms-sample<=150:
            raise BotFault('Blackboard requires the current joined native/rig publication')
        changed=self.owner is not None and owner!=self.owner
        if changed:
            self.cancelled += [{'name':o.name,'reason':'world_or_presentation_owner_changed'} for o in self.pending]
            self.pending.clear();self.interrupt_epoch+=1
        elif self.state is not None:
            prior=self.state['native']
            if ((prior['not_alert'] and not native['not_alert']) or native['player_life']<prior['player_life']
                    or (not critical_wound(prior) and critical_wound(native))):
                self.interrupt_epoch+=1
        self.owner=owner;self.state=copy.deepcopy(state);self.received=self.clock();self.revision+=1
        self.history.append({'revision':self.revision,'state':self.state,'received':self.received})
        self.events.emit('tactical_blackboard',revision=self.revision,interrupt_epoch=self.interrupt_epoch,
            owner=owner,native=native,rendered_frame=frame,decoded_pixels_in_blackboard=False,
            queued=[o.name for o in self.pending],owner_changed=changed)

    def publish_enemies(self, snapshot):
        self.fresh()
        if (tuple(snapshot.get(k) for k in ('mission','location'))!=self.owner[:2]
                or snapshot.get('truncated') is not False or snapshot.get('unavailable_loaded')!=0):
            raise BotFault('Hot enemy memory requires a complete same-world native roster')
        self.enemy_snapshot=copy.deepcopy(snapshot);self.enemy_received=self.clock()
        self.events.emit('tactical_enemy_memory',snapshot=snapshot,revision=self.revision,
                         time_scope='bounded paginated native reads; not one atomic enemy frame')

    def enqueue(self, name, priority=20, **payload):
        self.fresh()
        if name not in ('secure_camp','body_case','emplacement_case') or type(priority) is not int:
            raise BotFault('Only registered bounded field objectives may enter the queue')
        if any(o.name==name for o in self.pending):raise BotFault('Objective is already queued')
        self.order+=1;job=Objective(name,priority,self.order,self.owner,copy.deepcopy(payload))
        self.pending.append(job);return job

    def fresh(self):
        if self.state is None or not 0<=self.clock()-self.received<=self.max_age:
            raise BotFault('Tactical blackboard is stale; reacquire state before dispatch')
        return self.state

    def intent(self, *, known_battle=False):
        native=self.fresh()['native']
        if critical_wound(native) or native['player_life']<self.healthy_life*.35:choice='recover_health'
        elif not known_battle and native['not_alert'] is False:choice='counterattack'
        elif needs_recovery(native,self.healthy_life):choice='recover_health'
        else:
            choice=min(self.pending,key=lambda o:(-o.priority,o.order)).name if self.pending else 'observe'
        self.events.emit('tactical_queue_decision',intent=choice,revision=self.revision,
                         interrupt_epoch=self.interrupt_epoch,owner=self.owner)
        return choice

    def finish(self, job, outcome):
        self.fresh()
        if job not in self.pending or job.owner!=self.owner:
            raise BotFault('Cannot complete a cancelled or foreign-owner objective')
        self.pending.remove(job);self.completed.append({'name':job.name,'outcome':copy.deepcopy(outcome)})
        self.events.emit('tactical_objective_finished',name=job.name,outcome=outcome,
                         native_acceptance_is_scoped=True)

    def plan_token(self):
        self.fresh();return (self.owner,self.interrupt_epoch)

    def accept_plan(self, token):
        self.fresh()
        if token!=(self.owner,self.interrupt_epoch):
            raise BotFault('Queued route was invalidated by detection, damage or an owner change')
