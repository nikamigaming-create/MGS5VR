# Current work and test build

The release goal is playable TPP from start to finish, with reliable physical
weapons, optics and binoculars, the existing left-arm weapon/status display and
left-arm HUD popups, and the left-hand iDroid requested on October 4. Pause uses a spatial stereo
panel. Floating firearm aiming overlays stay off. No desktop/input automation
or Steam shutdown is needed by the local workflow.

## October 14 simulator-upgrade continuation

The user closed the first fresh MGSV session for an upgrade; its clean exit
before bot input is not a game-crash reproduction. Meta XR Simulator `v207.0`
was replaced by `v207.1`. The ignored local test configuration now names the
installed `v207.1` manifest. Windows' 64-bit physical runtime remains Meta
Quest Link. Source checkpoint `857a6d2` is pushed to the development branch;
the published `play/` pin has not advanced.

Retained `20261014T235242773390Z` genuinely reloaded mission `10100` at
`[1102.845,147.337,-1452.049]`, in `Seq_Game_BeforeRescueBoy`, with the adult
objective still clear and zero children rescued. Both arrival eyes show the
field and connected hands/sleeves. This proves that objective and a field
entry survived the restart; it does not verify reloading at the previous
test's final `[1387.221,129.985,-1491.565]` pose.

The navigation importer dropped explicit native boundary links when two
coincident endpoints rounded to different millimetres. The owned rescue-area
pair differs in Y by `0.000376142` metres. The importer now requires reciprocal
native tile/group references, the observed external-record sentinel, unique
endpoints in both directions and at most 2 mm of full 3D disagreement. It
does not connect nearby unreferenced floors. All 44 navigation checks pass.
The Africa audit resolves 27,068 of 27,452 external records; 384 external and
2,049 split-group records remain non-traversable. Retained preflights
`20261015T000344933438Z` and `20261015T000651416555Z` still refuse a route into
the rescue rooms before dispatch. Their remaining disconnected topology is
not proof that the native player's cave entrance is inaccessible.

Retained route `20261015T000942570512Z` stopped in native wall cover at the
outer fence. A read-only status inventory established `PlayerStatus.BEHIND`,
rather than a completed fence vault. The observer now records that registered
status. Retained `20261015T001517562219Z` used the existing VR stance control
to crouch in cover; `20261015T001619118256Z` used the existing VR dive control
to leave it, with real `BEHIND=false`, `NORMAL_ACTION=true` and native
displacement. Both final eye pairs were reviewed. The subsequent 220-node
route `20261015T001718034754Z` reached the mine approach, and the 51-node
walking route `20261015T001946741109Z` reached
`[1397.139,130.729,-1516.690]`; both latter final eyes were reviewed.

Owned locator arrays and native `Fox.StrCode32` joins identify the actual
door pivots: the ordinary cave gate `...vrtn002_gim_n0002` is at
`[1398.026,129.933,-1513.923]`; the rescue demo gate
`...vrtn001_gim_n0000` is at `[1428.311,126.500,-1519.492]`; the inner boy gate
`...vrtn002_gim_n0001` is at `[1419.915,124.977,-1537.309]`.
The native lock query reports the ordinary gate unlocked and both rescue
gates locked. The owned arrays, path hashes and read-only receipts remain
private. The earlier enemy-check centre and nav-volume centres are distinct
from these locator pivots.

Retained `20261015T002405232994Z` reviewed the ordinary gate in both eyes.
Retained `20261015T002707870529Z` made the bounded VR approach;
`20261015T002940905389Z` then stopped on native collision with no movement
progress. Retained `20261015T003057771431Z` delivered VR Interact while facing
it, but both final eyes still show a closed gate. Its passed head/input
predicate is **not** a door-opening or rescue pass. Native alert remained
active through the prone and released observation runs
`20261015T003524422930Z` and `20261015T003651397104Z`; the latter timed out.

Retained combat `20261015T004131426704Z` selected exact living native guard
`sol_diamond_0005` (1043). The corrected paired rifle motion established
attached support, 7.15 cm barrel lateral error and a 2.84-degree view angle
before a real two-round discharge (`PRIMARY_HIP` 30 to 28). Native actor life
then became 2; buddy contribution is not excluded. This proves the bounded
paired-motion/discharge case, not exclusive hit attribution. Both aim-probe
eyes were reviewed: the prone body/shoulder obstructs much of the left eye
when firing back across the native body heading. Physical sights, body
visibility and complete rifle acceptance remain open.

**Continuing work:** resolve the native cave approach and alert conditions,
then perform the five rescues and the genuine results-to-cutscene handoff.
No child rescue, mission result page or whole-mission acceptance has occurred.
The runtime used above is still DLL `0daabcf78b03`; a required development
build/check of the new tooling is pending. Full-game and physical Quest
acceptance remain open. The older recovery checkpoint below retains its
original scope and is superseded by these exact reload and field observations.

## October 14 recovery checkpoint

The recovered source and subsequent tooling checkpoints through `2dcc960` are
now on `origin/codex/october-11-patch`, with the remote commit verified against
local Git. The checked development build at that commit passed all 56 CTest
groups and installer checks. `play/` remains the published October 11 payload
and unchanged manifest. Both slots verify; the obsolete-build inventory is
empty. Local receipts are in `artifacts/dev/one-zero-current/`, including
`recovery-off-machine-verification.json` and the checked-slot verification.

Mission 18 continuation on DLL `0daabcf78b03`: the actual mission list showed
18: Blood Runs Deep, and both the native pending sortie and loaded field code
are `10100`. The earlier `10091` assignment incorrectly treated a registry
index as the displayed episode. Its regression obligation is now attached to
`10100`, and the observer reads the native pending sortie code directly.

Retained `20261014T203801017996Z` exposed the complete native loadout, characters
and authored grey preview in all ten reviewed settled eye images. The prior
stretched cabin preview is resolved in this static repeat; excess vertical
canvas padding, head motion and complete transition review remain open.
Retained `20261014T204314877782Z` and `20261014T204434101140Z` selected Commence
Mission and ASAP through VR Confirm. Native loading confirmation in
`20261014T204606976112Z` genuinely started mission `10100`; its immediate pair
caught the authored episode-title fade and is not a field visual pass.
Retained `20261014T205019447458Z` used effective VR Interact at the helicopter
exit prompt and returned to real on-foot `Seq_Game_BeforeRescueBoy`, vehicle
ID `65535`, normal action and a saved checkpoint. Both after eyes were reviewed
with visible terrain and connected hands/sleeves. Objectives, moving-body and
optics interactions, and the reported results handoff remain pending.

The first field route stopped before dispatch because the reader rejected
17 hash-checked Africa NAV2 tiles. The owned corpus contains fifteen exact
112-byte empty split tiles and two nonempty `201403240` graphs with the same
bounded NavWorld layout. Development now admits those records; empty tiles
preserve provenance and supply no nodes or edges. Unknown date codes and
unverified nonempty `201403241` layouts still fail. All 725 Africa tiles now
parse, with 1,371,861 nodes and 4,331,494 explicit edges; 2,049 unresolved
portals remain non-traversable. All 41 navigation checks pass, including
directed topology, malformed records and empty-tile refusal to create routes.
The actual field approach repeats below use that exact map identity.

The first parsed route then aborted on its starting node because the path
search's elapsed time had consumed the movement-stall deadline before its
first pulse could be renewed. The executor now refreshes the live owner/pose
after planning. Multi-page enemy observations run with input released and
exclude only that neutral interval from the stall clock; actual lack of
directed progress still expires. All 41 navigation checks pass, including
world changes during slow planning and refusal to treat neutral time or
lateral jitter as progress. Retained `20261014T211108904110Z` and
`20261014T211559960702Z` made real waypoint progress but stopped at their
bounded deadlines; neither is a completed-route pass. The next retained leg,
`20261014T214505813998Z`, reached Bampapeve and correctly refused a crawl turn
whose native body did not align. Retained walking repeat
`20261014T215041909121Z` completed the native route to within two metres of
adult target `sol_target_0000` (1079); both final eyes show the camp, connected
hands/sleeves, and the native Fulton/carry prompts.

At that approach the target was asleep (native life 3). Before the selected
Fulton case could dispatch, the native target became dead (life 1) and
`mission18_adult_target_clear` became true. Retained `20261014T215257985790Z`
therefore failed its entry predicate with **no action dispatched**. This is
native adult-objective progress, not a successful VR extraction test, and its
cause has not been attributed to a particular shot or ally. All five children
remain unrescued.

The ranged rifle bot probe `20261014T213148377165Z` correctly refused a trigger
when its actual barrel oscillated away from the selected target. The bot now
prepares the firing hand and measured authored support contact from the same
publication and rotates both together, rather than copying a socket already
constrained by the old left hand after moving only the right. Native VR rig,
bindings, barrel/visibility gates and collision stay intact. Optional combat
`--target` names one exact living native soldier and cannot substitute a nearer
guard. All 47 combat checks pass. The closer repeats
`20261014T214835818220Z` and `20261014T215523448502Z` found their selected
actors already incapacitated and dispatched no trigger; live acceptance of
the paired-motion correction remains pending.

The native enemy-check centre near the rescue door is
`[1427.132,126.410,-1519.600]`; it is **not a measured door transform**.
Retained `20261014T215800913605Z` refused that endpoint before input because
the imported directed graph has no connected route to it. A supported exterior
approach is reachable. Retained `20261014T220145559410Z` traversed 506 waypoints
before releasing input during a native action at an outer fence; ordinary
foot controls subsequently returned without further input. Retained
`20261014T221033508427Z` completed the next 210-waypoint route to the mine
approach, with both final eyes reviewed. All five exact native children
`hos_diamond_0000` through `0004` were present and alive (IDs 2048 through 2052).

Retained `20261014T221744229814Z` published the requested bounded headset turn
while keeping both controller poses in LOCAL space; both final eyes show the
rock face between this approach and the hostage area. The earlier look case
`20261014T221547694439Z` had identical entry/exit predicates and was correctly
rejected before pose dispatch. The corrected repeat requires the measured
head rotation. No rescue-door action, child extraction, or result-page pass
has occurred. The current navigation import is a partial planning prior, not
proof that the game's entrance is unreachable.

Before cleanup, the last native state was mission `10100`,
`Seq_Game_BeforeRescueBoy`, position `[1387.221,129.985,-1491.565]`, adult
objective true, zero children rescued, and `gvars.mis_checkPoint == 0`.
No reload at that field position has been verified. The exact stopped state
is retained in `mission18-recovery-final-native.json`; source-eye recordings
are distinct from the reviewed final compositor stills. Cleanup closed the
owned MGSV and simulator processes with zero sampled buttons/axes/triggers
and no open Pause/iDroid menu. Steam stayed running and Windows' 64-bit
OpenXR runtime remains Meta Quest Link. The cleanup log is
`mission18-recovery-session-cleanup.log`.

The 1.0 source work survives the interrupted `Find mod download count` task.
The published October 11 source is commit `87e51f2`, still present on
`origin/main` after a fresh fetch. Later implementation, tests, licensed
controller/font assets, icons and acceptance documents had remained
uncommitted; the recovery checkpoint on `codex/october-11-patch` preserves
those changes in Git. Saves, owned game data and retained test evidence remain
local in their existing private/ignored trees. The stray
`cinematic-transfer-eye/` diagnostic directory is now ignored and preserved.

At recovery, development DLL `0daabcf78b03` matched the working source and
installed test game. Its October 14 build passed all 56 CTest groups and
installer checks. `play/` still matches the published `experimental-2026-10-11`
pin, DLL `3e6e3d6e9818`; `Play.cmd` restores that release, and `Play-Dev.cmd`
selects checked development. No publication or pin promotion occurred.

**Resume point:** reopen the native Mission 18 saved entry, then locate the
actual rescue-door approach around the observed rock face. The last test's
adult objective was complete, but its field state is not a verified reload
checkpoint. The five rescues and reported results-to-cutscene handoff remain
pending. Sortie canvas padding/motion and the paired-rifle bot correction
still need their own repeats. The interrupted thread's retained
run, `20261014T192109259237Z`, opened the left-hand
iDroid in the Africa ACC (`40020`, story 33). Its action was observed, but
visual review is pending and cleanup failed with `Observed action prerequisite
changed`; the run remains failed and does not verify deployment. Recovery
closed the exact recorded test session through `workspace.py stop-sim`,
retaining Steam and restoring the per-game Meta runtime.

The genuine fresh campaign is preserved at mission 1 checkpoint `2448458239`.
The prologue reached results and mission 1 under requested damage protection;
normal-difficulty completion remains open. Stationary mounted body views were
reviewed on `92014e6012e5`; moving, larger leans, outfits and vehicle/cabin
bodies remain open. Mission 1's handheld iDroid tutorial repeat also remains
failed. [VR_1_0_POLISH.md](VR_1_0_POLISH.md) carries the full acceptance scope,
and [MISSION_ACCEPTANCE.json](MISSION_ACCEPTANCE.json) records all 51 required
missions with zero full-mission acceptance passes. Physical Quest acceptance
remains pending. These are the continuing 1.0 obligations.

October 14 mission coverage: `MISSION_ACCEPTANCE.json` records all 51 released
TPP mission codes from the live native registry, excluding its 11 unavailable
entries. Mission 18 (`10100`) has an explicit results/input/cutscene regression
obligation. No entire mission or physical-headset pass is inferred from this
inventory.

Checked DLL `580e0afd8ff9` reproduced native score and Heroism in both eyes over
the retained stereo surround. Retained `20261014T172413728112Z` visibly advanced
Score to Heroism with VR Confirm; `20261014T172531428385Z` exited to actual
`Seq_Demo_Bridge2`/clear state 4, although its immediate capture hit the authored
black fade. `20261014T172634443183Z` observed native head/viewer movement in that
scene; its forward eye pair is reviewed, with the authored dark/blurred world.
The native cassette reward and vehicle report remained visible. Confirm in
`20261014T174149617727Z` genuinely reached mission `10020`, story 1 and its
loading confirmation. No desktop focus or progression writes were used.

Native mission 1 Resume reached `Seq_Demo_ArrivalInAfghanistan`, then genuine
horseback tutorials and `Seq_Game_RescueMiller`, checkpoint `2448458239`.
The loading case's optional two-second attention capture rejected an authored
fade before its outcome deadline. The bounded observer now retains such a
noninteractive, neutral authored fade and continues the same deadline without
replaying input; its 296 bot checks pass. The iDroid tutorial-open repeat
`20261014T175039775796Z` failed its expected handheld outcome and remains
unaccepted, despite later native tutorial progression.

Mounted inspection `20261014T175744358588Z` reproduced absent rider legs with
visible tracked hands/sleeves. Its down-view eye pair is reviewed; a camera prop
also appears as a floating grey box. Development now admits an original-skin
horse body with native saddle/pelvis/leg animation, limited spine/arm solving
and the existing 2 cm eye bound. It does not apply on-foot grounding or head
turning to the rider's lower body. This change still needs the actual-game
repeat; vehicle/cabin bodies and the camera prop remain separate open defects.
The owned test session was closed with Steam retained and no test menu open.

Checked DLL `92014e6012e5` genuinely reloaded the saved mission 1 checkpoint.
Mounted run `20261014T180946420591Z` retained an admitted body in four down/side/
8 cm lean phases, with at most 8 mm anatomical eye displacement. All eight
eye images were reviewed: visible native poncho/body and connected hands/sleeves;
the earlier right-side camera fallback no longer floats there. Moving, larger
leans, other outfits, vehicles and cabin bodies remain unaccepted.

The existing `helicopter-rewards-user-20261011` profile has native story 33 and
all 51 released missions open, including `10100`. It was loaded for direct
Mission 18 testing; the genuine fresh mission 1 profile remains preserved.
Retained VR menu runs `20261014T182648000323Z`, `20261014T183415809591Z`,
`20261014T183709804442Z` and `20261014T183922775024Z` closed the real Choose a
Rival Help, selected the visibly confirmed 18: Blood Runs Deep and accepted it.
Native landing-zone confirmation reached `Seq_Game_MissionPreparation_SelectItem`.
Settled both-eye run `20261014T184533245258Z` exposed the loadout UI stretched
into the cabin and a wrong backdrop. Development now classifies those exact
native sortie states as a complete native spatial canvas, retaining active
3D scene ownership and the established handheld iDroid. Actual sortie repeat,
Mission 18 objectives and its reported results handoff remain pending.

## October 13 fresh campaign and body motion

October 14 continuation: actual native traversal cleared the lobby rubble and
the QTE in retained run `20261014T125836680908Z`, then naturally reached the
ambulance, outside and `Seq_Game_VolginRide` checkpoint `1787647407`. The chase
driver was aborting on an unspent shot and lowering the weapon between pulls.
Retaining its physical grip through the initial gun handoff and between pulls
produced twelve observed native discharges and shell reloads in recorded run
`20261014T144339620170Z`; native shot logs retain `barrel_dot=1`. That attempt
died and does not accept the chase. Development adds the original native gun
aim target as a read-only observer and honors requested God mode only in the
actual hospital combat ride. Hospital lesson damage and retail sequence/event
ownership stay intact. Current chase completion and normal-difficulty aiming
remain pending. Earlier temporary hospital scripted-failure suppression was
removed; it is not active in this session. Published `play/` remains unchanged.

On checked DLL `37e63eff3eae`, the requested native damage protection kept
Snake alive and the game naturally entered `Seq_Demo_Bridge`, advanced story
to 1, established Cyprus clear type 11 and selected next mission 10020. It
then remained in native `MISSION_GAME_END` with loaded result resources and
an ended scene while VR still classified the screen as a stale cinematic.
The captured eye was black. Development now recognizes the native end UI
from its public phase, definite clear and loaded result queries, retaining
active 3D and recorded cinematic ownership. This is a reproduced transition
defect; its actual-game repeat and the next mission remain unaccepted.

The live original-renderer comparison in `20261014T165310133151Z` exposed the
actual Heroism result page and its A: Exit Results control. Immersive VR was
restored; the comparison's final hidden-editor inspection hit a null rendered
publication and remained failed. The following genuine VR Confirm in
`20261014T165539771392Z` advanced native clear state to `SHOW_CREDIT_END` and
`Seq_Demo_Bridge2`; its old held-phase assertion failed after that real change.
Retained run `20261014T165921013669Z` observed the subsequent native 3D scene
with independent head/spectator motion and video; visual review remains pending.
Development now sends native mission-end canvases through the same retained
stereo-world/native-screen path used by other non-world surfaces, leaving
immersed 3D scenes under their existing owner. The current repeat is pending.

The hospital bot now reads hash-checked owned NAV2 topology and trigger bounds
for floor-aware A* routes. It follows every intermediate node, measures progress
toward the current node, and remembers failed directed edges for five minutes
before replanning. Player roots above the floor no longer select isolated bed
nodes. Sparse nodes beyond a trigger are allowed when the actual route crosses
its volume. The stair route uses the witnessed hospital root-to-floor offsets
and native edges between both floors. Ordinary VR stick input remains the only
movement actuator; collision and actor transforms are unchanged.

The user explicitly requested God mode for campaign debugging. The personal
setting is now On. The existing release excludes hospital damage protection,
so this live test also uses a temporary hospital-only damage owner and scripted
failure guard. These are debug runs, not normal-difficulty or release acceptance.
Run `20261014T000909465393Z` entered the actual soldier encounter and genuinely
saved checkpoint `2024168408`; run `20261014T001736179650Z` reached the Stairway
cinematic. Run `20261014T010001480002Z` traversed both stair flights and entered
`Seq_Demo_Corridor0`; both captured eyes were reviewed with visible actors and
world. It naturally continued into `Seq_Game_Corridor1`. The owned Corridor1
callback requires a VR movement cue; run `20261014T010536551461Z` started
`Seq_Demo_Corridor1`, but its immediate capture was a black fade, so that case
remains failed. Outside, horse/car handoff and subsequent reload checks remain
pending. The normal-difficulty searchlight defect remains open.

