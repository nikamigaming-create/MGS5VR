# October 21 hand fixes on main

Rifle support fingers now visibly curl in both eyes through three shots after
moving aim. Native reload replenishes5->9; reacquisition returns to the curled
grip in both eyes. Retained140842395426Z/141139376668Z. Mid-reload paired capture
expired its input lease; do not relabel that runner failure as a pass. Broad FP02
remains open for bolt-action and physical checks.
Right-wrist coupling reproduced in140937499000Z: fixed HMD/left controller,
right yaw+/-30 turns chest~30 degrees and moves left palm8.5/12cm. Correction
limits hand-driven torso heading to an acquired two-handed weapon hold. Build
completed; actual-game repeat next. Current live game is the preceding build,
mission10020 RescueMiller on foot, neutral; do not deploy over the live process.

# October 21 gameplay result: sortie preview head restored

Development bc7dd67830ce restores only mod-owned hiding during the exact native
sortie preview. Both eyes now show Snake's complete head on SelectItem and
ASAP/drop-time screens. Departure returns to clear first-person horseback view
in both eyes. FP14 fixed in development; physical headset acceptance not claimed.
Retained runs: 20261021T134958388735Z, 20261021T135550994795Z.
Native loading confirmation progressed despite runner transition failure; that
failure remains retained. Current field work is in mission10020 tutorial, with
ordinary stow and rifle support-hand movement/reload next. Published play unchanged.
Local and origin have only main; old remote branch tips are archive tags.

# October 21 consolidation on main

User requires one development branch, main, and gameplay fixes as outcomes.
The leftover owned game/simulator session has been closed; Steam retained.
No Python gameplay runner was running at cleanup. GitHub has no open PR.
Current source and overnight work are being consolidated on main. The older
README branch is patch-equivalent to incorporated work; the September grenade
prototype failed hand visibility and is superseded by the current throw path.
Both old branch tips remain accessible under archive/ tags, not work branches.
Published play remains unchanged. All broad field defects remain open.
Next gameplay target: finish rifle support-hand motion, reload and release;
then proceed through the remaining field list. Earlier live PIDs below are stale.

# Current plan — ACC readiness

## October 21 overall bug list resumed — active, support-finger candidate73fe7d0b521d under test

Previous stop after two scoped regressions did not fulfill the user's complete
field-fix list. Overall goal is active; do not close it on another narrow pass.
Published play b627c7cacc9a unchanged. Previous game15688/simulator45436 closed
neutral. Candidate73fe7d0b521d passed58 groups and installer checks and is installed;
game72840 is live on candidate73fe7d0b521d. Retained native lesson-open
run20261021T131651898999Z passed its open case; generic cleanup refused unknown
menu before any closing input. Terminal retained through authored timeout; read-
only witness is active. Explanation is playing with active unsuspended camera.
After actual RescueMiller return: ordinary stow, dismount, moving/reload cup
verification and right-wrist isolation. No active input runner currently.

Onff0dd2c746a6, retained082239334618Z moves the supported back-slot rifle and
fires3 real trigger edges,1 round each, with support retained. Both eyes still
show open support fingers. Eight read-only comparisons of18 shared body/bionic
joints match within1.73e-6, arguing against stale modular geometry in this run.
Dossier: D:/Dev/Tools/Ghidrust/workspace/evidence/mgs5vr/rifle-support-fingers/.
Candidate change uses the existing cupped finger solver only for acquired long-
gun support outside native manipulation; native reload/bolt fingers stay owned.
Build log: artifacts/dev/field-report-review/rifle-support-cup-build.log.
Next: verify checked candidate in actual game for moving contact/recoil, reload,
return to cup and free-hand release, then continue remaining checklist.

Runner evidence gate now rejects partial shot sets, unknown/failed discharge,
lost support or unsampled trigger instead of returning generic telemetry success.
6 focused tests and310 bot tests pass. No game acceptance inferred from those.
Ordinary tutorial flow in080443 progressed to binocular/zoom; physical2x->4x works.
ZoomTutorial requires village radio candidate, not a zoom message. Native timeout
occurred before target/intel/marking completion; those lessons remain unproven.
Blank loading-transition capture failure in075433 retained, not a game freeze.

## October 20 targeted regressions completed — checked development ff0dd2c746a6

Owned game50688/simulator32416 are CLOSED. Before stop all physical/native inputs
were neutral, faults0, menu/idroid/pause false, saving false and camera active /
unsuspended. Steam retained. Cleanup receipt:
`artifacts/dev/field-report-review/scene-owner-final-cleanup-before-stop.json`.
Published play pin `b627c7cacc9a` unchanged and verified; development and installed
DLL `ff0dd2c746a6`. Play.cmd restores published play; Play-Dev.cmd selects dev.
All58 automated groups and installer checks passed. Documentation changes after
the build explain source-identity drift; the checked game binary is unchanged.

**Exact tutorial regression passed in actual game under simulator.** Retained
`20261021T034930898656Z`: genuine Phantom Limbs departure, original grip/aim poses,
lesson opened once and retained across native timeout and authored explanation.
Read-only witness:1009 samples over539.6s, every camera active and unsuspended;
explanation at303.3s, RescueMiller return at422.4s. Both45s composited-eye takes
capture the actual timeout handoff and player return; chronology reviewed. Native
log confirms same open UI-only terminal ownership suspended for scene and restored
to the same player. Ordinary Back, immediate Menu reopen and final Back all pass.

The previous148d candidate kept the camera live but lost terminal ownership during
the movie: Back/Menu stow failed in030853, held Back refused before dispatch in
032054. New guarded provenance retains only an already-owned same open resource;
close/replacement/loading/player mismatch invalidates it. Negative unit cases pass.
No native timers, saves or lesson progression flags were edited. This timeout
path does not certify native binocular/intel/marking lessons or full missions.

**Rifle regression passed on the same checked binary.** Retained runs
`20261021T040358781890Z` (hip) and `20261021T040558709267Z` (back): each28 tracking
samples,24 with combined right-grip/aim fault12; camera active/unsuspended and
support attached throughout. Both eyes at late loss/recovery retain each rifle.
Held-trigger fault/recovery samples38/37 show mapped RT0 and unchanged ammunition;
release clears suppression. Three fresh presses discharge from the expected slot
(hip2 rounds per press, back1). Open support fingers/contact remain a visual gap;
these scoped passes do not close broad FP01/FP02 or physical-headset acceptance.

Evidence summaries and original videos:
`artifacts/dev/field-report-review/scene-owner-fixed-witness-summary.json`,
`scene-owner-fixed-rifle-summary.json`, `scene-owner-fixed-timeout-left/`,
`scene-owner-fixed-return-left/`. Capture cadence~3fps is not headset performance.
Earlier034658 departure held-button assertion missed a transition; native sortie
advanced. Failure retained and actual screen inspected before continuation.

**Next:** FP02 moving support/contact and precision-weapon recoil/bolt-action;
FP07 native left-hand contact/projector alignment; then reproduce FP03 forced
Y-action rotation with effective bindings audited. Physical Quest testing remains
open. Read FIELD_FIXES.md and FREE_PLAY_ACCEPTANCE.json before resuming.

Runtime check: per-game HeadsetRuntime is Meta and both Play commands explicitly
pass -MetaLink; CheckOnly succeeds. Windows global ActiveRuntime was observed as
Virtual Desktop at final cleanup. Restoring mandated Meta default was denied by
Windows registry permissions, so that global value remains Virtual Desktop.
Physical MGS5VR launch still explicitly selects Meta; no SteamVR switch occurred.

## October 20 resumed checks — scoped rifle pass, exact tutorial repeat pending

Candidate `148d5ca51161` is unchanged. Owned game/simulator are CLOSED; Steam
retained, per-game and global runtime restored to Meta Quest Link. Before stop:
menu/idroid/pause false, both tracking masks0, all physical/native controls neutral,
camera active and unsuspended, saving false. Receipt:
`artifacts/dev/field-report-review/resumed-cleanup-before-stop.json`.
Published `play/` pin `b627c7cacc9a` unchanged; installed payload matches dev.
Play.cmd restores the published pin; Play-Dev.cmd selects checked development.
Source identity differs because of harness/document edits; no game binary changed.

