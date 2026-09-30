"""Persistent campaign scheduling and evidence accounting.

A native predicate is one evidence layer. It never closes a community report
or certifies the pixels, headset experience, or a teaching film.
"""
from __future__ import annotations

import hashlib
import json
import copy
from pathlib import Path

from .core import BotFault, matches
from .session import reset_post_continue_hand_poses, run_suite
from .idroid import inspect_idroid


IDENTITY_KEYS = ("exe_sha256", "dll_sha256", "controls_sha256", "config_sha256")
PROCEDURES = {"idroid-palm-motion": ("tools/gameplay_bot/idroid.py", inspect_idroid)}


def fingerprint(identity):
    values = {key: identity.get(key) for key in IDENTITY_KEYS}
    if any(not isinstance(value, str) or len(value) != 64 for value in values.values()):
        raise BotFault("Campaign requires executable, DLL and both configuration hashes")
    return values


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def file_hash(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def checked_path(root, relative):
    root = Path(root).resolve()
    path = (root / relative).resolve()
    if not path.is_relative_to(root):
        raise BotFault("Campaign paths must remain inside the workspace")
    return path


def validate_campaign(campaign, root):
    entries = campaign.get("suites")
    if not isinstance(entries, list) or not entries:
        raise BotFault("Campaign needs a nonempty suite queue")
    ids = set()
    for entry in entries:
        identifier = entry.get("id")
        if not isinstance(identifier, str) or not identifier or identifier in ids:
            raise BotFault("Campaign suite IDs must be unique and nonempty")
        ids.add(identifier)
        if not entry.get("coverage_refs"):
            raise BotFault(f"{identifier}: missing coverage references")
        if entry.get("suite"):
            suite = read_json(checked_path(root, entry["suite"]))
            cases = suite.get("cases", [])
            if not cases or any(not case.get("before") or not case.get("after")
                                or not case.get("steps") for case in cases):
                raise BotFault(f"{identifier}: incomplete executable cases")
        elif entry.get("procedure"):
            if entry["procedure"] not in PROCEDURES or not entry.get("before"):
                raise BotFault(f"{identifier}: unknown procedure or missing entry predicate")
        elif not entry.get("blocked_reason"):
            raise BotFault(f"{identifier}: missing suite or explicit implementation blocker")
    return entries


def unstarted_campaign(campaign, root, identity, error):
    entries = validate_campaign(campaign, root)
    return {"schema": 1, "identity": fingerprint(identity), "status": "failed", "error": str(error),
            "release_ready": False, "visual_acceptance": "pending", "headset_acceptance": "not_run",
            "suites": [{"id": entry["id"], "coverage_refs": entry["coverage_refs"],
                        "status": "blocked" if entry.get("blocked_reason") else "not_run",
                        "reason": entry.get("blocked_reason", "Session never reached suite dispatch: " + str(error))}
                       for entry in entries]}


def run_campaign(behavior, campaign, root, checkpoint, identity, *, enter_game=None,
                 resume=None):
    """Run every eligible suite on one connection, retaining every unmet gate.

    Resume skips only a fully observed suite of the same definition and build.
    Partial suites restart from their declared entry state; no input is replayed
    from an interrupted step. An uncertain transport stops all further input.
    """
    entries = validate_campaign(campaign, root)
    current = fingerprint(identity)
    previous = {}
    if resume:
        if resume.get("identity") != current:
            raise BotFault("Resume belongs to a different DLL, executable or configuration")
        previous = {row["id"]: row for row in resume.get("suites", [])}
    arrival = enter_game() if enter_game else None
    if arrival is not None:
        if arrival.get("status") != "observed_arrival":
            raise BotFault("Campaign arrival was not observed")
        reset_post_continue_hand_poses(behavior, arrival)
    records = []
    stopped = None

    def publish():
        payload = {"schema": 1, "identity": current, "suites": records,
                   "status": "in_progress", "visual_acceptance": "pending",
                   "headset_acceptance": "not_run", "arrival": arrival}
        checkpoint(payload)
        return payload

    for entry in entries:
        record = {"id": entry["id"], "coverage_refs": entry["coverage_refs"],
                  "status": "not_run", "visual_acceptance": "pending"}
        if not entry.get("suite") and not entry.get("procedure"):
            record.update(status="blocked", reason=entry["blocked_reason"])
        elif stopped:
            record.update(status="not_run", reason=stopped)
        else:
            procedure = PROCEDURES.get(entry.get("procedure"))
            source = procedure[0] if procedure else entry["suite"]
            suite_path = checked_path(root, source)
            definition_hash = file_hash(suite_path)
            record.update(suite=source, suite_sha256=definition_hash)
            old = previous.get(entry["id"], {})
            if old.get("status") in ("observed_pass", "previously_observed") and old.get("suite_sha256") == definition_hash:
                record.update(status="previously_observed", previous=old,
                              reason="Same build/configuration and suite; retained native evidence only")
            else:
                suite = entry if procedure else copy.deepcopy(read_json(suite_path))
                for case in suite.get("cases", []):
                    case["source_case_id"] = case["id"]
                    case["id"] = entry["id"] + "__" + case["id"]
                    case["depends_on"] = [entry["id"] + "__" + name for name in case.get("depends_on", [])]
                try:
                    behavior.adapter.release()
                    state = behavior.adapter.observe(native=True)
                    configured = {row["name"]: row["value"] for row in behavior.adapter.bindings.get("settings", [])}
                    required = suite.get("required_settings", {})
                    if any(configured.get(key) != value for key, value in required.items()):
                        record.update(status="blocked", reason="Effective settings do not match suite prerequisites",
                                      required_settings=required)
                    elif not matches(state, entry["before"] if procedure else suite["cases"][0]["before"]):
                        record.update(status="blocked", reason="Current native scene does not satisfy the suite entry state",
                                      required_state=entry["before"] if procedure else suite["cases"][0]["before"], observed_state=state)
                    else:
                        def case_checkpoint(rows):
                            record["cases"] = list(rows)
                            checkpoint({"schema": 1, "identity": current, "status": "in_progress",
                                        "suites": records + [record], "visual_acceptance": "pending"})
                        result = procedure[1](behavior.adapter, behavior) if procedure else run_suite(behavior, suite, case_checkpoint)
                        record.update(result)
                        errors = [row for row in result["cases"] if row["status"] == "failed"]
                        unsafe = any(row.get("failure_phase") == "dispatch" or row.get("release_error")
                                     or row.get("capture_error") for row in errors)
                        if unsafe:
                            stopped = "Prior suite has ambiguous input/transport failure; inspect evidence before resuming"
                except Exception as error:
                    record.update(status="failed", reason=str(error))
                    stopped = "Session/transport failure; remaining inputs were not dispatched"
                    try:
                        behavior.adapter.release()
                    except Exception as release_error:
                        record["release_error"] = str(release_error)
        records.append(record)
        publish()
    result = publish()
    result["status"] = "failed" if any(row["status"] == "failed" for row in records) else "incomplete"
    result["native_suites_observed"] = sum(row["status"] in ("observed_pass", "previously_observed") for row in records)
    result["blocked_suites"] = sum(row["status"] == "blocked" for row in records)
    result["release_ready"] = False
    checkpoint(result)
    return result


def inventory(root, bindings, equipment_inventory=None):
    """Preserve distinct report, situation, action and equipment denominators."""
    root = Path(root)
    issues = read_json(root / "tools/gameplay_bot/catalogs/community-issues.json")
    contexts = read_json(root / "docs/NATIVE_CONTEXT_COVERAGE.json")
    history = read_json(root / "tools/gameplay_bot/catalogs/historical-features.json")
    rows = []
    for row in issues["reports"]:
        rows.append({**row, "kind": "report", "status": "unproven"})
    for row in contexts["records"]:
        rows.append({"id": row["id"], "kind": "situation", "title": row["context"],
                     "family": row["family"], "status": "unproven",
                     "acceptance": [action["outcome"] for action in row["required_actions"]],
                     "source": "docs/NATIVE_CONTEXT_COVERAGE.json"})
    for row in history["features"]:
        rows.append({**row, "kind": "historical_feature", "status": "unproven"})
    for row in bindings.get("actions", []):
        rows.append({"id": "ACTION." + row["name"], "kind": "control_action", "title": row["name"],
                     "status": "unproven", "effective_binding": row,
                     "acceptance": ["Eligible native context, actual action outcome, visible feedback and neutral exit"],
                     "source": "effective-bindings.json"})
    if equipment_inventory:
        matrix = read_json(equipment_inventory)
        for row in matrix.get("equipment", []):
            rows.append({"id": "EQUIPMENT." + row["id"], "kind": "equipment", "title": row["id"],
                         "status": "unproven", "access": row.get("access", "not_established"),
                         "acceptance": row["required_actions"], "grade": row.get("grade"),
                         "source": str(equipment_inventory)})
    ids = [row["id"] for row in rows]
    if len(ids) != len(set(ids)):
        raise BotFault("Duplicate coverage IDs")
    return rows


def summarize_runs(root, rows, run_paths, target_identity):
    """Keep historical observations visible without giving them current credit."""
    target = fingerprint(target_identity)
    root = Path(root).resolve()
    index = {row["id"]: {**row, "evidence": []} for row in rows}
    runs = []
    for run_path in run_paths:
        run = Path(run_path).resolve()
        if not run.is_relative_to(root):
            raise BotFault("Evidence runs must remain inside the workspace")
        identity_path, result_path = run / "identity.json", run / "result.json"
        if not identity_path.is_file() or not result_path.is_file():
            runs.append({"path": str(run), "status": "missing_identity_or_result"})
            continue
        identity, result = read_json(identity_path), read_json(result_path)
        current = fingerprint(identity) == target
        suites = list(result.get("suites", []))
        # Standalone/supervised runs keep their original result intact. A
        # reviewed sidecar links exact case indices to coverage obligations.
        links_path = run / "coverage-links.json"
        if links_path.is_file():
            links = read_json(links_path)
            if links.get("result_sha256") != file_hash(result_path):
                raise BotFault("Coverage links belong to a different result file")
            cases = result.get("cases", [])
            for link in links.get("cases", []):
                offset = link.get("case_index")
                if type(offset) is not int or not 0 <= offset < len(cases) or cases[offset]["id"] != link.get("case_id"):
                    raise BotFault("Coverage link does not identify an exact recorded case")
                case = cases[offset]
                suites.append({"id": case["id"], "status": case["status"], "cases": [case],
                               "coverage_refs": link["coverage_refs"], "reason": link.get("reason")})
        runs.append({"path": str(run), "current": current, "status": result.get("status"),
                     "result_sha256": file_hash(result_path), "identity": fingerprint(identity)})
        for suite in suites:
            for reference in suite.get("coverage_refs", []):
                if reference not in index:
                    raise BotFault("Unknown coverage reference: " + reference)
                index[reference]["evidence"].append({"run": str(run), "suite": suite["id"],
                    "current": current, "native_status": suite["status"],
                    "result_sha256": file_hash(result_path), "reason": suite.get("reason"),
                    "case_ids": [case["id"] for case in suite.get("cases", [])]})
                # One suite can demonstrate a subfeature without resolving the
                # whole linked report. Never promote the report automatically.
                if current and suite["status"] == "failed":
                    index[reference]["status"] = "failure_observed"
                elif current and suite["status"] in ("observed_pass", "previously_observed"):
                    if index[reference]["status"] != "failure_observed":
                        index[reference]["status"] = "partial_native_evidence"
    return {"schema": 1, "target_identity": target, "rows": list(index.values()), "runs": runs,
            "complete": False, "release_ready": False,
            "limits": ["Simulator observations do not certify headset comfort or tracking",
                       "Linked suites may cover only part of a report or situation",
                       "Every equipment ID retains its own access and action obligations",
                       "No report is closed without review of all required evidence"]}
