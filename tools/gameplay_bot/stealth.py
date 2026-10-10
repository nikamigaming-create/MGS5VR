"""Equip one explicitly reviewed loadout through the physical wrist picker."""
from .combat import admit, battle_checkpoint
from .core import BotFault, atomic_json


def prepare_loadout(live,policy,*,output,anchor,nonlethal=True):
    state,_=admit(live,anchor)
    required=policy.get('native_before') if isinstance(policy,dict) else None
    category_ok=(policy.get('category')=='secondary' and policy.get('shot_slot')=='SECONDARY') if nonlethal else (
        policy.get('category')=='primary' and policy.get('shot_slot') in ('PRIMARY_HIP','PRIMARY_BACK'))
    if (not isinstance(required,dict) or not category_ok
        or policy.get('card') not in ('up','down','left','right')
        or not policy.get('reviewed_label')
        or not policy.get('evidence_run')):
        raise BotFault('Stealth requires an explicit reviewed nonlethal loadout policy')
    if any(state['native'].get(k)!=v for k,v in required.items()):
        raise BotFault('Reviewed stealth loadout does not match the current native fixture')
    if state['native']['ammo_in_weapon_'+policy['shot_slot']]<=0:
        raise BotFault('Reviewed stealth weapon has no ammunition')
    live.release()
    live.execute({'op':'pose','hand':'left','position':[-.04,-.16,-.55],
                  'orientation':[0,.70710678,0,.70710678]})
    live.execute({'op':'equipment_select','category':policy['category'],'card':policy['card']})
    state,_=admit(live,anchor)
    result={'status':'observed_vr_selection','policy':policy,'state':state,
            'captures':battle_checkpoint(live,'stealth-loadout-selected',state),
            'visual_acceptance':'pending current name, suppressor and hand review',
            'limit':'custom slot identities are fixture-specific, not global weapon identities'}
    atomic_json(output/('stealth-loadout.json' if nonlethal else 'detected-heavy-loadout.json'),result)
    live.events.emit('stealth_loadout_selected',policy=policy,captures=result['captures'])
    return result
