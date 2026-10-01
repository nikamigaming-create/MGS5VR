"""Portable behavior contracts, durable events and validated-graph planning."""
from __future__ import annotations

import heapq
import itertools
import json
import math
import pathlib
import time
import threading
import uuid


class BotFault(RuntimeError):
    pass


class BlankCompositorFrame(BotFault):
    def __init__(self, message, state=None):
        super().__init__(message)
        self.state = state or {}


class StateDeadline(BotFault):
    def __init__(self, label, state):
        super().__init__(f"{label}: state deadline exceeded; last scene={state.get('scene', scene(state))}")
        self.state = state


class AttentionRequired(StateDeadline):
    """Stop dependent actions and hand a current, neutral scene to the supervisor."""
    def __init__(self, label, state):
        BotFault.__init__(self, f"{label}: transition needs review after two seconds; last scene={state.get('scene', scene(state))}")
        self.state = state


def atomic_json(path, value):
    path = pathlib.Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + "." + uuid.uuid4().hex + ".tmp")
    try:
        temporary.write_text(json.dumps(value, indent=2, allow_nan=False), encoding="utf-8")
        deadline = time.monotonic() + .5
        while True:
            try:
                temporary.replace(path)
                break
            except OSError as error:
                # Windows readers can briefly deny delete-sharing on the
                # destination. Retry the same file transaction, never input.
                if getattr(error, "winerror", None) not in (5, 32, 33) or time.monotonic() >= deadline:
                    raise
                time.sleep(.01)
    finally:
        temporary.unlink(missing_ok=True)


def scene(state):
    """Conservative classification. An absent publication is not false."""
    if state.get("loading") is True:
        return "loading"
    if state.get("title") is True:
        return "title"
    if state.get("menu") is True:
        return "menu"
    if state.get("demo") is True:
        return "cinematic"
    if any(state.get(k) is not False for k in ("loading", "title", "menu", "demo")):
        return "unknown"
    if state.get("camera_active") is not True or state.get("camera_available") is not True:
        # Presentation can deliberately disable the tracked camera while the
        # native game remains playable on a quad. Require native evidence;
        # an inactive VR camera alone still cannot establish gameplay.
        native = state.get("native", {})
        sequence = native.get("sequence")
        if not (native.get("title") is False and type(native.get("mission")) is int
                and native["mission"] > 0 and isinstance(sequence, str)
                and sequence.startswith("Seq_Game_") and native.get("status_NORMAL_ACTION") is True):
            return "unknown"
    return "cabin" if state.get("cabin") is True else "gameplay"


def matches(state, predicate):
    if not isinstance(predicate, dict) or not predicate:
        raise BotFault("An explicit nonempty state predicate is required")
    for path, expected in predicate.items():
        value = state
        for part in path.split("."):
            if not isinstance(value, dict) or part not in value:
                return False
            value = value[part]
        if isinstance(expected, dict) and set(expected) == {"near", "tolerance"}:
            target, tolerance = expected["near"], expected["tolerance"]
            def finite_number(number):
                try:
                    return type(number) in (int, float) and math.isfinite(number)
                except OverflowError:
                    return False
            targets = target if isinstance(target, list) else [target]
            if not targets or not all(finite_number(v) for v in targets) \
                    or not finite_number(tolerance) or tolerance <= 0:
                raise BotFault("Near predicates need finite numbers and a positive absolute tolerance")
            values = value if isinstance(value, list) else [value]
            if isinstance(value, list) != isinstance(target, list) or len(values) != len(targets) \
                    or not all(finite_number(v) for v in values) \
                    or not all(abs(a-b) <= tolerance for a, b in zip(values, targets)):
                return False
            continue
        # bool must not silently equal 0 or 1 from a different schema.
        if type(value) is not type(expected) or value != expected:
            return False
    return True


