"""One live connection with a bounded, observation-bound decision queue.

The fast loop owns neutral input and reports meaningful changes. A model worker
reads its events, reviews new blockers, and submits an explicit semantic case.
Unrecognized UI never authorizes a generic confirm/retry loop.
"""
import json
import time
import uuid

from .core import BotFault, atomic_json, matches
from .live import digest
from .session import run_suite


STATE_KEYS = ("scene", "title", "menu", "idroid", "loading", "demo", "activation")


def signature(state):
    return {name: state.get(name) for name in STATE_KEYS}


def meaningful_change(previous, current):
    """Ignore a shallow `unknown` scene label, but keep every native flag.

    The fast observation omits the Lua/native detail used by ``publish`` and
    may classify an otherwise unchanged player scene as unknown when the
    camera publication is temporarily unavailable. That loss of evidence
    depth is not itself a scene transition. Decision admission still performs
    a fresh native observation and exact signature check.
    """
    latest = signature(current)
    for key in STATE_KEYS:
        if key == "scene" and latest.get(key) in (None, "unknown"):
            continue
        if previous.get(key) != latest.get(key):
            return True
    return False


def decision_cases(decision):
    if decision.get("kind") == "case":
        cases = [decision.get("case")]
    elif decision.get("kind") == "plan":
        cases = decision.get("cases")
    else:
        return []
    if not isinstance(cases, list) or not 1 <= len(cases) <= 32:
        raise BotFault("A reviewed plan requires between one and 32 cases")
    identifiers = set()
    for case in cases:
        if not isinstance(case, dict) or not all(case.get(k) for k in ("id", "before", "steps", "after")):
            raise BotFault("Decision requires complete semantic cases")
        if case["id"] in identifiers:
            raise BotFault("Repeated case ID in one plan; no implicit retry loops")
        identifiers.add(case["id"])
    return cases


def validate_decision(decision, request, state, completed, now_ns=None):
    identifier = decision.get("id")
    if not isinstance(identifier, str) or not identifier or identifier in completed:
        raise BotFault("Decision ID is missing or already consumed")
    if decision.get("observation_id") != request["observation_id"]:
        raise BotFault("Decision belongs to an older observation")
    if decision.get("kind") not in ("case", "plan", "inspect", "stop"):
        raise BotFault("Unsupported supervised decision")
    if decision["kind"] in ("case", "plan"):
        cases = decision_cases(decision)
        age = ((time.time_ns() if now_ns is None else now_ns)-request.get("observed_unix_ns", 0))/1e9
        if not 0 <= age <= 30.:
            raise BotFault("Observation is older than 30 seconds; inspect fresh eyes before input")
        if signature(state) != request["state_signature"] or not matches(state, cases[0]["before"]):
            raise BotFault("Observed scene changed; review a new observation before input")
        # A model's decision cites the actual neutral image it inspected.
        if decision.get("reviewed_capture_sha256") not in [c["sha256"] for c in request["captures"]]:
            raise BotFault("Decision must cite an image from the current observation")
    return identifier


def execute_plan(behavior, decision, records, *, checkpoint, clock=time.monotonic):
    """Run known steps continuously; stop before all dependents on the first failure."""
    cases = decision_cases(decision)
    started = clock()
    for index, case in enumerate(cases):
        behavior.events.emit("plan_progress", decision_id=decision["id"], case_id=case["id"],
                             case_index=index, cases_total=len(cases))
        result = behavior.case(case)
        result.update(decision_id=decision["id"], plan_index=index)
        records.append(result)
        checkpoint(records)
        if result.get("release_error") or result.get("capture_error") or result.get("failure_phase") == "dispatch":
            raise BotFault("Ambiguous dispatch/transport failure; supervised input stopped")
        if result["status"] != "observed_pass":
            remaining = [item["id"] for item in cases[index+1:]]
            behavior.events.emit("plan_interrupted", decision_id=decision["id"], case_id=case["id"],
                                 remaining=remaining, reason=result.get("error"), elapsed=clock()-started)
            return "Case needs review: "+result["id"]+"; dependent cases stopped"
    behavior.events.emit("plan_finished", decision_id=decision["id"], cases=len(cases), elapsed=clock()-started)
    return "Reviewed plan completed: "+decision["id"]


