"""Local field-kit service. One input owner; identical observations for both supervisors."""
from __future__ import annotations

import copy
import hashlib
import json
import pathlib
import queue
import secrets
import subprocess
import threading
import time
import uuid
from collections import Counter, deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit

from .core import Behaviors, BotFault, Events, atomic_json
from .live import ROOT, InputLease, Live, digest, game_identity, load_tool
from .session import run_suite


CATALOG = [
    dict(id="equipment", title="Equipment quick access", category="VR interaction", seconds=25,
         source="field-context-roundtrip.json", cases=["equipment-picker-open-release"],
         description="Hold the equipment picker, observe its input context, release back to gameplay."),
    dict(id="commands", title="Hold to command", category="VR interaction", seconds=25,
         source="field-context-roundtrip.json", cases=["commands-held-open-release"],
         description="Hold Commands and verify entry and release through the configured VR binding."),
    dict(id="binoculars", title="Physical binoculars / equip + stow", category="VR interaction", seconds=45,
         source="field-context-roundtrip.json", cases=["vr-binocular-latch-enter", "vr-binocular-latch-stow"],
         description="Equip and stow the existing VR binoculars. Enemy acquisition, marking and zoom need separate cases."),
    dict(id="posture", title="Stand / crouch / prone / return", category="Locomotion", seconds=65,
         source="posture-roundtrip.json", cases=None,
         description="Verify all four native stance transitions, ending standing. Requires standing on safe ground."),
]


def suite_for(identifier):
    entry = next((item for item in CATALOG if item["id"] == identifier), None)
    if entry is None:
        raise BotFault("Unknown scenario")
    value = json.loads((ROOT / "tools/gameplay_bot/suites" / entry["source"]).read_text(encoding="utf-8-sig"))
    if entry["cases"]:
        value["cases"] = [case for case in value["cases"] if case["id"] in entry["cases"]]
    value["continue_after_outcome_failure"] = False
    return value


def state_key(state):
    native = state.get("native", {})
    return {**{key: state.get(key) for key in ("scene", "title", "menu", "demo", "loading", "idroid")},
            "mission": native.get("mission"), "sequence": native.get("sequence"),
            "context": state.get("controls", {}).get("context")}


class Stopped(BotFault):
    pass


class KitEvents(Events):
    def __init__(self, output, identity, callback):
        self.callback = callback
        super().__init__(output, identity)

    def emit(self, kind, **values):
        event = super().emit(kind, **values)
        self.callback(kind, values)
        return event


