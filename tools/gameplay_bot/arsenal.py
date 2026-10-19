"""Bounded physical weapon-family checks using native identity and ammunition."""
import math
import time
from pathlib import Path

from .core import BotFault, atomic_json, matches
from .live import channel, held_input_evidence, rotate
from .session import _controller_pose
from .combat import (admit, controller_pair, comfortable_grip, paired_aim_position,
                     mul, inverse, support_poses, effective_pitch)

ROOT = Path(__file__).resolve().parents[2]
SLOTS = ("PRIMARY_HIP", "PRIMARY_BACK", "SECONDARY")

def finite(v):
    return type(v) in (int, float) and math.isfinite(v)


def inventory(native):
    result = {}
    for slot in SLOTS:
        identity, ammo = native.get("weapon_" + slot), native.get("ammo_in_weapon_" + slot)
        if type(identity) is not int or identity < 0 or not finite(ammo) or ammo < 0:
            raise BotFault("Source-verified native identity/ammunition missing: " + slot)
        result[slot] = {"custom_or_equipped_id": identity, "ammo": ammo,
                       "sub_ammo": native.get("ammo_sub_in_weapon_" + slot)}
    return result


def discharge(before, after, expected_slot=None, ammo_field='ammo'):
    if any(before[s]["custom_or_equipped_id"] != after[s]["custom_or_equipped_id"] for s in SLOTS):
        return {"status": "unknown", "reason": "native_loadout_changed_during_burst"}
    delta = {s: before[s][ammo_field] - after[s][ammo_field] for s in SLOTS}
    changed = [s for s, n in delta.items() if n > 0]
    if any(n < 0 for n in delta.values()) or len(changed) > 1:
        return {"status": "unknown", "reason": "ambiguous_ammo_change", "ammo_delta": delta}
    if not changed:
        return {"status": "fail", "reason": "no_native_ammunition_discharge", "ammo_delta": delta}
    if expected_slot is not None and changed != [expected_slot]:
        return {"status": "fail", "reason": "different_slot_discharged", "ammo_delta": delta,
                "actual_slot": changed[0], "expected_slot": expected_slot}
    return {"status": "pass", "scope": "one_native_slot_ammunition_discharge",
            "actual_slot": changed[0], "rounds": delta[changed[0]], "ammo_delta": delta,
            "global_definition_proven": False, "hit_or_damage_proven": False, 'ammo_field': ammo_field}


def measured_powers(observer):
    descriptor = observer.get("optical_descriptor")
    if observer.get("optical_descriptor_read") is not True or not isinstance(descriptor, list) or len(descriptor) != 4:
        return None
    if any(type(n) is not int or not 0 <= n <= 255 for n in descriptor):
        raise BotFault("Invalid actual native optical descriptor")
    if descriptor == [1, 0, 0, 2]:
        return [1]
    if descriptor[3] != 1:
        return []
    result = []
    for n in descriptor[:3]:
        if n == 0:
            break
        if not 1 <= n <= 16:
            raise BotFault("Native optic power exceeds source-supported range")
        result.append(n)
    return result


def guard(live, plan, anchor=None, health=None):
    state, current = admit(live, anchor)
    n = state["native"]
    if not matches(n, plan["world"]):
        raise BotFault("Exact native field fixture changed")
    if n.get("status_CARRY") is not False or n.get("status_CQC_HOLD") is not False or n.get("status_BINOCLE") is not False:
        raise BotFault("Field weapon owner has carry/CQC/binocular interference")
    if health is not None and n["player_life"] < health:
        raise BotFault("Health decreased; stop the arsenal sweep for battle response")
    inventory(n)
    definition = plan.get("configured_definition")
    if definition and n.get("weapon_" + definition["slot"]) != definition["runtime_id"]:
        raise BotFault("The controlled fixture's exact global weapon ID no longer occupies its assigned slot")
    if definition and 'customized_index' in definition and n.get('customized_weapon_'+str(definition['customized_index'])) != definition['base_id']:
        raise BotFault('Native customized weapon base changed during the check')
    return state, current