**Next: repeat the exact original tutorial failure.** Ordinary open/Back/reopen/
stow passed, but the original freeze retained an unclosed terminal across the
authored timeout/camera handoff. Do not close that defect from the easier flow.
Launch development with workspace.py launch-sim, inspect actual save/scene and
paired eyes, then use genuine Phantom Limbs departure. Keep the terminal retained
through its authored handoff under observation; no timer, sequence or save edits,
no automatic Pause/Skip. Restore inputs/poses and close the owned test session.
Last observed native world: mission10020, Seq_Game_RescueMiller, on foot near
(1449.62146,337.78680,1466.38550). Do not assume Continue preserves this exact state.

Retained rifle checks:
- `20261021T023544588585Z`: back-slot rifle remains visible in both loss and
  recovered eyes during8s right grip+aim invalidity. All29 sampled states retain
  support and active unsuspended camera;25 samples have fault12.
- `20261021T023824584002Z`: held trigger starts after confirmed loss; mapped RT
  remains0 and native ammunition unchanged through recovery and release.
- `20261021T024003656861Z`: repeats safe recovery, then3 fresh trigger edges
  each discharge1 round from PRIMARY_BACK with support retained.
- `20261021T024113307573Z`: primary-hip rifle visible in both loss/recovery eyes;
  all28 sampled states retain support/camera. Held-trigger safety passes. Fresh
  trigger reduces PRIMARY_HIP ammunition by3, but old checker wrongly expected
  PRIMARY_BACK and reports different_slot_discharged. Preserve that failure;
  selected-slot expectation is now corrected for future runs.
- `20261021T023915446376Z`: support-acquisition admission failed before fault or
  trigger; bounded support-publication wait added to the harness.

These are scoped simulator checks, not FP01/FP02 closure. Other grip-release,
menu/input-owner/reference-space transitions, physical Quest and open support
fingers remain open. Name weapons by verified native slot unless stronger model
identity evidence exists. The loadout preview placed AM MRS-71 in the back slot;
do not call the hip-slot variant AM MRS-71 from the card name alone.

Harness: tools/gameplay_bot/weapon_probe.py adds optional
tracking_trigger_recovery, confirmed loss before trigger, mapped-attack and native
ammo checks, neutral release, bounded acquisition and selected-slot firing checks.
310 existing bot tests and4 recovery-evidence tests pass; Python compile passes.
Local policies: private/resume-rifle-trigger-recovery.json,
private/resume-rifle-trigger-rearm.json, private/resume-primary-rifle-full-recovery.json.
Run them through `workspace.py bot --command body --suite <policy> --seconds 100`.

Tutorial evidence: retained supervisor `20261021T021943336936Z` passed native
iDroid lesson open, ordinary Back to BinocularsTutorial, reopen after visible
native allowance and second stow. Paired eyes render throughout. A30.187s left
compositor take has75 frames at2.4845 captured fps; start/middle/end reviewed:
artifacts/dev/field-report-review/tutorial-reopen-resumed-left/capture.json.
Earlier recorder failed with zero frames due to wrong endpoint; retain it.
Recorder endpoint: MGS5VR_OPERATOR_URL=http://127.0.0.1:8731.
Physical binocular raise advanced to IntelRadioTutorial, ahead of the narrow
BINOCLE after-predicate. One intel request had no current native target. Later
explanation progression is NOT accepted intel/marking action proof. No timer,
sequence or save was manually edited. Fresh paired player return:
`20261021T023437357914Z`; ordinary dismount: `20261021T023517962986Z`.

Setup/failure history retained in20261021T021029190440Z,20261021T021349078893Z,
20261021T021533458086Z and20261021T021917348749Z. Harness failures include absent
controls.menu_y, unsupported array-index dotted predicates, premature SelectDetail
expectation before ASAP, and unsupported90s await_state. Use whole-array near
predicates and supported bounded waits; inspect each actual page before input.

## October 20 tutorial half-state fix — candidate 148d5ca51161

Actual departure succeeded in retained run 20261021T011643134858Z:
Phantom Limbs, AMMRS71 loadout, ASAP, native arrival. The tutorial opened
successfully in 20261021T013219288334Z but cleanup could not close its owned
terminal. Native progression subsequently reached Seq_Game_RescueMiller while
VR remained suspended (reason7, playerHeadUnavailable), with menu/idroid true
and no current rendered rig. The user independently supplied the frozen
horseback/map screenshot. Runs 20261021T013713159974Z and
20261021T014146341442Z retained blank live-eye failures. One native B recovery
pulse followed by release did not clear it; no progression was edited.

Root cause in head_camera.cpp: the retained live-handheld menu branch demanded
the retired player owner across an authored camera handoff, blocking both the
shot and the exact player rejoin. The candidate bypasses that old menu anchor
only for the unpaused field handheld handoff, admits the normal verified shot/
player transaction, then rebuilds the menu anchor from the fresh returning
player. Regression cases cover reused and replaced return cameras and reject
missing player publication. 58/58 test groups and installer checks passed.
Actual-game repeat remains pending; do not mark fixed from unit tests.

Owned frozen game/simulator closed, Steam retained. Candidate148d5ca51161
installed and launched (PID43812 at launch); published play unchanged.
Rifle tracking continuity change is included but its field regression is still
pending. Resume fresh state before input; do not assume the earlier tutorial
recipe or native sequence. No Pause menu or held test input was left behind.

## October 20 live departure investigation

Retained supervised run `20261021T010239099242Z` confirmed actual Missions
navigation, opened the list, moved one row from hospital to Phantom Limbs
(visible Rescue Miller description), and pressed Deploy. A subsequent native
dialog is partly below the eye crop and has NOT been acknowledged. The bounded
600-second supervisor ended and cleaned up; inspect current state before resuming.
No field regression or mounted-weapon pass resulted.

Do not repeat fixed open/tab/confirm recipes assuming a reset page. The native
tab/selection survives some close/open paths. Use supervised per-image decisions.
Physical input audit, not gamepad buttons, proves a handheld menu channel was
sampled: both grips can produce controls.native_buttons=0 while the native UI
does change tabs. Run `20261021T005718727964Z` incorrectly required button512
and failed its harness assertion. In the retained supervised run, a right-grip
next-tab physically moved Mother Base→Map→Missions. Previous-tab was sampled
but did not visibly change Mother Base in that attempt; do not claim a pass.

For pose-only inspections, rendered.left_grip is in the declared local tracking
space: a near predicate on its position verifies a real pose change without
using an unchanged menu flag as an outcome. Raising the hand to y=.15 was queued
after the supervisor's deadline and was NOT executed. Next session should use
that verified pose predicate to bring the Deploy dialog into view, then continue
through the reviewed native departure. Use a total duration sufficient for
image review; keep each individual decision bounded and fresh.

The earlier apparent restriction was a Daily Bonus Received notice, confirmed
from an analytical crop of the original eye image. It was cleared through the
ordinary menu confirmation, not a Lua/save edit.

Active marked work list: [FIELD_FIXES.md](FIELD_FIXES.md). Detailed evidence:
[FREE_PLAY_ACCEPTANCE.json](FREE_PLAY_ACCEPTANCE.json). Continue implementation
and actual-game verification; no defect is closed by a plan or a build alone.

Latest ACC navigation: `20261021T003039295686Z` tested left-hand yaw +/-90
and ordinary stow; rotating the physical hand exposes native map pixels, still
tilted. `20261021T003616781896Z` opened iDroid and requested next tab with the
effective grip focus. The final left eye shows an informational overlay on
the map rather than a verified Missions page. Ordinary stow passed. This run
is retained; do not send blind confirm or infer mission selection from input.
Current native context is ACC40010, not the prior ACC40050 departure context.
Candidate field regression and mounted-weapon reproduction remain pending.

## October 20 additional physical-headset field feedback

FP03 now covers the common direction snap across contextual Y actions: vehicle
entry, table/sandbag climbing and Fulton. Test each from multiple headings.
FP15 records mortar eyes inside the emplacement, FP16 small flat-rock slipping
and brief stops, FP17 shortened arms and right-wrist-driven shoulder/left-arm
motion. These are user reports, not newly reproduced passes or confirmed causes.
For FP17, controller_rig.cpp currently supplies primary aim to trackedTorsoHeading
whenever both presentation hands qualify in front; the call does not explicitly
require a supported weapon. Investigate this coupling with head/left hand fixed,
bare hands and weapon variants, then physical Quest acceptance. Do not change
the rig on source suspicion alone. Keep these alongside the original open work.

