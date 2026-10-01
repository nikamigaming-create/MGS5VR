# Current work and test build

The release goal is playable TPP from start to finish, with reliable physical
weapons, optics and binoculars, the existing left-arm weapon/status display and
left-arm HUD popups, and the right-hand iDroid. Pause uses a spatial stereo
panel. Floating firearm aiming overlays stay off. No desktop/input automation
or Steam shutdown is needed by the local workflow.

## Current result: September 30

### User request status

The release is **not accepted from start to finish**. Keep this list separate
from build passes and from older scene evidence.

The current local work adds a shared live 3D settings editor and revises the
iDroid holding frame. All 54 VR adjustments have a fitting view; hand/device
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
acceptance remain open. The September 30 GitHub prerelease is unchanged.
The editor passed 38 headless fixture checks, including complete save/reload,
discard, orbit/zoom, accurate fractional text scale and reset preserving
handheld mode. The retained controller guide still resolves 98 action locations
across eight contexts. The focused bot contracts now have 127 checks.

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

Another player reported the helicopter upgrade lock-up while in the ACC;
the user did not personally experience it. The retained native
Mother Base -> Development -> Helicopter list/Back pass was in field Mission 6,
so it does not reproduce that ACC report. The exact reported ACC screen remains unknown. Native ACC iDroid development
has now been reached through the existing VR bindings: Mother Base has Customize
first and Development second, unlike the field recipe. The Support Helicopter
Armament list opens, and its ordinary exit requires three Back edges. A prior
run failed the old two-edge cleanup; the test loop now allows four guarded edges
and never starts a second cleanup chain after an ambiguous failure. The separate
helicopter customization selector and upgrade purchase remain untested.
See ACC_MENU_REGRESSION.md for retained successes and the forced-menu failure.
A verified disposable campaign is used; no save replacement or purchase was
performed in the resumed session. The field baseline's
selected helicopter grades were already developed, so Confirm did not open a
purchase dialog. Item/upgrade confirmation and purchase completion remain open.
The list can
take several seconds to populate. A test that sent an extra Back after an
already-developed item did nothing has been retained as a test failure,
not a reproduced game lock-up.

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

The cold ACC iDroid can display a native Choose a Rival Help overlay. After
closing the reviewed help card, tab/Back input still failed to change its page
in the latest cold fixture. Held-Back's existing direct terminal stop closed
its UI but left an overlapping Pause/player device state; immediate reopening
failed. Deferred-close, cancellation-permission and ACC pad-exclusion experiments
did not repair that failure and were reverted. Cleanup now refuses an overlapping
iDroid/Help owner and closes only the exact owned test session on failure.
Forced stow/reopen and the reported helicopter upgrade lock-up remain open.

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

The cutscene's exact hologram distance and scale have **not** been recovered.
The current 8 cm default is approximate. The native device sockets, parts
attachments and matching cutscene animation/UI package are recovered, but
their raw canvas units and effect offsets are not a measured screen fit.
Do not replace this remaining native projection work with another guessed
default or describe the present fit as an exact cutscene match.