The saved hospital campaign was genuinely reloaded through VR Continue, and
the stair traversal passed again in retained run `20261014T021057852109Z`,
saving checkpoint `2981112761`. The Corridor1 movement cue passed in
`20261014T021444473871Z`. Ended corridor scenes return real player control
while their sequence names still start with `Seq_Demo_`; the bot now reads
that actual control state and the native `canSkip` gate. Retained runs
`20261014T025807605154Z` and `20261014T030425482881Z` advanced segments 1--6
through the effective VR movement stick. The final curtain scenes completed
naturally and saved checkpoint `2401495818`, reaching
`Seq_Game_AfterCurtainRoom`. Only the Corridor2 after-eye pair has been reviewed
in this newer segment chain; other visual acceptance remains pending.

The curtain exit is traversed. The native graph's closest Volgin event node
was disconnected; the planner now selects the closest reachable candidate via
existing directed portals. Steering joins the published camera to its associated
HMD pose. Posture changes update root-to-floor offsets; dense waypoints use a
45 cm arrival radius and allow two bounded crawl-turn recoveries per waypoint.
These changes preserve native collision, actor transforms and sequence ownership.

Retained run `20261014T043316454905Z` crawled 7.48 metres from the ward corner,
respected the native movement lock and entered the corpse-corridor trap. Its
35-second scene expectation expired before the authored execution finished, so
that run remains failed. `Seq_Demo_VolginVsSkullSoldier` then began naturally.
Retained run `20261014T043517994527Z` verified head turns, physical lean and 39 cm
of viewer movement with zero native actor axes. All twelve phase eye images were
reviewed, with visible world, soldiers and bodies. The 7.37-second source-eye
recording has a phone-sized H.264/AAC copy at
`artifacts/dev/one-zero-current/hospital-scene-mobile.mp4`; it is a scene sample,
not the requested complete showcase. Native foreground depth of field remains.
The scene completed naturally and saved checkpoint `2088951606` in
`Seq_Game_AfterCorridor`. Retained run `20261014T043936430296Z` healed the leg
through the effective VR interact binding; native `mvars.cureFinished` also
confirms the healing animation's completion.

Retained run `20261014T045110569874Z` reproduced dropped VR stance in the native
stationary standing mode. Development DLL `2eba47c8d6b7` retains on-foot
stance/run/dive mapping there; the native game still owns its action masks.
The 56 build checks and installer checks pass. Exact stationary-mode live
retest remains pending. Ordinary crouch passed in scratch run
`20261014T045954642733Z`, whose artifacts expired before retention; it is not
retained acceptance evidence.
Saved checkpoint reload passed through the native VR title/loading confirmations
in scratch run `20261014T045752515606Z`; its artifacts expired before retention,
so it is not retained acceptance evidence. The saved hospital disclosure requires a stable
owner before its single A pulse; a premature pulse in `20261014T045540041508Z`
was ignored and that run correctly failed.

The post-corridor gate and both stair flights are traversed. Native inspection
confirmed the authored closed-gate collision was already disabled; geometry
was not changed. Retained `20261014T052057883866Z` passed the real gate opening.
Retained `20261014T053935637105Z` aligned at the lower doorway's outer NAV2 node
before crossing its jamb. Its later gun expectation failed at an NPC pause;
that run establishes only the observed doorway movement.

Retained `20261014T054111417785Z` followed eighteen connected lower-floor nodes
and entered `Seq_Demo_GetGun` through 9.91 metres of actual movement. Its after
capture caught the native black fade, so the case remains failed. The scene
finished naturally in `Seq_Game_GunTutorial`, checkpoint `3803578635`.
Retained `20261014T054813791197Z` passed physical pistol readiness; both held
eye images were reviewed, with visible world, hands and authored gun.
`20261014T055227482161Z` established three hand-aimed native discharges, seven
rounds to four. Its prop-hit observer was wrong: `extinguisherFinished` is set
by the next route trap, not proof of hitting the prop. That case remains failed;
the diagnostic now calls this field `hospital_gun_route_unlocked`, and the
shot helper explicitly reports discharge only. Extinguisher collision remains
unverified.

Retained `20261014T055922644267Z` followed the pistol-to-entrance route and passed
`Seq_Demo_BeforeEntrance`; both after eyes were reviewed. The following entrance
scene saved checkpoint `214801881`. Its dedicated cinematic test started after
that scene had ended and correctly failed. The entrance hides Ishmael during
sneaking, so owned-route navigation now reads the mission movement lock without
requiring an NPC it is not following. The native main-hall stair route reached
the bottom but cut across a railing. Retained `20261014T060721684177Z` passed the
short player route beyond it. `20261014T060809243978Z` then passed the connected
main-floor route into `Seq_Demo_HijackAmbulance`.

Retained `20261014T060911680453Z` passed head turns, 12 cm lean and 38.47 cm of
viewer movement during the ambulance ambush, with zero native actor axes. All
twelve phase eye images were reviewed: the world, floating child and soldiers
remain visible. Its 13.8-second source-eye recording is a progress sample,
not the complete showcase or physical-headset acceptance.

The ambush genuinely reached `Seq_Game_VolginInEntrance`, checkpoint
`990761331`. The next evade prompt requires walking into `trap_HeliRotor`;
waiting for it was an operator error. Volgin completed his fatal grab while
the operator inspected data. Debug damage protection retained 6000 health but
did not prevent the native death animation, so subsequent movement failed.
The bot now reads that native fatal-attack flag and refuses movement rather
than mistaking protected health for a usable actor. The owned temporary guard
was removed and the session closed for a genuine checkpoint reload. Its six
blocked GunTutorial failures mean these runs are campaign debugging only.
Normal-difficulty prologue, evade, hospital exit, car/horse handoff, subsequent
reloads, all missions and physical Quest acceptance remain open. The published
play pin remains unchanged.

The field body correction on DLL `ed5ce9e88d7b` passed 96 fixed-hand head-turn
samples across standing and prone, with no rejected body and a maximum eye
component error of 0.733 mm. All sixteen head-turn eye images and all sixteen
standing/crouched/prone/lean/walk eye images in retained run
`20261013T144526899192Z` were reviewed. Prone torso heading now follows the
native crawl heading. The fixed-neck solve advances the rendered pelvis by
only the minimum required clearance, bounded to 12 cm, while retaining the
original foot targets. Unexpected overhead gear in the rightward prone view
still needs its own correction. This is one field body/outfit, not complete
body or physical-headset acceptance.

The user explicitly authorizes accepting first-launch agreements and using
disposable campaign saves. The six original save files remain backed up;
continued test progress need not be rolled back between sessions. Genuine
cleared-save startup exposed native mission 10010 during first-launch Terms,
a 3D opening while retail TitleMode remains set, and a hospital title menu
that bypasses the saved-game helicopter widget. The guarded startup reader
now handles these exact native owners through the effective VR bindings.
Retained run `20261013T154044028662Z` reached the first hospital title on DLL
`e0cd10465c37`; both actual START GAME menu eyes in
`20261013T154757255671Z` were reviewed. Retained run
`20261013T154848295896Z` confirmed START GAME through VR and entered the actual
hospital campaign without writing story or sequence state.

All twelve acted-scene eye images in `20261013T154923145244Z` were reviewed:
the doctor and world remain visible during head turns, a 12 cm physical lean
and 41 cm of viewer movement, with zero native actor-control axes. The bed
view also exposed foreground geometry clipping. Looking upward in retained
run `20261013T160157618627Z` reached `Seq_Game_FewDaysLater1` but left the
retail look history at zero and did not advance the lesson. This is a failure,
not prologue completion. The locally owned lesson bytecode confirms the native
right-stick history and greater-than-0.5 direction threshold.

The two-second physical head-look gesture passed in the actual hospital on
DLL `cc2182251e28` and again on `0bc5c0cb8ddf`. Retained run
`20261013T171822548115Z` entered `Seq_Demo_FewDaysLater2` through the retail
lesson outcome, with no story or sequence writes. All eight eye images were
reviewed. Near-head concealment still did not remove the foreground obstruction;
that visible defect remains open.

The native name/birthday form previously lost its authored scene owner after
the instruction popup closed. Candidate `0bc5c0cb8ddf` preserves that exact
hospital profile owner, its native body animation and its spatial menu.
Both form eyes in retained run `20261013T172106652105Z` were reviewed. Selecting
Enter Name through VR in `20261013T172621139128Z` opened the native text editor,
but its expected popup observation was wrong; that run is not a completed
name-entry pass. The native form still requires text input.

Development candidate `cc52a0345ef4` entered AHAB through the controller-operated
keyboard and the verified retail Fox character/down/up path. Retained runs
`20261013T182456338863Z` and `20261013T182559257520Z` show five balanced down/up
events and ten conversions; both after-eye pairs were reviewed. Native profile
confirmation then advanced into the one-week-later scene and the actual
`Seq_Game_AvatarEdit` checkpoint. Keyboard occlusion remained a visible failure.

Candidate `3056df4c8304` moves that keyboard into a final compositor layer with
matching physical hit targets and submission/ownership guards. Its new keyboard
visibility repeat is pending. The original avatar preview was reproduced as a
broken narrow strip in retained run `20261013T183946188177Z`. Preserving the
native editor's own camera restores its complete preset grid and live head
preview. Retained runs `20261013T185355708367Z` and `20261013T190044728082Z`
show Resume Game and VR preset Confirm working; both after-eye pairs were
reviewed. The editor still had a black surround and an oversized portrait canvas.

Checked candidate `1043b4c09aa0` separates a clear-scene stereo backdrop from
live fade frames, blurs that retained backdrop, and crops the native centered
16:9 editor canvas without stretching it. Retained editor runs
`20261013T192526145968Z`, `20261013T192928857966Z` and
`20261013T193232823297Z` show the full preview, physical controls and anchored
panel in both eyes. The retained bed image stays head-locked during head turns;
that surround is still a visible failure. Avatar confirmation legitimately
advanced through Quiet's appearance/exit to `Seq_Game_EscapeFromAwakeRoom`.
The named Quiet demo was incorrectly classified as ordinary gameplay because
retail marks it playable. The classifier now preserves named acted scenes;
its actual Quiet repeat remains pending.

Candidate `b8fe59e6bbd9` fixes the reported hospital hair/bandage obstruction.
The owned hospital body has an integrated head, restored by each successful
body publication; the separate `msk0_main0_cov` face cover has no head group.
The admitted skin now conceals the integrated head after restoring the body,
and the exact owned face cover is included in modular concealment. Only normal
draw membership changes; native shadow bits, torso and arms are retained.
Genuine saved-game Resume in retained run `20261013T195650118957Z` returned
directly to the hospital crawl with a clear view in both eyes. All ten after-eye
images in `20261013T200102842571Z` were reviewed across opposing 22-degree head
turns, looking 40 degrees down, an 8 cm lean and return. Both movement eyes in
`20261013T200348377788Z` were reviewed after 35.1 cm of native crawl displacement
through the effective physical movement axis. The obstruction stays absent,
and the body and head shadow remain visible. The bounded runner now recognizes
this exact hospital crawl lesson despite retail `NORMAL_ACTION` being false;
other incapacitated owners remain refused. The receipt is
`artifacts/dev/one-zero-current/head-obstruction-after/verification.json`.

The campaign has since genuinely advanced through `Seq_Demo_Heli` and
`Seq_Demo_Volgin` to `Seq_Game_AfterVolginDemo`, checkpoint `2474680955`.
The runner's new crawl helper originally scheduled asynchronous stick input
and released it immediately; it now holds and observes the requested interval.
The hospital rail executor reads the retail rail state, follows the actual
native body heading at turns, carries the simulated hands with that body turn,
renews independently expiring input, measures progress, and releases on scene
handoff. Two bounded released recoveries precede a reported obstruction; it
never disables collision, changes the rail, writes transforms, or opens Pause.
Retained run `20261013T204225226670Z` advanced 8.0 metres in 20 seconds with no
recovery. Its expected cutscene name was wrong, so the case remains failed;
the actual next scene was Volgin. Both captured cutscene eyes were reviewed:
Snake's complete native actor and Ishmael are visible in the stereo corridor.
The prologue is still incomplete. The rail runner now admits the witnessed
retail crouch/stand states as well as crawling, and releases movement while
reacquiring a delayed same-owner rig. Retained run `20261013T211230936459Z`
advanced another 8.98 metres in 16.34 seconds with zero recoveries, naturally
entered `Seq_Demo_Cure`, and passed the native scene handoff case. Both final
eyes were reviewed: Snake and Ishmael remain visible in the hospital world.
An earlier turn in `20261013T210353598527Z` stalled rig publication and produced
camera reason 8; movement was released. That renderer interruption remains an
acceptance gap even though the next run resumed and the bot now has bounded
released recovery. The locomotion suite has 44 passing checks, including real
hold duration, early cinematic release, full-interval observation and rail
posture/delayed-publication guards.

Retained Cure viewing run `20261013T211340127914Z` completed head turns,
12 cm physical lean and independent viewer input with neutral native actor
axes/buttons. All twelve final-eye images were reviewed. Actors and world
remain visible; authored depth of field blurs the nearer actors in several
phases, so this is not crisp-image or complete cinematic acceptance.
The campaign genuinely saved checkpoint `1550134470` and resumed into
`Seq_Game_EnterSmokeRoom`. Reviewing captures while that live scene was idle
led to native game-over; genuine VR Continue/Start Mission recovered the
checkpoint. Recovery and motion are now queued together.

The owned smoke-room sequence explicitly requires CRAWL, and the effective
VR stance hold passed the real prone transition in subsequent runs. Direct
steering to Ishmael was obstructed. A bounded corridor waypoint in
`20261013T214003067799Z` passed the observed entrance and approached the real
Ishmael actor, but Snake returned to crouching and movement stopped. The next
run `20261013T214408685866Z` tried two bounded VR prone requests and stopped
for review without completing that native handoff. These smoke-room runs
remain failed; the route and stance/mask handoff are not accepted. The bot
records an obstruction and releases for replanning instead of repeatedly
renewing the same free-movement edge. No collision, actor transform, story
or sequence state was changed by these executors.

The supplied `02_controller_rig.patch` targets an older controller rig. Current
development already retains complete grip/aim presentation pairs through idle
tracking loss and blends recovery for 100 ms, scoped to owner/model/camera and
OpenXR reference/focus epochs. Raw tracking remains separate from presentation.
Run `20261013T204722318319Z` correctly refused to arm without the explicit
diagnostic session flag. The subsequent retained run `20261013T210220765734Z`
on DLL `b8fe59e6bbd9` injected five seconds of right grip/aim raw tracking loss
in the actual hospital. Both cases passed. Forty-five observations over a
continuous 6.047-second interval included loss and recovery; every sample kept
first person active, suspension off, camera reason zero, the right palm tracked
for presentation, and body rejection zero. Raw grip/aim tracking recovered.
Both eyes during loss and after recovery were reviewed with a connected right
hand/forearm and visible world. See
`artifacts/dev/one-zero-current/right-controller-loss-verification.json`.
This stationary simulator diagnostic does not verify physical Oculus controller
inactivity, locomotion during loss, or other modes. The supplied patch has not
been applied wholesale; existing production tracking behavior is preserved.

The subsequent hospital run recovered the owned `s10010_sequence.fox2` trigger
bounds and imported both native Cyprus stage navigation tiles. Following
Ishmael directly was the wrong route: it led into a counter or an occupied
scripted detection region. Native floor nodes now guide the side-room doorway.
Run `20261013T224525256939Z` genuinely entered `Seq_Demo_UnderBed`; the old
executor rejected a legitimate scene change during fresh-rig acquisition.
That handoff is now checked again after acquisition, and scripted movement
locks keep inputs released during a bounded wait. The focused locomotion suite
has 47 passing checks. These Python changes do not alter production hand tracking.

The effective VR crawl stick advanced the real `Seq_Game_UnderBed` cue to
`Seq_Demo_UnderBed2` in `20261013T225101556830Z`. Its immediate capture was a
black fade and is not visual acceptance. The native campaign then saved
checkpoint `2498716581` and entered `Seq_Game_EscapeFromSmokeRoom`.
Its initial `INTRUDE`/`SUBJECT` crawl constrains heading. Run
`20261013T225829620190Z` passed a bounded physical diagonal crawl with actual
native displacement. Subsequent backward VR movement in `20261013T230348359204Z`
cleared the bed constraint, and native movement in `20261013T230709674225Z`
reached the helicopter encounter and saved checkpoint `4085983624`.
The rest of the hospital remains pending. Raised bed-top navigation nodes must not be used as
the supporting floor for this crawl. No actor, collision, story or sequence
state was written. Failed exploratory routes remain failed.

A new player report describes VR input ceasing after Mission 18, Blood Runs
Deep: repeated desktop clicks are needed before VR A can advance successive
results screens. This specific results/loading/cutscene input handoff still
requires an actual Mission 18 reproduction. The delivered-XInput audit can
distinguish continued XR polling from a stalled native poll or incorrect
action owner. No desktop focus workaround has been added.

All 56 automated groups and installer checks passed for this runtime candidate.
Recorded-movie presentation, the head-locked editor surround, the compositor
keyboard repeat, full prologue/mission coverage, remaining body variants and
physical Quest acceptance stay open. Both local launch slots remain available;
the published `play/` pin is unchanged.

## October 12 headset controls editor

Development adds a context-aware spatial controller editor. Hold Menu + left
thumbstick click for 650 ms, select an action, record a physical button/chord or
stick axis, and confirm the proposal. The live native mode selects the action
list; other modes can be inspected explicitly. Licensed 3D Quest controller
drawings highlight assigned, pressed and touched inputs. Parser conflicts,
external-edit protection, neutral live application and the closing input gate
keep the working personal layout intact. Read [the controls](HEADSET_CONTROLS_EDITOR.md).
Retained simulator run `20261013T004804322796Z` verifies ACC opening, both-eye
head movement, physical chord recording, cancellation, confirmed live remapping,
conflict rejection, closing and reopening, and automatic binocular mode. The
saved binocular binding equips the physical prop after closing the panel.
Driving was inspected explicitly from ACC; that is not a native driving pass.
The final checked-DLL and cleanup receipt is kept in
`artifacts/dev/controls-editor-current/verification.json`. Physical-headset fit
and broader mode/mission acceptance remain open. The published play pin stays unchanged.

## October 12 camera gallery, Freeze and ACC grip routing

Development adds physical rear Back, Next, Print, Delete, Live and Freeze
controls to the camera. The latest presentation uses small arrows, printer,
trash, camera and pause/play icons, keeping the existing press targets and
hover feedback. Delete asks first with check/X confirmation buttons.
The saved-photo library is separate from physical prints: deletion preserves
every printed copy and its own caption, drawing and placement. Notes have no
digital history; Trash asks before permanently disposing of the current object.
The shared library supports the launch cabin, native ACC and ordinary fields,
including Mother Base. Read [the camera controls](CABIN_STUDIO.md).

Freeze registers only the mod's `MGS5VR_Camera` native world pause. A previously
accepted native camera/player anchor is retained while current OpenXR eyes and
native tracked skin continue publishing. The lens and held props consume that
same completed skin. Resume/stow release the pause; focus/tracking/scene loss
also cancel the request. Ordinary photos leave the world live. The title cabin
does not offer world Freeze. No stock Pause menu is opened by these controls.

The reported ACC right-grip leak was reproduced as native aim trigger 255.
Ordinary ACC grip now consumes stock aim/fire triggers while the physical VR
binocular equip, hand aim, zoom, acquisition, explicit mark and stow paths are
preserved. The dropped camera's repeated room conversion also accumulated
quaternion drift and prevented later pickup; stored rigid transforms now retain
unit rotations.

Retained actual-game runs cover title gallery/deletion (`20261012T225613720658Z`),
fresh title reload (`20261012T230333328503Z`), ACC Freeze (`20261012T230738812387Z`)
and ACC gallery/note disposal (`20261012T230818831460Z`). On DLL `137e40b62b47`,
field mission 10195 (`20261012T231500469722Z`) held native clock 76684 unchanged
through opposing head/camera motion and a photo; Resume advanced 76688 to 76700.
Mother Base 30050 (`20261012T231848654031Z`) passed live photos, browse/reprint,
Confirm/Cancel, digital deletion preserving prints, and physical field-print
pickup/stow. Both eyes of those motion/control/print checkpoints were reviewed.
Fresh native ACC reload (`20261012T232347216217Z`) restored the exact five saved
photos and eleven physical prints without reviving deleted photos or notes.
Final ACC run `20261012T232525121439Z` on that DLL held right grip with zero
native aim/fire triggers and stock BINOCLE false, then equipped/stowed the
physical VR binoculars. Both-eye stowed and equipped pairs were reviewed.