def run_supervised(behavior, seconds=900., clock=time.monotonic, sleep=time.sleep, initial_suite=None):
    live, events = behavior.adapter, behavior.events
    live.supervised = True
    deadline = clock()+seconds
    completed, records = set(), []
    decision_path = events.output / "decision.json"
    request = None
    previous = None
    next_observe = 0.
    last_decision_digest = None
    next_health = 0.
    waiting_since = clock()
    notified_wait = False

    def publish(reason):
        live.release()
        state = live.observe(native=True)
        token = uuid.uuid4().hex
        paths = live.capture("supervisor-"+token[:10])
        payload = {"observation_id": token, "observed_unix_ns": time.time_ns(),
                   "state": state, "state_signature": signature(state),
                   "reason": reason, "captures": [{"path": p, "sha256": digest(p)} for p in paths],
                   "decision_file": str(decision_path), "input_owner": "single_supervised_runner"}
        atomic_json(events.output / "supervisor.json", payload)
        events.emit("attention_required", label="supervisor", reason=reason, state=state,
                    observation_id=token, captures=payload["captures"])
        return payload

    if initial_suite is not None:
        # The caller explicitly selected this suite on the CLI. Its complete
        # native guards are validated before input; no model approval gap is
        # inserted between launch and the already selected work.
        events.emit("preselected_suite_started", suite=initial_suite)
        def initial_checkpoint(rows):
            records[:] = [{**row, "decision_id": "preselected-suite"} for row in rows]
            atomic_json(events.output / "supervised-cases.json", {"cases": records})
        initial_result = run_suite(behavior, {**initial_suite, "continue_after_outcome_failure": False}, initial_checkpoint)
        if any(row.get("release_error") or row.get("capture_error") or row.get("failure_phase") == "dispatch" for row in records):
            raise BotFault("Ambiguous preselected suite failure; supervised input stopped")
        request = publish("Preselected suite "+initial_result["status"]+"; "+str(len(initial_result["not_run"]))+" dependent cases not run")
    else:
        request = publish("Ready for a reviewed semantic case; no input is being held")
    waiting_since = clock()
    previous = request["state_signature"]
    while clock() < deadline:
        if decision_path.is_file():
            decision_digest = digest(decision_path)
            if decision_digest != last_decision_digest:
                last_decision_digest = decision_digest
                try:
                    decision = json.loads(decision_path.read_text(encoding="utf-8-sig"))
                    if not isinstance(decision, dict):
                        raise ValueError("Decision must be a JSON object")
                except (ValueError, UnicodeError) as error:
                    events.emit("decision_rejected", reason=str(error))
                    continue
                state = live.observe(native=True)
                try:
                    identifier = validate_decision(decision, request, state, completed)
                except BotFault as error:
                    # Consume invalid decisions too: no endless retry of a stale command.
                    if isinstance(decision.get("id"), str):completed.add(decision["id"])
                    events.emit("decision_rejected", reason=str(error), decision_id=decision.get("id"))
                    request = publish(str(error));previous=request["state_signature"]
                    waiting_since=clock();notified_wait=False
                    continue
                completed.add(identifier)
                decision_record = events.output / "decisions" / (uuid.uuid4().hex+".json")
                atomic_json(decision_record, {"decision": decision, "observation": request,
                                             "accepted_unix_ns": time.time_ns()})
                events.emit("decision_accepted", decision_id=identifier, observation_id=request["observation_id"],
                            rationale=decision.get("rationale"), decision_record=str(decision_record))
                if decision["kind"] == "stop":
                    break
                if decision["kind"] in ("case", "plan"):
                    atomic_json(events.output / "supervisor-health.json", {
                        "updated_unix_ns": time.time_ns(), "state": "executing_reviewed_plan",
                        "decision_id": identifier, "cases_completed": len(records)})
                    reason=execute_plan(behavior, decision, records, clock=clock,
                        checkpoint=lambda rows: atomic_json(events.output / "supervised-cases.json", {"cases": rows}))
                else:reason="Fresh inspection requested"
                request=publish(reason);previous=request["state_signature"]
                waiting_since=clock();notified_wait=False
        if clock() >= next_observe:
            state=live.observe()
            if meaningful_change(previous,state):
                events.emit("milestone_reached", label="supervisor-scene-change", before=previous, state=state)
                request=publish("Native scene changed while awaiting the next decision")
                previous=request["state_signature"]
                waiting_since=clock();notified_wait=False
            next_observe=clock()+.25
        if clock() >= next_health:
            atomic_json(events.output / "supervisor-health.json", {
                "updated_unix_ns": time.time_ns(), "state": "awaiting_worker_decision",
                "observation_id": request["observation_id"], "waiting_seconds": clock()-waiting_since,
                "cases_completed": len(records), "input_held": bool(live.held)})
            next_health=clock()+1.
        if not notified_wait and clock()-waiting_since >= 10.:
            events.emit("supervisor_waiting", reason="Worker has not supplied the next decision within ten seconds",
                        observation_id=request["observation_id"], elapsed=clock()-waiting_since,
                        captures=request["captures"])
            notified_wait=True
        sleep(.05)
    live.release()
    atomic_json(events.output / "supervisor-health.json", {
        "updated_unix_ns": time.time_ns(), "state": "ended", "cases_completed": len(records),
        "input_held": False})
    return {"status": "supervised_session_ended", "cases": records, "visual_acceptance": "pending",
            "release_ready": False, "decisions_consumed": sorted(completed)}