def action_guard(state, plan):
    result = {"state_before": {"scene": "gameplay", "menu": False, "idroid": False,
            "camera_active": True, "controls.context": "gameplay", "activation": state["activation"]},
        "native_before": {**plan["world"], "player_vehicle_id": 65535,
            "status_CARRY": False, "status_CQC_HOLD": False, "status_BINOCLE": False}}
    definition = plan.get("configured_definition")
    if definition:
        result["native_before"]["weapon_" + definition["slot"]] = definition["runtime_id"]
    return result


def static_ready(live, plan, label, state, support=False):
    step = {"op": "action", "name": "gameplay.ready_weapon", "seconds": .60,
            "static_grip_capture": True, "capture_while_held": label,
            "while_held": {"rendered.native_firearm": True,
                "rendered.weapon_support_attached": support}, **action_guard(state, plan)}
    if support:
        step["include_support"] = True
        step["while_held"]["rendered.weapon_support_tracked"] = True
    result = live.execute(step)
    if not isinstance(result, dict) or not isinstance(result.get("state"), dict):
        raise BotFault("Stationary grip observation missing")
    return result


def zoom_cycle(live, plan, ready_state, anchor, health, label, deadline):
    """Distinct physical zoom edges under a sampled, independently expiring grip."""
    powers = measured_powers(ready_state["rendered"].get("weapon_native_observer", {}))
    result = {"status": "unknown", "measured_native_powers": powers, "samples": [],
        "dual_sights": "unknown_no_complete_attachment_inventory",
        "visual_acceptance": "pending_both_eye_review"}
    if powers is None:
        result["reason"] = "descriptor_unavailable"; return result
    if not powers:
        result["reason"] = "no_supported_scope_descriptor_iron_or_alternate_sight_review_required"; return result
    if ready_state["rendered"].get("scope_tracked") is not True:
        result.update(status="fail", reason="scope_descriptor_present_but_scope_untracked"); return result
    actions = {a["name"]: a for a in live.bindings["actions"]}
    rb = actions["gameplay.ready_weapon"]["bindings"][0]
    zb = actions["gameplay.zoom"]["bindings"][0]
    if rb["gesture"] != "level" or not set(rb["inputs"]).issubset({"left_grip", "right_grip"}) or zb["gesture"] != "press":
        raise BotFault("Source supported physical ready/zoom bindings changed")
    if zb["inputs"] != ["right_stick_up"]:
        raise BotFault("Arsenal zoom cycle needs the reviewed right-stick-up edge")
    # No guessed hold duration/second scope switch. Each power uses a fresh
    # ready lease and one press; source eye video retains the actual glass.
    for index in range(len(powers) + 1):
        if time.monotonic() >= deadline:
            raise BotFault("Arsenal wall deadline")
        state, _ = guard(live, plan, anchor, health)
        try:
            live.input([channel(t) for t in rb["inputs"]], .03, lease_seconds=3.)
            end = min(deadline, time.monotonic() + 1.4)
            since = None
            while True:
                raised = live.observe()
                audit = held_input_evidence(raised, rb["inputs"], after_sample_ms=state["now_ms"])
                if audit and raised.get("rendered", {}).get("scope_tracked") is True:
                    since = audit["sample_ms"] if since is None else since
                    if audit["sample_ms"] - since >= 200:
                        break
                if time.monotonic() >= end:
                    raise BotFault("Current scope/ready input did not publish before zoom")
                time.sleep(.025)
            before = raised["rendered"]["scope_magnification"]
            live.input([channel(t) for t in zb["inputs"]], .20, lease_seconds=.60)
            end = min(deadline, time.monotonic() + .40)
            sampled = None
            while time.monotonic() < end:
                held = live.observe()
                audit = held_input_evidence(held, rb["inputs"] + zb["inputs"], after_sample_ms=raised["now_ms"])
                if audit:
                    sampled = held; break
                time.sleep(.015)
            keep = {(v["hand"], v["component"], v.get("sub_component")) for v in [channel(t) for t in rb["inputs"]]}
            live.release(keep=keep)
            if sampled is None:
                raise BotFault("Physical scope zoom edge was not sampled")
            after = live.observe()
            row = {"index": index, "before_power": before, "after_power": after.get("rendered", {}).get("scope_magnification"),
                "edge_state": sampled, "state": after, "source": "native_source_eye_video_checkpoint"}
            checkpoint = label+'-power-'+str(row['after_power'])+'-'+str(index)
            grip = _controller_pose(live, 'right', 'grip', 'local')
            aim = _controller_pose(live, 'right', 'aim', 'local')
            pair = controller_pair(grip, aim)
            live.release()
            row['inspection'] = sight_inspection(live, plan, {'state': after},
                grip['position'], grip['orientation'], pair, anchor, health, checkpoint)
            captured = row['inspection'].get('capture', {})
            row['eyes'] = captured.get('captures', [])
            row['aligned_power'] = captured.get('state', {}).get('rendered', {}).get('scope_magnification')
            result["samples"].append(row)
            live.events.emit("arsenal_scope_power", label=label, **row)
        finally:
            live.release()
    # Releasing ready resets selection in some modes. Report the actual
    # sequence rather than assume continuity across those releases.
    observed = {r["before_power"] for r in result["samples"]} | {r["after_power"] for r in result["samples"]}
    result.update(status="pass" if set(powers).issubset(observed) else "fail",
        observed_powers=sorted(p for p in observed if finite(p)),
        reason=None if set(powers).issubset(observed) else "not_all_measured_powers_observed",
        acceptance_scope="telemetry_zoom_powers_only")
    return result