The earlier word-button phone video is `artifacts/dev/camera-manager-current/MGS5VR-Camera-Phone.mp4`:
22.152 seconds, 720 x 1280, 76 actual right-eye frames in two labeled takes.
Every source frame was reviewed. Capture cadence was 3.4--3.5 fps; the silent
portrait edit keeps original timing without cropping or interpolating gameplay.
This does not certify high-rate motion, physical headset ergonomics, every
mission or whole-game acceptance. `build/dev/` is the checked candidate for
`Play-Dev.cmd`; the published `play/` pin stays unchanged. The final receipt is
`artifacts/dev/camera-manager-current/verification.json`.

## October 12 physical cabin props and hand contact

Development replaces the floating cassette rack/radio with six physical tapes
on the native brown seat and insertion/ejection at the helicopter's existing
embedded player. The separate cutscene player is no longer drawn as a duplicate.
Tape weight/contact, casing labels, pickup, drop/recovery and native Continue
are implemented. The embedded lid's native animation binding remains unknown.
Read [the workbench controls and limits](CABIN_STUDIO.md).

The reported detached camera/paper hand presentation was reproduced. New held
props followed the raw controller after the anatomical arm had limited its
reach or contacted a surface. Camera, marker, papers and held tapes now join
the solved native palms in the same skin publication. Camera contact uses an
authored side socket; paper uses an edge grip and appropriate finger curl.
Native firearm wrist aiming no longer overrides a held studio prop. A separate
quad triangulation error caused half-missing box faces and was corrected.
The native left bionic iDroid and left forearm HUD retain their existing fit.

The mod-authored instant camera uses its actual lens for a separate native
scene pass, live finder and PNG/physical print; its shutter consumes firearm
input. Notes support Permanent Marker captions, spatial keyboard and freehand
marker strokes. Papers attach/detach on the measured native photo wall and
persist with their photos, captions and drawings outside MGSV save files.
Props use verified depth from the matching native eye camera/projection.
World enemy labels are masked by the physical binocular housing; the native
acquired marks and reticle remain in the actual optic.

Retained workbench run `20261012T191009165360Z` on DLL `7cf6b1b01747` passed all
16 functional phases, with both camera and board eyes reviewed. Later DLL
`369a855abe1c` enlarges short captions. Its fresh-launch board run
`20261012T193243872218Z` restored the exact pinned note/drawing/Polaroid in both
eyes. Field run `20261012T193635692919Z` passed native Continue, zoom/acquire,
clear with no automatic return mark, explicit moving-target re-mark and a real
field photograph with no native weapon trigger. The clear/return/zoom/mark and
camera pairs were reviewed. Run `20261012T193959514809Z` additionally reviewed
left-hand iDroid contact at two reachable poses, ordinary stow and immediate
reopen in both eyes. These are scoped simulator checks, not physical headset,
every wrist angle, every mission or all foreground-occlusion acceptance.

The bot now approaches native furniture before reaching, refreshes the cabin
anchor after motion and preserves measured grip/aim pairs. Binocular captures
retain the sampled left support grip; the free hand rests below the camera
after stow. Earlier failed or unnatural test poses remain investigation data.
No production binocular or iDroid flow was replaced by stock gamepad operation.

The [owned item catalog](ITEM_CATALOG.md) indexes 15 asset archives and five
media files with zero scan errors: 780 equipment definitions, 139 item-folder
models and 416 animation resources. It retains variants, rigs, references and
scoped interaction observations, and exposes unverified behavior/clip bindings.
No native instant-camera model has been confirmed. The searchable private view
is `private/item-catalog/index.html`; it is not release game data.

The game and Config icons use the supplied Snake portrait with a VR headset,
exactly one VR (red V, ivory R) and a gear badge for Config. Matching icons are
embedded in the launchers and used by the local shortcuts. The desktop preview
remains 1280 Ã— 720. This simulator uses its 1680 Ã— 1760 eye recommendation and
runtime FOVs; its enclosing native canvas is 2368 Ã— 2576. Historical 2520 Ã— 2640
checks below describe earlier settings, not a headset requirement.

`build/dev/` contains the checked candidate for `Play-Dev.cmd`; `play/` remains
the last published `experimental-2026-10-11` payload for `Play.cmd`. No release
or play-pin promotion is authorized by this development work. The final local
verification receipt is `artifacts/dev/world-contact-current/verification.json`.

## October 11 field iDroid and combat preferences

D-Dog's cabin disappearance is corrected on checked development DLL
`3e6e3d6e9818`. The actual native fade writer used the stock camera position
instead of the rendered tracked cabin view; iDroid bypassed that fade. The
correction supplies the tracked camera point to the native dog query. Five
closed/open/stowed/turn/lean checkpoints retained opacity 1.0 and were reviewed
in both compositor eyes. The same scene also reproduced on the published pin.
Read [the correction and evidence](CABIN_DOG_VISIBILITY_FIX.md).

Checked development (recorded in `build/dev/BUILD.json`) adds launcher toggles for god mode and acquired
enemy world markers, and retains the close-cover firing preference (Off by
default). The launcher passed 217 checks across all 59 settings and 104 actions.
Actual native enemy fire left health at 7,801 with god mode On and reduced it to
7,330 after Off. Marker On/Off/On and head-motion outcomes were reviewed in both
eyes with no native world recon silhouette shader. A stable native obstruction
blocked the VR pistol trigger with cover blocking On (10 rounds unchanged); Off
allowed the shot (10 â†’ 9). These are scoped simulator checks.
Combat and page-loop takes used `e1dc46d11bc7`; the later candidate retains
those implementations and adds an exact handset pad-owner diagnostic. Its build passed
55/55 automated groups (including actual shader marker pixels) and installer
checks.

Two field iDroid defects were reproduced before correction: opening the handheld
changed the head's orientation reference, and an owned pad-release block delayed
fresh movement after stow. Matched actual-game repeats demonstrate both fixes.
Left grip + right-stick left/right now sends LB/RB. Without left grip the left
stick retains movement; with grip it navigates menu lists. Left stick click and
the existing right-grip page loop are retained. Personal handheld mode stays selected.

Fresh ordinary field checks on `3e6e3d6e9818` passed current RIGHT-stick tabs,
the retained RIGHT-grip loop, focused LEFT-stick row navigation, 2.42 m of
LEFT-stick movement with the handset open and grip released, and stow/next-tap
reopening. The same physical binocular take passed A clear, look-away/return
without reacquisition and explicit RIGHT-trigger re-marking. The smaller world
label and all 14 outcome pairs were reviewed in both eyes. Evidence is in
`artifacts/dev/idroid-stick-dog-current/markers-tabs-final/`.

The intermittent reported checkpoint hang has not been reproduced on a matching
save. The source clip ends during loading. Native failure cleanup closed the
owned handset, but the isolated failure panel was black and retry did not enter
loading; native failure-menu/retry acceptance remains open. An aggregate native
pad-protection diagnostic was mistaken for the mod's registration; the extra
handoff change was removed and an exact owner readout added. Read
[the changes, tests and limits](FIELD_IDROID_COMBAT_FIXES.md). Physical headset
acceptance and remaining full-game cases stay open. The published pin is unchanged.

## October 11 NVG obstruction fix

The reported practically blind NVG view was reproduced at safe offline Mother
Base, mission 30050. With native NVG equipped, its own goggles model filled both
eyes with black/yellow geometry. The green night-vision effect was already
working. Earlier picker captures never equipped NVG and do not qualify as this
reproduction.

Development DLL `02b9d914dd8c` hides only the normal and rain-cover goggles
resources in the current character's typed native item interface before draw
preparation. Native equip/on/off state and the night-vision filter are retained.
Ordinary hands, the bionic forearm, iDroid and physical binoculars retain their
existing behavior. Visibility restoration uses the existing owned-model lease.

The normal NVG model passed actual-game simulator checks in both eyes: clear
view while on, yaw Â±45 degrees, pitch Â±35 degrees, positional lean, native None
selection to turn off, and immediate native re-equip. The retained 35-second
composited-eye video contains 102 actual frames (about 2.89 capture fps); that is
capture cadence, not game frame rate. Both-eye images and the decoded video were
reviewed. The build passed 55/55 automated groups and installer checks. The safe
test uses already-owned native equipment through a bounded Lua fixture; it does
not certify the physical wrist picker's NVG slot, rain-weather presentation or
physical headset comfort. Read [the fix and evidence](NVG_VISIBILITY_FIX.md).

Your original six paired save files were restored and verified after testing.
The game/simulator is closed, Steam retained, wired Link's Meta OpenXR runtime
restored, and personal handheld iDroid mode remains selected. `play/` is unchanged.

## October 11 helicopter and Rewards corrections

The Rewards confirmation failure was reproduced using the retail bonus-popup
UI without granting a reward. Its separate bonus-popup owner was missing from
menu routing. Development now recognizes that native modal, presents it on the
existing stereo panel and routes the existing Confirm binding. On checked DLL
`da914b1531d7`, the settled native Rewards popup accepted A and dismissed; both
outcome eyes were reviewed. The later NVG build retains this correction.

The helicopter passenger camera now keeps its authored seated offset on the
native passenger root. Cabin movement is transported using the carrier pose
before HMD rotation, so turning or pitching the head does not rotate that walk
offset around the seat. Native outbound flight and grounded passenger recordings
exercised yaw, pitch, lean and cabin walking on `da914b1531d7`; both outcome eyes
were reviewed. Pickup/return and physical headset comfort remain open. Evidence
is retained in `artifacts/dev/helicopter-rewards-current/`.

D-Dog's initial bone-only observations did not establish visibility. The later
typed native opacity trace and both-eye closed/open/stowed repeat establish the
handheld cabin correction described above. The user's handheld mode is saved.

## October 11 wired Link runtime correction

The user's 08:48 Play-Dev launch selected SteamVR, confirmed by both its
launch receipt and the actual game's runtime log. Windows' global runtime
registration still pointed to SteamVR. Both local Play commands now explicitly
request Meta Quest Link through `-MetaLink`, so their default no longer depends
on that registration or a missing launcher preference. Explicit runtime
arguments remain available in the shared launcher.

At the user's request, Windows' 64-bit OpenXR default was changed to Meta
Quest Link with the validated installed Oculus runtime. A fresh native OpenXR
probe detected runtime `Oculus` and system `Meta Quest 3`, with a recommended
1872 x 2016 pixels per eye. This verifies runtime/headset discovery; physical
gameplay acceptance remains the user's upcoming test. Steam was retained, no
game was launched for this correction, and the published pin is unchanged.
Receipts are in `artifacts/dev/meta-openxr-default-elevated-result.json` and
`artifacts/dev/meta-headset-launch-recovery-before.json`.

## October 11 published pin and testing candidate

`play/` retains the last successfully published release and its matching
launcher. The current pin is `experimental-2026-10-11`, DLL `3e6e3d6e9818`;
its recorded archive SHA-256 matches the published GitHub asset. `Play.cmd`
verifies and restores that slot before launching with the saved Meta OpenXR
headset runtime. The published pin remains separate from October 12 workbench
and camera development; local tests never authorize replacing it.

`build/dev/` holds the latest completed, checked candidate recorded in its
`BUILD.json`, for testing with `Play-Dev.cmd`. Both launch paths are available
without changing the other slot. The launcher executable is included in each
slot. Development tests and simulator sessions do not advance the published
pin. Headset launch checks passed; physical headset acceptance remains open.

For an authorized release, package checked development while preserving the
existing published pin. After the requested server/GitHub publication succeeds
and its uploaded payload is verified, pin that exact candidate. Both slots
match at that point, then development can diverge again. A source push, draft
release or package preparation does not change the pin.

## October 11 controller mapping review

The complete launcher catalog passed 208 checks across 103 actions, 29 inputs,
10 contact inputs, 10 axes and eight contexts. The development automated suite
passed 55/55 groups and installer checks. Retained ACC runs
`20261011T134433036774Z` and `20261011T134534399362Z` passed two entry and 18
supervised cases, including LEFT-hand handheld iDroid motion, focus, Next and
Previous pages, stow and next-tap reopening. Both outcome eyes were reviewed.
Equipment/Commands ownership and binocular latch/stow are scoped ACC passes.

The narrated controller-mapping film, complete saved-action index and evidence
report are in `artifacts/dev/controller-mapping/`. The film uses actual browser
remapping, zoom boxes, arrows, side-by-side comparisons, wipes and a separately
labeled actual-game final-eye clip. Read
[the mapping review](CONTROLLER_MAPPING_REGRESSION.md) for reproduction and
remaining coverage. The runner recognizes the typed Online Information
Next/Close selector, with at most eight released confirmation edges. It still
rejects first-run agreements with their different native owner.

The test game/simulator is stopped, personal settings restored and `play/`
unchanged. Physical wired Meta Quest Link touch/headset acceptance, complete
missions and all native contexts/items/weapons/vehicles remain open. The
earlier rapid-close-during-reopening observation also remains open; the fresh
settled stow/reopen pass does not resolve that timing case.

## October 10.1 spatial Development hotfix

The hotfix target is `experimental-2026-10-10.1`. Its gameplay changes remove
the mod-owned spatial iDroid world pause and finish ordinary ACC native stow
before a first-tap reopen. The retail pause owners for actual lessons and the
explicit Pause menu keep their existing behavior.

The spatial freeze was reproduced on Mother Base with development DLL
`3a35173cfa9d`: short Back reached the native pad/UI but Helicopter Development
remained on the child grid. Unregistering only `MGS5VR_iDroid` immediately
restored Back; restoring its tutorial pause reproduced the failure. The mask
was stopping the child's model/exit work, not its rendered UI.

Working candidate DLL `278e218e1291`, built **2026-10-11 01:33:56 UTC**, passed
**55/55** automated groups and installer checks. Retained actual-game spatial
run `20261011T014251563347Z` checked Mother Base Weapons/Items sustained
scrolling, a weapon upgrade dialog and Cancel, Buddy and Helicopter scrolling
and Back, complete native stow and first-tap reopening. Run
`20261011T021354946660Z` checked the corresponding ACC grids and Back, a weapon
instant-completion dialog and Cancel, and suit/gear scrolling with the native
equip notice. No MB Coins were spent. The final ACC four-case run
`20261011T022616783621Z` passed stow and first-tap reopening twice and ended with
native SUBJECT false, iDroid/Pause closed and all inputs neutral. Both eyes
were reviewed through the stated outcomes. Final release binaries are rebuilt
from the isolated committed hotfix; their identity is recorded in BUILD.json.

The archived `regression-middle-20261005` fixture proved another save's ACC
path, but native story 33 and 454 developed items showed the same late-game
progression. It does **not** establish earlier-account or first Development
lesson coverage. The original six paired save files were restored and verified.
The owned simulator/game session is stopped; Steam remains running and the
physical-play runtime is restored to Meta OpenXR. The investigation receipt is
`artifacts/dev/acc-development-current/spatial-investigation.json`.

Headset, first-playthrough lessons, all transactions, continuous full missions,
free-roam Mother Base tab wrap and invisible vehicle long guns remain open.
The separate near-cover block toggle and expanded coverage tooling remain
working changes and are outside this hotfix. No whole-game certification is
claimed by scoped menu checks. Earlier build and pin statements below are
historical.

## October 10 headset-feedback implementation (before publication)

The development candidate, `0a0d5f0f26cf`, passes all 54 automated test groups
and installer checks (`driver-ready-freshness-build.log`). Retained run
`20261010T180016091187Z` passes all four cases: stationary jeep sidearm fire
(8 to 7 rounds, primary/back ammunition unchanged at 30/200, pedals neutral),
LEFT-hand +35-degree steering, both-hand -35-degree steering, and normal
exit using a 0.08-second Y tap. The steering sweeps deliver full native
positive/negative input. The player finishes on foot with every input released.
These are stationary driver firing and steering checks; moving-vehicle
handling, the R08 obstructed-barrel negative, tab-wrap acceptance and physical
headset acceptance remain open. The release is not certified.
Post-shot stereo seated views and the exit image were reviewed; continuous
held-gun visibility is not accepted by those captures. The owned simulator
session is stopped, Steam remains running, and both the per-game and saved
launcher runtime are restored to Meta OpenXR. The `6ac2` pin is unchanged.
The scoped receipt is `driver-final-acceptance-0a0d.json`.

An earlier shot on this DLL is preserved in `driver-sidearm-shot-0a0d.json`:
the selected sidearm fired from 10 to 9 while its fixture incorrectly expected
primary-rifle ammunition to decrease. That fixture remains failed; the final
8-to-7 sidearm case above passes with the correct selected-slot assertion.

The preceding candidate, `439294973d88`, also passed 54 automated test groups
and installer checks. On that DLL, retained run `20261010T172426703635Z`
passes four helicopter-flight cases: bare LEFT grip does not enter native
binocular mode, and physical binocular equip, LEFT support and stow work.
Both final eyes were reviewed: physical binoculars remain, without a second
stock overlay. Native Y exit reached the field in `20261010T172459733214Z`;
that case failed its obsolete landing-zone position guard, so only the exit
transition is accepted, not the whole fixture.

Retained run `20261010T172857273358Z` passes three cases: closing the native
Exit Tutorial notice, ordinary held Back for 1.2 seconds followed by immediate
iDroid reopening, and final stow/cleanup. Ammunition stayed at 30/200/10.
The preceding actual Help mode 9 stow in `20261010T172801794846Z` closed the
iDroid but presented that native notice; its outcome check failed until the
next run acknowledged the notice with A. This is observed native tutorial
behavior, not a complete pass for every forced guide.

On the same DLL, `20261010T173716923612Z` passes four primary-weapon cases:
selection/readiness, native reload, entering prone and clear-muzzle firing
while still prone (31 to 30 rounds). `20261010T174500064295Z` passes two
stationary wheel sweeps: LEFT hand at +35 degrees and both hands at -35
degrees deliver full positive/negative native steering, with neutral pedals
and a neutral release. These checks establish steering range/input delivery,
not moving-vehicle handling.

Jeep entry with a single 0.08-second Y tap passes in
`20261010T174323243118Z`; the subsequent driver-gun readiness case times out.
A follow-up correction in `0a0d5f0f26cf` refreshes the existing readiness
acknowledgement from the ongoing native weapon phase; the stationary sidearm
firing result above verifies that corrected path.

The R08 obstructed-barrel negative, moving-vehicle handling
and current-build tab-wrap acceptance remain pending. These scoped results
do not certify the release or physical-headset readiness. The preceding live
candidate, `165320977b11`, passed scoped simulator checks for Continue, prone
clear-muzzle fire, native jeep entry and LEFT/both-hand wheel ownership. On
that build, driver weapon eligibility acknowledged the request, but drawing
remained unfinished and the firing check did not consume ammunition.
Eligibility alone is not a visible or usable gun. Ordinary iDroid
stow also exposed a stale native Back packet, and tab cycling exposed a second
advance. The new build includes both corrections and fixes the disagreement
between initial/ongoing native driver eligibility that repeatedly cancelled
gun drawing, plus the separate native fire and pedal-history input paths.
The current ACC/deployment investigation and exact evidence are tracked in
`artifacts/dev/headset-feedback-current/requests.json`.

The current source contains exclusive LEFT-grip iDroid focus, neutral-stick
handoff between control owners, a held-firearm exception with separate native
UI trigger filtering, and one tab advance per squeeze with root-page wrap.
The native iDroid Previous prompt uses its effective dedicated chord rather
than claiming that the focus grip alone changes tabs.

Driving uses the physical wheel with a stronger 35-degree range and independent
LEFT/empty-RIGHT hand contacts. A right-grip squeeze stays assigned to its
chosen interaction until release. It no longer opens the equipment picker;
the separately configured optional picker retains its own exclusive input.
Personal-gun input now has a guarded native semantic adapter: RIGHT grip away
from the wheel requests the gun, the firing trigger leaves the pedals, and
LEFT stick vertical supplies acceleration/braking after centering. Rendering
and firing require a fresh native weapon-ready acknowledgement. Stationary
driver sidearm firing is verified above; this does not establish every weapon
or moving-vehicle handling.

