# September 20 beta finish and community reports

This consolidates the September 13–20 reports supplied by the user. Reports
describe different builds and personal configurations. A source correction or
unit-test pass is not a headset gameplay pass. Older rescue/handoff documents
are historical; this is the current triage plan.

## Current release: September 20 experimental

The user approved packaging the v12 candidate after the Steam launcher fix.
[Release notes](RELEASE_2026-09-20.md) describe the shipped candidate and its
limits. The inventory below is the earlier triage record; it does not imply
that every community issue was fixed or retested. The iDroid tutorial escape
and launcher startup have since been addressed.

## Next local candidate

The next candidate defaults iDroid and Pause to a tilted, world-anchored panel
while retaining the 3D scene and head tracking. iDroid uses its own named retail
pause registration with the native iDroid tutorial mask, which leaves UI updates
running; the terminal's verified close-request byte releases only that
registration before the native stow animation. Waiting for the open bit to
clear also deadlocked exit and was corrected. The ordinary menu mask
was rejected in SIM because it froze opening and navigation. Handheld iDroid
and wrist Pause are opt-in with `settings.handheld_menus=1`.

The source also adds editable player/hand fit, menu size/depth/tilt, weapon
smoothing, support grip/detach radii and relaxed/touched finger curl. Binocular
eye alignment tolerates a wider eye box without enlarging the physical lens.
Dog-specific hand pushback is removed. Cabin movement uses native swept camera
clearance and floor checks instead of the initial small ray envelope.

Controller ownership now preserves connected XInput pads in 3D and theatre.
Theatre uses normal native buttons and a recentered 12-metre screen; Menu+B
toggles presentation. Loading may not override the manual choice. Menu axes no
longer overwrite native menu routing. These source changes need the exact
candidate's SIM and headset checks; no all-controller claim is implied.

Release build and all 18 automated suites pass, including hidden native launcher
button dispatch, headless editing/validation, 5120x4096 configuration, menu
anchoring and controller ownership. The test does not establish actual 5K
headset performance. Meta platform-menu/idle reproduction, physical Xbox input,
tanks/turrets, long play sessions and the raised-arm sleeve defect remain open.
The bounded adaptive pacing portion of s-ilent's fork is included; its unsynchronized
worker is not. Vinion's alternative filter and the supplied C++ replacements
are not represented as integrated. See [performance findings](PERFORMANCE.md).

### Reverb G2 / SteamVR report on the preceding build

The tester reports that the helicopter menu works, enemy interrogation works,
and the binocular culling defect is gone. Preserve these results as headset
feedback for that build, separate from the local candidate's SIM checks.

Still reported: the handheld iDroid is sideways, one-hand gun pitch is too low,
binoculars require controllers inside the G2's reliable tracking distance,
prone firing sometimes produces a UI sound until standing and lying down again,
and arms feel short with restricted IK. The candidate adds adjustable binocular
eye distance and defaults menus to the paused quad. The one-hand correction
swings the authored muzzle axis toward the runtime aim pose at the tracked palm
pivot; the wrist remains the common owner of the hand, weapon, scope and shot
socket. This still needs the affected G2 user's physical-controller check. In
SIM, a prone shot blocked near a well edge fired after crawling backward without
standing. The reported UI sound, short-arm reach, sleeve deformation and prone
model fading remain open.

## Historical handoff: v11

[The v11 playtest note](PLAYTEST_V11_2026-09-20.md) records the current DLL,
completed high-risk SIM checks and remaining failures. Map geometry now has
projected canvas bounds. Pause arm tracking, the forced FOB tutorial and full
notification routing remain open. The user requested a bounded headset playtest
before completing the full showcase matrix. Public beta acceptance is pending.

## Historical SIM result: v8

The equipment sequence passes the bounded SIM check. This is **not yet the
next physical-headset candidate**: Pause and iDroid map clipping still fail.
Keep testing in SIM until those display faults are corrected.

- Native left-trigger preview shows the game's four real category cards.
  All four categories, primary browsing, B back, release, reopening, independent
  head motion and left-arm motion completed in one continuous 43.5-second take.
  Permanent status stays on the left forearm; category cards and descriptions
  remain above that wrist. Both-eye compositor checkpoints accompany the take.
- Ordinary iDroid follows the cupped right palm. Pan, zoom and B close respond,
  including opening and closing again after the equipment sequence. **Zoomed
  terrain/roads escape the intended panel bounds.** This is a rendering failure.
- Pause stops native arm animation. Moving the controller does not move the
  visible arm. The panel remains near the frozen left arm, but is oversized and
  partly off-screen in the tested view. Head movement still updates the world.
  The attempted paused-skin refresh was removed because it moved the attachment
  without moving the visible arm. Neither Pause tracking nor layout passes.