def human_hand(live, hand, position, orientation, pair):
    aim_orientation=mul(orientation,inverse(pair['orientation']))
    live.execute({'op':'motion','seconds':.75,'hands':[{'hand':hand,'base_space':'view',
        'position':position,'orientation':orientation,
        'aim_position':paired_aim_position(position,aim_orientation,pair),
        'aim_orientation':aim_orientation}]})


def human_support(live, frame, pair):
    grip,aim=support_poses(frame,pair)
    live.execute({'op':'motion','seconds':.75,'hands':[{'hand':'left','base_space':'local',
        'position':grip['position'],'orientation':grip['orientation'],
        'aim_position':aim['position'],'aim_orientation':aim['orientation']}]})


def sight_inspection(live, plan, one, gp, go, pair, anchor, health, label):
    """Move the actual controller to the published sight; never move the optic."""
    state = one['state']; frame = state['rendered']
    observer = frame.get('weapon_native_observer', {})
    scoped = frame.get('scope_tracked') is True
    sight = frame.get('scope_ocular') if scoped else observer.get('sight_rear_local')
    if not scoped and observer.get('sight_rear_valid') is not True:
        return {'status': 'unproven', 'reason': 'no_published_sight_socket'}
    if not isinstance(sight, dict) or not isinstance(frame.get('head_pose'), dict):
        return {'status': 'unproven', 'reason': 'missing_head_or_sight_pose'}
    head = frame['head_pose']
    # Authored simulator right-eye inspection pose, not physical-user calibration.
    collimator = (scoped and frame.get('scope_dot_reticle') is True
                  and frame.get('scope_magnification') == 1
                  and not frame.get('scope_opaque_at_one_power'))
    target_view = [.032, 0., -.40 if collimator or not scoped else -.20]
    target = [a+b for a,b in zip(head['position'], rotate(head['orientation'], target_view))]
    delta = rotate(inverse(head['orientation']), [a-b for a,b in zip(target, sight['position'])])
    hand = [a+b for a,b in zip(gp, delta)]
    if math.sqrt(sum(v*v for v in delta)) > .50 or not -.8 < hand[2] < -.04:
        return {'status': 'unproven', 'reason': 'inspection_outside_bounded_hand_reach', 'target_grip': hand}
    try:
        human_hand(live, 'right', hand, go, pair)
        current, _ = guard(live, plan, anchor, health)
        captured = static_ready(live, plan, label+'-sight', current)
        return {'status': 'observed_pose', 'kind': 'magnified' if scoped else 'native_sights',
                'target_view': target_view, 'target_grip': hand, 'capture': captured, 'visual_acceptance': 'pending'}
    except BotFault as error:
        return {'status': 'fail', 'error': str(error)}
    finally:
        live.release()
        human_hand(live, 'right', gp, go, pair)


