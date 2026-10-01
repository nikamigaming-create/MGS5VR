# Map and state routing for full-game VR tests

The bot needs connected spatial and behavioral models. A spatial route reaches
a place. A state route reaches a menu, native role, interaction, mission step or
recovery state. Both planners use observed native outcomes and retain unknown
connections. Full-game discovery and acceptance are incomplete.

## Working code

`NavigationAtlas` in `tools/gameplay_bot/navigation.py` groups owned NAV2 files
by native location code and validates their hashes before loading the selected
world. `gameplay-navigate.py` selects the map from a fresh native observation.
The route admits stable on-foot VR control before recentering or movement and
rejects a location/mission change. It uses the existing floor-aware A*, native
posture acknowledgments, collision feedback, bounded leases and obstacle memory.
Tiles from different locations cannot be joined by coincident coordinates.
NPC navigation remains a planning prior; player clearance requires actual walking.

`StateGraph` in `tools/gameplay_bot/state_graph.py` builds a directed graph of
state obligations and guarded transition recipes. It uses A* with a zero
heuristic until an admissible tighter estimate is known. An edge declares entry,
semantic VR actions, actual expected outcome, source recipe hash and cost.
Plan identity also includes the runner's source hash, executable, DLL and both
effective personal INIs, so a code or configuration change invalidates its evidence.
Disabled personal bindings, unresolved state identities, missing recipes and
progression actions without an isolated fixture stay blocked.

The planner can choose an uncovered reachable edge with a declared return to
the neutral entry state. It produces a proposal for the existing supervised
runner. Runtime execution requires fresh final-eye review, fresh native guards
and an observed outcome for each step. A failed step stops its dependents;
delivery of an input is never accepted as completion. Plans do not dispatch input.

`observation_key` records mission/location/sequence, menu/control ownership,
native travel role, demo/tutorial/result status, progression and posture.
Missing exact menu page/parent/focus or mounted role is retained explicitly.
Completeness of those recorded dimensions does not mean the entire game's state
has been discovered. Newly encountered authored states and branches must extend
the model rather than being forced into the nearest known label.

## One map to review

```powershell
python tools/workspace.py model --run artifacts/bot/runs/20260930T215049265217Z
python tools/workspace.py coverage --menus --run artifacts/bot/runs/20260930T215049265217Z
```

The fixed view is `artifacts/dev/coverage/game-model.html`; its data is
`game-model.json`. Filter/select a state and inspect its incoming/outgoing
connections, guards and blockers. Green edges mean authored probes are available,
not that gameplay has passed. The separate menu view shares the same folder.
Generation is read-only with respect to the game and reuses existing owned files;
it creates no distribution, copied retail asset tree or new dated map folders.

The planner is also connected to the maintained native VR runner. A selected
ready transition is compiled into a guarded roundtrip, entered through the
existing physical Continue path, executed on one connection and cleaned up:

```powershell
python tools/workspace.py launch-sim
python tools/workspace.py bot --command state --transition probe.idroid.open --seconds 15
python tools/workspace.py stop-sim
```

The default bot remains observation. Pause requires an explicitly selected
`probe.pause.open` or `probe.pause.back`; it is not automatically selected by
the runner. A missing/disabled return route blocks dispatch before Continue.
Each transition has fresh entry/outcome predicates, captures and input release;
dependents stop after the first failure. This initial executable graph covers
coarse owner probes, not every menu page or native mode.

The factual owned inventory declares 13 native location codes and 78 mission-pack
identifiers, including the 62 campaign-list identifiers. Reserved, nonplayable
and progression/variant eligibility remain questions, so these are not counts of
accepted maps or completed missions. The model also retains the 35 native-context
obligations and the menu/state/presentation queue. Its coarse control-owner
probes are separate from exact native mode or menu-page recognition.

The current imported spatial data is **40 Afghanistan tiles, 44,171 nodes**.
It is regional Mission 6 data. The other declared location entries have no
imported graph in the current manifest; some can also require a different game
adapter. Unresolved portals remain blocked. No whole-map acceptance is claimed.

The authored sequence inventory now indexes the owned helicopter/ACC common
source and Mission 6 source: 35 named states and 30 directed literal target
mentions, with unresolved helper ownership and dynamic targets retained.
The indexer ignores comments/string contents and keeps branch conditions,
eligibility and semantic VR recipes unresolved. These authored identifiers
extend the graph, but none is a traversable test edge yet. In particular,
`Seq_Game_WeaponCustomize` also handles the native helicopter/vehicle selector;
its name alone does not identify which customization target is active.
The scenario query reads the authored `startCustomizeTarget` separately. State
observations require that target and exact UI page identity in customization,
even if the iDroid/menu open flags are absent. Its native readback still needs
the pending ACC run; an unavailable target remains missing.

`tools/index-authored-sequences.py` regenerates this factual metadata from an
explicit private source manifest. It reads existing owned files in place and
exports source hashes, identifiers and literal target references, without
copying retail script bodies. Source indexing does not execute Lua or unlock
a test edge.

## Finish the model against the actual game

1. Import/index the remaining owned navigation tiles by location/content revision,
   then exercise native collision, doors, stairs, bridges and streaming boundaries.
   Keep actor/posture capabilities and dynamic obstruction evidence with the route.
2. Import each eligible mission's authored sequence names, gates, objectives,
   transitions and recovery branches. The identifier inventory is only the entry
   list; it is not an authored mission state machine.
3. Publish exact native UI page, parent, focus, legal choices and input-lock state,
   plus native mounted/seat/weapon role and cover state. Current broad owner flags
   cannot safely drive an entire menu tree.
4. Add guarded VR recipes for enter, interact, confirm, cancel, close/reopen,
   fail, retry and return. Use isolated verified save/checkpoint fixtures for
   upgrades, purchases, deployment, abort/retry and progression changes.
5. Run each available transition, retain its exact build/config/source identity,
   and review both final eyes through motion and transitions. Complete a separate
   physical-headset pass for reading, controller usability and comfort.

The first expanded native loop is field -> iDroid tabs -> Mother Base/development
branches -> cancel/Back -> stow/reopen -> field control. ACC customization and
deployment/return, forced tutorials, cover, mounted weapons and mission
clear/death/retry follow as separate native states. Tests preserve physical
weapons/optics/binoculars, left-arm ordinary HUD and right-hand iDroid; native
integration stays behind VR interactions. Steam stays running and no desktop or
window-control automation is part of this model.

The helicopter upgrade lock-up was reported by another player in the ACC.
The field Mission 6 list/Back baseline does not reproduce it. ACC development
through iDroid and ACC helicopter customization remain separate candidate paths
until the reported screen is identified. A verified isolated save/checkpoint
fixture is required before exercising purchases or progression changes.