Returning to the ACC provides an effective-binding iDroid/deployment hint on
the LEFT forearm. A positively identified native field-helicopter passenger
gets cabin movement carried with the live helicopter, native collision and
normal Y exit. Authored cutscenes retain their native ownership.

The launcher editor, lessons and four PNG guides use the same effective
binding export, including migration of the legacy driving grip. The guides
reuse the launcher's field-kit artwork and actual 3D controller models. They live
in `docs/control-guides/`; regenerate them after the combined development
build. They describe controls, not completed gameplay acceptance.

Implementation and verification state for this feedback batch is recorded in
`artifacts/dev/headset-feedback-current/requests.json`. The earlier scoped
simulator results below belong to their named DLLs. They do not validate
these newer changes or clear the prone/cover firing report. No pinned-build
promotion is implied by this work.

## October 10 physical feedback: firing denied near cover and prone

First priority from the user's wired Quest Link session: repeated denial dings
and the cannot-fire wrist symbol while the visible tracked gun appears clear,
especially prone or near objects. This is the existing R08 issue. It is a
physical user report; the new source has not yet passed the reported case.
The native pre-fire clearance query now follows the tracked gun barrel and
authored muzzle for the verified local firearm owner. Original physics,
clearance radius, asynchronous result consumption, ammo and projectile
collision remain in charge. `tracked_weapon_obstruction=0` restores the
legacy clearance origin. Details are in `docs/NATIVE_WEAPON_OBSTRUCTION.md`.
Required acceptance: a clear gun fires prone/around cover, while an obstructed
barrel remains obstructed, without a stance reset.
The physical session log and exact report are retained privately at
`artifacts/dev/headset-feedback-current/r08-prone-firing.json`.

## October 10 tutorial and frame-delivery fixes

Local physical play uses wired Quest Link through Meta OpenXR. Windows still
registers SteamVR globally, so MGS5VR now honors the user's saved
`HKCU\\Software\\Nikami\\MGS5VR\\Launcher\\HeadsetRuntime` first. Both root
Play commands use the shared launcher, preserving the pinned payload. The
per-game runtime file also selects Meta for direct game launches. Explicit
runtime arguments override the saved choice; an invalid saved choice fails
instead of silently choosing another provider. Launcher regression tests pass.
The physical game was launched with Meta and its loaded LibOVR runtime verified;
this confirms provider selection, not full physical-headset acceptance.

Earlier development DLL `bfa228f7bc04` fixes four reproduced defects. Help over
first-open iDroid retains the live stereo camera; the native guide's own
pause retains the correct camera; fresh paused skin publication keeps the
LEFT handset attached to its hand and projection; and the XR owner returns
GPU-completed shared slots before the next runtime wait. The last change does
not alter image/pose provenance, cached frames, queue depth or resolution.

The real forced Security guide completed through VR controls: Security
Settings -> Command Platform -> Basic Settings -> close its Help card. Retained
run `20261010T123130663211Z` witnesses mode zero, native tutorial pause cleared
and live stereo. The game owns all progression; no pause/progression writes,
tutorial skip, security purchase or save replacement were used. This does not
establish permanent guide completion across a future native save/reload.

On this exact DLL, 44 distinct scoped field cases passed across retained runs
`20261010T123205186650Z` (31 passes) and `20261010T123624390618Z` (13 passes).
Coverage includes completed-guide stow; ordinary LEFT iDroid opening, hand/head
motion, stow and immediate reopening; wrist input contexts; physical binocular
equip/stow; Pause roundtrip; stance changes; synthetic head/view loss and
recovery; three actually loaded weapons selected/raised/fired/reloaded with
one/two-hand support; and the SVG-76 physical 1x -> 2x -> 1x switch.
The ALM fixture initially expected 198 loaded rounds, while native state was
200. It failed before dispatch. The inspected 200-round fixture passed; the
failed run remains retained. SVG-76, ALM48 and WU S.PISTOL reload capacities
observed were 31, 200 and 11. This is three loaded cards, not the full catalog.

The preceding `ce20be11da9d` candidate also passed 47 scoped checks before the
frame-delivery change. Its prior images/results remain evidence of that DLL,
not substitutes for the current rerun. All 145 decoded frames of the current
40-second LEFT compositor guide motion capture have been reviewed; the real
Help closes and the paused handset/projection move together. The low-rate
recording does not establish 90 Hz visual smoothness or physical comfort.
Current paired-eye outcome/held-ready captures are in the private report.

A quiet, unrecorded 45-second field comparison (first five seconds excluded)
measured fresh XR frames improving from 73.85-77.20 FPS (median 74.89) to
89.62-90.04 FPS (median 90.00) at the 90 Hz runtime. Native stereo production
remained 103.59-106.11 FPS, with zero empty submissions. This accepts only the
measured quiet-field frame-delivery repair, not heavy combat or headset timing.
The native build passes all 54 CTest groups and installer checks. The final
startup fade guard also passes nine focused supervisor tests; the final build
log and current verification result are recorded in the private report.

Full release acceptance remains open: complete missions/other guide modes,
fresh/middle/end saves, the 418 weapon definitions and their configurations,
full sight/muzzle/hit alignment, binocular acquisition/marking, vehicle flows,
held wrist-popup visual acceptance, sustained soak and physical headset use.
The off-axis weapon images cannot certify optic aperture or hit alignment.
No full-game, universal-player or 100-percent readiness claim is supported.

Exactly two builds are retained: `play/` stays the selected `6ac2` pin and
`build/dev/` contains these fixes. Use `Play-Dev.cmd` for the fixed candidate;
`Play.cmd` restores the selected pin. No promotion or release was performed.
The current result, exact identities, retained runs and cleanup status are in
`artifacts/dev/readiness-current/report.json`.

## October 10 original pinned-build simulator check â€” blocked

The pinned `6ac226f4bf2c` DLL was tested in the real Meta XR Simulator before
headset handoff. **Do not call this candidate release-ready or headset-accepted.**
The current report is `artifacts/dev/readiness-current/report.json`.

Cold startup needed four visually reviewed Online Information acknowledgments
because the startup bot did not recognize those pages. Continue then reached
Afghanistan free play, Mission30010 / `Seq_Game_FreePlay`. The first LEFT iDroid
opening entered forced guide mode 11 with Pause ownership and camera reason 7;
the runtime continued submitting at 90 Hz while new stereo production stopped.
The failed run is retained at `artifacts/bot/runs/20261010T111107368414Z`.
This reproduced a player-visible freeze, not a successful readiness check.

Bounded held Back recovered the live camera and mode zero, leaving the native
Exit Tutorial card. The explicitly selected Pause roundtrip then cleared that
card. This is guide suspension/recovery, never tutorial completion or a fix.
After recovery, 26 distinct scoped state checks passed: seven control checks,
seven ordinary iDroid lifecycle checks, Pause open/close, four stance changes,
and six synthetic head/view tracking loss/recovery checks. The clean run is
`artifacts/bot/runs/20261010T111749974073Z`; tracking evidence is
`artifacts/bot/runs/20261010T111648372960Z`. Both-eye outcome images and all 135
decoded frames of the 40-second LEFT compositor motion take were reviewed.
LEFT handset/screen motion, ordinary stow and immediate reopening worked in
that recovered session. Held wrist-popup visual acceptance, weapon sight/hit
alignment, binocular acquisition/marking, sustained soak and headset comfort
remain unproven by this check.

During the recorded clean run, logged fresh stereo rates were 77.0â€“86.5 FPS
(window median 81.4) against the 90 Hz simulator runtime. Concurrent source-eye
and compositor recording was active; these figures do not establish physical
headset performance or an unrecorded baseline. Performance remains unaccepted.

Final observation confirmed released inputs, no menu/Pause/iDroid, mode zero,
and an active unsuspended camera. The owned game/simulator session was closed,
Steam retained, and pinned `play/` restored to the game. Pin and personal INI
hashes are unchanged. Next work is the first-open forced-guide transition,
followed by a cold-start rerun with fresh-image and sustained-motion gates.

## October 10 pinned play and development builds

The user selected `6ac226f4bf2c` as the pinned playable candidate. Its existing
`play/` files and manifest remain unchanged. This is the October 5 evening
build that passed 54 CTest groups and installer checks; physical headset and
full-game acceptance remain open. The prior source-hash mismatch was exactly
the two later documentation updates, not a different runtime implementation.

There are now two retained local build slots: `play/` and `build/dev/`.
Normal `tools/build.ps1` work updates development only. `Play.cmd` verifies
the pin and restores it into the recorded game before headset launch.
`Play-Dev.cmd` selects development for a physical debug session; `launch-sim`
selects development for the bot. Both preserve personal settings. Only an
explicitly requested `workspace.py pin-build` replaces the pinned candidate.
Old packages and binary backups are removed by the guarded cleanup command;
the inventory and removal receipt live in `artifacts/dev/build-cleanup.json`.
Settings, saves, source, release notes and retained test evidence are preserved.

## October 5 duplicate handset and free field iDroid

The current recorded field checks now include a real mortar discharge and
distant impact. The maintained mortar bot resolves the exact owned fixture's
current streamed registry ID once, then freezes it for the run. Isolated
human-paced stick pulses cross the measured native dead zone; a final
observation after the last pulse fixes a false aim rejection. Pinned run
`20261006T041836747984Z` reaches the measured downrange bearing (yaw error
1.32 degrees, pitch error -1.49 degrees), delivers ready plus fire, and exits.
All 110 LEFT compositor frames were reviewed. The shell-loading animation,
launch gas, falling shell and distant impact are visible; both exit eyes show
the impact. A vertical white beam and stippled white patches remain visible;
the complete curved trajectory and exact impact coordinates are not accepted.
Private fixture placement does not accept a navigation route. Cleanup releases
inputs, exits, restores tracking and returns to the original field fixture.
All 54 CTest groups and installer checks pass in
`artifacts/dev/field-mortar-final-aim-full-build-20261005.log`.

Ordinary current-save Mother Base -> Development -> Weapons/Items -> Back ->
Back -> stow is now recorded on `6ac2`, with all 88 parent frames and ten
paired images reviewed. The native pages and single LEFT handset are visible,
mode remains zero, and cleanup leaves no menu open. This scoped pass does not
clear the forced tutorial spinner, other menu branches or fresh-save flows.
The separate binocular/iDroid take changes physical zoom 2x -> 4x and walks
about 0.63 native units; all 98 parent frames and eight paired images were
reviewed. Its centered projection crosses the optic's sightline. A subsequent
physical move of the LEFT handset aside, stow and reopening shows the world
in the right ocular with the projection clear; target acquisition/marking and
full optical acceptance still require their own evidence.

One private combined video, `field-kit-current-proof.mp4`, contains these
scoped mortar, pistol/iDroid, scope, menu and binocular takes plus the explicitly
identified earlier `3a8a` physical steering proof. All 488 delivered frames
match reviewed decoded source pixels; original cadence is retained with hard
chapter cuts. It is a 142-second evidence video, not whole-game acceptance.
Current `6ac2` truck-0003 entry attempts failed and were cleaned up; incoming
fire interrupted the native-button comparison before its comparative input.
Current cab/driving, every weapon/scope/configuration, complete menus and
fresh/middle/end progression remain open.

The expanded private `field-kit-current-proof-v2.mp4` now retains 567 exact
reviewed source frames across seven chapters (187 seconds). Its new current
loaded-card sweep completes in 39.94 seconds: native resources 27873, 93478
and 159074 are independently sampled and visible as SVG-76, AM MRS-4 and WU
pistol. Native ammunition decreases by two, three and one rounds respectively.
All 110 parent frames and all 24 sequential paired-eye images were reviewed.
One- and two-hand ready are observed on each; SVG-76 powers 1x and 2x are
exercised. The other two cards do not establish a variable-power descriptor.
Brief weapon pose jumps, exact contacts, hits and every fitted sight remain
unaccepted. This does not bind or clear the 418 native catalog definitions.
The wrapper now honors the compositor recorder's existing 45-second limit;
the earlier failed 60-second request dispatched no weapon checks. Cleanup
completes with no input, recording, menu or tracking error.

The expanded video replaces the centered-binocular chapter with the physical
centered/aside/stowed/reopened comparison. All 67 parent frames and fourteen
sequential paired-eye images are reviewed. The right ocular shows the world
with the handset aside, closed and reopened aside; the left eye remains
outside that right ocular at the measured hand pose. Acquisition, marking,
full optical alignment and physical-headset acceptance remain open. The
current measured truck-0002 doorway trial was interrupted by actual incoming
fire before boarding and restored the safe field fixture; it is not a driving
pass. The earlier steering chapter remains explicitly identified as `3a8a`.

The following current truck-0003 threat-response attempt also fails and is
cleaned up. Native visibility queries locate two unoccluded guards about four
and five units away, but measured two-hand barrel errors plateau around 0.21
and 0.37 units and never satisfy the existing 0.18 firing gate. No trigger is
dispatched. Actual alert and health loss then interrupt boarding; the safe
field fixture is restored. This retains a concrete bot aiming failure rather
than crediting camp clearance or current driving. The rig's support hand
guides weapon direction; independently changing the dominant orientation then
copying the existing support contact is a feedback limitation requiring a
coherent two-hand correction and a new actual-game reproduction. Final live
inspection confirms no Pause/iDroid menu and zero buttons, axes, triggers or
physical test inputs.

Candidate `6ac226f4bf2c` repairs the missing 1x dot reproduced on `02052`.
The unmagnified collimator retains native transparent glass and stereo pixels;
only its dot is drawn at the measured native rear sight point. It does not
request an extra magnified scene replay. All 54 CTest groups and installer
checks passed in `artifacts/dev/field-collimator-dot-full-build-20261005.log`.
The live `20261005-dual-scope-collimator-6ac2` run now completes physical
1x -> 2x -> 1x. All 88 LEFT compositor frames and six paired images were
reviewed: both powers show their dot, and the existing native magnifier hinge
moves 120 -> 0 -> 120 degrees. The right-eye 1x aperture is clear; the off-axis
left-eye 1x aperture is largely dark with a red point and a sliver of scene.
Handling still jumps briefly, and the low capture cadence does not clear the
earlier separated-lens defect. Full optical alignment, exact native reticle
texture, hits, catalog permutations and headset acceptance remain open.

The field Development page reproduced a loading spinner with forced mode 11.
Ordinary Back reached actual game XInput but did not close it. The existing
VR iDroid Menu tap stowed that UI-only owner and restored mode zero. Its native
Exit Tutorial card remained visible after the pause owner closed; LEFT-grip
focus plus the reviewed A Close input dismissed the card in both eyes. The
subsequent clean actual WU S.PISTOL case fires 11 -> 10, reloads to 11 and
stows/returns the single LEFT handset for support grip. All 107 decoded LEFT
parent frames and all eight paired images were reviewed. A stream-copy chapter
retains original frames/timing; capture throughput is 3.55 FPS, not game FPS.
These bounded recoveries do not accept
Development loading, all menus or the complete field-kit video.

The real `02052` run `20261005-dual-scope-alignment-02052` completed physical
1x -> 2x -> 1x inputs, with native hinge angles 120 -> 0 -> 120 degrees.
Both final eyes show the aligned 2x lens image and dot. All 152 decoded LEFT
compositor parent frames and all six paired images were reviewed. The visual
gate remains failed: the 1x dot is absent, and handling includes a brief
separated lens and weapon pose jumps. Capture throughput was 3.37 FPS, not a
measurement of game frame rate. Its reviewed proof remains with the private
case. This does not accept motion, hits, the full catalog or a physical headset.

Preceding candidate `02052fbf45bd` adds the missing fitted collimator/magnifier path.
The actual SVG-76 resource was joined through the native custom-weapon part
resolver to `st02_main0_def` and `st03_main0_def`, including its two named
magnifier bones. Owned glass measurements select separate 1x collimator and
2x magnifier apertures. The existing VR zoom binding moves the existing native
magnifier hinge to its authored aligned bind; no extra tube or housing is
drawn. A guarded native palette callback keeps that joint on its current gun
publication. The new dot reticle is functional presentation and is not yet
accepted as an exact reproduction of the native reticle texture.
All 54 CTest groups and installer checks passed in
`artifacts/dev/field-dual-magnifier-full-build-20261005.log`.
The checked stage's interrupted promotion was completed through the existing
workspace promotion/deployment functions after verifying its JUnit results,
installer log and every staged binary against `build/Release`.
Both-eye lens, hinge, motion and zoom acceptance remains pending the live take.

The `cb356` actual field take tested three distinct native resources: HIP
27873 (`[1,2,0,1]`), BACK 93478 (`[1,0,0,2]`), and secondary 159074
(`[0,0,0,0]`). Both eyes were reviewed for one-hand and acquired two-hand
contact; native ammunition decreased in the correct slots by 3, 3 and 1.
The HIP gun fails because the fitted 1x/2x scope never publishes a tracked
view. BACK collimator and secondary iron-sight acceptance remain unknown.
The actual LEFT compositor take is
`private/field-kit-current/20261005-three-loaded-weapons-cb356-video/simulator.mp4`.
All 135 decoded parent frames were reviewed. Capture completed without an
encoder error at 2.99 acquired frames per second; this is capture throughput,
not a measured game frame rate. Handling also exposes brief weapon jumps.
This run does not accept optics, hits, two-hand discharge or the 418-entry
catalog. New diagnostics observe the native assembled rear/front sight points
and their completed-skin LOCAL poses without accepting an unmeasured profile.

Preceding candidate `cb356d87ae77` passes all 54 CTest groups and installer checks
(`artifacts/dev/field-driver-menu-seat-focus-full-build-20261005.log`).
Native vehicle occupancy now remains separate from input availability: focusing
or closing handheld iDroid must preserve the occupied seat and forward facing.
The reproduced `3a8a` closing-frame jump had native travel mode 3 but menu
input context and disabled rig input. This new repair awaits live final-eye
verification; unit checks do not accept it.

The preceding `3a8a0ff61429` private truck-0003 fixture moved the actual vehicle
3.414 native units using physical LEFT-grip acquisition, a human-paced 30-degree
controller roll and the accelerator. Eight actual game XInput returns witnessed
steering and pedal delivery. Both cab eyes now face the native seated root and
use its authored head position; removing the standing backward/height fit clears
the shoulder mesh and restores native wheel reach. The retained real LEFT
compositor take is `20261005-truck-native-seat-night-wheel-3a8a-video`; all
decoded parent frames were reviewed. It is scoped evidence, not the full field
kit: two handheld transition frames still jump, and the complete case failed
its three-second exit deadline. Cleanup completed the actual exit, released
inputs, restored tracking poses and the native clock, and left no menu open.
The private exit bound is now seven seconds, matching the observed native
door animation. Fixture placement and clock changes do not accept navigation
or progression. Current live DLL is `6ac226f4bf2c`.

Preceding candidate `de2fa91c704b` repairs an actual mounted handheld-camera failure:
ordinary VR Interact boarded native truck 0002, but opening iDroid then placed
the final VR eyes at the native chase camera outside the truck. Live handheld
menus now join the exact fresh camera/player-head transaction and apply the
same first-person stabilization and height offset as ordinary play. All 54
CTest groups and installer checks passed. The installed game was relaunched
through existing Steam 8644; both reviewed arrival eyes in pinned
`20261005T231924974365Z` show actual healthy first-person field play.
Later native-root yaw and seated-position repairs above address the remaining
backward view and shoulder clipping exposed by this earlier candidate.

The recovered truck fixture also retained an old door position while native
truck 0002 rolled 1.51 units. It now follows fresh exact vehicle positions.
The preceding `69cf` run boarded with the ordinary VR binding and moved the
real truck 3.706 units with reverse; its forward/handheld driving case failed,
and its paired images expose the chase-camera defect. This is not accepted
driving footage. A subsequent daytime entry was interrupted by incoming fire;
the test incorrectly treated the native entry animation's temporary travel
mode as occupancy loss. That diagnostic guard is corrected. Genuine native
checkpoint recovery included declining the reviewed Chicken Hat prompt, then
loading acknowledgment; healthy field return is pinned in
`20261005T232816191769Z`. No health or progression was patched.

