"""Model-based test routing over guarded native/VR state transitions.

Plans are proposals for the existing supervised executor, not input dispatch.
Unknown states/edges remain in the model and cannot become traversable merely
because two screens look similar or share an input resolver category.
"""
import copy
import hashlib
import json
import math
from pathlib import Path

from .campaign import checked_path, file_hash, fingerprint, read_json
from .core import BotFault, astar, matches
from .menu_coverage import menu_inventory


def _merge(left, right):
    result = dict(left)
    for key, value in right.items():
        if key in result and (type(result[key]) is not type(value) or result[key] != value):
            raise BotFault("Conflicting transition guard: " + key)
        result[key] = value
    return result


def declared_model(root):
    model = read_json(root / "tools/gameplay_bot/catalogs/game-state-seed.json")
    menus = read_json(root / "tools/gameplay_bot/catalogs/vr-menu-paths.json")
    contexts = read_json(root / "docs/NATIVE_CONTEXT_COVERAGE.json")
    worlds = read_json(root / "tools/gameplay_bot/catalogs/native-worlds.json")
    for mode, title in menus["modes"].items():
        model["states"]["MODE.TPP." + mode] = {"title": title, "kind": "native_mode", "predicate": None,
            "missing": ["Exact native mode/role identity and authored entry/exit recipe"]}
    for row in menu_inventory(root):
        model["states"][row["id"]] = {"title": row["title"], "kind": "menu_path", "predicate": None,
            "mode": row["mode"], "presentation": row["presentation"], "missing": row["acceptance"]}
        if row.get("latest_report"):
            model["states"][row["id"]]["latest_report"] = row["latest_report"]
        model["transitions"].append({"id": "enter." + row["id"], "from": "MODE.TPP." + row["mode"] ,
            "to": row["id"], "cost": 1, "blocked_reason": "Exact page/choice identity and guarded native recipe not implemented"})
    for row in contexts["records"]:
        model["states"][row["id"]] = {"title": row["context"], "kind": "native_situation", "predicate": None,
            "identity_obligation": row["identity"], "missing": row["identity"].get("missing", [])}
    for world in worlds["locations"]:
        model["states"]["WORLD.TPP." + world["key"]] = {"title": "Native location " + world["key"],
            "kind": "world", "predicate": None, "location_code": world["code"],
            "missing": ["Owned navigation import, native player clearance, map/state transition coverage"]}
    for code in worlds["mission_codes"]:
        identifier = "MISSION.TPP." + str(code)
        model["states"][identifier] = {"title": "Native mission code " + str(code), "kind": "mission",
            "predicate": None, "missing": ["Eligible isolated checkpoint, authored sequence states, objective/failure/recovery transitions"]}
        model["transitions"].append({"id": "deploy." + identifier, "from": "MODE.TPP.acc", "to": identifier,
            "cost": 1, "requires_isolated_save": True,
            "blocked_reason": "Mission-specific deployment, sequence and isolated-save adapters not implemented"})
    model.update(discovery_complete=False, full_game_acceptance=False)
    return model