def run_sweep(live, plan, *, seconds=240., max_cards=3):
    if not 10 <= seconds <= 300 or not 1 <= max_cards <= 3:
        raise BotFault("Arsenal sweep must remain within 10..300 seconds and 1..3 loaded cards")
    started = time.monotonic(); deadline = started + seconds
    before, anchor = guard(live, plan)
    health = before["native"]["player_life"]
    result = {"schema": 1, "status": "running", "scope": plan["scope"], "cards": [],
        "catalog_queue": plan["catalog_queue"], "catalog_definitions_tested": 0,
        "native_inventory_before": inventory(before["native"]), "global_identity_binding": "unknown",
        "visual_acceptance": "pending_both_eye_review", "physical_headset_acceptance": False}
    pair = controller_pair(_controller_pose(live, "right", "grip", "local"), _controller_pose(live, "right", "aim", "local"))
    support_pair = controller_pair(_controller_pose(live, "left", "grip", "local"), _controller_pose(live, "left", "aim", "local"))
    for card in plan["cards"][:max_cards]:
        if time.monotonic() >= deadline or (live.events.output / "STOP").exists():
            result["status"] = "stopped"; break
        row = {**card, "status": "running", "one_hand": "not_run", "two_hand": "not_run",
            "firing": "not_run", "scopes": "not_run", "global_definition": None, "weapon_model": None}
        result["cards"].append(row)
        try:
            live.release(); state, _ = guard(live, plan, anchor, health)
            # Maintain genuine grip/aim calibration when posing a controller.
            gp = comfortable_grip(state["native"])
            # Choose level AIM, preserve the runtime grip/aim cant, and remove
            # only the effective personal firearm pitch. Identity GRIP had
            # pitched the sampled barrel downward by the controller cant.
            half=-math.radians(effective_pitch(live.bindings))/2
            go=mul([math.sin(half),0.,0.,math.cos(half)],pair['orientation'])
            ao = mul(go, inverse(pair["orientation"]))
            human_hand(live,'right',gp,go,pair)
            human_hand(live,'left',[-.04,-.16,-.55],[0.,.70710678,0.,.70710678],support_pair)
            live.equipment_select(card)
            human_hand(live,'left',[-.35,-.25,-.4],[0.,0.,0.,1.],support_pair)
            state, _ = guard(live, plan, anchor, health)
            row["native_inventory_selected"] = inventory(state["native"])
            row["selection_checkpoint"] = live.capture(card["case_id"] + "-selected")
            one = static_ready(live, plan, card["case_id"] + "-one-hand", state)
            row["one_hand"] = {"status": "pass", "scope": "native_ready_and_support_unattached", **one,
                "visual_acceptance": "pending_both_eye_review"}
            observer = one["state"]["rendered"].get("weapon_native_observer", {})
            row["native_rig_identity"] = observer
            row['sight_inspection'] = sight_inspection(live, plan, one, gp, go, pair, anchor, health, card['case_id'])
            if observer.get("weapon_definition") is not None or observer.get("weapon_model") is not None:
                raise BotFault("Unexpected model/definition resolver needs a reviewed contract")
            if one["state"]["rendered"].get("weapon_support_tracked") is True:
                human_support(live,one['state']['rendered'],support_pair)
                state, _ = guard(live, plan, anchor, health)
                try:
                    two = static_ready(live, plan, card["case_id"] + "-two-hand", state, True)
                    row["two_hand"] = {"status": "pass", "scope": "authored_support_attachment", **two,
                        "visual_acceptance": "pending_both_eye_review"}
                except BotFault as error:
                    row["two_hand"] = {"status": "fail", "reason": str(error)}
                    if live.transport_error:
                        raise
            else:
                row["two_hand"] = {"status": "unknown", "reason": "native_authored_support_unavailable"}
            state, _ = guard(live, plan, anchor, health)
            inspect_grip = (row['sight_inspection'].get('target_grip')
                            if row['sight_inspection'].get('status') == 'observed_pose' else None)
            if inspect_grip:
                human_hand(live, 'right', inspect_grip, go, pair)
            row["scopes"] = zoom_cycle(live, plan, one["state"], anchor, health, card["case_id"], deadline)
            if inspect_grip:
                human_hand(live, 'right', gp, go, pair)
            state, _ = guard(live, plan, anchor, health)
            native_before = inventory(state["native"])
            row["ammo_before"] = native_before
            expected = card.get("expected_slot")
            ammo_field = plan.get('ammo_field', 'ammo')
            if expected and native_before[expected][ammo_field] <= 0:
                row["firing"] = {"status": "unknown", "reason": "selected_slot_empty_no_reload_attempted"}
            else:
                live.events.emit("arsenal_fire_checkpoint_before", card=card, state=state)
                live.execute({"op": "action", "name": "gameplay.fire_or_cqc", "include_ready": True,
                    "seconds": plan["policy"]["burst_seconds"], **action_guard(state, plan)})
                after, _ = guard(live, plan, anchor, health)
                native_after = inventory(after["native"])
                row["ammo_after"] = native_after
                row["firing"] = discharge(native_before, native_after, expected, ammo_field)
                row["firing"]["before"] = native_before; row["firing"]["after"] = native_after
                row["firing"]["two_hand_fire_proven"] = False
                definition = plan.get("configured_definition")
                if definition and row["firing"]["status"] == "pass" and row["firing"]["actual_slot"] == definition["slot"]:
                    row["configured_global_definition"] = definition["definition"]
                    row["firing"]["global_definition_proven"] = False
                    row["firing"]["configured_slot_definition_proven"] = True
                    row["firing"]["render_model_definition_proven"] = False
                    row["firing"]["definition_binding_scope"] = "controlled_configuration_receipt_plus_independent_native_slot_identity_and_discharge"
                    for pending in result["catalog_queue"]:
                        if pending["definition"] == definition["definition"]:
                            pending.update(firing="observed_discharge", access="controlled_fixture", status="unknown",
                                one_hand=row["one_hand"].get("status"), two_hand=row["two_hand"].get("status"),
                                visual_acceptance="pending_both_eye_review", all_fitted_sights="not_proven")
                    result["catalog_definitions_tested"] = 1
                live.events.emit("arsenal_fire_checkpoint_after", card=card, state=after, discharge=row["firing"])
                row["after_fire_eyes"] = live.capture(card["case_id"] + "-after-fire")
            statuses = [row[k].get("status") for k in ("one_hand", "two_hand", "firing", "scopes") if isinstance(row[k], dict)]
            row["status"] = "fail" if "fail" in statuses else "unknown" if "unknown" in statuses else "observed_subchecks"
        except Exception as error:
            row.update(status="fail", error=str(error))
            result.update(status="failed", error=str(error)); break
        finally:
            live.release()
            live.events.emit("arsenal_card_finished", result=row)
            atomic_json(live.events.output / "arsenal.json", result)
    if result["status"] == "running":
        result["status"] = "field_batch_completed" if len(result["cards"]) == min(max_cards, len(plan["cards"])) else "stopped"
    result["elapsed_seconds"] = time.monotonic() - started
    result["counts"] = {"cards": len(result["cards"]), "native_discharges": sum(isinstance(r["firing"], dict) and r["firing"].get("status") == "pass" for r in result["cards"]),
                        "catalog_definitions_bound_and_discharge_tested": result["catalog_definitions_tested"],
                        "catalog_items_pending_full_acceptance": len(plan["catalog_queue"])}
    return result

