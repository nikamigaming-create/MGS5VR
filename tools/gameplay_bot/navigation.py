"""Owned NAV2 graph import, floor-aware A*, and native-feedback route policy.

Format reference: https://github.com/oldbanana12/Nav2. This reader never edits
the game or NAV2 files. NPC navigation is a planning prior; live player motion
must still validate each route. Asset data and generated maps stay private.
"""
from __future__ import annotations

import hashlib
import heapq
import json
import math
import re
import struct
from collections import defaultdict
from pathlib import Path

from .core import BotFault, atomic_json


class NavigationAtlas:
    """Select hash-checked owned tiles by native location, never by proximity."""
    def __init__(self, manifest, worlds, *, base=Path('.')):
        self.worlds = {row['key']: row['code'] for row in worlds['locations']}
        self.tiles = defaultdict(list)
        self.unknown = []
        files = {}
        for entry in manifest:
            asset = entry.get('path', '').replace('\\', '/')
            if not asset.lower().endswith('.nav2'):
                continue
            match = re.search(r'/location/([^/]+)/', '/' + asset.lstrip('/'), re.I)
            key = match[1].lower() if match else None
            if key not in self.worlds:
                self.unknown.append({'path': asset, 'reason': 'Native location identity unresolved'})
                continue
            path = Path(entry['file'])
            path = (path if path.is_absolute() else base / path).resolve()
            digest = entry.get('sha256')
            if not isinstance(digest, str) or not re.fullmatch('[0-9a-f]{64}', digest):
                raise BotFault('Owned navigation tile needs a SHA-256 identity')
            identity = (self.worlds[key], digest)
            if path in files and files[path] != identity:
                raise BotFault('One navigation file is assigned to conflicting worlds/revisions')
            if path not in files:
                self.tiles[self.worlds[key]].append({**entry, 'file': str(path)})
                files[path] = identity

    def select(self, location):
        if type(location) is not int:
            raise BotFault('Fresh native location code is required for navigation')
        entries = self.tiles.get(location, [])
        if not entries:
            raise BotFault(f'No imported navigation tiles for native location {location}')
        for entry in entries:
            path = Path(entry['file'])
            if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != entry['sha256']:
                raise BotFault('Owned navigation asset changed or is missing: ' + str(path))
        return NativeMap.from_files([entry['file'] for entry in entries])

    def summary(self):
        return {'discovery_complete': False, 'full_map_acceptance': False,
                'locations': [{'code': code, 'key': key, 'tiles': len(self.tiles.get(code, [])),
                               'coverage': 'partial_import' if self.tiles.get(code) else 'not_imported',
                               'player_clearance': 'unproven'} for key, code in self.worlds.items()],
                'unresolved_tiles': len(self.unknown)}


def read_nav2(path):
    data = Path(path).read_bytes()
    def unpack(fmt, offset):
        size = struct.calcsize(fmt)
        if offset < 0 or offset + size > len(data):
            raise BotFault("NAV2 record lies outside the owned file")
        return struct.unpack_from(fmt, data, offset)
    magic, length, entry, count = unpack('<4I', 0)
    origin = unpack('<3f', 32)
    divisors = unpack('<3H', 68)
    if magic != 201403242 or length != len(data) or not 0 < count <= 64 or not all(divisors):
        raise BotFault("Unsupported or truncated NAV2 header")
    if not all(math.isfinite(v) for v in origin):
        raise BotFault("Nonfinite NAV2 origin")
    groups = []
    for _ in range(count):
        kind, _, advance, payload, group, _, _ = unpack('<HHIIBBH', entry)
        if advance < 16 or entry + advance > len(data):
            raise BotFault("Invalid NAV2 section extent")
        if kind == 0:
            base = entry + payload
            offsets = unpack('<9I', base)
            node_count, edge_count = unpack('<2H', base + 36)
            flag_count, = unpack('<H', base + 46)
            nodes = []
            for i in range(node_count):
                point = unpack('<3H', base + offsets[0] + i * 6)
                nodes.append(tuple(origin[j] + point[j] / divisors[j] for j in range(3)))
            edges, cross_group, boundaries = [], [], []
            for i in range(node_count):
                start, segment, degree, portals = unpack('<HHBB', base + offsets[1] + i * 6)
                for j in range(degree):
                    target, index = unpack('<2H', base + offsets[3] + start * 2 + j * 4)
                    if target >= node_count or index >= edge_count:
                        raise BotFault("NAV2 adjacency references an absent node/edge")
                    weight, flag, _, _ = unpack('<HHBB', base + offsets[2] + index * 6)
                    if flag >= flag_count:
                        raise BotFault("NAV2 edge references absent flags")
                    flags, = unpack('<H', base + offsets[6] + flag * 2)
                    edges.append({'from': i, 'to': target, 'weight': weight, 'flags': flags})
                # Type B records are the engine's explicit connections between
                # coincident nodes in different segments of this same tile.
                if segment >= flag_count:
                    raise BotFault("NAV2 node references an absent segment")
                flags, = unpack('<H', base + offsets[6] + segment * 2)
                links = unpack('<' + str(portals) + 'H', base + offsets[3] + start * 2 + degree * 4)
                if portals == 1:
                    target = links[0]
                    if target >= node_count or math.dist(nodes[i], nodes[target]) > .002:
                        raise BotFault("NAV2 segment portal endpoints disagree")
                    edges.append({'from': i, 'to': target, 'weight': 0, 'flags': flags, 'portal': True})
                elif portals == 2:
                    cross_group.append({'from': i, 'group': links[0], 'to': links[1]})
                elif portals == 4:
                    # External tile encoding is retained. Only an exactly
                    # matching boundary endpoint in another file may join it.
                    boundaries.append({'node': i, 'raw': links})
                elif portals:
                    raise BotFault("Unsupported NAV2 portal encoding")
            groups.append({'id': group, 'nodes': nodes, 'edges': edges,
                           'cross_group': cross_group, 'boundaries': boundaries})
        entry += advance
    if not groups:
        raise BotFault("NAV2 file contains no navigation graph")
    return {'source': str(Path(path).resolve()), 'sha256': hashlib.sha256(data).hexdigest(),
            'groups': groups}