- A forced retail FOB tutorial initially prevented ordinary iDroid navigation.
  Diagnostic native Lua stopped that tutorial; normal map controls then worked.
  This is not a controller-only tutorial completion pass, and no persistent
  tutorial-completion flags or replacement saves were written.
- Fresh start, physical Continue-tape highlight/trigger, native Resume and
  arrival in mission 30010 FreePlay worked. All 15 CTest suites passed on v8.
  Notification/caption coverage, all weapons and physical-headset behavior
  remain unverified.

Installed/build DLL SHA-256:
`3a71980c7b82185d767ce1aa3be7753ba59c32f9db229d99c934bcfa6b3d15aa`.
The working tree contains additional uncommitted feature changes; the DLL is
not represented by `df9d972` alone. That commit contains the native preview fix.

Local review video: `artifacts/showcase-20260920/28-wrist-menus-v8/wrist-menus-v8.mp4`.
This is the native right-eye render texture cropped to the submitted display
FOV and rescaled, not a final-compositor recording. It has no audio. The source
recorder reports 1296 captured frames and nine dropped; the source encoder
emitted 1305 frames at 30 fps. Processing preserves all source encoded frames
and duration. `processing.json` records the crop, dimensions and hashes.

Failed-display captures are in `artifacts/gameplay-proof/`:
`wrist-v8-map-pan-zoom-right.png`, `wrist-v8-pause-right.png`,
`wrist-v8-pause-moved-right.png` and `wrist-v8-pause-head-turn-right.png`.

## Finish one candidate

1. Fix Pause's frozen arm publication and oversized native layout; clip zoomed
   iDroid map content to its handheld canvas. Preserve the working forearm HUD
   and native equipment preview/browse/Back behavior. Keep snap turning opt-in.
2. Exercise cold start, the physical Continue tape, native loading/Resume,
   equipment open/browse/back/close, both binocular eyes and menu motion in SIM.
   Record real compositor video and state exactly what it shows.
3. Build and package the same DLL with controls, launcher, known issues, source
   state and hashes. Preserve personal settings and saves. Run the package's
   installation/update/rollback checks.
4. Test that candidate in the physical headset using the short route below.
   Fix any startup, lost-world, unusable-menu or aiming blocker before public
   beta publication. Publish a bounded beta with remaining issues listed.

The candidate is a test handoff, not a claim that every mission, weapon,
controller runtime or third-party mod has passed.

## Short physical test route

Allow about 15 minutes; stop on a blocker and report the step and what happened.
The game log and candidate manifest identify the build, so there is no need to
reconstruct the whole session from memory.

1. Cold launch: logos remain at their intended screen distance. Enter the 3D
   cabin, move with the left stick, reach Continue until it highlights and
   squeeze that hand's trigger. Resume when the native loading screen offers it.
2. Raise/lower/rotate the left arm and turn the head independently. Permanent
   status stays on the forearm; popups follow the wrist and never jump to the face.
3. Hold left trigger without touching the stick: see the real four native
   equipment cards. Visit up/down/left/right, browse, press B, choose another
   category, then release. No split-second input race or accidental shot/use.
4. Cup the right palm, open iDroid, pan/zoom the map, change tabs and close.
   The handset and centered projection travel together. Open Pause with a
   left Menu hold, visit Options and return to play.
5. Raise binoculars: compare both eyes, zoom in/out, mark/unmark a guard and
   place a waypoint. Lower them: the surrounding terrain/buildings stay intact.
   Fire a pistol and a scoped rifle at a visible target; check aim and zoom.
6. Move controllers near the headset, above the head and below the waist;
   leave them idle, then move again. The world must remain visible when hand
   tracking is lost. Remove/reseat the headset and resume once.
7. Walk, smooth turn, crouch, prone, climb and pick up an item; check prompts,
   hands, weapon visibility and support grip. Mount/dismount the horse and
   inspect body alignment. Longer drift testing follows separately.
8. Grab/interrogate a guard, release or carry them, and open iDroid to inspect
   revealed intel. Record any missing command or invisible prompt.

## Issue inventory

