# Native navigation bot validation

The test bot reads the legally owned game's NAV2 graph and live soldier state.
It plans with A* and moves through ordinary configured VR inputs. It never sets
player position, mission progress, enemy state, or save data.

The private Mission 6 manifest includes 40 NAV2 tiles and 44,171 nodes. The map
PNG shows elevation-colored native edges, a planned route, and an actual enemy
snapshot containing 56 soldiers. The bounded image does not display every soldier.
This is regional coverage, not a claim to have mapped the entire game.

The parser bounds-checks native tables, preserves undecoded flags, and joins
only explicit matching portals. Unresolved links never make unknown space
walkable. Node positions and ordinary edges matched 32 exports from the
independent [reference parser](https://github.com/oldbanana12/Nav2Parser).
The [NAV2 research](https://github.com/oldbanana12/Nav2) documents the format.

A* preserves height and directed connectivity, penalizes nearby enemies, and
avoids observed blocked edges. Collision memory is scoped to map identity,
mission, and location, with a five-minute expiry. Actual player movement gives
collision feedback; an NPC edge alone does not prove player clearance.
The executor uses acknowledged standing/crouching/crawling, posture hysteresis,
bounded movement leases, sprint, and retreat over visited nodes after damage.
It stops on unsafe context changes and preserves failed-run evidence even when
the operator fails. General combat/shooting behavior is not validated.

Thirteen navigation tests cover height separation, explicit portals, disconnected
space, obstacle replanning/expiry, enemy costs, stance acknowledgement, danger
policy, parser rejection, slope arrival, stalled-node detection despite jitter,
and evidence preservation after transport failure.
Live traversal and cinematic acceptance are recorded separately in
RELEASE_VALIDATION_2026-09-28.md. These tests and the PNG do not prove completion.

`tools/gameplay-navigate.py` requires a private hash-checked asset manifest,
an installed game, and the regular simulator operator. Retail navigation data,
reference-parser sources, and private extraction outputs are not release assets.