| Request | Current result |
| --- | --- |
| One checkout and one stable playable folder | Implemented: this checkout, fixed play/, Play.cmd, automatic verified local synchronization. |
| Stop making dated local distributions | Implemented; 37 old generated folders/archives removed, 2.904 GiB reclaimed. |
| Keep Steam running; no computer control | Preserved throughout this work. Only owned test sessions and explicitly authorized unused simulator helpers are closed. |
| Left bionic forearm weapon display and ordinary HUD | Restored authored layout; both eyes and head/hand motion reviewed on the display baseline. Physical headset reading and every HUD context remain open. |
| Crisp high-resolution VR, stereo; desktop 720p | Native 2520 × 2640 per eye, distinct updating final eyes, desktop 1280 × 720. Continuous physical refresh and comfort remain open. |
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
| Live rotatable 3D previews for the settings | Installed: all 54 controls settings have a reference fitting view, with live edits, drag/orbit/zoom, front/side/top/back, filtering, save/reload, discard and numeric-fit reset. Runtime theatre dimensions are also previewed. Reference geometry is separate from native projection-effect and headset acceptance. |
| Default -30 degree weapon tilt and check the arsenal | Firearm-only fit implemented, configurable and installed; current-loadout one-hand scope direction inspected. All-weapon, impact, reload, support-contact and headset coverage remains open. |
| Helicopter upgrade menu coverage | Native ACC Development -> Support Helicopter Armament list reached; three-level ordinary exit observed on a historical run. Corrected-loop regression, selector exit, confirmation/purchase and the reported lock-up remain open. |
| Audit one-hand/two-hand control chords | Effective personal binding audit retained; no unsolicited remapping. Headset usability and crouch/prone community report remain open. |
| Align right-hand iDroid with its native emitter/cutscene and survive forced stow/reopen | Native attachment retained; natural holding frame and lower-edge screen anchor revised. Default and custom fitting controls pass scoped simulator motion/reopening checks. Native light-cone retargeting at custom offsets/rotations, forced tutorial stow and physical headset acceptance remain open. |
| Preserve Coco's fixes; burn down community reports; all weapons/missions/game completion | Coco's supplied changes retained. Community ledger and full-game acceptance remain open; an A* bridge route does not certify missions or combat. |
| Publish the new build on GitHub | September 30 experimental prerelease packages the tested local DLL and committed source; see RELEASE_2026-09-30.md. Full-game and headset acceptance remain open. |

The next iDroid work is the reproduced cold-ACC menu/stow/reopen failure, exact authored screen fitting and native light-cone retargeting,
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
**2520 × 2640 per eye** now produces updating, distinct final compositor eyes.
The compact readout is back on the **left forearm**, with readable weapon,
ammo and suppressor text. The fix restores the native HUD's authored 128 × 72
layout; it does not move arm bones or replace personal bindings.

Both final eyes were inspected through wrist translation/rotation and opposing
head/hand motion. Four 24-second recordings contain 308 decoded, distinct
frames with no blank frame or detached readout observed. These are sequential
left/right compositor captures at 3.0–3.4 samples/second, not synchronous stereo
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

Use this checkout. `play/` is the fixed current local test tree;
`Play.cmd` launches the installed game on the physical headset. The launcher
is always `play/MGS5VR-Launcher.exe`. `play/BUILD.json` identifies the source,
DLL and every installed file. Personal game settings remain authoritative.

```powershell
# Build, run checks, refresh the same play folder, and sync the recorded game.
tools/build.ps1

# Compare source, current play tree, installed DLL and issue status.
python tools/workspace.py status

# Test runtime is selected for the game process only; Steam stays open.
python tools/workspace.py launch-sim
python tools/workspace.py bot --command continue
python tools/workspace.py bot --seconds 15
python tools/workspace.py stop-sim

# Run a deliberate case, or a bounded route over the private owned NAV2 graph.
python tools/workspace.py bot --command run --suite tools/gameplay_bot/suites/pause-roundtrip.json
python tools/workspace.py navigate --goal X Y Z --seconds 60
```

`build.ps1 -NoDeploy` refreshes play without changing the game installation.
Normal synchronization requires MGSV closed, checks the recorded install's
ownership, preserves both personal INIs byte for byte, and updates actual DLL
hashes. One previous play tree and one small installed-binary rollback are kept
under build; no dated distribution folders or ZIPs are made for local changes.
Promotion updates changed files in place, commits BUILD.json last and preserves
unmanaged local files. An unchanged open launcher stays running; a blocked file
update rolls back changed files. Failed tests do not promote a build.

For an explicitly requested GitHub release, commit the public source, run the
same build/checks, then use `python tools/package-release.py --tag experimental-YYYY-MM-DD`.
The packager verifies the checked play tree and public defaults, includes the
committed source and per-file hashes, and writes to the fixed `build/github-release/`
directory. Private game data and simulator dependencies are rejected. Publishing
a prerelease does not close the remaining gameplay/headset acceptance queue.

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

The desktop preview is **1280 × 720**, independently of VR quality. Preserve the
active runtime's per-eye FOVs; a square target is not a substitute for correct
optics. Meta XR Simulator v207 recommends 1680 × 1760 here. The current test
renders native eyes at 2520 × 2640 (150% in each dimension) and filters each
eye separately into fixed 1680 × 1760 simulator swapchains, in linear light.
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