| Priority | Report | Current disposition and next check |
| --- | --- | --- |
| Before public beta | Black frames near HMD, above head, below waist or after idle; desktop returns to third person | Tracking-independent world publication and camera continuity changes are in the candidate. Physical occlusion/idle/resume still needs the affected runtime test. |
| Before public beta | Continue cannot be selected; cabin movement changes hidden menu focus | Spatial cabin input isolation is committed. Fresh SIM handoff and physical reach/locomotion acceptance are required. |
| Before public beta | Forearm HUD detached; popups in face; squashed menus; blank loadout | Forearm status and native equipment menus passed the v8 motion sequence. Pause freezes the visible arm and its panel is oversized/partly off-screen. Deployment/loadout and remaining notifications still need separate checks. |
| Before public beta | iDroid map escapes handheld panel when zoomed | Reproduced on v8. Palm attachment, pan/zoom input and B close work, but map geometry renders outside the intended canvas. Preserve native clipping when projecting to the handheld surface. |
| Before public beta | Left trigger gives no direction preview; menus require fast input | Fixed in df9d972. All four real categories, primary browsing, B back, closing and reopening passed the continuous v8 SIM take. Physical-controller ergonomics remain untested. |
| Before public beta | Binoculars hide half the map/buildings; one eye missing; weak zoom; marking absent | Both-eye optic, wide-world camera and recon changes are in the candidate. Recheck both eyes, exterior geometry, zoom, mark/unmark and map reveal together. |
| Before public beta | One-hand aim/pitch wrong; scope hard to reach; zoom toggle ineffective | Requires pistol/rifle/scope tests with the intended bindings and headset/controller geometry. Do not infer all-weapon coverage from one rifle. |
| Before public beta | Controls differ from manual; smooth setting ignored; Edit Controls invisible | Snap is opt-in. The active file is **mgs5vr-controls.ini beside the game's dinput8.dll**, not controls.ini or the extracted package copy. Existing layouts are preserved. Validate editing/reload and clear config errors on the packaged launcher. |
| Before public beta | Pickups, plants, diamonds or restrained people cannot be interacted with; prompts hard to see | Test native context/hold actions and wrist prompts on each affected object. Default Y is context/Fulton, B hold is carry/pickup; campaign state still determines availability. |
| Before public beta | Cannot find interrogation or pause/restart controls | Menu tap opens iDroid; Menu hold opens Pause. Native CQC/Commands need visible, usable guard choices and an actual intel-reveal/map sequence. |
| Extended beta | Body/legs drift; horse displaced after long play; toilet hiding puts view outside | Native head-offset stabilization changed; repeated mount/posture/hide cycles and a sustained session remain necessary. |
| Extended beta | Support hand pops/warps off; hands vanish near ground; weapons fade prone/near walls | Support retention and visibility changes exist. Test two-hand sweep, physical prone and environmental contact. |
| Extended beta | Red obstruction reticle blocks firing despite apparently clear VR muzzle | Native obstruction remains. Needs comparison of actual tracked muzzle and native collision/shot source; no blanket collision bypass. |
| Extended beta | Turrets/mortars cannot aim; tank lacks elevation; Pequod gun inaccessible | Native mounted look routes both axes. Verify access, traverse, elevation, firing and exit on each emplacement/vehicle; physical mounted hand aiming is unfinished. |
| Extended beta | Short standing height, walking bob/sway, nausea; headset removal crashes | Test calibration and real headset movement, including resume. Source smoothing alone does not establish comfort or fix a tracking/runtime crash. |
| Extended beta | Prologue cutscenes missing Snake/body; scripted transitions uncomfortable | Full-body/demo camera corrections exist. Full prologue and Skull Face jeep sequence remain distinct tests. |
| Extended beta | >4080 resolution launch failure; headset slower than desktop at 4K; directional/bino lag | Current configuration bounds allow up to 8192 and native render-size checks were extended. Actual >4K gameplay, GPU/runtime timing and sustained performance are not established. Desktop FPS is not headset FPS. |
| Compatibility | Infinite Heaven/IHHook blocks installation | Existing dinput8.dll is preserved. Safe loader coexistence is unresolved; do not tell users to overwrite/remove their loader as an installation fix. |
| Compatibility | SnakeBite mods, ReShade and DLSS compatibility | Individual users report some SnakeBite mods working. There is no general compatibility certification. IHHook, graphics injectors and save/gameplay mods need separate versioned tests. |
| Compatibility | DLL/INI swaps start flat; uncertainty about installation folder | Package and launcher must target the selected game's folder and expose the active configuration. Validate manual/full-package install and update without silently replacing personal controls. |
| Later features | Stance indicator, physical crouching, Quiet, two pickup rats, universal mod installer | Separate feature work. Do not delay the stability handoff or advertise unfinished interactions as working. |

## Launcher mod support

For this beta, preserve existing mods and provide a useful conflict diagnosis.
A future mod page can inventory installed loaders, open SnakeBite, document
supported combinations and offer reversible installation for explicitly
supported packages. Arbitrary mods cannot be promised to work together:
DLL entry points, archive replacements, Lua hooks and save changes have
different compatibility requirements. Start with Infinite Heaven + IHHook +
SnakeBite, then test graphics injectors separately. Do not bundle third-party
mods or game assets into this candidate.

## What is still needed from testers

No further reports are needed to start fixing. For a runtime-specific failure,
retain the candidate version, headset/controller model, OpenXR runtime and
connection (Link, Air Link or VD), per-eye resolution/refresh, exact action,
and the current game log. Those details distinguish a mod regression from a
configuration conflict or a runtime-specific failure. Existing save backups
should remain intact; use the user's own progress for the short test route.