class NativeMap:
    def __init__(self, tiles):
        self.positions, self.adj, self.sources = [], defaultdict(dict), []
        self.unresolved_portals = []
        portals = defaultdict(list)
        for tile in tiles:
            self.sources.append({'source': tile['source'], 'sha256': tile['sha256']})
            bases = {}
            for group in tile['groups']:
                base = len(self.positions)
                bases[group['id']] = (base, len(group['nodes']))
                boundary_ids = {b['node'] for b in group.get('boundaries', [])}
                for point in group['nodes']:
                    if len(point) != 3 or not all(math.isfinite(v) for v in point):
                        raise BotFault("Invalid native map position")
                    node = len(self.positions)
                    self.positions.append(tuple(point))
                    # Exact millimetre agreement, including height. Never join
                    # nearest X/Z nodes across a cliff or a missing tile.
                    key = tuple(round(v, 3) for v in point)
                    if node - base in boundary_ids:
                        for other, source in portals[key]:
                            if source != tile['source']:
                                distance = math.dist(point, self.positions[other])
                                self.adj[node][other] = (distance, None)
                                self.adj[other][node] = (distance, None)
                        portals[key].append((node, tile['source']))
                for edge in group['edges']:
                    a, b = base + edge['from'], base + edge['to']
                    if not base <= a < len(self.positions) or not base <= b < len(self.positions):
                        raise BotFault("Invalid native map edge")
                    # Euclidean length is a lower bound. Retain native flags
                    # without inventing posture semantics for undecoded bits.
                    self.adj[a][b] = (math.dist(self.positions[a], self.positions[b]), edge['flags'])
            for group in tile['groups']:
                for link in group.get('cross_group', []):
                    if link['group'] not in bases or not 0 <= link['to'] < bases[link['group']][1]:
                        # Split NAV2s may refer to a group in another file.
                        # Preserve the missing link; it is never traversable.
                        self.unresolved_portals.append({'source': tile['source'], **link})
                        continue
                    a = bases[group['id']][0] + link['from']
                    b = bases[link['group']][0] + link['to']
                    distance = math.dist(self.positions[a], self.positions[b])
                    if distance > .002:
                        raise BotFault("NAV2 group portal heights/positions disagree")
                    self.adj[a][b] = (distance, None)
        self.identity = hashlib.sha256('\n'.join(sorted(s['sha256'] for s in self.sources)).encode()).hexdigest()
        if not self.positions:
            raise BotFault("No owned navigation tiles")

    @classmethod
    def from_files(cls, paths):
        return cls([read_nav2(p) for p in sorted(set(map(Path, paths)))])

    def nearest(self, position, *, max_distance=4., max_height=2.5):
        candidates = ((math.dist(position, p), i) for i, p in enumerate(self.positions)
                      if abs(position[1] - p[1]) <= max_height)
        distance, node = min(candidates, default=(math.inf, None))
        if node is None or distance > max_distance:
            raise BotFault("Player/goal is outside the mapped supporting floor")
        return node

    def path(self, start, goal, *, blocked=(), threats=()):
        blocked = set(map(tuple, blocked))
        positions = self.positions
        if not 0 <= start < len(positions) or not 0 <= goal < len(positions):
            raise BotFault("Unknown route endpoint")
        # A threat cost encourages cover/avoidance without pretending that
        # distance alone proves enemy line of sight.
        risk = {}
        def penalty(node):
            if node not in risk:
                p = positions[node]
                risk[node] = sum(max(0., 1. - math.dist(p, t) / 65.) ** 2 * 12.
                                 for t in threats if abs(p[1] - t[1]) < 12.)
            return risk[node]
        queue = [(math.dist(positions[start], positions[goal]), 0., start)]
        costs, previous = {start: 0.}, {}
        while queue:
            _, cost, node = heapq.heappop(queue)
            if cost != costs[node]:
                continue
            if node == goal:
                route = [node]
                while node in previous:
                    node = previous[node]
                    route.append(node)
                return list(reversed(route))
            for target, (distance, _) in self.adj[node].items():
                if (node, target) in blocked:
                    continue
                candidate = cost + distance * (1. + penalty(target))
                if candidate < costs.get(target, math.inf):
                    costs[target], previous[target] = candidate, node
                    heapq.heappush(queue, (candidate + math.dist(positions[target], positions[goal]), candidate, target))
        raise BotFault("No connected native route after current obstacles/threats")