Candidate ACC test `20261021T002624654220Z` stopped before rotation because the
generic case rejected an already satisfied idroid=true outcome. No hand sweep
occurred. Cleanup succeeded: subsequent native inspection confirms menu=false,
idroid=false, pause=false and camera_active=true. Use an observation case with
actual pose evidence for the next inspection; do not treat unchanged menu state
as proof of hand movement. Live game remains PID48280, ACC40010.

## October 20 tracking continuity candidate

Development candidate `d1b6b258af5f` preserves previously published weapon-ready
through right-controller grip/aim loss only while the ready binding remains
held, normal on-foot gameplay owns input, and camera activation/reference space
are unchanged. Physical grip release and other input owners cannot acquire a
weapon through invalid tracking. Attack remains blocked through loss and a held
trigger on recovery; a tracked release rearms it. Final published readiness is
recorded after cabin/studio/editor suppression. Passive XR input resets this
continuity state. Existing handheld iDroid and vehicle owners retain their
separate input policies.

`tools/build.ps1` passed all 58 test groups and installer checks. Candidate is
installed for simulator testing; published `play/` is unchanged. Previous owned
game/simulator closed for deployment, Steam retained. New game PID48280. Actual
game regression is pending; do not treat these unit checks as a visual fix.

Run `20261021T002056701890Z` completed native Continue but resumed ACC40010,
not the prior mission10020 field. Weapon probe correctly refused its field
precondition without firing. `20261021T002311880177Z` opened the handheld
iDroid; reviewed left eye shows handset edge-on (existing presentation gap).
Generic cleanup rejected its menu state; explicit ordinary system.idroid stow
in `20261021T002400665810Z` passed and left idroid closed. No test menu remains.
Next: restore an unrestricted field through native departure before repeating
the held-rifle fault. The new candidate has not yet passed that actual-game
regression. Do not weaken field admission to run weapon tests in ACC.

## October 20 held-rifle tracking loss — visually reproduced

Retained run `20261021T001242421704Z` held both grips on the AM MRS-71
through an eight-second right grip+aim tracking fault. Both final-eye loss
captures show bare hands with the rifle absent; both recovery captures show
the rifle returning. Support drops by 0.08s and returns at 8.48s without a new
grip press. All 30 sampled camera states remain first person. This establishes
a tracking-loss weapon presentation failure, not firing-induced detachment or
the physical Quest camera fallback. Native firearm telemetry stays true and
therefore does not prove a visible weapon. Support fingers still appear open.

The harness previously rejected the loss capture because its inactive iDroid
focus exception required attached support. It now permits the bounded right
tracking-fault observation with both mapped grips, a native firearm, no terminal,
on-foot state and neutral native axes/triggers. Existing sampled-input and
mapping checks remain. All 310 gameplay-bot tests pass. The run completed with
input release and fault cleanup; no production build changed.

Next production investigation: xr_runtime.cpp withdraws weapon-ready when right
grip/aim becomes invalid, which withdraws native LT and support ownership.
Preserve an already acquired weapon through tracking loss using the same
player/session/reference-space ownership, while keeping attack gated and
preventing a held trigger from firing on recovery. Do not simply cache support
alone: the actual rifle disappears in both eyes. Verify grip release, owner
changes, menus, stow and recovery as well as the fault before accepting a fix.


## October 20 CQC observation correction

The CQC harness now observes nativeAction (4) after an already sampled attack,
while still rejecting new trigger dispatch in that state. It previously threw
"not on foot" before observing release. 17 CQC and 48 locomotion tests pass;
stale CQC mock fixtures were updated for the existing fresh-facing reads and
contextual flag. This is a harness correction, not a production game fix.
Retained run `20261020T235935512463Z` reached sol_02_34_0000 using native
movement, sampled one lowered RT-only pulse, stopped the remaining chain during
native action, then observed normal gameplay resume after 1.343s. Actor stayed
life0, no CQC_HOLD/CONTINUOUS was observed. Left released eye inspected: active
guard nearby, no menu. No CQC incapacitation or Fulton pass. All inputs released.
The earlier target sol_ruinsNorth_0001 moved uphill; run
`20261020T235801433315Z` reached its approach deadline without attack.
Cover movement `20261020T235643495166Z` travelled ~1.8m along the wall;
`20261020T235726334128Z` outward right movement exited cover. Both retained.
Continue from current field near (1634,322,1069), not the old barricade position.
Next CQC work should inspect actual close-range eligibility before repeating
attack inputs. The existing 1.75m admission bound is not native eligibility.

## October 20 cover investigation — confirmed unwanted aim-out displacement

User explicitly requires cover to work naturally in VR; do not disable native
cover or replace it with an awkward mode switch. User subsequently requested a
settings toggle for this level of immersion. Latest clarification: choices are
off or immersive; automatic aim-out movement is unwanted in either choice.
Immersive cover must recognise physical crouch, lean and position under native
eligibility rules without automatically translating the player or viewpoint.
Preserve every applicable native cover benefit, not merely the BEHIND flag.
Audit wall/corner and low-cover states, posture, physical occlusion and any
native concealment or weapon-stability modifiers separately; their precise
eligibility and effects remain to be established from owned native evidence.
Do not grant protection or concealment while the actual head/body is exposed,
or remove ordinary geometry/posture benefits when cover assistance is off.
Implement the toggle with working behavior, not as a nonfunctional setting.
Test both choices and safe preference changes around cover transitions.
Current live native world is
10020 / Seq_Game_RescueMiller at ruinsNorth low concrete/steel barricade.
Run `20261020T232724402363Z` exited cover through ordinary backward input.
`20261020T233005806843Z` forward movement did not enter cover (travelled along
its edge). `20261020T233120450194Z` left movement entered native BEHIND/SQUAT.
Retained `20261020T233250494696Z` measured 22cm physical right lean and 25cm
upward peek, followed by grip-ready aim and return. Both aim eyes reviewed.
HMD stayed exactly (0,.25,0) with identity orientation across ready, but the
view moved 0.924m horizontally with the native root. Read-only native snapshots
also show the actual player moving from (1634.127,324.074,1029.349) to
(1634.411,324.081,1028.471), BEHIND true→false, returning on release. This is
native automatic cover aim-out driving the view, not a controller tracking fault.
Physical lean and return retained cover and heading; aiming still needs correction.
Detailed evidence: artifacts/dev/field-report-review/cover-aim-displacement.json.
Centred-head control run `20261020T234904979967Z` reproduces the same
0.924m displacement when raising the rifle without any physical peek. HMD
remained (0,0,0), identity orientation. Both aim eyes reviewed. This isolates
weapon readiness as sufficient to trigger the unwanted step. Inputs released,
original poses restored; no new build or settings deployment.
Native cover control read confirms `PlayerDisableAction.BEHIND=8192` and
current action exclusion flags0. No exclusion has been applied. User preference
is resolved: off or immersive physical recognition, with no automatic aim-out
movement in either mode. Blanket disabling BEHIND is not an accepted fix.
Recovered private ABI notes: private/cover-native-investigation.json.
No production cover fix yet. Preserve collision and native cover while resolving
this unsolicited view shift. Current test ends back in cover, inputs released,
original simulator poses restored, no menus open. Seated/full-body preferences
remain enabled; published build unchanged.

CQC/Fulton is still unresolved. Pursuit harness now disables threat avoidance
only for explicitly selected CQC approach, suppresses intermediate route stills,
waits through native transitions and refaces the fresh actor before triggering.
Run `20261020T232813958806Z` delivered one sampled CQC trigger; following native
travel transition stopped its remaining pulses. No actor incapacitation or Fulton
was proved. Do not repeat distant target-specific CQC or label these as passes.

## October 20 moving rifle and field traversal

Run `20261020T225816870564Z` repeated three AM MRS-71 shots after paired
controller yaw changes (+12/-12/0 degrees). All 18 motion samples and all three
shot windows retained support; each shot consumed one PRIMARY_BACK round.
This remains one semiautomatic rifle, not bolt-action or physical-headset proof.