Native Options -> Control Type was reviewed in both eyes: the active setting
is Action Type. Y delivery was observed in the game's actual XInput return,
so Shooter Type was excluded as the cause. The nested Options inspection also
exposed the bot's two-level Pause cleanup bound; it now permits four guarded
Back edges. The 255 bot tests pass, including that nested cleanup regression.

The native bolt cycle can temporarily withdraw a valid firearm muzzle after
a shot. Combat now waits up to 2.8 seconds for the same fresh owner's muzzle,
within the existing grip lease, without replaying the trigger. All 43 combat
tests and the preceding full 54-group build passed. Current daytime combat
also recorded two rounds and one native incapacitation, without excluding
buddy attribution; this does not certify reliable autonomous camp clearance.

The already reviewed `69cf` pistol and rifle chapters now have hash-linked
parents, native receipts and all six paired checkpoints in
`private/field-kit-current/reviewed-idroid-chapters-proof.json`.
They remain two scoped chapters rather than the full field kit or acceptance
of every weapon on the newer build.

Retained `69cfeb25878c` has a reviewed final LEFT compositor chapter at
`private/field-kit-current/idroid-pistol-field-chapter.mp4`, trimmed from the
retained `20261005-idroid-sidearm-69cf-video` take. All 178 parent frames and
all 66 delivery frames were reviewed, alongside all six paired-eye return and
support checkpoints. The actual WU S.PISTOL resource is 159074: ammunition went
9 -> 8 after an ordinary 250 ms shot and 11 after native reload. One handset
returns in the LEFT bionic hand after reload; its projection and geometry stow
during actual two-hand support and return on LEFT-grip release. Human pacing is
200 ms taps, 350 ms modifier formation and 750 ms hand trajectories. Inputs
were released, the terminal closed and controller poses restored. The scene is
night; this accepts scoped visibility and stow/return, not full firearm sight,
muzzle/hit alignment, exact projection bounds or physical headset readiness.

The earlier current-build rifle chapter remains at
`private/field-kit-current/idroid-field-chapter.mp4`. All 70 delivery frames and
six paired checkpoints were reviewed. Device stow/return passes that scope,
but the native rifle leaves the hand during portions of its manipulation;
weapon reload alignment remains unaccepted. Neither chapter is the full kit.

The bounded bot's default cruise was below the actual native walking threshold:
0.30 held for 800 ms produced zero displacement on clear ground; 0.45 produced
1.335 native units. Default cruise is now 0.45 with the existing short leases,
fresh observations and settling gates retained. Pinned
`20261005T204235616850Z` reached a 0.35-unit local target with 0.523 units of
observed displacement and settled after release. The previous failure is also
pinned. This is one local walk, not full map/navigation acceptance. The full
`tools/build.ps1 -SkipBootstrap -NoDeploy` run passed all 54 CTest groups and
installer checks; `play/` was refreshed. This is historical `69cf` evidence,
not the identity of the current live DLL.

The next four configured weapon trials each attached native two-hand support,
but all retained rendered resource 27873 and optical descriptor `[1,2,0,1]`.
Typed slot changes and ammunition consumption do not prove four loaded models
or scopes. All four full weapon/scope results remain unaccepted. A subsequent
ordinary secondary/primary switch and source-backed native equipment-request
mask did not replace that rendered primary resource. The inspection harness
now respects effective aim pitch and the runtime grip/aim cant; no personal
binding or production attachment calibration was changed to disguise this.

An earlier `69cf` truck driving case failed: the revised fixture reached stable native ground
beside truck 0002, but ordinary VR Interact did not board. Its reviewed source
frame places the player beside the spare wheel, before the cab door. Earlier
low placement fell through the floor and is retained as failed evidence.
Actual Continue From Checkpoint and the loading confirmation recovered healthy
field play in pinned `20261005T205651184711Z`. Fixture placement is not route
acceptance, and no failed truck attempt is credited as driving coverage.

The reported doubled handset was reproduced in the actual field. The typed
native equipment inventory contained two visible ID499 instances after a cold
terminal transition. A temporarily unavailable typed owner was discarding the
VR allocation lease without retiring its native resource. The equipment pump
now retains that lease through unknown-owner gaps, and hides the exact owned
allocation before its native release. The UI-only field handset also needs its
native scene root and bounds moved with its completed wrist publication;
changing only its bone palette had left it culled at an old world position.

Candidate `eaf6f645152e` passed all 54 test groups and installer checks. Its
reviewed two-eye gun/iDroid capture shows one native handset in the LEFT bionic
hand and a native firearm in the RIGHT. The typed inventory contains one
ID499 instance. Forced SECURITY stow, immediate reopening and walking after
reopening also passed that bounded field run. Capture cleanup encountered an
operator transport failure after native stow and neutral input; a separate
bounded observation verified the closed terminal and released controls. This
is scoped simulator evidence, not acceptance of every weapon or gun alignment.

Actual follow-up reproduced reload closing the terminal without returning it,
and LEFT grip at the native support socket choosing menu focus instead of
weapon support. Native terminal input filtering alone did not repair reload:
the retail player action stops the terminal. The current implementation borrows
the exact UI-only owner with native Stop/Start, preserves the native handset
lease, sends one effective reload chord, and returns after actual manipulation
ends. Transient rig publications use at most 150 ms of same-owner control
history for hand-use arbitration; rendering still requires its fresh pose join.

Candidate `9b893adf64c5` passed all 54 test groups and installer checks. Retained
`private/idroid-current/20261005-human-hand-return-9b89/` passed a human-paced
field reproduction: 200 -> 197 -> 200 rounds, actual native reload manipulation,
automatic terminal/handset return, continuous support attachment and automatic
return on LEFT-grip release while the RIGHT ready grip remains held. The chord
was formed one button at a time, using 200 ms taps, a 350 ms modifier lead,
750 ms hand trajectories and a deliberate support hold. All six final-eye
images were reviewed, showing one LEFT handset on return and no handset or
projection during support. Cleanup released inputs, closed the terminal and
restored controller poses without error. These are simulator trajectories,
not physical-headset user testing or firearm alignment acceptance.

The associated retained final LEFT compositor chapter is
`private/idroid-current/20261005-human-hand-return-9b89-video/simulator.mp4`.
It contains 122 source frames over 35.172 seconds (3.47 capture frames/s),
without interpolation. Eight timed frames were reviewed alongside the paired
stills. It proves this bounded chapter, not the full requested field-kit.
Earlier `e0b9fc1c7491` paired images also show physical RIGHT binoculars
coexisting with the single LEFT handset; zoom/acquisition/mark alongside the
handset still need acceptance on the current candidate.

The full field-kit video, driving with iDroid, all weapon/scope combinations,
all menu/configuration combinations, genuine fresh/middle/end save and resume
coverage, mortar arc/impact and headset acceptance remain open. The user
explicitly requested a retail-created fresh campaign on October 5. Closed
middle-campaign snapshots are retained; they are not certified fully unlocked.
All further interaction cases must use ordinary human timing. The authored
hand/contact transforms are source-bound; exact native physical projection
canvas bounds remain a separate measurement. The pictured firearm pose still
requires its own muzzle, sight, hit and grip investigation before acceptance.

## October 5 retail fresh-save regression

The six TPP save payloads were backed up and cleared with the guarded local
`reset-new` operation. Retail created the fresh campaign itself. Its native
story is 0 and developed-equipment count is 0; no unlock or story flags were
patched. The closed native snapshot is retained as
`artifacts/test-profiles/regression-fresh-20261005`, alongside the protected
middle-campaign and original-save archives.

Autosave confirmation advanced, but the first-run Terms document did not open
after either an ordinary tap or a deliberate 800 ms confirm hold. Both final
eyes remained on the same Terms page. The input receipt's `observed_pass`
does not certify this interaction; its retained visual review records failure.
The later native trace proved that Confirm reached the retail edge getter.
The selected document action invokes the retail browser service, leaving the
same VR page visible. A separately sampled downward stick hold reproduced the
actual defect: physical LEFT Y was -1, but all mapped native axes were zero.
The Terms owner was being classified as a cinematic, which suppresses sticks.

Candidate `69cfeb25878c` passes all 54 test groups and installer checks. Its
typed, fresh Terms owner now retains the effective menu stick during that
startup cinematic. The speculative button-edge fallback was removed; the
native getter retains its original result. Readiness and native selected-action
observations prevent startup input during initialization or confirmation on the
wrong choice.

Retained `20261005T183251955121Z` moved the native selection down after a
222 ms sampled hold. Both eyes show Decline selected; a separate LEFT movement
in `20261005T183445108415Z` selected Accept in both eyes. Ordinary VR Confirm
then advanced Content Terms to Currency Terms, Currency Terms to Privacy, and
Privacy to the actual server-login progress dialog. The native story and
developed-equipment count remain zero, with no direct agreement or progression
writes. Privacy's 200 ms downward hold repeated to its final choice row; the
fixture expecting only the intermediate document row remains failed, and its
corrected follow-up checks the observed native selection before Confirm.

The final-eye Terms and Privacy handoff chapters are retained under
`private/fresh-startup-current/`, with truthful capture cadence. This accepts
the reproduced first-run menu-axis repair, not the fresh prologue, Continue,
save/resume, all menus or full field-kit video. Those broader checks remain open.

## October 4 native left-hand iDroid

Candidate `e8e3a4d3c90c` passed all 54 CTest groups and installer transactions.
Retained `20261005T042233419201Z` passed seven live simulator cases: natural
left-hand hold, pitch, opposing yaw, ordinary stow, immediate reopening and
final stow. All 14 outcome eye images were reviewed. The handset and screen
turn together, the native left cupped fingers hold the device, and the right
hand remains free. The preceding `a08af486bdd3` run had an edge-on natural
screen and is negative visual evidence, despite its native state checks.

The hold comes from the actual owned `p31_010010` arrival cinematic, including
the left wrist-relative device transform and 16 left finger joints. Steady
native palette reads corroborate that contact; asynchronous reads contain
motion outliers and do not certify every frame. Source-eye video is retained
separately from the final-eye stills. The existing left forearm HUD retains
its owner. Ordinary stow/reopen is scoped acceptance; forced tutorial stow,
physical headset use, every menu/configuration and whole-game acceptance
remain open.

The native cinematic UI job and its transform getters were recovered during
one further bounded reference replay. Its graph root shares the handset's
orientation and lies 9 cm forward of `CNP_HOLOGRAM`. The configurable default
has been corrected from 8 to 9 cm; this subsequent fit needs its own live
verification. Exact physical canvas bounds remain separate from that proven
root transform and are not yet a 100% cinematic-size match.

## October 4 immediate mortar firing proof

The October 4 field idle stalls were reproduced in the live simulator:
`xrEndFrame` repeatedly took 0.65--0.85 seconds, image age exceeded one second,
and GPU memory was 11.6 GB of 12.3 GB. The simulator's unused Bedroom synthetic
environment was still running alongside OpenNV. Stopping only that exact owned
batch-room child reduced GPU memory to 7.9 GB and restored approximately 90 XR
submissions per second. Subsequent idle windows had no slow submissions or
retained images over 500 ms; source pairs remained about 65--74 per second, so
this is not a zero-repeat or physical-headset performance claim. Both actual
eyes in retained `20261005T014926762193Z` show the native field and rig.
Before/after timing and GPU evidence is retained in
`artifacts/dev/idle-stereo-20261005/`. Both simulator launch paths now release
only the exactly recorded, isolated batch passthrough room for opaque MGSV VR,
preserving the live game and compositor. Shared clients and changed process
identities block that cleanup.

Follow-up `20261004T234601864851Z` reproduced horizontal aim response but
discarded both elevation pulses: native Y stayed zero while the physical stick
was nonzero. Stationary mortar entry leaves ON_VEHICLE false and clears
NORMAL_ACTION, so on-foot turn handling was consuming its aim. The runtime now
retains both native axes during the verified standing non-ordinary action,
excluding carry/CQC/treatment/optics and respecting menu/native-input ownership.
This is an action-state owner, not an exact mortar-ID getter. Handheld firearm
replacement/suppression is inactive during that owner. The bounded mortar case
now records the ready grip and all four aiming pulses before firing; aim/arc
remain false in raw results until pixels are reviewed.
The stationary action retains the on-foot button owner: a vehicle-style release
gate at this boundary cancels the held Y that is completing native entry.
Entry/exit now require a sampled native action acknowledgement, rather than
crediting an expiring unsampled short host delay. Active-camp damage allowances
are explicit policy values with a surviving-player floor; the default is zero.

Retained `20261004T231347` reached the reviewed Sloped West mortar002 operator
position, entered through physical Y, delivered one 180 ms VR right-trigger
pulse to native XInput `[0,255]`, and dismounted through Y. Its seven-second
source-eye/game-audio recording contains the native discharge flash and muzzle
smoke at approximately 4.33 seconds, following the shell-load animation. Both
released entry/fire/exit compositor eyes were inspected. Native health remained
7801 through this take. The firing result is a scoped simulator pass; impact,
barrel aiming axes, exact stationary mounted-ID getter, rig contact and physical
headset acceptance are separate.

The reusable `workspace.py bot --command mortar --policy ... --record --seconds
60` now approaches the reviewed operator position rather than treating the
authored mortar pivot as the player's stand point. It acknowledges the observed
stationary action without requiring the game's empty ridden-vehicle getter,
records three seconds of the native discharge response after one trigger and
dismounts. Damage, changed ownership and a total action deadline remain guards.
Raw action results retain pending visual review instead of crediting a delivered
trigger as a shot. The first shortcut attempt `20261004T232201287152Z` stopped
on actual native damage in the previously fired-on camp; it is retained as a
negative, not shortcut acceptance. Failed Y follow-up `20261004T231654744771Z`
started after dismount had moved Snake outside the prior interaction position.
The original successful firing recording is retained independently.

Retained `20261004T233252335383Z` then passed the repaired shortcut in
17.188 seconds from the reached mortar neighborhood: native entry, one 180 ms
physical RT with matching game XInput `[0,255]`, recorded discharge, native exit
and neutral release. Its nine-second source-eye/audio take was reviewed, and
health stayed 7801. Prior fresh-session approach `20261004T233035391364Z`
stopped on detection with full health; the explicit active-camp diagnostic
policy acknowledged that battle state before the successful remaining approach.
The 17-second result does not include cold boot or the earlier staging route.
All 54 CTest groups and installer transactions pass. Test game/simulator cleanup
retains Steam, restores the runtime lease and leaves no test menu open.

## October 3 idle stereo and helicopter menu investigation

Local candidate `72fdf5e65ad5` repairs a reproduced empty-layer path when raw
head/view validity disappears. The last admitted stereo pair retains its
original poses, FOVs and crops while the same camera and reference space still
own it. Invalid poses cannot admit new pixels; reference-space changes, camera
ownership changes and `shouldRender=false` remain separate boundaries.

Retained negative `20261004T045321827065Z` on diagnostic candidate
`f87d9be1302f` reproduced black final-eye output and seven seconds of empty XR
layers after raw head validity was removed in actual ACC. Retained
`20261004T050618985244Z` on the repair passed six seven-second loss/recovery
cases for head validity, view validity and both. All twelve outcome eyes were
reviewed; scoped XR logs contain no empty-layer event and recovery advances
camera/rig publications without changing activation/reference ownership.

Retained `20261004T060748849943Z` passed the same six diagnostic cases with
right-hand iDroid open. Both eyes were reviewed: the actual page was a
**Daily Bonus Received information card**, not Map. The later stow case failed
because Back advanced to a **Skulls Attack notice**; no unknown notice was
confirmed. Cleanup closed the owned game/runtime and retained Steam. These
scoped stereo results do not pass that failed lifecycle case or the menu tree.

Retained `20261004T054602946280Z` reviewed Choose a Rival dismissal, Missions
and Map tab changes, Mission List entry/Back, and ordinary root Back to the
cabin. A supervised tracking recovery wait stopped at its two-second review
threshold before the deliberate seven-second lease expired; the runner now
waits only for its exact armed lease and unchanged owner. Unexpected popups,
unarmed loss and expired loss retain the normal review gate. Native area
selection was active but invisible in the observed ACC state. Re-running the
game's story-dependent helicopter setup did not restore it, so that setup was
not adopted as a production fix. The free-roam route remains unresolved.

Public head-loss fixtures and exact remaining obligations are in
[the idle/menu report](HELICOPTER_IDLE_REPORT.md). Physical headset idle,
runtime stalls, continuous stereo, every menu/item and whole-game acceptance
remain open. The prior twenty-minute no-input simulator observation did not
reproduce the user's idle symptom and does not clear their physical report.

Retained `20261004T094310678869Z` adds fifteen bounded ACC navigation cases
and seventeen reviewed two-eye observations (34 images). Actual results cover
Choose a Rival dismissal, Missions list entry/paging/Back, Map, Mother Base,
Customize, Helicopter, Chassis grades, Back, discard confirmation and return to
the live cabin. Scratch case names claiming a category/filter change were
corrected in `visual-review.json`: the observed actions paged the mission list
and zoomed its miniature map. No helicopter upgrade was purchased. The cabin
baseline still lacks an instruction for opening iDroid; Map offered marker
placement rather than departure. The free-roam route remains unresolved.

An isolated save fixture is used for firing, vehicle and emplacement checks;
the user's exact prior save is preserved as `before-cockpit-arsenal-audit`.
Retained `20261004T101905227429Z` reviewed native Continue, a readable
`A â€” Resume Game` loading card and entry to Afghanistan free play. The first
weapon inspection in retained `20261004T102059202073Z` failed its unsupported
assumption that the selected AM M48 must publish a tracked round scope. It
does not establish a missing fitted sight. Gun readiness/firing and optic
eligibility are now separate checks. Every firearm variant, dual sight,
vehicle subtype, enemy mortar/turret and physical headset acceptance remains
open until its actual native use and visual outcome are retained.

Retained `20261004T104632827998Z` confirms native ammunition changes after
bounded VR ready/trigger input: AM M48 198â†’194, SVG-76 31â†’28 and WU S.PISTOL
9â†’8. Their held firearm and released wrist presentations were reviewed in both
eyes. A small-number reading error was corrected with an inspection crop:
the M48 wrist display correctly shows 194/400; no stale-display repair was made.
Scratch IDs calling the SVG-76 an MRS-4 are corrected by its actual visible
name in the review manifest. This establishes these discharges, not impact,
hand/sight alignment, every variant or every optic. The SVG-76 fixture publishes
native optical powers 1Ã—/2Ã— with no admitted scope portal. Native CNP queries
now consider every listed power; unknown geometry still requires calibration.

The bot's owned small-block navigation import now contains 659 Afghanistan
and 725 Africa NAV2 files. All forty prior Afghanistan tiles match their old
hashes. The expanded Afghanistan graph has 938,496 nodes and admits the current
fixture's supporting floor, previously outside the forty-tile import. Authored
locator files provide connected candidate routes to the loaded Sloped West
machine gun and mortars. Import completeness for these indexed small-block
packages is separate from physical clearance, vehicle roads, other map/package
classes, autonomous combat and mission completion. None of those is certified.

## October 4 autonomous field investigation

Local candidate `4a6e96f33675` passed all **46 CTest groups and installer
transactions** and is synchronized through the fixed `play/` tree. Its new
native cover observer is read-only: at most eight caller-selected NAV points,
four actual person-marker origins and 32 terrain rays on the native marker
job. Admission requires fresh same-owner focus, a joined observed root/native
eye and a bounded nonce/lease. Retained `20261004T172412196409Z` admitted
same-owner live obstruction probes and a mapped covered approach. It discharged
two WU rounds with support attached; guard 1167 entered Sleep while buddy
attribution remained possible. Other guards were still active when the
commander deadline ended the run. These are obstruction and scoped action
observations; enemy-eye, shadows, native AI concealment and completed camp
acceptance remain open.

The reusable camp policy now supports physical reconnaissance, exact positive
acquisition before explicit marking, close suppressed engagement with the
authored two-hand support contact, observation-only publication recovery,
health priority, and a reviewed primary fallback after detection. Moving
targets can be reacquired before dispatch without replaying an uncertain
trigger. Cover approaches and crouched peeks still require live verification.
The bot now composes each navigation turn onto its current head orientation;
the prior absolute replacement could undo the previous turn. Trigger pulses
have separate leases from the ready/support grips. Native pickup/carry/drop is
an explicitly selected bounded experiment with exact body and guard checks,
walking routes and no claimed direct carrier-to-body link. These corrections
still require a completed live encounter and both-eye action review.
The local director observes current artifacts and completed source-eye clips;
it does not provide an independent continuous final-eye feed.