def arrived(point, target, radius=1.4, max_height=2.5):
    """Use the same supporting-floor tolerance as nearest(), including slopes."""
    return (abs(point[1]-target[1]) <= max_height and
            math.hypot(point[0]-target[0], point[2]-target[2]) < radius)


class RouteProgress:
    """Detect lack of progress toward a node; sideways jitter is not progress."""
    def __init__(self):
        self.target = None
        self.best = math.inf
        self.last_progress = 0.

    def stalled(self, target, distance, now):
        if target != self.target or distance < self.best - .35:
            self.target, self.best, self.last_progress = target, distance, now
        return now - self.last_progress > 2.


def movement_mode(native, enemies, previous_life=None, previous_mode=None):
    """Choose a physical posture/speed from observed danger, never fire blind."""
    point = tuple(native['player_' + axis] for axis in 'xyz')
    life = native['player_life']
    if life <= 0:
        return 'dead'
    if previous_life is not None and life < previous_life - 100:
        return 'retreat'
    distance = min((math.dist(point, e) for e in enemies if abs(point[1] - e[1]) < 12.), default=math.inf)
    if native.get('not_alert') is False:
        return 'evade'
    # Separate entry/exit distances keep a walking patrol from making the bot
    # stand and crouch repeatedly at a single distance threshold.
    crawl_limit = 40 if previous_mode == 'crawl' else 30
    crouch_limit = 75 if previous_mode in ('crawl','crouch') else 65
    if native.get('not_alert') is not True or distance < crawl_limit:
        return 'crawl'
    if distance < crouch_limit:
        return 'crouch'
    return 'run'


def posture(native):
    for mode, status in (('crawl', 'CRAWL'), ('crouch', 'SQUAT'), ('stand', 'STAND')):
        if native.get('status_' + status) is True:
            return mode
    raise BotFault("Native player posture is unavailable")


def stance_press(current, desired):
    if current == desired:
        return None
    return .85 if desired == 'crawl' else .12


class ObstacleMemory:
    """Scene/map-scoped collision memory; dynamic blockers expire."""
    def __init__(self, path, map_identity, mission, now):
        self.path, self.map_identity, self.mission = path, map_identity, list(mission)
        self.entries = {}
        if path and path.exists():
            data = json.loads(path.read_text())
            if data.get('map_identity') == map_identity and data.get('mission') == self.mission:
                for item in data.get('edges', []):
                    a,b = item['edge']
                    if type(a) is int and type(b) is int and a >= 0 and b >= 0 and item['expires'] > now:
                        self.entries[a,b] = item['expires']

    def active(self, now):
        self.entries = {edge:expiry for edge,expiry in self.entries.items() if expiry > now}
        return set(self.entries)

    def remember(self, a, b, now):
        self.entries[a,b] = self.entries[b,a] = now + 300.
        if self.path:
            atomic_json(self.path, {'map_identity':self.map_identity,'mission':self.mission,
                'edges':[{'edge':list(edge),'expires':expiry} for edge,expiry in sorted(self.entries.items())]})
