# Current plan — ACC readiness

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