class StateGraph:
    def __init__(self, root, model, identity, bindings, *, isolated_save=False):
        self.build_identity = fingerprint(identity)
        self.nodes = copy.deepcopy(model["states"])
        self.edges = []
        actions = {row["name"]: row for row in bindings.get("actions", [])}
        seen = set()
        for source in model["transitions"]:
            edge = copy.deepcopy(source)
            name, a, b, cost = edge["id"], edge["from"], edge["to"], edge["cost"]
            if not name or name in seen or a not in self.nodes or b not in self.nodes:
                raise BotFault("Invalid or duplicate state transition: " + str(name))
            if type(cost) not in (int, float) or not math.isfinite(cost) or cost < 0:
                raise BotFault("State transition cost must be finite and nonnegative")
            seen.add(name)
            reason = edge.get("blocked_reason")
            if edge.get("requires_isolated_save") and not isolated_save:
                reason = reason or "Requires a verified isolated save/checkpoint fixture"
            if not reason:
                try:
                    path = checked_path(root, edge["suite"])
                    suite = read_json(path)
                    cases = [row for row in suite["cases"] if row["id"] == edge["case_id"]]
                    if len(cases) != 1:
                        raise BotFault("Recipe does not identify one exact case")
                    case = copy.deepcopy(cases[0])
                    if not case.get("before") or not case.get("after") or not case.get("steps"):
                        raise BotFault("Recipe requires before/actions/observed outcome")
                    if not self.nodes[a].get("predicate") or not self.nodes[b].get("predicate"):
                        raise BotFault("Native state identity unresolved")
                    for step in case["steps"]:
                        if step.get("op") == "action" and not actions.get(step.get("name"), {}).get("bindings"):
                            raise BotFault("Effective VR action unavailable: " + str(step.get("name")))
                    case["before"] = _merge(self.nodes[a]["predicate"], case["before"])
                    case["after"] = _merge(self.nodes[b]["predicate"], case["after"])
                    case.update(id=name, source_case_id=edge["case_id"], depends_on=[])
                    edge.update(case=case, suite_sha256=file_hash(path))
                except (BotFault, KeyError, OSError) as error:
                    reason = str(error)
            edge.update(ready=not bool(reason), native_acceptance="unproven", reason=reason)
            self.edges.append(edge)
        definition = {"nodes": self.nodes, "edges": self.edges}
        runner_root = Path(__file__).resolve().parent
        runner_files = sorted(runner_root.glob('*.py')) + [runner_root.parent / 'gameplay-bot.py']
        runner_hashes = {str(path.relative_to(runner_root.parent)): file_hash(path) for path in runner_files}
        self.identity = {"build": self.build_identity,
            "definition_sha256": hashlib.sha256(json.dumps(definition, sort_keys=True).encode()).hexdigest(),
            "runner_source_sha256": hashlib.sha256(json.dumps(runner_hashes, sort_keys=True).encode()).hexdigest()}

    def locate(self, observed):
        found = [name for name, node in self.nodes.items()
                 if node.get("predicate") and matches(observed, node["predicate"])]
        if len(found) != 1:
            raise BotFault("Native state is unknown or ambiguous; inspect fresh final eyes and extend the model")
        return found[0]

    def path(self, start, goal, *, identity, blocked=()):
        if fingerprint(identity) != self.build_identity:
            raise BotFault("State plan belongs to another game build or personal configuration")
        allowed = [edge for edge in self.edges if edge["ready"] and edge["id"] not in blocked]
        graph = {"identity": self.identity, "capability": "guarded_vr_state_probe", "nodes": self.nodes,
                 "edges": [{**edge, "validated": True} for edge in allowed]}
        nodes = astar(graph, start, goal, identity=self.identity, capability=graph["capability"])
        route = []
        for a, b in zip(nodes, nodes[1:]):
            route.append(min((edge for edge in allowed if edge["from"] == a and edge["to"] == b),
                             key=lambda edge: (edge["cost"], edge["id"])))
        return route

    def next_test(self, start, *, identity, covered=(), blocked=()):
        """Select an uncovered guarded edge with a declared return route."""
        if fingerprint(identity) != self.build_identity:
            raise BotFault("State plan belongs to another game build or personal configuration")
        candidates = []
        for edge in self.edges:
            if not edge["ready"] or edge["id"] in covered or edge["id"] in blocked:
                continue
            try:
                route = self.path(start, edge["from"], identity=identity, blocked=blocked) + [edge]
                route += self.path(edge["to"], start, identity=identity, blocked=blocked)
            except BotFault:
                continue
            candidates.append((sum(item["cost"] for item in route), edge["id"], route))
        if not candidates:
            return {"status": "no_guarded_roundtrip", "blocked": self.blockers(), "complete": False}
        _, target, route = min(candidates, key=lambda row: (row[0], row[1]))
        return {"status": "planned", "target": target, "start": start, "end": start,
                "identity": self.identity, "transitions": route, "complete": False,
                "execution_requires": ["Fresh final-eye review of actual pages/choices in an owned session",
                    "Fresh native entry and actual outcome checks for every transition",
                    "Release controls and restore the declared neutral exit on failure"]}

    def blockers(self):
        return [{"id": edge["id"], "reason": edge["reason"]} for edge in self.edges if not edge["ready"]]

    def probe_suite(self, transition, *, identity, start='owner.on_foot', max_wait=15.):
        """Compile only an explicitly selected guarded roundtrip for the runner."""
        if type(max_wait) not in (int, float) or not math.isfinite(max_wait) or not 0 < max_wait <= 600:
            raise BotFault('State probe observation budget must be in (0,600] seconds')
        found = [edge for edge in self.edges if edge['id'] == transition]
        if len(found) != 1 or not found[0]['ready']:
            raise BotFault('Selected state transition is unavailable: ' + str(transition))
        edge = found[0]
        route = self.path(start, edge['from'], identity=identity) + [edge]
        route += self.path(edge['to'], start, identity=identity)
        if not route or len(route) > 32:
            raise BotFault('State probe needs a bounded roundtrip')
        cases = []
        for index, item in enumerate(route):
            case = copy.deepcopy(item['case'])
            case['id'] = f"state-{index:02d}-{item['id']}"
            case['depends_on'] = [cases[-1]['id']] if cases else []
            case['timeout'] = min(case.get('timeout', 8), max_wait)
            case['entry_timeout'] = min(case.get('entry_timeout', 8), max_wait)
            cases.append(case)
        return {'schema': 1, 'cases': cases, 'continue_after_outcome_failure': False,
                'graph_identity': self.identity, 'selected_transition': transition,
                'transition_ids': [item['id'] for item in route], 'neutral_exit': start,
                'scope': 'Coarse control-owner roundtrip only; exact page, mode and headset acceptance remain open'}

    def export(self):
        return {"schema": 1, "identity": self.identity, "states": self.nodes, "transitions": self.edges,
                "discovery_complete": False, "full_game_acceptance": False}


def observation_key(state):
    """Retain incomplete observations without merging unknown pages into one state."""
    required = ["native.mission", "native.location", "native.sequence", "scene", "menu", "idroid",
                "controls.context", "controls.travel_mode", "native.player_vehicle_id",
                "native.demo", "native.tutorial_pause", "native.game_over", "native.story",
                "native.status_CRAWL", "native.status_SQUAT", "native.status_STAND", "native.status_CARRY"]
    native = state.get("native", {})
    if state.get("menu") is True or state.get("idroid") is True:
        required += ["native.menu_page", "native.menu_parent", "native.menu_focus"]
    if native.get("player_vehicle_id") != 65535:
        required += ["native.player_role"]
    values, missing = {}, []
    for path in required:
        value = state
        for part in path.split('.'):
            value = value.get(part) if isinstance(value, dict) else None
        if value is None or (isinstance(value, str) and value.startswith('unavailable:')):
            missing.append(path)
        else:
            values[path] = value
    encoded = json.dumps(values, sort_keys=True)
    return {"key": hashlib.sha256(encoded.encode()).hexdigest(), "dimensions": values,
            "identity_complete": not missing, "missing": missing,
            "limits": "Completeness applies to these recorded dimensions, not all gameplay state or acceptance"}