Native on-foot walking `20261020T230146797761Z` reached the mapped path below
the arrival ruins (13 segments, then stopped on a transient native travel-mode
change). First route attempt `20261020T225949485828Z` correctly refused an
unmapped start ~9.7m from the nearest imported node. Do not relax floor bounds.
Second route `20261020T230248034418Z` walked the native graph to
(1293.665,331.310,1308.475); its 90-second deadline ended the run with input
released. Route `20261020T230509291308Z` continued to (1517.816,328.234,1147.590).
Run `20261020T230726168176Z` reached the outpost and stopped for native alert
at (1610.231,322.656,1090.921), health7201.
No Fulton interaction or production camera fix has yet occurred.

Further native pursuit reached (1634.687,323.440,1043.533), health7201, with
no menu left open and all test inputs released. `20261020T230957841145Z`
completed the first guard approach; `20261020T231120074312Z` hit the graph-end
arrival-radius boundary. CQC runs `20261020T231218681628Z` and
`20261020T231425536746Z` dispatched no attack because the guard had moved beyond
1.75m. Bounded dynamic pursuit `20261020T231314548767Z` completed its second
route but hit transient native travel-mode admission before CQC. This is test
orchestration friction, not proof of a player-facing CQC defect. Next continuation
must wait for the native transition, reread the exact guard and pursue/act in one
connection; do not keep attacking stale coordinates. Actor target is
sol_ruinsNorth_0001. Fulton remains untested. Published play build unchanged.


## October 20 actual rifle firing continuation

Native mission10020 advanced to `Seq_Game_RescueMiller`. Dismounted through
`horse.interact` in retained run `20261020T225229112778Z`. Visible AM MRS-71
selection/support acquisition confirmed in both eyes in `20261020T225249174805Z`.
Run `20261020T225459731487Z` fired three PRIMARY_BACK rounds with independently
held grip leases; all sampled support contacts remained attached and each trigger
consumed one round. Both eyes after shot three were reviewed. This does not
close the reported sniper defect: bolt-action, moving grips and physical Quest
are untested; support fingers also appear open and need native-contact review.
The preceding run `20261020T225406089237Z` fired but its post-shot capture was
rejected by the harness; corrected to its established static-grip capture path.
No production change in this continuation. The installed c6 candidate remains
active, input released, no menus open. Continue from Afghanistan on foot near
the mission arrival ruins; nearest surveyed enemy outpost is ruinsNorth ~434m.
Do not repeat Mother Base weapon selection, which had native weapon restrictions.

## October 20 ongoing free-play acceptance

The user requested continued defect fixing and comprehensive combination testing.
`docs/FREE_PLAY_ACCEPTANCE.json` tracks known issues and exact coverage limits.
Development remains c6f6bebd10bd; no publication or pin promotion is authorized.
ACC controller probe `20261020T214111077797Z` completed 20 checkpoints: stationary
valid right controller, right grip/aim/both invalidity, and automatic recovery.
All sampled frames remained active and unsuspended. The preceding run
`20261020T214002922383Z` also retained both-eye combined-loss captures showing
first-person cabin presentation, but its harness return was missing (corrected).
Mother Base seated `20261020T214655180013Z` and standing
`20261020T214814573009Z` each completed 20 additional checkpoints, with late
loss samples beyond seven seconds and recovery without recenter. Combined-loss
late captures were inspected in both eyes: first-person view and retained right
hand survived. Physical Quest inactivity and held-weapon/iDroid combinations
remain unproven.
Handheld iDroid opening `20261020T214209961222Z` had active menu routing but an
unreadable edge-on presentation; retained as an open usability issue. The
existing spatial-menu preference allowed native Mother Base deployment, then
handheld mode was restored. Current session is active for field continuation;
the older cleanup receipt below belongs to the previous session.

Weapon investigation `20261020T214917646823Z` requested primary-right selection
with loaded sniper base 799. Four telemetry checkpoints reported native firearm
and support attachment, but both eyes showed bare hands. This is NOT a sniper
acceptance pass. Primary-left repetition `20261020T215230329998Z` was also
visibly bare-handed. Native disable-action flags were 524289; their full meaning
is not established. The queried known enum values did not explain those bits.
Do not change grip geometry based on this fixture until actual selected weapon
and visibility are established. The harness result now explicitly says
observed_support_telemetry; firing has not been tested.

Native walking `20261020T215348702924Z` completed 16 forward segments from the
helipad; disable-action flags remained 524289. Live development is on foot at
that endpoint, input released, handheld/full-body/seated preferences restored.
Next: repeat weapon selection off the helipad, verify native picker readiness
and actual weapon pixels, then measure uninterrupted grip through real shots.
Fulton yaw, the exact reported corridor, physical Quest idle behavior and other
ledger items remain open. No additional production change followed c6f6bebd10bd.

### Continued native menu and weapon investigation

Off-helipad repetition `20261020T215524759685Z` still did not show a rifle.
Recorded source-eye run `20261020T215634653721Z` shows a red prohibition mark on
GEIST P3 status. Its physical browse was sampled but native navigation axes were
zero. Video has 421 accepted source frames, 8 dropped samples, 7 explicit CFR
repeats; it is pre-runtime-composition diagnostic evidence, not final-eye video.

The normal native title return and Continue recovered ACC40050 in
`20261020T220853042200Z`; Steam and the owned game process remained running.
An intervening signed-out notice was reviewed and acknowledged in
`20261020T220541455871Z`. Free-roam area selection remains absent on the current
ACC Missions page. Source-derived `TppTerminal.ReleaseFreePlay()` diagnostic
`20261020T222055724136Z` left MSN_LOCATION visibility false before/after, with
story 33 and available GMP 4663940 unchanged. Do not promote it as a repair.

Mission-list selection still needs investigation: the three-down highlight
showed Hero's Way, but `20261020T222645501745Z` reached native sortie preparation
for Mission 1 Phantom Limbs. The visible selected loadout includes AM MRS-71.
No sniper firing pass has been claimed. Both preparation and drop-time previews
show a headless character; FP14 records that separate visual defect.

`20261020T223448208737Z` accepted the reviewed ASAP drop-time choice. Native
sortie departure is underway; observe its actual arrival before selecting a
field policy. The old field weapon probe now accepts an explicit world fixture,
but requires native guards and actual visible selection before firing evidence.
Handheld menus have been restored to 1. Published play remains unchanged.

Latest verified state: `20261020T224050797783Z` reached native mission 10020,
Seq_Demo_ArrivalInAfghanistan, demo=true, menu/pause=false. Native weapon-disable
flags are now 0 (Mother Base had 524289); customized back weapon remains 799
and PRIMARY_BACK remains 873. The owned game PID is still 37084. The last
45-second observation completed, so there is no pending bot process to poll.
Next: observe the live cinematic handoff, establish its actual playable sequence
and mounted/on-foot owner, and run the weapon probe with that explicit world.
No shot has yet been tested in this session. `20261020T223923092427Z` loading
Resume action reached a black transition capture and remains a failed visual
check; the later arrival evidence is separate.

## October 20 floating correction — development c6f6bebd10bd

Development `c6f6bebd10bd` corrects the reproduced seated recenter height drift:
recenter preserves the seated vertical calibration; entering seated mode still
calibrates it. To deliberately recalibrate, toggle seated mode off/on while
sitting upright. Runtime reference-space changes transport that calibration.
Standing floor rays now include the original native support level when the
retargeted ankle rises. A dedicated grounded leg solve reaches the sampled
contact without changing arm IK limits, torso width, head/hand tracking or
native actor collision. Temporary leg extension returns with head height;
the previous maximum-ever standing leg calibration is no longer accumulated.

`tools/build.ps1 -SkipBootstrap` passed 58/58 test groups and installer checks.
New core cases cover temporary extended reach, neutral return, crouch,
unreachable contacts, seated lowered recenter and tracking-space rebasing.
Live policies require two floor contacts, native gait-relative ankle clearance,
and return to baseline eye height and leg calibration. Simulator verification
is not physical-headset acceptance. Published `play/` remains unchanged.

Final candidate live runs on native Mother Base 30050:
- `20261020T212227524102Z`: standing height, 8 phases, eyes/root 1.65 m ->
  2.25 m -> 1.65 m; both floor queries retained throughout.
- `20261020T212306675044Z`: seated recenter/lean/crouch/prone/stand, 13 phases;
  returning after lowered recenter restores approximately 1.53 m eye/root.
- `20261020T212352729826Z`: standing recenter/lean/postures, 13 phases;
  standing after native crouch/prone returns to 1.65 m.