class Events:
    def __init__(self, output, identity, clock=time.monotonic):
        self.output = pathlib.Path(output)
        self.output.mkdir(parents=True, exist_ok=True)
        self.clock = clock
        self.origin = clock()
        self.run_id = str(uuid.uuid4())
        self.lock = threading.Lock()
        self.path = self.output / "events.jsonl"
        if self.path.exists():
            raise BotFault("Use a fresh run directory; previous evidence is never overwritten")
        atomic_json(self.output / "identity.json", {"run_id": self.run_id, **identity})

    def emit(self, kind, **values):
        event = {"run_id": self.run_id, "event": kind,
                 "monotonic_seconds": self.clock() - self.origin,
                 "unix_ns": time.time_ns(), **values}
        if __import__("sys").platform == "win32":
            import ctypes
            ticks, frequency = ctypes.c_int64(), ctypes.c_int64()
            ctypes.windll.kernel32.QueryPerformanceCounter(ctypes.byref(ticks))
            ctypes.windll.kernel32.QueryPerformanceFrequency(ctypes.byref(frequency))
            event["qpc_100ns"] = ticks.value * 10000000 // frequency.value
        with self.lock:
            with self.path.open("a", encoding="utf-8") as stream:
                stream.write(json.dumps(event, allow_nan=False) + "\n")
        if kind == "attention_required":
            atomic_json(self.output / "attention.json", event)
            print(json.dumps({"event": kind, "case": values.get("label"), "reason": values.get("reason"),
                              "captures": values.get("captures", []), "path": str(self.output / "attention.json")}), flush=True)
        return event


class Behaviors:
    def __init__(self, adapter, events, clock=time.monotonic, sleep=time.sleep):
        self.adapter, self.events = adapter, events
        self.clock, self.sleep = clock, sleep

    def wait_for(self, predicate, timeout, label, stable=2, native=False, milestones=()):
        if not 0 < timeout <= 600 or not 1 <= stable <= 20:
            raise BotFault("Invalid bounded observation wait")
        deadline = self.clock() + timeout
        count = 0
        last_scene = None
        started = self.clock()
        seen_milestones = set()
        notified = False
        while True:
            state = self.adapter.observe(native=True) if native or any(k.startswith("native.") for k in predicate) else self.adapter.observe()
            current = state.get("scene", scene(state))
            if current != last_scene:
                self.events.emit("state", label=label, state=state)
                last_scene = current
            count = count + 1 if matches(state, predicate) else 0
            if count >= stable:
                self.events.emit("predicate_satisfied", label=label, predicate=predicate,
                                 state=state)
                return state
            for milestone in milestones:
                name = milestone["id"]
                if name not in seen_milestones and matches(state, milestone["state"]):
                    seen_milestones.add(name)
                    self.events.emit("milestone_reached", label=label, milestone=name,
                                     elapsed=self.clock()-started, state=state, expected=predicate)
            if not notified and self.clock()-started >= 2.:
                notified = True
                captures = []
                if not getattr(self.adapter, "held", False):
                    captures = self.adapter.capture(label.replace(":", "-")[:90]+"-attention")
                self.events.emit("attention_required", label=label, expected=predicate, state=state,
                                 elapsed=self.clock()-started, captures=captures,
                                 reason="Expected transition has not completed; inspect the current intermediate menu or scene")
                if getattr(self.adapter, "supervised", False):
                    raise AttentionRequired(label, state)
            remaining = deadline - self.clock()
            if remaining <= 0:
                raise StateDeadline(label, state)
            self.sleep(min(.1, remaining))

    def case(self, case):
        """RPC success never passes a case; require preconditions + a new outcome."""
        case_id = case["id"]
        if not case.get("steps") or not case.get("before") or not case.get("after"):
            raise BotFault(f"{case_id}: steps, before and after predicates are required")
        after_stable = case.get("after_stable_samples", 2)
        if type(after_stable) is not int or not 1 <= after_stable <= 20:
            raise BotFault("Outcome stability must be 1..20 observed samples")
        self.events.emit("case_started", case_id=case_id, case=case)
        result = {"id": case_id, "status": "failed", "visual_acceptance": "pending"}
        phase = "entry"
        try:
            self.adapter.release()
            during = case.get("during")
            before = self.wait_for(case["before"], case.get("entry_timeout", 8), case_id + ":entry",
                                   native=bool(during and any(key.startswith("native.") for key in during)))
            if during and (case["steps"][-1].get("op") not in ("action", "menu_navigate") or matches(before, during)):
                raise BotFault("While-held case needs a final action and an initially unsatisfied during predicate")
            if not during and matches(before, case["after"]):
                raise BotFault("Outcome already satisfied before action; no action effect proven")
            result["before"] = before
            phase = "setup"
            for step in case.get("setup", []):
                self.adapter.execute(step)
            self.adapter.capture(case_id + "-before")
            phase = "dispatch"
            for index, step in enumerate(case["steps"]):
                if during and index == len(case["steps"]) - 1:
                    observed = self.adapter.execute({**step, "while_held": during,
                                                     "capture_while_held": case_id + "-during"})
                    if not observed or not matches(observed.get("state", {}), during):
                        raise BotFault("Action did not return an observed while-held outcome")
                    result["during"] = observed["state"]
                    result["during_captures"] = observed.get("captures", [])
                    if observed.get("visual_checkpoint"):
                        result["during_visual_checkpoint"] = observed["visual_checkpoint"]
                else:
                    self.adapter.execute(step)
            phase = "outcome"
            after = self.wait_for(case["after"], case.get("timeout", 8), case_id + ":outcome",
                                  milestones=case.get("milestones", ()), stable=after_stable)
            phase = "capture"
            captures = self.adapter.capture(case_id + "-after")
            result.update(status="observed_pass", before=before, after=after, captures=captures)
        except Exception as error:
            result["error"] = str(error)
            result["failure_phase"] = phase
            if isinstance(error, AttentionRequired):
                result["attention_required"] = True
            if phase == "outcome" and isinstance(error, StateDeadline):
                result["last_state"] = error.state
                # This only says the entry predicates still match. It does not
                # assert that the action had no effect elsewhere in the world.
                result["entry_predicates_still_match"] = matches(error.state, case["before"])
            try:
                result["captures"] = self.adapter.capture(case_id + "-failure")
            except Exception as capture_error:
                result["capture_error"] = str(capture_error)
        finally:
            try:
                self.adapter.release()
            except Exception as release_error:
                result.update(status="failed", release_error=str(release_error))
            self.events.emit("case_finished", result=result)
        return result