Retained `20261004T175758596525Z` moved from staging through two mapped
approaches, with run/crouch, full health and no detection. Before its third
route, the left final compositor eye was entirely black immediately after a
harness recenter. Both released cleanup eyes showed the world again. The run
stopped and closed its owned game/runtime before firing, body handling or
mounting. Repeated recenter was removed from autonomous route boundaries;
standalone recenter interaction remains available. This is a retained negative,
not proof that the user's headset idle report or native recenter is repaired.

Retained `20261004T161525577828Z` demonstrates run/crouch/crawl progression,
physical binocular zoom 2x to 4x, current exact-actor acquisition and explicit
mark of guard 1166, and two suppressed WU rounds with native support attachment.
Guard 1166 then entered Sleep. The later expired diagnostic lease stopped that
run before the emplacement; scoped observations do not certify a whole mission.

Retained `20261004T170412185254Z` demonstrates three WU rounds at camp with
support attached and full health: one at guard 1167, then two at guard 1214,
whose native life became Sleep. Buddy attribution is not excluded. Both final
eyes at the later stop show Snake low behind a tall crate wall blocking aim.
The commander request expired before a fresh clue was supplied. It did not
reach the mortar. Retained `20261004T165246164071Z` reached native camp
incapacitation and health recovery but failed the harness's incorrect registry
category/type comparison before mounting. That comparison is corrected using
actual `gimmickType` 5 and native `type` 81; the mortar still needs live proof.

Retained `20261004T163607271650Z` reproduced a 15-second compositor RPC stall
during a held gun inspection. Recorded held inspections now place explicitly
qualified checkpoints in continuous NativeEyeAV source-eye video. They create
no final-compositor still or stereo acceptance. Neutral observations still
capture both eyes. Cadence loss/repeats, source lineage and real audio remain
retained. Mortars, turrets, trucks, roads, CQC, carried-body identity, concealment,
all firearm/scope variants, complete missions, native binocular-command leakage
and physical-headset readiness remain open.

## October 4 field-kit and direct mortar follow-up

Candidate `14a36ed5e5ac` passed all **53 CTest groups and installer
transactions** and is synchronized through `play/`. Its added native input
observer records actual XInput returns, separately retaining index zero, exact
packet values, caller, source and timestamps. It changes no input mapping or
gameplay mask. The new bot binocular presentation preserves the measured
grip/aim pairs, places the left hand at the authored support cup and samples
head/hand movement instead of the previous 133â€“162 degree single-packet turns.
Live support, motion and eye acceptance remain pending.

Retained `20261004T194951821331Z` published full physical/mapped forward input
without native player displacement at staging. Its failure is downstream of
the XR sampler; that earlier candidate has no current XInput return witness.
Retained `20261004T200116814810Z` then used an explicit VR crouch entry and
completed the native run/crouch/crawl route to the mortar, reaching
`[88.6299, 333.5233, 99.4788]` with full health and no alert. This scoped result
does not identify the exact downstream reason for the previous stall.

Retained `20261004T200403232896Z` records the actual authored mortar and a
sampled Y press from a crouched 1.25-unit approach. It did not acknowledge the
expected mounted ID. The requested head pose was published before the press;
the first saved before-mount observation was stale and is corrected by an
explicit publication wait. The next selected permutation stands and approaches
more closely. Mount identity, barrel motion on all axes, shell fire, dismount
and the native stationary-aim adapter remain unresolved.

The private `showcase/current-field-kit/snake-current-field-kit.mp4` working cut
contains 112 seconds of fresh retained source-eye footage, game audio and the
original field-kit artwork. It labels failed firing/recovery and unshown
mounts/driving. NativeEyeAV precedes compositor crop/composition; this cut is
not final-eye stereo or complete-game acceptance. The exhaustive private state
matrix lists every requested boolean combination and ordered transition, with
unknown/illegal states separate. Enumerating a case does not pass it. Original
six-file save `before-cockpit-arsenal-audit` remains protected while isolated
fixtures are in use.

## October 3 experimental patch

The installed local candidate `92d1a69368bd` passed all **44 CTest groups and
installer transactions**, including **120 real D3D11 mailbox checks**, and
was used for the native validation below. Final package `BUILD.json` and
`RELEASE.json` identify the exact committed source and files. Packaging for
`experimental-2026-10-02.1` is in progress; it is not published yet. The
published October 2 release below is a separate build.

This candidate retains paired grip/aim poses for both hands and makes the
right-hand handheld iDroid the public default, preserving explicit spatial
preferences. On handheld opening, a native body update within 150 ms defers
the separate paused-body refresh to avoid detaching already-published
equipment. Five additional guarded chrome layouts bring device routing to
25 layouts, using the exact native handset pose and observed open ownership.
Ordinary left-arm HUD and shared Mission Orders remain outside that device
suppression. Native authored transforms are unchanged.

Spatial iDroid now omits only the mod-owned `MGS5VR_iDroid` world pause during
verified native cabin play. Spatial field pausing remains; closing releases
the mod registration before the native open bit clears. This is the candidate
repair for the ACC Development exit failure, not a verified causal diagnosis.
The build also retains completion-fence stereo transfer, source-ordered
recording and separate layer-preparation/`xrEndFrame` timing.

Retained `20261003T104542847534Z` passed 13 state cases on this candidate in
spatial mode. Both Weapons/Items and Buddy Equipment opened their actual child
grids and each returned to the Development category root after **one ordinary
120 ms VR Back**; both before/after eyes were reviewed. The Weapons/Items
result also has an independent review manifest. Helicopter opened its armament
grid, but the next Back decision was not consumed before the supervisor's
1800-second deadline. Security Devices was not tested.

Retained `20261003T112828492997Z` passed eight handheld-mode state cases.
Weapons/Items returned from its settled child grid to the Development category
root after one 120 ms VR Back, with both before/after eyes reviewed. The ninth
case, category Down, failed with `Native menu stick stayed held after release`.
The runner neutralized input and closed its owned game/runtime; Steam remained
running. The cause is not established. Handheld Buddy Equipment and both-mode
Helicopter/Security exits remain untested.

Retained `20261003T114628779393Z` passed seven handheld field cases: opening,
cant, right/left yaw, ordinary Back, immediate reopening and final stow. All
14 outcome PNGs were reviewed. Open outcomes show a full-size right-hand
phone, right arm and left bionic arm; stow clears the device UI. Initial and
reopen left-eye captures caught UI fading while the later right-eye captures
were settled. These sequential outcome images establish the scoped functional
result, not continuous boundary or simultaneous stereo behavior. A preceding
recording attempt stopped before cases at the 25 GiB reserve, so it supplies no
transition video.

Retained `20261003T120302596202Z` passed four spatial field cases: open, one
120 ms Back, immediate reopening and a final 120 ms Back. All eight outcome
PNGs were reviewed: both arms are visible, the open Map is present and Back
clears its UI. At final stow the native handset is mid-stow in the left capture
and gone in the later right capture. This is scoped lifecycle evidence, not
simultaneous stereo, continuous animation or world-motion pause verification.
The retained `20261003T115540064524Z` attempt failed only its incorrect `pause=true`
fixture expectation; 53 open observations matched every other guard.
`nativePauseMenuOpen` denotes Pause/Help UI, not the mod-owned world pause.
The corrected private fixture uses `pause=false` and retains all other guards.

Retained `20261003T114817998847Z` passed 12 tracking state cases, including
five independent injected raw-pose loss/recovery periods after iDroid opened.
The measured loss holds span 5.500â€“5.891 seconds; loss observations remain
inside the eight-second leases. All ten loss and ten recovery PNGs were
reviewed, showing both arms, the full-size phone, Map and world. The audit
confirms restored raw tracking flags, stable activation/epoch, active/focused
camera state and advancing camera/rig publications. Native
player sequence stays fixed while the menu is open, and the still records
lack immutable source-frame identity. This does not establish physical Quest
idle recovery, opening while tracking is lost, API-stall recovery, absence of
repeated images or continuous animation.

The reproduced prior failure remains specific: on `7a5b00774fd2`, Up responded
in spatial Weapons/Items, but three sampled 120 ms B presses with neutral
releases left the grid open. Retail input confirmed delivery. Ordinary field
iDroid on that candidate passed seven state cases with both outcome eyes
reviewed, yet the exact recording still showed a detached small handset at
first opening and left-forearm Map chrome for one closing source. The current
handheld outcome pass does not yet clear those exact continuous-boundary defects.

Remaining functional checks cover **both handheld and spatial modes**:
field menu navigation, continuous transitions and the
unfinished ACC category/mode combinations. Earlier sustained motion and
intermittent arm visibility failures remain in the coverage ledger. An earlier
startup timeout before the first game image was followed by a successful
same-DLL boot; its initial cause remains unexplained.

Performance is still unresolved. A prior `f694be5341fb` capture-off field
window measured 87.67 XR submissions/s and 54.98 fresh pairs/s; exact
`xrEndFrame` reached 811.755 ms while layer preparation stayed below 0.182 ms.
Those measurements identify a submission stall, not its cause or the current
candidate's performance. Recommended per-eye resolution, FOV and freshness
limits remain intact. Physical Quest motion, readability and performance need
hardware testing; simulator results do not establish them.

Supply-drop cues, intermittent weapon selection, forced tutorial transitions
and cutscene camera/animation reports remain open. The reported fight and
weapon-selection symptom are not yet identified precisely. Existing
replacement cues do not identify supplies.

The user authorized a targeted experimental patch without waiting for broader
proof coverage. Explicit `--allow-known-issues` packaging retains the five
blocked feature records and their full coverage in `RELEASE.json`; it does
not mark defects resolved. Clean committed source, a checked build, installer
output, public defaults and file hashes remain required. Whole-game and
physical-headset acceptance remain incomplete. See the
[patch notes](RELEASE_2026-10-02.1.md) and
[regression coverage](REGRESSION_COVERAGE.md). Test cleanup releases controls
and closes only owned game/runtime sessions; Steam is retained.

### October 2 release source

The October 2 experimental release contains the verified Map marker/footer
binding fixes, native prompt route corrections, support-hand contact repair,
bound-touch input ownership and bounded test recovery. Release notes are in
`docs/RELEASE_2026-10-02.md`; the package's `BUILD.json`, `RELEASE.json` and
`SOURCE_REVISION.txt` identify its exact committed source and checked files.
The final native candidate `600ec71d1b2a` passed all 41 CTest groups and
installer transactions. Local work continues in the fixed `play/` tree.

Retained `20261002T145255433589Z` passed Map opening, native-button mode on/off,
Back, immediate reopening and final Back on restored personal settings.
All twelve outcome eyes were reviewed. The footer now shows the effective
Y, left-click, right-click and LT/RT bindings, returns to native graphics in
native-button mode, and restores VR captions afterward. The actual native
Map uses mode 82/help IDs 35--38; earlier mode 46/47 admission rejected every
row. Admission now requires the observed owner, node, row and mode identity.
Long remaps, other languages and physical-headset readability remain open.

Retained `20261002T145122033446Z` reached Mission 10020 through physical
Continue on that candidate; both final arrival eyes were reviewed. Earlier
`20261002T144635365551Z` on `e7091ee0d2a5` passed standing, crouched, prone,
crouched and standing with sampled VR A input and neutral cleanup. A 120 ms
tap crouches; the separately tested 300 ms hold went prone. This confirms
the scoped existing stance flow, not a reproduction or closure of every
reported hardware crouch issue.

Retained `20261002T135259921572Z` passed the reviewed autosave action and reached
the native title sequence with clean supervisor exit. Retained
`20261002T135547048922Z` reached Mission 10020 through physical Continue, with
both final arrival eyes reviewed. These are startup/arrival results only.

Retained `20261002T144514844155Z` on `e7091ee0d2a5` passes physical binocular
equip, fixed-pose 2x-to-4x-to-2x zoom and stow. All eight final-eye images were
reviewed: magnification changes inside the physical ocular while housing,
palm contact and the outside-world view stay fixed; stow removes the housing.
The scoped manifest is `artifacts/bot/acceptance/binocular-fixed-pose-e709.json`.
Enemy acquisition, explicit marking, continuous motion and headset acceptance
were not tested. Native material insertion remains zero despite observed
three-target binds, so the lighting repair is not accepted. The experiment
now defaults off, including older INIs without the setting; the existing
physical housing rendering remains available. Explicit opt-in preserves the
same strict source, geometry and material checks.

Retained `20261002T142717882392Z` on `77ade9c8950d` passed ordinary iDroid
opening, two hand poses, stow, immediate reopening and final stow; all twelve
outcome eyes were reviewed. The optional native effect writer now admits the
observed complete three-particle batch with transactional transform/bounds
checks. Native writes were observed, but exact cone endpoint alignment,
forced tutorial stow/reopen and headset acceptance remain open. Cone fitting
and its diagnostics remain off by default. A separate custom-fit replay was
reviewed but its scratch pixels expired before promotion; it is excluded from
retained visual acceptance.

The bot waits for real pixels after a sampled startup action only while the
known boot owner and released controls remain verified. Exact read-only title
observations may use one eight-second response deadline on the same request;
actions retain the ordinary deadline and are never replayed. The focused
supervisor and behavior suites pass 27 and 198 tests. The native title replay
exercised that admission; its responses were fast, so the delayed-response
branch has unit coverage rather than a native timeout-recovery pass.

The next playability work remains binocular environment lighting, forced
tutorial iDroid lifecycle, cover/turret/traversal defects, all-weapon sight,
muzzle, hit and reload alignment, and continuous physical-headset acceptance.
These scoped simulator passes do not certify whole-game completion. Personal
settings are restored after tests, owned test sessions are closed, and Steam
is retained.

### Earlier investigation: October 2 overnight

The installed local candidate `83fce4fc8ebc` passes all 41 CTest groups and
installer transactions. The exact owned canonical native material shader was
imported locally and verified; source archives and saves were untouched. This
candidate adds guarded binocular native material insertion, bounded iDroid
Light discovery for already-loaded effects and one bounded recovery for an
unsampled axis pulse. Its native lighting and projection acceptance is pending.

The previous local candidate `0143f88c264c` passed all 41 CTest groups and
the installer transactions. `play/` and the recorded game installation match;
personal controls remain unchanged. Its current primary firearm support attachment passed
five scoped simulator cases in retained `20261002T095140317683Z`: exact contact,
rigid translation left and right, distant-hand rejection and release to one
hand. All ten during-checkpoint final eyes were reviewed and the scoped
manifest is `artifacts/bot/acceptance/field-smg-support-0143.json`. Continuous
motion, finger articulation, reload, the reported compact silent long gun,
muzzle/hit alignment and physical-headset acceptance remain open.
Runtime resource `27873` does not identify a weapon class/model/definition.
The native left-arm display separately showed AM MRS-4, DMG31/175; legacy
`smg` case/file names are retained trace links, not a verified SMG classification.

The same candidate reproduced the binocular housing's environment-lighting
mismatch. A native GBuffer material insertion is being implemented; no lighting
fix or projection-cone fit is accepted yet. The early-field movement probe also
failed in `field-mg-approach-06`: a 654 ms simulator `xrEndFrame` blocked across
the entire independently expiring 250 ms stick pulse. Controls stayed neutral
and the actor did not move. The runner must distinguish that unsampled pulse
from consumed input while keeping freshness, expiry and displacement limits.
Retained `field-mg-approach-07` then passed the short bootstrap with two sampled
250 ms pulses and the native NAV2 route to the coarse turret goal, life6000.
No swallowed-pulse retry occurred in that successful replay. Enemy fire later
interrupted stationary turret review; no mount input was sent and the owned
test session was closed. Steam remains running. Turret, cover and whole-game
gates remain open.

On `0143f88c264c`, ordinary field Pause open/Back/immediate reopen/second Back
passed in retained `20261002T104509634657Z`, with all eight final eyes reviewed.
`20261002T105356113073Z` separately reached Options and Control Settings in
both eyes. Three rapid sampled Back presses returned only to ordinary Pause;
later maintained cleanup returned the live field. The nested Back case is
failed and retained for timing investigation, not a proven menu lock or
all-Options acceptance. Future Back steps require released stabilization and
fresh page review between transitions.

The previous integrated candidate `369713f9251c` passed all 41 CTest groups and
the packaged installer transactions. `play/` and the recorded game installation
match; personal controls remain unchanged. This build adds the corrected
native R3 destinations for Items Use and Commands Confirm, the native vehicle
L1 destination, and ownership recovery for every effectively bound capacitive
touch sensor. Hardware touch dispatch and these additional prompt families
still require native acceptance.

Fresh popup diagnostics now distinguish the current numeric/StringId identity
from the signed last completed response. Unknown readiness remains unknown;
the completed response is never treated as the highlighted choice. Input-tag
traces have a separate bounded budget so typewriter subtitles cannot consume
the menu audit. These are investigation tools, not menu pass results.

The candidate also observes the popup's current selection separately from its
last response. A typed native update callback supplies a bounded owner lease;
the reader requires a unique fresh owner, matching dialog/backlink, valid
button count/index and stable repeated reads. Unknown choice remains null.
The bot now honors popup prerequisites on cursor navigation and rejects a
changed dialog before dispatch. Native choice-reader acceptance is pending.

Retained `20261002T073555368643Z` reproduced the helicopter customization
selector, Back dialog, physical selection of Discard, cabin return and immediate
iDroid reopening. Both final eyes were reviewed. Daily Bonus and Skulls Attack
information cards were then acknowledged separately and the physical map
returned. Mother Base Help opened with the effective Menu hold. A reused Confirm
close recipe failed and left Help visible, so the whole investigation is
**failed**; the private close recipe now follows its actual Hold Menu footer and
still requires a fresh native replay. The maintained failure cleanup closed the
owned game/runtime, retained Steam and left no test menu open. No upgrade
purchase/completion or whole-menu certification follows from this selector run.

The early field fixture exposed a separate startup Online Information branch.
Automatic Continue timed out in `20261002T081345580078Z`; both eyes showed the
unhandled information page. Supervised `20261002T081959231430Z` separately
reviewed and exercised Next, Close and the return to the title. This is retained
startup investigation, not field or full-game acceptance.

On public DLL `959f9ac763fb`, retained run `20261002T050047479366Z` passed
ordinary field Pause open, Back, immediate reopen and second Back. Both final
eyes were reviewed at each outcome; the spatial panel and live field return
were visible. Tutorial Resume/Skip, nested Options and cutscene Pause remain
separate cases.

Retained investigation `20261002T051649249386Z` reproduced the binocular
housing's environment-light mismatch at sunset and night. Its physical ocular
reached about 4 cm, then the 3 cm safety limit; stowing restored gameplay.
An overstrict transient grip-position predicate failed at 4 cm. The firearm
inspection then failed to establish an active native firearm, so the overall
run is **failed**, not acceptance. Inputs were released and the owned game
session closed. Original settings and six original save files were restored
before the next isolated ACC replay. Do not infer weapon readiness, continuous
stability or headset comfort from the close-up stills.

The ongoing scope remains every known community defect and the user's reports:
menu and upgrade recovery; accurate binding prompts; physical optics and hit
alignment; binocular lighting/contact; iDroid emitter/screen fit and forced
stow; weapon/support/reload contact; cover/Y camera continuity; turret pitch and
reticles; traversal; left-arm HUD readability and temporal stereo stability.
An automated build pass does not close this scope.

The completed-login notice was reproduced in both final eyes in retained
`20261002T055911070140Z`: startup remained at `Seq_Demo_LogInKonamiServer`,
native popup template 1 / StringId `0x4dc3cae5b486`, with **Logged in to server**
visible. The runner now acknowledges only that verified active identity, once,
and rechecks it before dispatch. Retained `20261002T060448782406Z` exercised
the corrected branch and physical Continue to the ACC; neutral inputs and hand
poses were restored. Progress, unknown results and closed dialogs receive no
generic confirmation.