- `20261020T212441785555Z`: seated raised-head sequence, 8 phases.
All 42 phases passed body validity, two floor queries, native gait-relative
ankle contact, and recovered height/calibration checks. Both eyes were reviewed
at standing high and seated rise-after-recenter. Machine-readable metrics:
`artifacts/dev/field-report-review/floating-fix-matrix.json`.
These are simulator/native-game passes; physical Quest acceptance remains open.
The final build log is `artifacts/dev/field-report-review/floating-build-3.log`.

Traversal follow-up on the same candidate completed 52 bounded native movement
segments across seated/standing helipad approaches, a standing sideways-head
approach and the covered passage. At the stair-side obstruction, full-body ON
`20261020T212928537477Z` and OFF `20261020T213001227871Z` both stopped at
native Z approximately -16.399, with only 0.064/0.095 m displacement. Those are
stopped investigations, not passes. Four left sidesteps followed by twelve
forward segments completed through the passage with body ON in
`20261020T213034880385Z`; both endpoint eyes were reviewed. This establishes
that particular native obstruction and an open route beside it. It does not
reproduce or dismiss the user's unspecified tight-corridor sticking.
No actor transforms, collision radius, native capsule or torso width changed.

Cleanup: the owned MGSV/simulator session is closed; Steam PID 21668 retained.
Seated/full-body/handheld-menu preferences are restored to 1/1/1, Windows Meta
OpenXR is restored, and both launchers retain `-MetaLink`. Installed DLL matches
published `play/` `b627c7cacc9a`; checked development `c6f6bebd10bd` is available
through `Play-Dev.cmd`. No release or pin promotion occurred. Cleanup receipt:
`artifacts/dev/field-report-review/floating-fix-cleanup.json`.

Additional field feedback (received during this investigation): preserve the
reported FPS/stability/image-quality and truck-driving improvements. Open
reproduction targets are support-hand grip loss after firing precision weapons,
Fulton forcing camera yaw, and Quest 3 right-controller inactivity causing a
third-person fallback. Existing `f8cf03f` already targets valid inferred idle-controller poses and
support-contact retention. Those reports need targeted live reproduction; no
new fix or physical-headset acceptance is claimed for them in this pass.

The first corrected build's seated 13-phase regression passed in native Mother
Base (`20261020T211356414311Z`). Standing test `20261020T211443891001Z`
exposed a separate calibration defect after crouch-to-stand: the native STAND
flag arrives before the rise animation settles, so the first low eye sample
was retained as standing calibration while the camera anchor later rose.
A 1.65 m tracked head produced approximately 2.18 m eye/root height and the
high excursion exceeded the leg solver's limit. This failed run is retained.
The next candidate calibrates from the same settled posture anchor used by the
camera, retaining native gait suppression and the physical floor reference.

## October 20 broad mode sweep: reproduced floating, corridor cause still open

User requested checking all relevant modes without assuming when the report
started. Fresh bounded tests covered seated/standing height, lowered and lateral
recenter, crouch/prone/stand, full yaw rotations across 179/181 degrees,
full-body/arms-only movement, sideways head displacement, and Mother Base
stairs. Retained run matrix: `artifacts/dev/field-report-review/matrix.json`.

- Seated recenter while lowered 0.5 m, then returning upright, reproduces
  floating presentation on BOTH published `b627c7cacc9a` and development
  `6df8978d1845`. Published run `20261020T204757436347Z` raises eye/root height
  from approximately 1.54 to 2.04 m and rendered ankle/floor clearance from
  approximately 0.11 to 0.53 m. Native actor position does not explain that
  lift. Home recenter restores it. Development `20261020T203724273227Z`
  reproduces the same behavior. This is a specific trigger, not proof that it
  caused the user's unidentified spontaneous episodes.
- Standing +0.6 m head displacement loses both floor queries while body.valid
  stays true on both builds (`20261020T203454014685Z`, published
  `20261020T204845330063Z`). Returning to baseline recovers both queries and
  baseline eye height. Zero clearance in a zero-query frame is unavailable
  evidence, not grounded feet. Published and development both show this limit.
- Standing lowered-recenter and posture sequence `20261020T203626095100Z`
  returns to its starting eye/root height. Seated and standing yaw sequences
  `20261020T204437825450Z` / `20261020T204521863638Z` retain their respective
  heights through full turns. No lasting size growth was reproduced here;
  the maximum leg-fit latch remains a source lead, not a proven cause.
- Four short movement comparisons (seated, standing, 0.5 m sideways head,
  arms-only) completed all segments. The stair railing stopped movement with
  full body ON (`20261020T204037970661Z`) and OFF
  (`20261020T204111389138Z`). Moving inside the rails allowed eight ascent
  segments with body enabled (`20261020T204137572707Z`); sideways-head ascent
  also completed (`20261020T204359178742Z`). These are stair/rail observations,
  not a reproduction or dismissal of the reported corridor sticking.
- Initial descent `20261020T204225730060Z` exceeded the bot's one-unit limit
  after release and stopped. Keeping that limit and reducing target/pulse
  length completed ten descending segments in `20261020T204314575319Z`.
  Do not convert the first run into a pass or a game collision fix.
- Published restart/Continue `20261020T204643441480Z` reached Mother Base
  normally before its height probes. Earlier retained truck before/after
  `20261020T190927001168Z` / `20261020T191301504346Z` and 16-phase ACC body
  `20261020T191602069538Z` were reviewed again. They do not establish fresh
  vehicle enter/exit, transit-helicopter or physical-headset coverage; these
  remain open along with the exact corridor and perceived oversized body.

This pass changes diagnostic tooling only. Explicit `--slot play` now permits
bounded published-baseline comparison without replacing either slot. Body
policies use `--suite`; misplaced `--policy` is rejected rather than silently
running a different case. Test settings/tracking poses were restored and owned
simulator sessions closed. No production correction or universal acceptance is
claimed from this investigation.

## October 20 field report: size, floating and corridor sticking

User reiterates frequent floating, an oversized / bulky feeling body and
difficulty navigating tight corridors. Treat all three as open defects, not
resolved by the earlier seated-mode sweep or truck-body fix.

Supplied recording: `C:/Users/nbrys/Downloads/METAL GEAR SOLID V- THE PHANTOM PAIN - 2026-10-20 5-12-33 AM.mp4`
(17.365 seconds, 1920x1080, 30 fps). One-second overview samples and the
5-second body frame were reviewed; this is not a full temporal or stereo
review. The downward view shows the torso and arms occupying much of the
image, followed by movement across an open Mother Base deck. The recording
does not establish physical body scale, measured foot clearance or a blocked
tight-corridor attempt. Build identity and seated/standing setting for this
recording remain unknown. Derived review frames are in
`artifacts/dev/field-report-review/`.

Source lead: `standingLegScale` in `src/controller_rig.cpp` retains the maximum
requested floor-fit reach (up to 1.5) until owner/model changes. This could
retain a transient standing-mode leg extension; it does not establish a wider
native collision capsule or explain a seated-mode report. Reproduce before
changing the fit. Compare eye/root separation, native and rendered foot
clearance, and accepted movement through the same narrow passage, including
head displacement and recenter transitions. No production fix or acceptance
is claimed from this report review.

## October 20 active follow-up: body continuity, height, palm and rocket arm

User acceptance targets: Continue enters ACC through ordinary menus (cassette
optional); body remains present with native seated legs in vehicles/helicopter;
recenter/posture/scene transitions cannot accumulate body size or eye height;
iDroid follows the tracked palm without rotating the wrist to face the screen;
rocket-arm flight has a compact live view at the left forearm/socket, primary
left-arm steering and optional left-stick steering.

- Truck absence reproduced in `20261020T190927001168Z`: all four phases excluded
  full body. Candidate `86928b277465` retains native seated pelvis/legs and joins
  original skin eyes. `20261020T191301504346Z` admits all four phases, including
  lean; paired seated eyes reviewed. This is truck coverage, not all vehicles.
- ACC `20261020T191602069538Z` admits all 16 body phases. The transit helicopter
  exclusion remains open and has not been removed without live verification.