class FieldKit:
    def __init__(self, game, proxy, controls, output, *, worker=True):
        self.game, self.proxy, self.controls = map(pathlib.Path, (game, proxy, controls))
        self.output = pathlib.Path(output).resolve()
        self.output.mkdir(parents=True, exist_ok=True)
        self.lock = threading.RLock()
        self.cancel, self.shutdown = threading.Event(), threading.Event()
        self.commands = queue.Queue()
        self.mode, self.epoch = "human", 1
        self.busy, self.connected, self.status = False, False, "Disconnected"
        self.observation, self.telemetry, self.identity = None, {}, {}
        self.timeline, self.results = deque(maxlen=80), []
        self.media = {}
        for path in sorted(self.output.glob("*/result.json"))[-100:]:
            try:
                result = json.loads(path.read_text(encoding="utf-8"))
                if result.get("scenario") not in {item["id"] for item in CATALOG}:
                    continue
                # Bind archive paths to the actual directory, not stored path strings.
                result["output"], result["run_id"] = str(path.parent), path.parent.name
                observation = result.get("observation")
                if observation:
                    for capture in observation["captures"]:
                        image = pathlib.Path(capture["path"]).resolve()
                        if not image.is_relative_to(path.parent.resolve()) or digest(image) != capture["sha256"]:
                            raise ValueError("Archive image identity does not match")
                        key = uuid.uuid4().hex
                        self.media[key] = image
                        capture["url"] = "/media/" + key
                self.results.append(result)
            except (OSError, ValueError, KeyError, TypeError):
                continue
        self.live = self.lease = None
        self.worker = threading.Thread(target=self._loop, name="field-kit-input-owner", daemon=True)
        if worker:
            self.worker.start()

    def event(self, kind, values):
        with self.lock:
            if isinstance(values.get("state"), dict):
                self.telemetry = dict(values["state"])
                self.telemetry["fieldkit_observed_unix_ns"] = time.time_ns()
            if kind in {"semantic_action", "case_started", "case_finished", "attention_required", "scenario_finished",
                        "supervisor_changed", "cleanup", "error", "connected", "disconnected", "observation_ready"}:
                self.timeline.append({"time": time.time(), "kind": kind, "detail": copy.deepcopy(values)})

    def catalog(self):
        latest = {}
        for result in self.results:
            latest[result["scenario"]] = result
        return [{**entry, "last_result": latest.get(entry["id"]), "ready": True} for entry in CATALOG]

    def snapshot(self):
        # No role-specific filtering: exactly this object goes to the UI and agent API.
        with self.lock:
            return copy.deepcopy(dict(schema=1, supervisor=self.mode, epoch=self.epoch, connected=self.connected,
                busy=self.busy, status=self.status, observation=self.observation, telemetry=self.telemetry,
                identity=self.identity, scenarios=self.catalog(), timeline=list(self.timeline), results=self.results,
                release_ready=False, visual_acceptance="separate review required"))

    def admit(self, role, request):
        with self.lock:
            action = request.get("action")
            if action == "set_mode":
                if role != "human" or request.get("mode") not in ("human", "llm"):
                    raise BotFault("Only the human supervisor can select Human or LLM")
                self.mode, self.epoch = request["mode"], self.epoch + 1
                if self.busy:
                    self.cancel.set()
                self.event("supervisor_changed", {"mode": self.mode, "epoch": self.epoch})
                return {"accepted": True, "epoch": self.epoch}
            if action in ("stop", "disconnect") and role == "human":
                self.cancel.set()
                if action == "disconnect":
                    self.busy = True
                    self.commands.put(("disconnect", {}, None))
                return {"accepted": True}
            if role != self.mode or request.get("epoch") != self.epoch:
                raise BotFault("Supervisor changed; this source or decision epoch is no longer current")
            if action == "stop":
                self.cancel.set()
                return {"accepted": True}
            if self.busy:
                raise BotFault("Runner is busy; stop or wait for the current operation")
            if action not in ("connect", "observe", "continue", "run", "disconnect"):
                raise BotFault("Unsupported action; submit a catalog scenario, not arbitrary input")
            if action == "connect" and self.connected:
                raise BotFault("Already connected")
            if action != "connect" and not self.connected:
                raise BotFault("Connect to an existing regular simulator session first")
            if action in ("run", "continue"):
                self._validate_observation(request)
            if action == "run":
                identifiers = request.get("scenarios")
                allowed = {item["id"] for item in CATALOG}
                if (not isinstance(identifiers, list) or not identifiers or len(identifiers) > len(allowed)
                        or any(not isinstance(item, str) or item not in allowed for item in identifiers)
                        or len(set(identifiers)) != len(identifiers)):
                    raise BotFault("Select unique executable scenarios from the library")
            self.busy, self.status = True, action.capitalize() + " queued"
            self.cancel.clear()
            self.commands.put((action, copy.deepcopy(request), (role, self.epoch)))
            return {"accepted": True}

    def _validate_observation(self, request):
        obs = self.observation
        if not obs or request.get("observation_id") != obs["id"]:
            raise BotFault("Refresh and review the current observation")
        if not 0 <= time.time() - obs["time"] <= 30:
            raise BotFault("Observation is over 30 seconds old; refresh both eyes before running")
        if request.get("reviewed_capture_sha256") not in [item["sha256"] for item in obs["captures"]]:
            raise BotFault("Decision must reference an image in the current observation")

    def check_stop(self):
        if self.cancel.is_set() or self.shutdown.is_set():
            raise Stopped("Stopped by supervisor; inputs are being released")

    def _events(self, label):
        path = self.output / (time.strftime("%Y%m%d-%H%M%S") + "-" + label + "-" + uuid.uuid4().hex[:6])
        return KitEvents(path, self.identity, self.event)

    def _publish(self, reason):
        self.live.release()
        before = self.live.observe(native=True)
        paths = self.live.capture("observation")
        after = self.live.observe(native=True)
        if state_key(before) != state_key(after):
            raise BotFault("Scene changed during the eye captures; refresh before deciding")
        captures = []
        for index, path in enumerate(paths):
            path = pathlib.Path(path).resolve()
            if not path.is_relative_to(self.output):
                raise BotFault("Capture is outside this kit's evidence directory")
            key = uuid.uuid4().hex
            self.media[key] = path
            captures.append(dict(eye="left" if index == 0 else "right", url="/media/" + key,
                                 path=str(path), sha256=digest(path)))
        observation = dict(id=uuid.uuid4().hex, time=time.time(), state=after, captures=captures,
                           reason=reason, identity=self.identity, capture_note="Sequential final-compositor eyes; not a synchronized stereo video")
        atomic_json(self.live.events.output / "observation.json", observation)
        with self.lock:
            self.observation = observation
        self.event("observation_ready", {"reason": reason, "id": observation["id"]})

    def _connect(self):
        self.lease = InputLease()
        self.lease.__enter__()
        self.identity = game_identity(self.game)
        self.identity["bindings_tool_sha256"] = digest(self.controls)
        bindings = json.loads(subprocess.check_output([str(self.controls), "--bindings-json",
                             str(self.game / "mgs5vr-controls.ini")], text=True, encoding="utf-8"))
        events = self._events("connection")
        atomic_json(events.output / "bindings.json", bindings)
        self.live = Live(self.proxy, self.game, bindings, events)
        self.live.background_check = self.check_stop
        self.live.ready()
        self.live.release()
        self._publish("Connected to regular simulator")
        self.connected = True
        self.event("connected", {"pid": self.identity["process"]["ProcessId"]})

    def _disconnect(self):
        try:
            if self.live:
                self.live.background_check = None
                self.live.close()
        finally:
            self.live = None
            if self.lease:
                self.lease.__exit__(None, None, None)
                self.lease = None
            self.connected = False
            self.event("disconnected", {})

    def _cleanup(self, scenario, baseline):
        """Only restore state this selected scenario could have changed; never open Pause."""
        self.live.background_check = None
        self.live.release()
        state = self.live.observe(native=True)
        same_mission = state.get("native", {}).get("mission") == baseline.get("native", {}).get("mission")
        if scenario == "binoculars" and same_mission and state.get("controls", {}).get("context") == "binoculars":
            self.live.execute({"op": "action", "name": "binoculars.stow", "seconds": .15})
            Behaviors(self.live, self.live.events).wait_for({"controls.context": "gameplay"}, 5, "cleanup-optic")
        if scenario == "posture" and same_mission and state.get("scene") == "gameplay" and baseline.get("native", {}).get("status_STAND") is True:
            for _ in range(2):
                state = self.live.observe(native=True)
                if state.get("native", {}).get("status_STAND") is True:
                    break
                target = "status_SQUAT" if state.get("native", {}).get("status_CRAWL") is True else "status_STAND"
                if state.get("native", {}).get("status_CRAWL") is not True and state.get("native", {}).get("status_SQUAT") is not True:
                    raise BotFault("Cannot identify posture for cleanup")
                self.live.execute({"op": "action", "name": "gameplay.stance", "seconds": .15})
                Behaviors(self.live, self.live.events).wait_for({"native." + target: True}, 5, "cleanup-posture")
        self.live.release()
        self.event("cleanup", {"scenario": scenario, "inputs_released": True})

    def _run(self, identifiers):
        for identifier in identifiers:
            self.check_stop()
            events = self._events(identifier)
            self.live.events = events
            atomic_json(events.output / "bindings.json", self.live.bindings)
            suite = suite_for(identifier)
            atomic_json(events.output / "suite.json", suite)
            self.status = "Running / " + identifier
            baseline = self.live.observe(native=True)
            result = dict(status="failed", cases=[])
            try:
                result = run_suite(Behaviors(self.live, events), suite,
                                   lambda records: atomic_json(events.output / "cases.json", records))
            except Exception as error:
                result.update(error=str(error))
            finally:
                try:
                    self._cleanup(identifier, baseline)
                    self._publish(identifier + " / final evidence")
                    result["observation"] = self.observation
                except Exception as error:
                    result.update(status="failed", cleanup_error=str(error))
                self.live.background_check = self.check_stop
            if self.cancel.is_set() or self.shutdown.is_set():
                result["status"] = "stopped"
            result.update(scenario=identifier, run_id=events.output.name, finished=time.time(), output=str(events.output), identity=self.identity,
                          suite_sha256=digest(events.output / "suite.json"), visual_acceptance="pending", release_ready=False)
            atomic_json(events.output / "result.json", result)
            with self.lock:
                self.results.append(result)
            self.event("scenario_finished", {"scenario": identifier, "status": result["status"]})
            if result["status"] != "observed_pass":
                break

    def _loop(self):
        try:
            while not self.shutdown.is_set():
                try:
                    action, request, owner = self.commands.get(timeout=.8)
                except queue.Empty:
                    if self.connected:
                        try:
                            self.live.background_check = None
                            self.live.observe(native=True)
                        except Exception as error:
                            self.status = "Observation unavailable: " + str(error)
                    continue
                try:
                    self.status = {"connect": "Connecting to regular simulator", "continue": "Continuing through recognized startup",
                                   "observe": "Capturing both eyes", "disconnect": "Releasing and disconnecting"}.get(action, "Starting queue")
                    if owner is not None and owner != (self.mode, self.epoch):
                        raise Stopped("Queued decision superseded by supervisor switch")
                    if action == "disconnect":
                        self._disconnect()
                    elif action == "connect":
                        self._connect()
                    else:
                        self.live.background_check = self.check_stop
                        self.check_stop()
                        if action in ("continue", "run"):
                            self._validate_observation(request)
                            if state_key(self.live.observe(native=True)) != state_key(self.observation["state"]):
                                raise BotFault("Game state changed since the reviewed images; refresh first")
                        if action == "continue":
                            module = load_tool("fieldkit_startup", "gameplay-bot.py")
                            module.continue_game(self.live, Behaviors(self.live, self.live.events))
                        elif action == "run":
                            self._run(request["scenarios"])
                        # Cleanup is completed before the new supervisor gets a fresh decision point.
                        self.live.background_check = None
                        self.live.release()
                        self._publish("Stopped" if self.cancel.is_set() else "Ready for next decision")
                    self.status = "Ready" if self.connected else "Disconnected"
                except Exception as error:
                    self.status = str(error)
                    self.event("error", {"error": str(error)})
                    try:
                        if self.live:
                            self.live.background_check = None
                            self.live.release()
                        if action == "connect":
                            self._disconnect()
                    except Exception as cleanup:
                        self.event("error", {"cleanup_error": str(cleanup)})
                finally:
                    with self.lock:
                        self.busy = False
                    self.commands.task_done()
        finally:
            self._disconnect()

    def close(self):
        self.cancel.set()
        self.shutdown.set()
        if self.worker.is_alive():
            self.worker.join(timeout=45)


