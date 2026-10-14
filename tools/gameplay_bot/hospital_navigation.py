"""Owned hospital event routes, with floor-aware A* and observed obstacles.

NPC NAV2 topology is a planning prior. The hospital executor still submits
ordinary VR input and measures real player progress; this never changes game
collision, actor transforms, or sequence state.
"""
import hashlib
import json
import math
import time
from pathlib import Path

from .core import BotFault
from .navigation import NativeMap, ObstacleMemory, NoConnectedNativeRoute


ROOT = Path(__file__).resolve().parents[2]


class HospitalEventRoute:
    def __init__(self, manifest, event, sequence, native):
        path = Path(manifest)
        path = path if path.is_absolute() else ROOT / path
        data = json.loads(path.read_text(encoding='utf-8'))
        if (data.get('mission') != 10010 or data.get('location') != 30
                or data.get('story') != 0 or native.get('mission') != 10010
                or native.get('location') != 30 or native.get('story') != 0
                or native.get('sequence') != sequence):
            raise BotFault('Hospital navigation requires the exact native event owner')
        def owned_file(row):
            source = Path(row['file'])
            source = source if source.is_absolute() else ROOT / source
            digest = hashlib.sha256(source.read_bytes()).hexdigest()
            if digest != row.get('sha256'):
                raise BotFault('Owned hospital navigation source changed; reimport it')
            return source
        owned_file(data['scene'])
        self.nav = NativeMap.from_files([owned_file(row) for row in data['tiles']])
        target = data.get('events', {}).get(event)
        if (not isinstance(target, dict) or target.get('sequence') != sequence
                or not isinstance(target.get('center'), list) or len(target['center']) != 3
                or not isinstance(target.get('size'), list) or len(target['size']) != 3
                or any(type(v) not in (int, float) or not math.isfinite(v)
                       for v in target['center'] + target['size'])
                or any(v <= 0 for v in target['size'])
                or type(target.get('support_y')) not in (int, float)
                or not math.isfinite(target['support_y'])
                or type(target.get('goal_support_y', target['support_y'])) not in (int, float)
                or not math.isfinite(target.get('goal_support_y', target['support_y']))):
            raise BotFault('Hospital event needs its owned trap and witnessed supporting floor')
        self.event, self.sequence = event, sequence
        self.floor = target['support_y']
        self.goal_floor = target.get('goal_support_y', self.floor)
        self.root_offset = native['player_y'] - self.floor
        offsets = target.get('root_to_floor_offsets')
        self.posture_offsets = offsets
        if offsets is not None:
            active = [key for key in ('CRAWL', 'SQUAT', 'STAND') if native.get('status_' + key) is True]
            if (not isinstance(offsets, dict) or len(active) != 1
                    or type(offsets.get(active[0])) not in (int, float)
                    or not math.isfinite(offsets[active[0]])):
                raise BotFault('Hospital route needs its calibrated native posture offset')
            self.root_offset = offsets[active[0]]
            self.floor = native['player_y'] - self.root_offset
        if not 0.1 <= self.root_offset <= 1.5:
            raise BotFault('Hospital route lacks a witnessed root-to-floor offset')
        self.center, self.size = target['center'], target['size']
        self.goal = [self.center[0], self.goal_floor, self.center[2]]
        # Retail NPC nodes are sparse near event volumes. A route edge can
        # cross the trap even though its endpoint is a few metres beyond it.
        # Keep the map's ordinary snap radius and stop on actual trap entry.
        self.goal_candidates = sorted((math.dist(self.goal, point), node)
            for node, point in enumerate(self.nav.positions)
            if abs(point[1]-self.goal_floor)<=.15 and math.dist(self.goal, point)<=4.)
        if not self.goal_candidates:
            raise BotFault('Hospital event is outside the mapped supporting floor')
        self.goal_node = None
        posture = tuple(native.get('status_' + p) for p in ('CRAWL', 'SQUAT', 'STAND'))
        self.memory = ObstacleMemory(path.with_name(path.stem + '-obstacles.json'),
            self.nav.identity, [10010, 30, 0, sequence, self.goal_floor, self.root_offset,
                                'scripted_stand_forward_v1', *posture], time.time())
        self.route, self.index, self.replans = [], 0, 0
        self.plan(self.player(native))

    @staticmethod
    def player(native):
        point = [native.get('player_' + axis) for axis in 'xyz']
        if any(type(v) not in (int, float) or not math.isfinite(v) for v in point):
            raise BotFault('Hospital player position is unavailable')
        return point

    def observe_posture(self, native):
        if self.posture_offsets is None:
            return
        active = [p for p in ('CRAWL', 'SQUAT', 'STAND')
                  if native.get('status_' + p) is True]
        if len(active) != 1 or active[0] not in self.posture_offsets:
            raise BotFault('Hospital route lost its calibrated native posture')
        self.root_offset = self.posture_offsets[active[0]]

    def plan(self, player):
        # The actor root is above the floor and can be nearer an isolated bed
        # node. Select the witnessed supporting floor, never root height.
        start = self.nav.nearest([player[0], player[1] - self.root_offset, player[2]],
                                 max_distance=2., max_height=.75)
        blocked=self.memory.active(time.time())
        # The observed corridor's closest event node belongs to a separate
        # hallway island. The next closest candidate is reachable through the
        # ward's real portal edges. Distance alone cannot choose the endpoint.
        for _, goal in self.goal_candidates:
            try:
                route=self.nav.path(start,goal,blocked=blocked)
            except NoConnectedNativeRoute:
                continue
            self.goal_node,self.route,self.index=goal,route,0
            return
        raise NoConnectedNativeRoute('No connected hospital event candidate on the mapped floor')

    def target(self, player):
        while self.index < len(self.route):
            point = self.nav.positions[self.route[self.index]]
            if (math.hypot(point[0] - player[0], point[2] - player[2]) >= .45
                    or abs(point[1] - (player[1] - self.root_offset)) > .35):
                return point
            self.index += 1
        # A nearest node can sit outside a small event box. Approach the
        # authored center after following every real graph edge.
        return self.goal

    def inside(self, player):
        return all(abs(player[i] - self.center[i]) < self.size[i] / 2 for i in range(3))

    def obstructed(self, player):
        if self.index == 0 or self.index >= len(self.route) or self.replans >= 2:
            raise BotFault('Hospital event approach blocked; no further known edge to replan')
        edge = self.route[self.index - 1], self.route[self.index]
        self.memory.remember(*edge, time.time())
        self.replans += 1
        self.plan(player)
        return edge

    def evidence(self):
        return {'event_name': self.event, 'map_identity': self.nav.identity,
                'support_y': self.floor, 'node_count': len(self.route),
                'goal_support_y': self.goal_floor, 'root_to_floor_offset': self.root_offset,
                'goal_node': self.goal_node, 'goal_candidates':len(self.goal_candidates),
                'node_index': self.index, 'replans': self.replans,
                'predicted_player_clearance': 'unknown',
                'route': [list(self.nav.positions[n]) for n in self.route]}