- Native title selection 0 (Continue) now routes to existing selection 1
  (RestartHeli) only in the unlocked helicopter title. Retail handlers share
  the DLC/loading path. Hospital/New Game/MGO remain native. 58 isolated Lua
  checks pass both locally and in-game. Ordinary menu run
  `20261020T192115709988Z` selected 1; retained arrival
  `20261020T192142060352Z` reached ACC. That run's saved destination already
  was ACC after the earlier return. Retained `20261020T192810738005Z`
  then set only the title's in-memory next mission to 30010 and used ordinary
  menu Continue: selection 1 reached ACC 40010. The prior in-memory value was
  restored; no saved game was replaced.
- Retained `20261020T192209318026Z` exercises ACC iDroid open/move/stow/reopen
  and unfocused walking. It predates the palm orientation fix: the helper
  explicitly forced the wrist from aim to orient the handset. Source now makes
  the tracked palm own the wrist and keeps the native device-in-wrist mount.
  Candidate `9fb90fe62dc6`, retained `20261020T192841739169Z`, keeps the
  rendered palm within 0.00004 degrees of the tracked-grip target through all
  six open/move/walk observations. The identity-grip screen is edge-on; natural
  palm-up screen usability still needs paired-eye visual review.
- Large floating/size and corridor sticking remain unconfirmed. Inspect the
  standing leg-fit scale latch and repeat recenter/seat/posture transitions.
  Do not claim the earlier seated-height sweep proves standing-floor behavior.
- Rocket-arm live-view/left-arm steering request is open. Existing flight view
  depends on the iDroid display path; inspect actual flight before replacement.
- Cabin follow-up: contextual left-forearm hints now cover camera capture and
  gallery/print controls, held notes/photos, pen writing and board placement.
  Holding both pen and paper prioritizes writing plus pinning. Field photo
  release says cabin storage, rather than promising an unavailable board.
  Candidate `6df8978d1845` passes 58 automated groups and installer checks.
  Retained `20261020T194218562117Z` completes actual note-only, note+pen and
  pen-only pickup/release states. Both-eye captures show the note and combined
  writing/pinning instructions. Pen-only instructions change correctly but
  extend beyond the lower image edge at the captured pose; unrestricted
  readability is not accepted. The same captures reproduce paper intersecting
  the wrist instead of a fingertip pinch and the poor pen grip. Those remain
  failures despite working prompt transitions.
- Retained gallery run `20261020T193218705990Z` reached capture, browse,
  reprint and confirmed digital deletion with physical copies retained. It
  failed at `note-editor-open`, so it is not a complete gallery/workbench pass.
  Grip/pinch animation, pen seating, cassette triangles, front-slot printing
  and visible paper stacking remain open. Current captures still auto-print.

Development only; published `play/` remains unchanged.

## October 20: floating / tight-passage report investigation

Simulator-tested the installed published `b627c7cacc9a` with its preserved
personal settings (`seated_mode=1`). Continued the current save in Afghanistan
and exited its truck through the effective VR interaction. No saves were
swapped and no native player transform or collision shape was changed.

- Retained `artifacts/bot/runs/20261020T185733342809Z/roomscale.json`
  contains 23 measured head-motion phases. Vertical down/up and lateral return
  recovered the same eye Y, including observed turns through 315 degrees.
  A 25 cm upward head offset raised displayed foot clearance from roughly
  0.108/0.110 to 0.265/0.249 native units while the native root stayed fixed.
  This isolates presentation from actor motion; it does not establish the
  reported spontaneous floating. The longer sweep failed on an operator
  pose-call timeout and must not be marked passed. A new observation recovered
  the same live game without restarting it.
- Native stand/crouch/prone/stand predicates passed in retained run
  `20261020T190226900908Z`. A later settled observation returned eye-to-root
  height to approximately 1.53 m and foot clearance to its baseline.
- Standalone bot movement stalled at its old 0.15 braking input after moving
  0.292 native units (`20261020T190014970356Z`). Matching the existing
  `walk_local` 0.35 brake / 0.1-second pulses reached 0.533 for the 0.35 target
  and settled (`20261020T190149098191Z`). This is a bot correction, not a
  gameplay collision fix; overshoot remains and precision navigation is open.
- Tight-corridor sticking is not reproduced. The short truck-target route
  probe was rejected before navigation because its 0.25 arrival radius was
  outside the planner's 0.75..3.5 contract. No obstacle/corridor pass is claimed.
  Next useful proof is a matched ordinary-head versus displaced-head traversal
  of a witnessed narrow passage, with native root, accepted eye and input
  observations. Keep standing-mode/floor tracking separate from this seated run.

## October 19 evening: comfort hotfix candidate

User identified published `3cb686fd4f1c` as the report baseline and requested
quick precision changes with automated checks, leaving physical testing to them.
This supersedes extended live reproduction for this pass. The short baseline
and Continue observation were stopped; the owned simulator/game are closed.

- New `settings.seated_mode` is on the first live VR Settings page. Enable it
  while sitting; Apply calibrates the current seated eye level to native stance
  height. This revision originally recalibrated on Recenter; the October 20
  correction above supersedes that behavior. Physical lean and native
  crouch/prone remain relative to that baseline. The October 20 correction also
  adds temporary grounded leg reach in seated mode. Standing remains the default.
- Recenter and a live LOCAL-space rebase preserve the native anatomical height
  and posture stabilization rather than reseeding from transient animation.
- Runtime-valid inferred controller poses remain usable when optical tracking
  flags drop on idle controllers. Invalid poses still release controls. This
  targets the reported idle fallback; the physical Quest trigger is unverified.
- Prone camera offsets use actor-root heading instead of alternating chest yaw.
- Support contact is retained in the weapon attachment frame, including changes
  relative to the wrist. Native reload/bolt animation still owns manipulation.

The user reports testing development `b627c7cacc9a` and explicitly authorizes
publication as `experimental-2026-10-19.1`. All 58 automated groups and installer
checks passed. This does not establish universal mission or headset coverage.
Publish and verify the checked payload before advancing `play/` to that release.
Both launch paths retain wired Meta Quest Link.

October 19 release decision: the user reports having completed headset testing
and explicitly authorized publication of the current experimental candidate.
This supersedes the earlier publication hold, not the documented feature
limitations. Turret hand controls and complete ACC loadout holsters remain
unfinished; no full-game or universal headset pass is claimed.

## Mounted-gun investigation, October 18 (local time)

Not ready for mounted-hand or release acceptance. `mounted_grip.hpp` is a
tested acquisition/release helper only; it is not connected to native turret
aiming or the live hand rig. Development `3cb686fd4f1c` completed 58 automated
groups and installer checks; those checks do not prove mounted gameplay.

- Ground fixture `slopedWest_gun001` and alias `slopedWest_gun002` resolve to
  the same actor 41984, type 82. Fresh native observation reported it broken.
  The isolated test used native `TppGimmick.ResetGimmick` to restore its visible
  model, explicitly as fixture setup, not gameplay evidence. A longer Y hold
  returned it to broken/absent state, consistent with the shared Fulton action;
  no extraction-result event was captured. Do not reuse held Y as mount entry.
- The intact, visible handle-side attempt `20261019T052142692794Z` sampled
  native Y (`32768`) but the character remained action 1 / travel mode 1.
  Closer approach `20261019T052218267570Z` also failed to establish mounting.
  Exact mounted actor/seat ownership, authored grip/pivot, hand aiming and
  release remain unverified. Native reset did not prove interactive recovery.
- Helicopter fixture `helicopter-rewards-user-20261011` resumed in Africa ACC
  mission 40020. Deployment probe `20261019T052620423318Z` stopped when a native
  Rival help card interrupted focus. The existing acknowledgment case passed
  in `20261019T052817180110Z`; ordinary Back passed in `20261019T052902787729Z`.
  Reopening and mission selection passed its scoped predicate in
  `20261019T052927784404Z`, with a map captured. This never reached a helicopter
  door gun: helicopter grip, aiming, firing and exit are still NOT TESTED.
- Test session stopped, no test menu retained, Steam untouched. Original
  `acc-holster-original-20261019` campaign restored with all six files verified.
  Do not publish or describe either mounted class as implemented or accepted.

This is the active plan. Update it in place; historical experiments are evidence,
not competing plans. User priorities confirmed October 18, 2026.