Retained `20261002T060559660205Z` completed a bounded ACC investigation with
clean menu/input/pose cleanup. Both eyes were reviewed for actual Development
Weapons/Items, Buddy Equipment, Helicopter and Security Devices lists and their
ordinary Back paths. One reused Weapons/Items recipe instead reached Helicopter,
and a presumed Customize route reached Resources; those labels are not accepted
as their intended destinations. Helicopter selector Cancel/Discard, upgrade
completion, all other modes and full-game acceptance remain open.

The test-loop continuation adds bounded proxy stderr diagnostics and one
read-only reconnect after an unheld capture timeout. Its original partial pair
is retained and invalidated; fresh native observations and a completely new pair
are required. Input RPCs are never replayed. Actual recovery still needs a native
timeout replay. The supervisor's review window is now 60 seconds for two-eye
review/tool latency; native control freshness and presentation gates are retained.
New opt-in iDroid Light and weapon-component diagnostics identify existing
native owners/resources without changing geometry, bones or control behavior.
The Light cone's separate player owner and transform repair remain unresolved.

Retained `20261002T065631090452Z` on candidate `fd35a92ed161` exercised
physical Continue after the autosave and exact completed-login notices and
reached the ACC with restored hand poses. Both arrival eyes were reviewed.
Retained investigation `20261002T065751143255Z` reached the actual Development
category page, returned to Mother Base, selected the reviewed Customize row
**above** Development, and entered the actual helicopter selector. Its Back
dialog had Cancel selected; physical Confirm returned to the same selector,
with both final eyes reviewed. The overall investigation is **failed** because
the bounded review deadline expired while that selector remained open and
cleanup refused an unreviewed exit. The exact owned game/simulator session
was then closed; Steam remained running. This is scoped Cancel-path evidence,
not full upgrade or successful roundtrip acceptance. Discard and immediate
reopening still require their own completed replay.

The noncompact two-hand guidance now follows the acquired native support-palm
contact rather than the muzzle axis. A measured exact-contact counterexample
previously swung the primary palm 18.4 degrees and moved the authored contact
10.1 cm. Eight geometry checks now preserve exact contact, follow moved contact
and retain rigid-frame invariance. The compact FAKEL branch, muzzle socket and
shot axis are unchanged. Native rifle motion and physical grip comfort are open.

The optional binocular lighting probe identifies exact native pixel shaders,
reads their bounded constant buffers asynchronously without a GPU wait, and
requires the accepted stereo source plus matching native camera matrices before
reporting a coherent frame. It changes no rendering. The housing lighting fix
still needs these actual head-pass values; a diagnostic contract pass is not
visual acceptance. The first ordinary iDroid replay installed its Light probe
but produced no graph registration. Bounded hook diagnostics now distinguish
the missing loader path from rejected identities; no projection fix is claimed.

### User request status

The release is **not accepted from start to finish**. Keep this list separate
from build passes and from older scene evidence.

The current local work adds a shared live 3D settings editor and revises the
iDroid holding frame. All 55 VR adjustments have a fitting view; hand/device
position and rotation, screen width/distance/origin/rotation, left-arm HUD,
optics and spatial menus update immediately in the reference preview. Resetting
a fit preserves bindings and interaction modes. See VR_FIT_SETTINGS.md.

The local fit build, identified in `play/BUILD.json`, uses controller grip position with the same-frame
pointing orientation, retaining the native right-wrist CNP and cupped fingers.
The hologram now rises from a lower-edge anchor above the projector. The first
anatomical-palm attempt made it edge-on in game and was rejected. The revised
default and configured-fit runs each pass seven native cases, including
distinct grip/pointing bases, inspection and ordinary immediate reopening.
The retail light cone still fails to retarget to every nonzero screen
offset/rotation. That defect, forced tutorial stow and physical ergonomic
acceptance remain open. Public package identity is recorded in RELEASE.json;
the local candidate and historical runs keep their own exact build identities.
The editor passed 38 headless fixture checks, including complete save/reload,
discard, orbit/zoom, accurate fractional text scale and reset preserving
handheld mode. The retained controller guide still resolves 98 action locations
across eight contexts. The focused bot contracts now have 138 checks.

Inline VR prompts use the effective personal bindings and shared native action
markup, keeping translated captions and unidentified icons. Compact `LT/RT`
fits Map's bottom-row slot in both eyes. Five scoped Map cases passed in retained
`20261001T225420725862Z`, including default Confirm dispatch and native-button
mode on/off. Its stock A graphic remained visible, so prompt replacement is not
accepted. The failed group/widget hiding changes are removed; the current
direct Confirm-image build `822a8077d38a` now passes seven scoped default cases
in retained `20261002T003629964618Z`, with both-eye review of default prompts,
native-mode stock-image restoration and steady reopening. Remapped run
`20261002T004055181967Z` passes five native cases: both eyes show `[X + L GRIP]`
with no stock A, and that physical chord reaches native Confirm. Its initial
reopening pair still spans the native animation. Controls were restored exactly.
The reviewed Map scope is recorded under `artifacts/bot/acceptance/`; other
prompt families and physical headset acceptance remain open. Map captions now
default on; the broader prompt adapter remains opt-in. The expanded footer
is still pending the native repair described above. See VR_CONTROL_PROMPTS.md.

Coco's `scope_stabilizer.patch` filters native ocular drift relative to
the authoritative aiming hand, with the objective sharing the same rigid
correction. **Steady scope glass** defaults on and remains separate from the
older optional hand/sight stabilization. Synthetic jitter/reset checks pass;
this is not physical headset acceptance. Coco's world-brightness bug was
reproduced in native Mission 10020: moving only the physical binocular changed
the head adaptation by 0.30 in its retail scalar. The repair gives the lens
separate registered CPU luminance textures, GPU readbacks and adaptation
history, restoring the head owner immediately afterward. Cloning only the GPU
readbacks was insufficient because their CPU destination remained shared.

The matched no-lens/shared/isolated field run `20261002T023059054018Z` and cold
default-on replay `20261002T024344212455Z` each pass 16 scoped samples with
both-eye review. In the latter, head means are -13.3062 without the extra lens
and -13.3091 with isolation; the lens still adapts independently and head
motion still changes ordinary exposure. These are native scalars, not claimed
EV units. This repair is now **on by default** in `play/` on supported TPP;
Ground Zeroes stays untouched. Firearm-specific motion, continuous flicker,
other lighting scenes and physical headset acceptance remain open. See
WEAPON_SCOPES.md and `artifacts/bot/acceptance/optic-exposure.json`.

The local weapon fit adds a separate **One-hand weapon tilt**, default -30
degrees, applied through the common native firing wrist. Gun, hand, physical
sight and muzzle retain one owner. Binoculars, iDroid, menu pointers and the
left-arm display do not receive that trim. Older personal INIs with a nonzero
general right-hand pitch receive zero additional automatic trim; explicit
firearm-pitch values remain authoritative. See VR_FIT_SETTINGS.md.

The current-loadout inspection covers FAKEL (SLEEP), AM MRS-71 and URAGAN-5
AIR-S. Both scoped guns publish a barrel/optic axis at the requested -30
degrees in the stationary one-hand case. This does not certify impacts,
reloads, support contact, weapon transitions or physical headset comfort.
The latest retained run `artifacts/bot/runs/20260930T215049265217Z` on local DLL
`5761c47a74a0` passes 19 scoped cases: the three one-hand loadout inspections,
support acquisition on both long guns, seven iDroid fit/ordinary-reopen cases,
and seven helicopter-list navigation/Back cases. Both-eye support contact was
reviewed, but the reported FAKEL grip feel and every reload/transition remain
open; acquisition alone does not certify a perfect grip.
The owned equipment worklist contains 418 firearm definitions across 83 model
entries, including eligibility and variant questions; those are a coverage
queue, not 418 passing weapon tests. See WEAPON_FIT_AUDIT.md.

The reproduced completed-FOB ACC **Choose a Rival** lock-up is repaired.
Its native guide disabled all 78 menu entries and retained its own pause after
Help dismissal. Recovery restores each captured prior disabled bit, preserves
independent restrictions and releases the exact guide pause handle, only in the
completed ACC/main-game context. ACC root Back completes the closure already
requested by the native menu. It does not cancel unfinished tutorials or edit
campaign progression.

Candidate DLL `d893eb437a06`, retained run `20261001T181154160585Z`, reached the
actual Missions, Map, Mother Base, Development and Support Helicopter Armament
pages through physical VR bindings. Eleven scoped cases passed: three Back
levels restored the live cabin camera/control owner, then physical iDroid
reopened with Development still eligible. Both eyes of the leaf, cabin and
reopening were reviewed. The later Daily Bonus test rejected its own already-
satisfied outcome before input; the overall run remains partial.
A second cold boot repeated Help dismissal and restored eligibility. Its
12-sample Back test exceeded the two-second review deadline despite a returned
live cabin. The maintained `acc-completed-rival-roundtrip.json` uses three
stable samples with the same camera/control predicates, matching cleanup.
Retained `20261001T183825927585Z` proved physical Confirm dismisses
Daily Bonus and reveals the separate Skulls Attack informational card.
That card received no stale confirmation. See ACC_MENU_REGRESSION.md.

Final DLL `b6fd45533f6f` passed all four maintained completed-Rival cases on
another cold boot in retained `20261001T190258541444Z`. Both final-eye pairs
show the actual Help dismissal, live cabin return and reopened iDroid. Static
captures establish those page/ownership results; they do not establish
continuous flicker-free or physical-headset acceptance.

The current default-exposure build `431d6cb9a5ad` repeats all four maintained
Rival cases on another cold boot in retained `20261002T025907840443Z`.
Both eyes of the Help dismissal, cabin return and immediate reopening were
reviewed. Its test launcher uses the existing Steam client and a temporary
game-local runtime configuration; cleanup restores the exact prior bytes or
the original absence, preserving any concurrent personal edit.

Community Development and stair reports supplied October 1 have unknown build
identity; the user says they may predate the local fixes. Both attached videos
show metal stairs/catwalk movement, not Development. Keep the menu report and
traversal report separate and exercise the current candidate in ACC and on foot.
Retained warm run `20261001T191943458398Z` missed a 120 ms iDroid tap; the
300 ms tap in `20261001T192821425533Z` opened the same native ACC correctly.
That run also exposed bot cursor overshoot: two sampled holds moved Rewards
to Staff Management instead of Development. Cursor steps now use one fresh
sampled edge; an iDroid scroll hold must be explicitly selected. The longer
default iDroid tap remains below the effective personal hold boundary.
Native replay passed on cold `20261001T193945164460Z` and warm
`20261001T194125435098Z`: the default tap opened and two sampled edges reached
Development exactly. Player bindings are unchanged.

Warm ACC runs reviewed all four Development categories. The weapon cost/time
confirmation canceled successfully. Security Devices returned visually to its
parent in `20261001T195300890687Z`, but a later neutral-input acknowledgment
timed out; that case stays failed. Fresh `20261001T200807859685Z` repeated the
Security entry/Back successfully and dismissed both the Daily Bonus and distinct
Skulls Attack information cards through reviewed physical Confirm.
That run also purchased the eligible AM D114 LA development: native GMP fell
by exactly 1,633,120, Common Metal by 31,300 and Minor Metal by 29,800, matching
the reviewed confirmation. Completion is pending its native development timer;
the developed count stayed 453. Cleanup restored the live cabin camera, neutral
controls and saved hand poses. Nineteen scoped action outcomes passed; these
are not nineteen complete menu families or stereo/flicker acceptance.

The completed ACC fixture has all 19 native helicopter development definitions
already completed. The reviewed body grades and Speaker Confirm are no-ops,
not successful new upgrades. `tools/development-state.lua` reads actual
completion, eligibility, grades and resources for selecting a useful fixture.
Eligibility alone also includes already-completed items. The earlier paired-save
fixture reached Mission 10020, where Mother Base's reviewed page was empty.
That early field state does not establish usable on-foot Development. Retained
`20261001T213743341533Z` passed physical iDroid open, root Back and immediate
reopening. Cleanup returned the live field camera, neutral input and saved hand
poses. The bot's transient null-input-audit cleanup failure is repaired; the
freshness and stable-playable-state requirements remain in force.

The distinct ACC helicopter customization selector now has a retained native
entry/Back/discard/live-cabin/immediate-iDroid-reopen pass in
`20261002T040239079392Z` on DLL `431d6cb9a5ad`. Ten scoped action outcomes pass;
both eyes of the selector, cancellation dialog, cabin and reopening were reviewed.
Back opens a cancellation dialog with Cancel selected; it does not immediately
return to the cabin. The corrected bot supports the exact selector and its
popup through the effective VR menu stick. The reopening stills span a queued
Daily Bonus card, so they do not certify a stable common menu image or continuous
refresh. The original saves and personal controls were restored; the temporary
runtime configuration was removed and Steam remained running.

The community helicopter-upgrade report remains open beyond these scoped paths:
helicopter purchase/result, locked and unavailable grades, selector part changes,
unfinished guides and physical headset use need separate acceptance.
The paired-save fixture tool supports verified
campaign/personal snapshots and six-file rollback while Steam remains running,
with both cloud mirrors required to be disabled. The private
`acc-completed-fob-baseline` reproduces this completed-guide flow; it does not
make every upgrade eligible or certify purchase completion.

The all-menu VR queue now keeps menu families, native game states and handheld,
left-arm/spatial presentations separate. `python tools/workspace.py coverage --menus`
refreshes one fixed review view. Page/choice discovery, guarded nested recipes,
progression-changing isolated fixtures and physical-headset acceptance remain
open. See VR_MENU_COVERAGE.md; a matrix count is not the number of game screens.

The map/state foundation now has a native-location-selected navigation atlas and
a directed state-transition planner. The current owned manifest still covers
only 40 Afghanistan tiles; the other declared worlds remain unimported. The
factual registry and declared state/transition obligations are viewable with
`python tools/workspace.py model`. Four coarse control-owner probes have guarded
recipes. The authored inventory adds 35 named states from the owned ACC/common
helicopter and Mission 6 sources, with literal target references retained as
blocked obligations. Exact menu pages, native roles, sequence conditions and
VR recipes remain unresolved. See GAME_STATE_MODEL.md. This is infrastructure, not full-game
acceptance, and its counts are not a percentage of the game.
An explicitly selected guarded transition can now run through
`tools/workspace.py bot --command state --transition probe.idroid.open`;
the state planner compiles a route with a neutral return, and the existing
native VR runner stops dependent steps on the first failure. Pause still
requires an explicitly named Pause transition.

The resumed native session reaches the ACC (mission 40010) by the real Pause
Return to ACC confirmation and subsequent physical Continue. Steam stays running.
The state-planner route command itself has not been replayed in game. The native
Pause navigation/return result and ACC observations do not certify every graph
transition, menu page or mission.

The live ACC also exposed a rendering bug: it drew the title-only cassette rack
with Continue/Options/Delete/ Quit choices during ordinary cabin play. That
extra render branch is removed; the real title rack and physical Continue still
work. Both final eyes were reviewed at the title and in the native ACC cabin.

Earlier completed-guide menu, held-Back and stale-camera failures remain in
ACC_MENU_REGRESSION.md. Cleanup requires the live camera and rig input to return;
a cleared terminal flag alone cannot pass. Unknown native Help/popup owners
receive no ordinary navigation or blind confirmation. Failed owned sessions
close with Steam retained.

Startup testing also exposed a popup-admission race and a login result stall.
A popup that closes during capture now returns to observation only when no
input was dispatched. Unknown login dialogs receive no blind confirmation:
two cold boots progressed naturally, while another stopped on Logged in to
server. That result identity and reliable unattended cold startup remain open.

The live Pause navigation run reproduced repeat overshoot: three sampled holds
selected Options. The corrected Pause path uses a single fast sampled edge,
with full Lua prerequisites checked before input and after release; three edges
now select the actual Return to ACC row in both eyes. iDroid's existing sampled
hold is preserved. A separately selected `pause_popup` owner requires an explicit
popup prerequisite and visual review before selecting any confirmation.
The resumed supervisor also failed on a Windows status-file replacement conflict.
Status writes now retry only the same atomic file transaction for at most 500 ms;
the previous complete document remains readable. A real Windows reader-handle
test passes. These repair scoped test-loop defects; the ACC upgrade report remains open.

The ACC exit trace confirms that helicopter customization uses the native
`Customize_Abort`/End path and `CustomizeSelector` pad mask; held-Back iDroid
recovery cannot close that selector. The scenario query now reports the native
customization kind and our separate player-pad exclusion. A guarded, unrun
`acc-helicopter-customization-back.json` probe sends one ordinary VR Back only
on an already-open helicopter selector, rechecks its native identity immediately
before input and requires return to `Seq_Game_MainGame`/cabin. It performs no
purchase, forced terminal close or save write. This is reproduction tooling,
not a claimed fix, and does not cover development through iDroid.

The cutscene's hologram graph-root transform has now been recovered: handset
orientation, 9 cm forward of `CNP_HOLOGRAM`. Physical canvas bounds remain
unresolved. Raw canvas units and effect offsets alone do not establish them;
the graph-root measurement is not a claim of a 100% cinematic display match.

| Request | Current result |
| --- | --- |
| One checkout and one stable playable folder | Implemented: this checkout, fixed play/, Play.cmd, automatic verified local synchronization. |
| Stop making dated local distributions | Implemented; 37 old generated folders/archives removed, 2.904 GiB reclaimed. |
| Keep Steam running; no computer control | Preserved throughout this work. Only owned test sessions and explicitly authorized unused simulator helpers are closed. |
| Left bionic forearm weapon display and ordinary HUD | Restored authored layout; both eyes and head/hand motion reviewed on the display baseline. Physical headset reading and every HUD context remain open. |
| Crisp high-resolution VR, stereo; desktop 720p | Native 2520 Ã— 2640 per eye, distinct updating final eyes, desktop 1280 Ã— 720. Continuous physical refresh and comfort remain open. |
| Stop simulator blinking and stale test loops | Large swapchain failure repaired; decoded-eye blank/freeze checks and bounded cleanup implemented. Reviewed recordings contain no observed blank frame; sub-sample flicker is unproven. |
| Pause and optional off-wrist HUD as spatial stereo | Paired off-wrist baseline passes retained. Current iDroid build passes native pause open/close and both-eye review. Physical headset and all off-wrist contexts remain open. |
| Optional scope stabilization | Installed at personal 100 ms; measured sampled-grip yaw jitter fell 72.5%. Public default remains off. Physical comfort and sight/muzzle/hit checks remain open. |
| Bring physical binoculars closer to the face | Installed: separate 3 cm safety clearance from 10 cm optical reference. Native observed ocular now reaches approximately 4 cm when requested, instead of stopping at 10 cm. |
| Keep binoculars inspectable, hand-aimed, zooming, acquiring and explicitly marking | Existing interactions preserved; no second zoom window. Current-build mission/acquisition/marking regression remains open. |
| Binoculars and hands always in the same space | Existing common final-palm publication preserved. Intermittent report and transition coverage remain open. |
| Match binocular lighting to the world | Open. Custom housing lighting is fixed and does not yet consume the native environment. Lens lighting under hand motion also needs reproduction. |
| Keep cover/lean/shoot and Y contextual actions without a third-person camera pull | Open. Native cover remains enabled; reproduce the reported wall transition before changing camera ownership. |
| Remove remaining turret reticles; preserve native yaw and pitch | Open. Need native mounted-state and vertical-travel reproduction. |
| Repair silent other long gun's support hand | Open. Native wrist selection identifies the other long gun as FAKEL (SLEEP). Native support-socket telemetry is installed; reproduce the bad two-hand contact before changing grip behavior. |
| Map capacitive face-button, stick, trigger and thumb-rest contacts independently | Implemented and installed: ten separate inputs, parser checks and fresh simulator runtime initialization pass. Physical sensor acceptance remains open because the simulator operator cannot synthesize capacitive contact. |
| Show the actual mappings and a quick video in the launcher | Installed in the fixed play tree: saved mappings, alternatives, one-hand/two-hand chords, ten amber contact surfaces and red pressed surfaces. 98 UI actions and eight settings-bridge contexts checked; 44-second personal mapping video available at artifacts/dev/controller-mapping.mp4. |
| Live rotatable 3D previews for the settings | Installed: all 55 controls settings have a reference fitting view, with live edits, drag/orbit/zoom, front/side/top/back, filtering, save/reload, discard and numeric-fit reset. Runtime theatre dimensions are also previewed. Reference geometry is separate from native projection-effect and headset acceptance. |
| Default -30 degree weapon tilt and check the arsenal | Firearm-only fit implemented, configurable and installed; current-loadout one-hand scope direction inspected. All-weapon, impact, reload, support-contact and headset coverage remains open. |
| Helicopter upgrade menu coverage | Completed Rival restriction repaired; current candidate reaches the actual ACC helicopter list, three-level exit and physical reopening. Selector, locked grades and purchase/result coverage remain open. |
| Audit one-hand/two-hand control chords | Effective personal binding audit retained; no unsolicited remapping. Headset usability and crouch/prone community report remain open. |
| Align right-hand iDroid with its native emitter/cutscene and survive forced stow/reopen | Native attachment retained; natural holding frame and lower-edge screen anchor revised. Default and custom fitting controls pass scoped simulator motion/reopening checks. Native light-cone retargeting at custom offsets/rotations, forced tutorial stow and physical headset acceptance remain open. |
| Preserve Coco's fixes; burn down community reports; all weapons/missions/game completion | Coco's scope-glass stabilizer is integrated and configurable. Independent lens exposure is on by default after matched native field comparisons and a cold replay. Firearm-specific optics, remaining reports and full-game acceptance remain open. |
| Publish the new build on GitHub | September 30 experimental prerelease packages the tested local DLL and committed source; see RELEASE_2026-09-30.md. Full-game and headset acceptance remain open. |

