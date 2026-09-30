# September 19 release candidate

**Superseded investigation record. RC1 was rejected because its flat title fallback violates the user's requirement to remain in 3D. The current build is not release-approved. Use [the current handoff](MAX_HANDOFF_2026-09-19.md) for exact state and remaining work; historical visual claims below are not current acceptance.**

The September 16–19 tester report is the acceptance list. A code correction or
passing unit test is not a physical-headset pass. The experimental cassette and
actor cabin is disabled by default (`opening.interactive_cabin=0`); the complete
native title menu remains available.

| Report | Candidate change / acceptance still required |
| --- | --- |
| Menu / Continue stalls | Loading cameras cannot replace the live native loading and Start Mission screen. Title UI suppression ends when Title ends. Test covers loading camera publication and fresh gameplay recovery. Live handoff required. |
| Cannot progress through prologue | Scenes without an accepted player rig retain native held buttons and both movement/look sticks. Test covers fallback without toggling the user's layout. A full prologue playthrough remains required. |
| Blackout near HMD / after controller idle | A valid native skin and headset transaction may publish while controllers are occluded. The fallback carries no invented tracked hands or weapon interactions. Unit regression added; physical controller occlusion remains required. |
| Body / horse alignment drifts | Horizontal head stabilization now follows changed native pose offsets rather than freezing the first offset for the session. Regression verifies convergence after a posture/mount displacement. Horse, hiding and prolonged headset motion remain required. |
| Left support hand pops off | An acquired support contact no longer releases merely because wrist inspection or the acquisition cone changes. Explicit release, tracking loss and distance still release it. Final-eye motion required. |
| Action / pickup prompts require wrist angle | Context prompts use a shared source-head plane and bypass wrist visibility gating. Other HUD layers retain their existing policy. Both-eye interaction check required. |
| Mounted weapons cannot aim | Existing current-source fix preserves both native mounted look axes and suppresses detached mounted snap offsets. Turret, mortar, tank and Pequod coverage remains required; this is not an all-vehicle claim. |
| Controls differ from manual | Current shipped defaults use A stance, B carry, Y context and X commands. Existing personal configurations are preserved; updating a DLL does not replace them. |
| Smooth turning ignored | Existing live controls loader and native-smooth tests pass. Edit `mgs5vr-controls.ini` beside the installed game DLL, then release inputs for two seconds. A different file named `controls.ini` is not loaded. |
| Edit controls does nothing | Launcher no longer applies SW_HIDE to the editor's first WinForms window. PowerShell's console remains hidden. |
| Cannot open pause / restart | Default Menu hold opens Pause; Menu tap opens iDroid. Native layout uses Menu then left grip for Start and Menu then right grip for Back. |
| Squashed menus | Native flat menu/cinematic fallback uses the authored 16:9 aspect even with square eye render targets. High-resolution visual check required. |
| Binocular lag / marking | Existing source includes physical ocular gating, dwell marking and binocular-only recon policy. Performance and all target types are not certified by this patch. |
| Close-to-ground hand / weapon fade | Existing visibility and near-plane fixes remain. Physical prone/environment contact case remains unproven. |
| Obstruction prevents firing | Native obstruction rules remain. This patch does not bypass collision or claim that all firing tests use the tracked muzzle. |
| Resolution above about 4K | Not validated. Existing 4096-per-dimension launcher bound remains; no larger-resolution support claim. |
| Desktop faster than headset at 4K | No performance claim. Needs GPU/runtime timing on the affected hardware. |
| Infinite Heaven install conflict | Not resolved. Installer continues preserving an existing dinput8.dll; do not overwrite another loader. |
| Stance indicator / physical squat | Suggestions remain outside this stability candidate. |

Release gate: build and CTest pass, fresh both-eye menu/loading/gameplay captures,
then headset checks for occlusion, support grip, mounted aiming and a complete
prologue. Do not label this candidate as fixing every reported issue.

## Current evidence

Candidate DLL SHA-256:
`6AF620B091B6090AEF4473D1DF6E5CF4219C42BA20F104755B92F9A40026717A`.
Release build, 14/14 CTest suites and 4/4 recorder tests pass. The controls
editor opens the installed configuration and validates it successfully.

The first live run failed visual acceptance: the prologue retained TitleMode
while its title widget was hidden, allowing an invalid immersive title camera.
The corrected build reads TitleMode independently of the widget, keeps native
title/intro pixels on a 16:9 screen, and clears retained stereo surroundings.
This reader is installed even when the optional developer command pipe is off.

The corrected cold launch reached the hospital intro with native captions and
credits visible in both final eyes, without the black planes in the rejected
capture. Evidence is under `artifacts/gameplay-proof/rescue-hospital-title-stable-20260919-{left,right}.png`.
This proves the captured title/intro presentation only, not completion of the
prologue or the full Continue-to-field route.

The candidate's top-level `mgs5vr.ini` enables VR for manual DLL+INI installs;
the installer template under `config` retains explicit installation switches.
Existing users should use the launcher Update action to preserve their layout.