October 18 follow-up: mounted helicopter guns and ground turrets must support
optional physical hand acquisition. Hands snap to the weapon's authored grips;
hand movement drives its native traverse/elevation within mechanical limits.
Keep existing stick aiming available. Grabbing must not fire; release, tracking
loss and dismount must release input and return hands without an aim jump.
The current mounted path routes native stick axes; snap-to-grip and physical
mounted aiming are not implemented. Treat helicopter and ground weapons as
separate native-owner cases. Do not reuse free-held firearm or steering-wheel
grip ownership without verifying the mounted gun's actual handles and pivot.

Body-gear follow-up: the user's field pictures correctly show an existing
weapon mount; do not describe all gear visibility as absent. Fresh Africa ACC
inspection `20261019T045031268242Z` reproduces the missing body-mounted guns.
Read-only `private/gear-acc-equipment.json` finds loaded customized gun part
models at the cabin display positions. `private/gear-field-equipment.json`
finds those assemblies following the player in the field. The hip assembly's
position matches the completed ASRROOT mount, but a direct rotation match was
not established; do not ship a guessed orientation offset. The item allocation
table is separate from the customized firearm service: an empty item table
does not mean firearms are unloaded. No new production attachment change was
made during this diagnostic pass. Field inspection `20261019T045547068090Z`
is retained evidence, not whole-loadout or grab acceptance. Test sessions closed,
original campaign restored with six files verified, and published slot restored.

## Active priority: iDroid Rocket Arm display

Headset handoff, October 18/19: the user will test before any release push.
Do not publish until their headset testing and release decision. Development
completed automated and installer checks; physical headset acceptance is pending.
The original campaign `acc-holster-original-20261019` was restored with all six
files verified. The simulator session is stopped. The installed published slot
was restored; `Play-Dev.cmd` selects development using wired Meta Quest Link.

Scoped simulator result superseding the failed candidates below: `5f73f4917b24`
shows live guidance at the left-hand native iDroid projector, its holding hand,
and live body/world through launch, steering, ground impact and return in both
`artifacts/rocket-arm/idroid-feed-visible-left` and `idroid-feed-visible-right`.
Post-flight iDroid open/stow/reopen passed four retained cases in
`artifacts/bot/runs/20261019T042907036259Z`. Ground impact is observed; destruction
of a target is not proven. Capture cadence is about 3 fps, not game FPS.
The latest checked development additionally isolates a failed remote lens matrix
from the valid Snake head passes; this last guard has not had a fresh live run.
Pre-stowed device state, tracking loss, interruptions, other arm variants, the
broader UI inventory and full ACC loadout holstering remain open. These scoped
passes do not certify every weapon, whole missions or headset readiness.

The user now requests the native Rocket/Blast Arm guidance feed **inside the
left-hand iDroid holographic projection**, before further target footage.
Preserve native steering, acceleration and return; do not open the ordinary
map menu during flight or replace the HMD's view with the projectile camera.
The intended iDroid interface will own selectable tabs for native game content,
Rocket Arm guidance and, later, YouTube. YouTube is future scope, not implemented.

Current simulator evidence under `artifacts/rocket-arm/`: `body-launch-take1`
shows full-body look-down and native Blast Arm launch; `wrist-motion-take2`
shows the existing floating feed, steering toward the ground, impact smoke
and feed closure. These are left-eye compositor recordings at roughly 3–4
captured frames/second, not game FPS or stereo acceptance. No destructible-target
kill is proven, and neither take shows guidance in the physical iDroid.
Development of that integration remains open. The bounded UI draw inventory
diagnostic is built in checked development `5ce96f47edf3`. Flight samples in
`artifacts/rocket-arm/feed-diagnostics/samples.jsonl` show camera reason 7
(`playerHeadUnavailable`), then inactive VR camera and the XR flat-screen
fallback. The earlier wrist-attachment interpretation was premature. Integration
must preserve fresh Snake world/body/hands while capturing the projectile feed
separately; retained stereo imagery does not establish live body rendering.
The UI audit therefore includes camera ownership handoffs and fallback triggers,
not only UI resource routing. The current 96-node inventory is a discovery aid,
not an exhaustive census.
The routing and coverage plan is tracked in `docs/NATIVE_UI_SURFACES.json`;
unknown native layers must be identified by owner/producer, not assigned a
permanent destination from draw order or camera depth alone.
Native flight follow-up: `artifacts/rocket-arm/owner-flight/samples.jsonl`
reproduces rejection at 12.07 m camera-to-head distance, while the native head
continues updating. The 12 m ordinary-camera guard is the immediate cause.
`owner-flight-producer` records the typed owner selecting family 2/mode 10
for preparation, mode 11 for flight, then family 5/mode 1 on return. Static
inspection ties mode 11 to the producer's native category-14 branch; a forced
mode override is excluded. The first integration candidate keeps fresh exact
player/skin joins, retains Snake's root heading and renders the separate camera
into the authored left-hand iDroid projector using native ID499 equipment.
This candidate is UNDER TEST, not visual acceptance. In particular, remote-view
visibility/LOD and the native handset's per-frame completion require live proof.
Live rejection: candidate `819910777d82` kept the body visible in flight but
showed an unsupported handset without its guidance screen; the launched fist
was absent. Return ended with scene replay reason 3 (matrix inverse check),
not a proven completed capture. Follow-up `81addea77104` completed a 45-second
left-eye take including impact and return (`idroid-source-left`), with fresh
body publications and no camera cancellation in that run. It still fails:
the feed texture is captured but the exact handset resolver returns no match,
and the hand remains hidden. Read-only `idroid-parts-trace` confirms the native
modular hand model and wrist palette persist during launch; visible-group
counts change from three to two. Do not bypass frame matching, broadly expose
all model groups, or call either candidate accepted. Both-eye feed, grip,
return stability and the broader UI audit remain open.
Candidate `ecb2e0c13f6b` visibly restores the tracked left hand holding ID499
during flight (`idroid-hand-feed-left/review.jpg`). The change exposes only the
observed `0xf948d635` group of owned modular resource `0x84a2a348c3d25162`
during head replay, then restores its native visibility. The native projectile
retains gameplay ownership. Guidance remains blank: the body/rig stamp matches,
but the handset matrix changes before rendering; widening the A8 caller gate
did not produce an observed native A8 call. The same take reproduces lens
inverse error 0.00305150822 with quaternion norm squared 0.9999989886 near impact.
Candidate `879e4fb3e25d` normalizes only the verified projectile replay rotation,
preserving the raw owner join and the original matrix threshold. Its observed-pose
regression passes; live verification and matrix-change diagnosis are in progress.

Navigation toward an outpost was stopped when the user prioritized the display.
The isolated late-game test save is `rocket-arm-native-loadout-current`; restore
`acc-holster-original-20261019` before returning the installation for normal play.

## Required cabin flow

1. Full body and aligned hands while sitting, standing, walking and turning.
   No routine recentering. Preserve field weapons and optics.
2. iDroid available immediately in the LEFT bionic hand. Open, navigate,
   select, Back, stow and reopen reliably; walk when not focusing its controls.
3. Pause, settings and remapping accessible through effective VR bindings.
   Back/Cancel returns to the previous page or cabin, without stranded input.
4. Normal menu access must not require handling a cassette or any novelty prop.
5. Wall weapons are optional inspect-and-return interactions; no cabin firing.
6. Camera, notes, cassette handling and other props are optional. They must not
   steal controls from the core flow. Camera and notes work in either hand.

## Body-mounted loadout request — October 18

October 19 follow-up: the live-session blocker below was recovered in a fresh
owned simulator session. Retained `20261019T020921847335Z` passed ordinary title
menu access and Continue without cassette handling. `20261019T020946979570Z`
passed ACC Pause/open/Back, with both eyes reviewed. `20261019T021049134493Z`
passed all 16 ACC body poses; seated images show torso, legs and arms, but no
loadout guns. `20261019T021126403051Z` passed repeated left-hand iDroid open,
move, stow, reopen and locomotion after correcting the test's missing input
hold delay. These are scoped simulator observations, not headset acceptance.

The actual ACC loadout reports customized weapon IDs 871/873/875. A guarded
native PRIMARY_1/PRIMARY_2 visibility request did not make them appear in
retained `20261019T021359933306Z`; no production visibility toggle was added.
Mission selection reached the A Hero's Way confirmation and landing-zone map,
but departure/return and body continuity through deployment remain unverified.

### Menu visibility fallback — October 19