The next iDroid work is unfinished-guide/upgrade transaction coverage, exact authored screen fitting and native light-cone retargeting,
then physical fitting/readability. FAKEL (SLEEP) support-hand reproduction,
cover camera ownership, mounted weapon aim/reticles, binocular native lighting
and full mission regression remain on the existing release queue.

The September 30 released DLL is `edc6960890a4b45f`. It retains the left-arm display,
binocular clearance, scope stabilization, touch inputs and launcher tour. The
iDroid repair replaces the guessed anatomical palm mount with the measured
native right-wrist attachment and the device's named connector/hologram sockets.
That build's upright simulator grip holds the device upright; the projection inherits
the final rendered device pose. The authored native mount and screen stay
together during hand motion. See `docs/IDROID_ALIGNMENT.md`.

The released build's 38 automated checks and installer transactions pass. Its focused bot suite
has 93 passing checks, including safe input cleanup and explicit numeric pose
tolerances. Native Continue reached Mission 6; all 11 selected iDroid/menu cases
passed on this DLL. Both final eyes were reviewed at normal and opposing side
poses, through ordinary close/reopen and pause. The opening animation appears
in early captures; the settled interface is visible in full in both eyes.
These sequential stills establish scoped fit and menu behavior; they do not
establish continuous flicker, refresh rate, forced tutorial stow or headset
readability. Exact results are in `artifacts/dev/idroid-alignment-acceptance.json`.
The test game and its owned simulator were closed with inputs released and
menus closed. Steam and the user-requested launcher remain running. Both
personal INIs retain their recorded hashes.

The display baseline DLL is `f8dda93e8d1d4aec`. All 38 automated
checks and the installer transaction checks passed on that baseline. Fresh Mission 6 gameplay at
**2520 Ã— 2640 per eye** now produces updating, distinct final compositor eyes.
The compact readout is back on the **left forearm**, with readable weapon,
ammo and suppressor text. The fix restores the native HUD's authored 128 Ã— 72
layout; it does not move arm bones or replace personal bindings.

Both final eyes were inspected through wrist translation/rotation and opposing
head/hand motion. Four 24-second recordings contain 308 decoded, distinct
frames with no blank frame or detached readout observed. These are sequential
left/right compositor captures at 3.0â€“3.4 samples/second, not synchronous stereo
video or a measurement of headset refresh. Flicker between samples, physical
comfort and the rest of the game still require acceptance.

On that display baseline, Pause opened and closed on a spatial stereo panel with wrist HUD enabled and
explicitly disabled. Off-wrist ordinary HUD also remained spatial in both
eyes. iDroid opened and closed normally, but the tested hand pose clipped its
projection in the left eye. That historical failure is retained; the native
mount repair and current review above supersede its fit result.

Exact build hashes, retained runs and scoped review outcomes are recorded in
`artifacts/dev/display-acceptance.json`. Personal VR settings and controls were
restored byte for byte. Test inputs and poses were released/restored, test menus
closed, and both owned game sessions stopped. Steam was retained.

The first physical `Play.cmd` launch exposed a stale global runtime entry for
the removed Meta XR Simulator v205. The recorded previous Oculus runtime was
valid, but the launcher skipped that fallback when the active file was missing.
The fallback now handles a missing active manifest as well as a selected
simulator, without changing the global registry. `Play.cmd` retains a fixed
`artifacts/dev/headset-launch.log` and keeps errors visible. A connected Quest 3
was detected and MGSV was launched through Oculus; its focused XR session and
live controller inputs were observed. The user's game exited normally on
September 29 at 18:56 PDT; subsequent simulator checks use owned test sessions.
Never inject or release inputs in a user-owned play session.

The binocular clearance and adaptive scope stabilization are installed in
DLL 10726111959b. Native Mission 6 before/after observations show the close
ocular change and the measured grip-jitter reduction. The static AM MRS-71
grip inspection is retained under artifacts/bot/runs/20260930T031625630435Z.
The earlier DLL e54dac7bc79f added all ten touch inputs and native support-socket
diagnostics. Its native Continue reached Mission 6 with the new controls/rig
telemetry. The current DLL retains that work and adds the native iDroid repair;
the launcher tour is verified and synchronized into the same play tree.

## One place to work

Use this checkout. `play/` is the last published release, and `build/dev/` is
the checked testing candidate and only development tree. Each `BUILD.json`
identifies its source, checks and files. `Play.cmd` restores pinned binaries before physical headset launch;
`Play-Dev.cmd` installs development instead. Personal game settings remain
authoritative. Close MGSV before switching between different binaries.

```powershell
# Build and check development without changing pinned play or the installed game.
tools/build.ps1

# Explicitly install development for debugging (MGSV must be closed).
tools/build.ps1 -Deploy

# Compare both build slots, the pin, installed DLL and current source.
python tools/workspace.py status

# After an authorized release is published and its exact payload verified:
python tools/workspace.py pin-build

# Inventory old copies; add -Apply to remove them after both slots verify.
tools/clean-local-builds.ps1

# A temporary game-local runtime lease is restored by stop-sim; Steam stays open.
python tools/workspace.py launch-sim
python tools/workspace.py bot --command continue
python tools/workspace.py bot --seconds 15
python tools/workspace.py stop-sim

# Run a deliberate case, or a bounded route over the private owned NAV2 graph.
python tools/workspace.py bot --command run --suite tools/gameplay_bot/suites/pause-roundtrip.json
python tools/workspace.py navigate --goal X Y Z --seconds 60
```

`build.ps1 -NoDeploy` remains a compatibility alias for the default. Explicit
deployment checks the recorded install's ownership, preserves both personal
INIs byte for byte and records the selected slot and actual binary hashes.
Transactional staging and rollback are removed after a successful update or
completed recovery. An interrupted recovery is preserved for repair, never
silently discarded. No dated packages or persistent rollback builds are kept.
Compiler intermediates and the installed game are outputs of the two slots.
Replacement commits BUILD.json last and preserves unmanaged local files.
An unchanged open launcher stays running; failed checks leave both slots intact.

`launch-sim` installs the checked development slot, then asks the already-running
Steam client to start MGSV and records
the exact new game/runtime process generations. It temporarily selects the
simulator in `mgs5vr-runtime.ini`, without changing Steam's environment or the
global OpenXR runtime. `stop-sim` restores that file after the owned session
exits; an originally absent file is removed. Concurrent edits are preserved
and reported instead of overwritten. A timed-out launch remains recorded for
exact cleanup. The pinned build remains at `play/` throughout. Use `Play.cmd`
for the next physical play session to restore its binaries automatically.

For an explicitly requested GitHub release, commit the public source, run the
same build/checks, then use
`python tools/package-release.py --tag experimental-YYYY-MM-DD`.
The packager verifies checked development and public defaults, includes the
committed source and per-file hashes, and writes to the fixed `build/github-release/`
directory without replacing the published play pin. After successful publication
to the requested destinations, verify the uploaded archive against its recorded
checksum and verify that development still matches the packaged candidate before
running `python tools/workspace.py pin-build`. Both slots then match the release;
subsequent development builds change only `build/dev/`. Remove temporary release
archives after verification. Private game data and simulator dependencies are
rejected. Publishing a prerelease does not close the remaining gameplay/headset
acceptance queue.

The local settings file is `private/workspace.json`. The operator dependency
is imported once to `.deps/meta-xr-operator`, removing the other-repository
dependency. Retail assets and navigation remain private. Bot scratch evidence
lives under `artifacts/bot`; `latest.json` is the stable pointer. Retention keeps
three completed scratch runs, targeting 2 GiB. The latest active run and pinned
evidence can exceed that budget; use recordings deliberately. `workspace.py pin`
protects the latest run before citing it in acceptance. Existing historical
acceptance evidence is preserved. Only an explicitly selected case opens Pause.

## What the audit establishes

The September 30 experimental prerelease includes the retained September 27/28
candidate work, Coco's six supplied patches and the native A* bot. The previous
public release was September 24. All 55 community reports and 77 scoped claims
remain in the evidence ledger:

| Claim state | Count |
| --- | ---: |
| Simulator verified | 13 |
| Automated contracts verified | 4 |
| Failed | 4 |
| Needs physical headset | 7 |
| Not tested | 40 |
| Not implemented | 9 |

The ledger's RC5 DLL is c69b75c6. The installed build audited on September 29
was bc37bd31, while its install record still named RC4. The fixed workflow
records the actual current build and separates historical evidence from it.
Passing a build does not close those reports.

Coco's visibility, bob/height, cinematic camera, recovery, tracking continuity,
lower-body and shoulder work is retained. The native-map bot has demonstrated
Mission 6 bridge arrival using A* and ordinary VR inputs. General combat and
full mission completion remain unaccepted. Mission 1's real binocular lessons
and explanation/return passed on RC5; that is not complete Mission 1 acceptance.

## Immediate acceptance queue

1. Finish iDroid recovery acceptance: the current native connector/hologram
   mount passes normal grip, both-eye front/side fit and ordinary close/immediate
   reopen. Forced FOB tutorial stow and physical headset use still need actual
   game reproduction. Do not replace the native attachment with a palm guess
   or infer those outcomes from ordinary reopening. See `docs/IDROID_ALIGNMENT.md`.
2. Verify scope/reflex/holographic and muzzle/hit alignment per weapon family,
   zoom, eye, head/hand motion, reload, cover, and transition. Round scopes
   already use native sockets; reflex/holographic coverage is incomplete.
   Binoculars must remain inspectable in the hand, with one attached optic
   view, physical zoom, visible-enemy acquisition and explicit marking.
3. Audit the effective personal gestures for same-hand versus two-hand use.
   Preserve the established mapping during defect repair. Simplify only with
   explicit physical testing and current-context conflicts checked. GitHub
   crouch/prone issue #11 remains open; contracts alone do not close it. The
   recorded personal mapping uses two hands for reload (left grip + B), left
   hand only for binocular equip (left grip + Y), and right hand only for weapon
   switching (right grip + stick click). See `artifacts/dev/control-audit.json`.
4. Extend the left-arm and stereo acceptance to high-frequency capture,
   physical Quest motion/reading, all HUD contexts and recovery transitions.
5. Complete native mission/checkpoint routes, cinematic and display/recovery
   cases, and physical Quest testing. Burn down the rest of the community
   ledger against the same identified build before the next GitHub release.

Fresh baseline evidence is in `artifacts/dev`: `baseline`, `pause-baseline`,
`idroid-baseline`, `off-wrist-baseline`, `off-wrist-pause`. Native outcomes and
both-eye images are retained. The original personal INI hashes were restored;
the test game/simulator were closed. Preserved runtime work is archived in
`artifacts/dev/preserved-runtime-work.diff`. Release notes identify the scoped
September 30 prerelease; it does not claim full-game acceptance.

## Display and test-loop recovery

The desktop preview is **1280 Ã— 720**, independently of VR quality. Preserve the
active runtime's per-eye FOVs; a square target is not a substitute for correct
optics. Meta XR Simulator v207 recommends 1680 Ã— 1760 here. The current test
renders native eyes at 2520 Ã— 2640 (150% in each dimension) and filters each
eye separately into fixed 1680 Ã— 1760 simulator swapchains, in linear light.
Physical runtimes retain the direct native-resolution path. This avoids the
simulator's failed large-swapchain path without lowering native scene detail
to desktop resolution. Earlier stale large-target takes remain failed evidence.

The bot checks both decoded compositor eyes for blank or frozen gameplay,
including left-arm motion and stationary active gameplay. Startup waits for
native Lua readiness and the real title/loading fade; a blank gameplay frame
still fails. Pause may legitimately retain static pixels. The final source
and compositor recordings have separate provenance; a native recording alone
does not certify the simulator output. Recorder overhead lowered measured XR
submission cadence during some runs, so a 90 Hz physical performance claim
has not been made.

Historical display evidence and current iDroid evidence are pinned under
`artifacts/bot/runs`; each record identifies its tested DLL:

- `20260929T233417986949Z`: native Continue, left-arm cases, two-eye stills,
  `temporal-accepted` wrist motion and `head-temporal` head/hand motion.
- `20260929T233927210347Z`: deliberate Pause and iDroid roundtrips; iDroid fit
  remains open despite successful native open/close.
- `20260929T234722570591Z`: explicitly disabled wrist HUD, front stereo HUD,
  Pause roundtrip and cleanup. Original settings restored afterward.
- `20260930T062309020656Z`: current native iDroid attachment, centred normal
  grip, front/opposing side views, ordinary close/immediate reopen, pause and
  final close; 11 native passes and scoped two-eye visual review. Tutorial
  forced stow and physical headset acceptance remain open.

The local build cleanup reclaimed 2.904 GiB from 37 generated distribution
folders/ZIPs, retaining unique binaries and their inventory in
`artifacts/dev/retired-builds`. Local iterations use the same `play/` tree.
# October 12 live settings, body and camera repair

Development adds the headset gear tab for all supported live VR gameplay
settings and a VR on/off switch that keeps the panel available. Mapping
retains its existing capture, conflict, confirmation and release ownership.
The camera filter no longer reseeds from animation when two publishers share
a timestamp; native PlayerStatus now owns stance transitions. On-foot body
visibility requires a complete admitted native spine/arm/leg solve with
camera clearance and native ground constraints. Cinematics preserve authored actor skins and add
independent spectator movement in noninteractive shots. See
`docs/NATIVE_BODY_VIEW.md` and `docs/HEADSET_CONTROLS_EDITOR.md`.

The unchanged installed baseline's native posture roundtrip is
`artifacts/bot/runs/20261013T010851425099Z`; walking sway samples are in
`20261013T011336052710Z`. A fresh story profile stopped at the native Terms
owner (`20261013T011650365936Z`), so it does not establish cinematic visibility
acceptance. Current build, final-eye inspection and exact cleanup receipts
are collected in `artifacts/dev/live-vr-body-current/`. The published `play/`
pin stays unchanged. Full-body field, outfit coverage, seated avatars, complete
cutscene sequences and physical Quest acceptance remain separate gates.

On checked DLL `9c8ef2716159`, retained run `20261013T024522086164Z` applied and
restored boolean, numeric and enum settings, switched VR off/on with the editor
available, confirmed/cancelled a physical remap, rejected a conflict and equipped/
stowed physical binoculars. Both eyes of sixteen setting/remap/VR checkpoints
were reviewed; panel closure is supported by the native bot assertions. The
previous VR-on duplicate automatic-entry toggle was reproduced and corrected.

The measured owned eye fit removed the earlier stretched cloth and near-face
intrusion in both eyes of all eight phases of `20261013T023620914011Z` on
`2bbe736a023a`. The clearer `artifacts/dev/live-vr-body-current/eye-boots/`
inspection on `9c8ef2716159` still failed lower-body visibility and exposed a
near-169-degree pelvis/upper-body heading split. Coherent full-body render yaw
and published anatomical eye diagnostics were exercised in retained eight-phase run
`20261013T030910923796Z` on `11c8ba4614c5`. Eighteen final-eye images, including
`coherent-body-boots/`, were reviewed: hands/sleeves remain connected, the old
cloth tears are absent and the deeper view exposes both boots. Camera/body/grass
still obscure sole-to-floor contact; large-lean clipping and physical comfort
remain unaccepted. These field fixes do not admit seated or mounted bodies.

The final eye diagnostics exposed 7--10 cm of head displacement: rotating around
a fixed pelvis made the eyes orbit and exceeded the nearly straight native spine
reach. The complete render pose now receives eye-centered yaw and translation
before the limited spine/contact solve. Both early and final admission require
an anatomical eye error at most 2 cm; failure retains the established hand rig.
Native game actor movement/collision remain unchanged. The actual prone repeat
then exposed a separate 107.6 mm eye-basis discrepancy: the cached camera HEAD
transform and the actual animated bone-4 eye calculation agree in standing but
differ in prone. The final correction supplies anatomical eyes from the owned
original skin transaction rather than treating the cached camera basis as the
animated head. A prone ankle above the eyes also needs a foot-specific native
ray ceiling; arm/head contact policy stays unchanged. Floor contact, genuine
stance transitions and body appearance still require the final actual-game
repeat recorded in the verification receipt below.

One legitimate loading Resume confirmation started native mission 10020's
authored arrival scene and naturally returned to `Seq_Game_IDroidTutorial`.
`artifacts/dev/live-vr-body-current/cinematic-native-arrival/` contains reviewed
both-eye Snake/Ocelot visibility. The harness stopped at the changed sequence;
that take does not establish spectator movement or complete scene acceptance.
The later `cinematic-native-immediate-viewer/` take on `11c8ba4614c5` used that
same natural arrival, with zero replay requests. All fourteen final-eye images
were reviewed: native Snake/Ocelot/horse heads and bodies remain visible, HMD
motion changes the framing and the return restores both hands without Pause.
Four spectator-axis samples reached the viewer with fresh neutral retail XInput.
The later `cinematic-final-immediate/` take on `efeb56da218e` confirms actual
extra viewer translation: submitted follow offset moved 6.18 cm with fixed HMD,
four real stick samples and fresh neutral retail gamepad delivery. It completed
naturally and restored hands/controls without replay requests or Pause. Its
authored dust obscures face detail; earlier clear actor pairs remain separate
visual evidence. The result does not accept every movie or geometry clearance.

The unheld field instant camera was reproduced at lower-chest height: its
documented hip placement actually followed HMD yaw/position. It now joins the
admitted right hip, with render and pickup sharing the same guarded LOCAL
socket. Held camera, cabin resting/drop behavior, gallery and independent prints
retain their existing paths. Unadmitted body frames retain the reachable fallback.
The focused core contract passed; the actual-game repeat is recorded in the final
`artifacts/dev/live-vr-body-current/verification.json` receipt. That receipt also
records final development checks and exact save/settings/runtime/session cleanup.

The practical whole-game queue is [VR_1_0_POLISH.md](VR_1_0_POLISH.md).

The owned-eye repeat `20261013T044750075435Z` on DLL `7e4c780a36cd` completed
all eight stance/lean/walking checkpoints, with both native terrain hits in
prone and a maximum fitted-eye error below 0.5 mm. All sixteen eyes were
reviewed: connected sleeves/hands and restored standing legs/boot tops, with
the initial left inner strap triangles still unresolved. The requested prone
side/down checkpoint retained a neutral head pose and supplies no prone
torso/leg acceptance. Terrain-hit counts do not establish sole contact.
The same DLL passed retained settings/remap/binocular run
`20261013T045032866973Z` and physical camera pickup/release in
`hip-camera-owned-eye/`. Held camera contact is clear in both eyes; storage
was mostly hidden by the belt. An outward/front hip correction retains the
measured native arm reach and is repeated in the final verification receipt.