def coverage():
    source = ROOT / "docs/COMMUNITY_VERIFICATION_2026-09-27.json"
    data = json.loads(source.read_text(encoding="utf-8"))
    reports = data["reports"]
    return {"reports": reports, "counts": dict(Counter(claim["status"] for report in reports for claim in report["claims"])),
            "source": str(source), "source_sha256": digest(source), "scope": "Historical release evidence; not results from this field-kit session"}


def make_server(kit, port):
    tokens = {"human": secrets.token_urlsafe(32), "llm": secrets.token_urlsafe(32)}
    ui = ROOT / "tools/test-field-kit"
    assets = {"/": ui / "index.html", "/app.js": ui / "app.js", "/style.css": ui / "style.css",
              "/field-header.svg": ROOT / "docs/images/field-header.svg",
              "/paper-grain.svg": ROOT / "tools/launcher-ui/paper-grain.svg"}

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def respond(self, status, data, kind="application/json"):
            body = json.dumps(data, allow_nan=False).encode() if kind == "application/json" else data
            self.send_response(status)
            self.send_header("Content-Type", kind)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("Referrer-Policy", "no-referrer")
            self.send_header("Content-Security-Policy", "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' blob:; connect-src 'self'; frame-ancestors 'none'")
            self.end_headers()
            self.wfile.write(body)

        def authorized(self):
            origin = "http://127.0.0.1:" + str(self.server.server_port)
            if self.headers.get("Host") != urlsplit(origin).netloc or self.headers.get("Origin", origin) != origin:
                raise PermissionError("Local origin required")
            authorization = self.headers.get("Authorization", "")
            return next((role for role, token in tokens.items() if secrets.compare_digest(authorization, "Bearer " + token)), None)

        def do_GET(self):
            try:
                role = self.authorized()
                path = urlsplit(self.path).path
                if path in assets:
                    import mimetypes
                    self.respond(200, assets[path].read_bytes(), mimetypes.guess_type(str(assets[path]))[0] or "text/html")
                elif not role:
                    self.respond(401, {"error": "Session token required"})
                elif path == "/api/state":
                    self.respond(200, kit.snapshot())
                elif path == "/api/coverage":
                    self.respond(200, coverage())
                elif path.startswith("/media/") and path[7:] in kit.media:
                    self.respond(200, kit.media[path[7:]].read_bytes(), "image/png")
                else:
                    self.respond(404, {"error": "Not found"})
            except PermissionError as error:
                self.respond(403, {"error": str(error)})
            except (OSError, ValueError) as error:
                self.respond(500, {"error": str(error)})

        def do_POST(self):
            try:
                role = self.authorized()
                if not role:
                    return self.respond(401, {"error": "Session token required"})
                if self.path != "/api/command":
                    return self.respond(404, {"error": "Not found"})
                length = int(self.headers.get("Content-Length", "0"))
                if not 0 < length <= 16384:
                    raise ValueError("Invalid request length")
                request = json.loads(self.rfile.read(length))
                if not isinstance(request, dict):
                    raise ValueError("Request must be an object")
                self.respond(202, kit.admit(role, request))
            except PermissionError as error:
                self.respond(403, {"error": str(error)})
            except (BotFault, ValueError, TypeError) as error:
                self.respond(409, {"error": str(error)})

    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    origin = "http://127.0.0.1:" + str(server.server_port)
    return server, {"url": origin, "human_url": origin + "/#token=" + tokens["human"], "tokens": tokens,
                    "pid": __import__("os").getpid(), "schema": 1}