def astar(graph, start, goal, *, identity, capability, blocked=()):
    """A* with h=0 until a native adapter proves a tighter lower bound.

    Only full, explicitly validated directed edges are traversable. Node IDs
    include floor/cell identity in the adapter, never flattened X/Z coordinates.
    This planner alone is not a native movement-query adapter or a playable bot.
    """
    if graph.get("identity") != identity or graph.get("capability") != capability:
        raise BotFault("Navigation graph belongs to another scene/build or actor capability")
    nodes = graph.get("nodes", {})
    if start not in nodes or goal not in nodes:
        raise BotFault("Unknown route endpoint")
    edges = {key: [] for key in nodes}
    for edge in graph.get("edges", []):
        if edge.get("validated") is not True:
            continue
        a, b = edge["from"], edge["to"]
        cost = edge["cost"]
        if a not in nodes or b not in nodes or not math.isfinite(cost) or cost < 0:
            raise BotFault("Invalid validated navigation edge")
        if (a, b) not in blocked:
            edges[a].append((b, cost))
    order = itertools.count()
    pending = [(0., next(order), start)]
    costs, previous = {start: 0.}, {}
    while pending:
        cost, _, node = heapq.heappop(pending)
        if cost != costs[node]:
            continue
        if node == goal:
            path = [node]
            while node in previous:
                node = previous[node]
                path.append(node)
            return list(reversed(path))
        for target, weight in edges[node]:
            candidate = cost + weight
            if candidate < costs.get(target, math.inf):
                costs[target], previous[target] = candidate, node
                heapq.heappush(pending, (candidate, next(order), target))
    raise BotFault("No validated route to target")


class ProgressGuard:
    """Stop an ineffective movement segment rather than oscillating forever."""
    def __init__(self, timeout=2., minimum_progress=.08):
        self.timeout, self.minimum_progress = timeout, minimum_progress
        self.best, self.since = math.inf, None

    def observe(self, distance, now):
        if not math.isfinite(distance) or distance < 0:
            raise BotFault("Invalid native distance")
        if self.since is None or distance < self.best - self.minimum_progress:
            self.best, self.since = distance, now
        elif now - self.since >= self.timeout:
            raise BotFault("No native movement progress; release input and replan")
