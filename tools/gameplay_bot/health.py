"""Retreat over observed ground, then verify real regeneration or first aid."""
import math
import time

from .core import BotFault, atomic_json
from .locomotion import _position
from .route import navigate, set_posture
from .session import preserved_tracking_poses


class HealthRecoveryNeeded(BotFault):
    pass


def critical_wound(native):
    values=[native.get('status_'+name) for name in ('INJURY_LOWER','INJURY_UPPER')]
    if any(type(v) is not bool for v in values):
        raise BotFault('Critical-wound observation is unavailable')
    return any(values)


def needs_recovery(native, healthy_life):
    life=native.get('player_life')
    if type(life) not in (int,float) or not math.isfinite(life) or life<=0:
        raise BotFault('Player health is unavailable or dead')
    return critical_wound(native) or life<healthy_life


def retreat_goal(point, visited, healthy_life, nav=None):
    # Only actually walked positions observed before damage are eligible.
    for sample in reversed(visited):
        candidate=[sample['player_'+axis] for axis in 'xyz']
        if (sample['player_life']>=healthy_life and sample.get('not_alert') is True
            and 25<=math.dist(point,candidate)<=60):
            if nav is None:return candidate
            try:
                start=nav.nearest(point);end=nav.nearest(candidate)
                nav.path(start,end)
            except BotFault:continue
            return list(nav.positions[end])
    raise BotFault('No retained healthy traversal supports a bounded retreat')


def recover_health(live,nav,*,output,anchor,healthy_life,visited,enemy_reader,
                   deadline,obstacle_file=None):
    from .combat import admit, battle_checkpoint
    output.mkdir(parents=True,exist_ok=True)
    result={'status':'running','goal_life':healthy_life,'samples':[],
            'scope':'observed recovery to prior life; unknown maximum is not 100 percent'}
    try:
        with preserved_tracking_poses(live):
            live.release();state,_=admit(live,anchor);result['before']=state
            goal=retreat_goal(_position(state),visited,healthy_life,nav)
            route=navigate(live,nav,goal,output=output/'retreat',enemy_reader=enemy_reader,
                mode='auto',max_seconds=min(45.,max(.001,deadline-time.monotonic())),
                obstacle_file=obstacle_file,expected_location=anchor[1],arrival_radius=1.5,recenter=False)
            result['retreat']=str(output/'retreat'/'result.json')
            if route['status']!='observed_native_route':raise BotFault(route.get('error','Recovery retreat did not arrive'))
            state,new_anchor=admit(live)
            if new_anchor[:2]!=anchor[:2] or new_anchor[3:]!=anchor[3:]:
                raise BotFault('Recovery retreat changed its native world or owner')
            anchor=new_anchor
            set_posture(live,'crawl')
            state,_=admit(live,anchor)
            result['captures']=battle_checkpoint(live,'recovery-before',state)
            if critical_wound(state['native']):
                # Contextual aid is the same physical VR interaction action.
                # A wound count alone never authorizes Y near a body or vehicle.
                live.execute({'op':'action','name':'gameplay.interact','seconds':.12})
                result['first_aid_requested']=True
            until=min(deadline,time.monotonic()+20.)
            previous=state['native']['player_life']
            while time.monotonic()<until:
                state,_=admit(live,anchor);native=state['native']
                result['samples'].append({k:native.get(k) for k in (
                    'player_life','not_alert','player_injury_count','status_INJURY_LOWER',
                    'status_INJURY_UPPER','status_CURE')})
                if native['player_life']<previous:
                    raise BotFault('Recovery position is still taking damage; do not idle or claim cover')
                previous=native['player_life']
                if (not needs_recovery(native,healthy_life) and native.get('status_CURE') is False):
                    result['status']='observed_native_health_recovered';break
                atomic_json(output/'progress.json',result)
                time.sleep(.1)
            else:raise BotFault('Native recovery deadline; injury/life did not establish recovery')
            result['after']=state;result['captures']+=battle_checkpoint(live,'recovery-after',state)
    except Exception as error:
        result['status']='failed';result['error']=str(error)
    finally:
        live.release();atomic_json(output/'result.json',result)
    return result