Development `00fe4d22f14e` adds verified active native popup ownership to the
menu detector independently of terminal/Pause flags. Open menus whose device
surface is unavailable use the existing anchored spatial plane instead of
being suppressed. Ordinary HUD and outgoing device close animations retain
their existing routes. `native_ui_renderer.menu_fallback_sources` counts
fallback source publications; it is not proof of final-eye pixel visibility.
All 58 automated groups and installer checks passed. Startup autosave notice
was detected, reviewed in both eyes and acknowledged with VR Confirm in
retained `20261019T023221556911Z` (paired notice images are retained in
`20261019T023138677842Z`). The left-controller-loss test in retained
`20261019T023711803247Z` failed visual review: a retained native handset pose
kept the menu classified as device-ready while the content moved off-screen.
Checked development `f7eed7b44757` additionally requires left grip tracking for
the device route. Retained `20261019T024201929160Z` repeats the seven-second
loss and recovery: both loss images show the complete menu on one spatial
plane over the cabin. Automated groups remain 58/58. Arbitrary-dialog coverage
remains pending. This is not a universal desktop-pixel watchdog or a guarantee
that every native UI owner has been identified.

Snake must visibly carry his actual equipped loadout, including iDroid, on
proper body mounts. Touching reachable gear gives a haptic contact cue. The
user selected a **one-second grab grace period** after that cue, allowing a
small hand drift before squeezing. Releasing a grabbed item returns it to its
mount. Preserve existing weapon/optic interactions, left-hand iDroid and left
forearm status. Gear visibility, contact, held presentation and return must
refer to the same native item and move with the completed body/hand frame.

Implementation remains OPEN. Detailed transform math, timing, binding conflicts, source evidence and proposed
code work are recorded in [BODY_GEAR_PLAN.md](BODY_GEAR_PLAN.md). Native per-item
attachment and equip/stow research remain open; this is not acceptance.

Development now retains the verified `SKL_300_ASRROOT` hip mount when the full
body is admitted, and preserves suppression for the arms-only fallback.
Retained before/after runs `20261018T233135652452Z` and
`20261018T233604434062Z` show the stock returning in both eyes; all four observed
body poses remained valid. This is scoped field visibility, not all-gear fit.

`GearContact` implements the one-second candidate window, fresh grip edge,
item-change invalidation, tracking/source loss and bounded spatial eligibility.
The existing body-mounted field camera is its first integration, including
same-update return to the current mount. Automated boundary/integration tests
pass. Live attempts `20261018T234031445240Z` and `20261018T234159193788Z` failed
pickup. Contact-confirmed attempts `20261018T234507504840Z` and
`20261018T234853822514Z` also failed: the candidate disappeared before squeezing,
within the one-second interval and with the observed hand still near the mount.
The exact invalidation cause is unresolved; do not attribute it to stale test
position alone. Candidate-loss diagnostics are in development.
Native weapons, binoculars and iDroid are not yet connected to this grab path.
See [native findings](BODY_GEAR_NATIVE.md) for ownership and lifecycle gaps.

Latest checked development candidate `8d737ec12660`: 58/58 automated groups and
installer checks passed. Published play remains unchanged. Campaign backup
before transition testing: `body-gear-original-20261018`.

Diagnostic-build live retries stopped before field verification: blank startup
eye (`20261018T235144861658Z`), missing title-anchor sample
(`20261018T235215397260Z`), then stale compositor eyes
(`20261018T235334739406Z`). A fresh owned-session launch subsequently timed out
waiting for simulator initialization / the first game image. These failures
are not gear acceptance. No ACC departure/return was performed.

Recovery found C: completely full. The simulator's 1 GB log was preserved on
D: in retained run `20261018T235447829508Z`, freeing space without deleting
evidence. A subsequent launch succeeded, but run `20261018T235819985614Z`
still failed a blank right-eye check during title handoff. The runtime logged
stale rig publications and roughly 2.2 new native pairs/second. Disk recovery
therefore did not resolve the live-session blocker. All six campaign/profile
files matched the pre-test backup. Owned game/simulator closed; published
`3e6e3d6e9818` restored to the installation, Steam retained.

Required integrated acceptance: select the actual loadout in the ACC, handle
and inspect it there, deploy, use it in the field, return and handle it again.
Include changed/depleted items, menu interruption, tutorial stow and immediate
iDroid reopen. Retain the no-cabin-firing policy. No full-loop pass is claimed.

Retained investigation run `20261018T224746514907Z` reached the saved field
but the existing body case stopped at `Body feet lacked complete native floor
clearance`, before its downward inspection captures. This is a failed baseline,
not gear or body acceptance. No gear implementation was changed in this run.
Owned test session closed using `workspace.py stop-sim`; Steam retained.

## Acceptance procedure

For each reachable menu record entry, navigation, selection, Back, Cancel,
confirmation, repeat entry and return to cabin. Deletion tests use disposable
fixtures and check cancellation, selected-item identity and persistence.
Unknown or unavailable pages remain untested, never implied passes.
Review both final eyes for body, device, panel and hand presentation.
Simulator results do not establish physical headset or whole-game acceptance.

## Actual results and current blockers

- Candidate 9e82957fb4a3: all 57 automated build groups and installer checks
  passed. Available through Play-Dev.cmd; published play remains unchanged.
- Cabin workbench run 20261018T193238548690Z: 21 interaction phases passed,
  including both-hand camera/shutter, both-hand note pickup, typing, writing,
  board attachment, cassette insertion, fingertip eject and carrier recovery.
  Paired captures reviewed. This is scoped prop evidence, not cabin acceptance.
- Retained iDroid run 20261018T200711267449Z passed two open/hand-motion/stow
  cycles, immediate reopening, 2 cm of unfocused cabin locomotion and clean
  stow. Both motion eyes reviewed. This does not cover all terminal pages.
  Integration repeat 20261018T201610397615Z also passed after direct-menu work.
- Retained body run 20261018T200643064319Z passed all 16 seated, standing,
  physical-step, full-turn and return poses without recentering. Fresh body
  and both hands remained valid throughout. Standing and 181-degree paired
  captures reviewed; legs now support the tracked pelvis. Physical headset
  comfort, continuous gait and stick-turn coverage remain separate checks.
- Pause open/Back round trip completed in 20261018T194202637882Z;
  paired spatial panel review passed.
- Retained settings/remap run 20261018T195727303166Z passed settings
  Cancel/Apply/restore, VR off/on, binding recording and cancellation, apply,
  conflict rejection, driving bindings and using the remapped binocular control.
  VR-on and conflict-rejection paired eyes reviewed. Personal bindings restored.
- GPU contention cleared and fresh live runs resumed. Retained return-to-title
  dialog run 20261018T200902231755Z passed opening, cancellation, return to Pause
  and closing Pause. Intentional title transition also worked, but its generic
  cleanup gate incorrectly expected a live cabin camera on the title screen.
- Direct native title menu is now the source default; Menu toggles optional
  props. Rig admission is preserved while menu and prop input are separated.
  Retained run 20261018T202159448701Z passed optional-prop availability, return
  to ordinary menu, and Continue without a cassette. Both menu eyes reviewed;
  removed only the witnessed scene-camera backdrop order 166. Earlier broader
  suppression removed text and was rejected. Menu contrast over the bright
  window still merits improvement; not a polished presentation acceptance.
- All menu branches, deletion permutations, wall-weapon handling, title-screen
  full body, NPC persistence and physical headset acceptance remain OPEN.
- Cleanup verified all original saves, four personal INIs, six photos and board
  data by hash; both build slots and installed published pin match. Meta Quest
  Link restored; owned simulator closed; temporary probe removed. Receipt:
  artifacts/dev/cabin-mechanism/cleanup.json.

## Work and cleanup

Use this checkout and tools/build.ps1. build/dev is the checked test candidate;
play is the published pin and must not be replaced without an authorized release.
Play.cmd and Play-Dev.cmd retain wired Meta Quest Link. Desktop stays 1280x720;
VR uses runtime per-eye recommendations. Do not restart Steam or automate windows.
Use bounded workspace.py bot cases; release controls and close test-opened menus.
Restore original saves, personal INIs and CabinStudio content after testing.
Current restoration receipt/backup root: artifacts/dev/cabin-mechanism.
Historical CURRENT text is preserved there as CURRENT-before-consolidation.txt.
