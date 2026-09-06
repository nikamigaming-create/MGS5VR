# Development handoff — 2026-09-06

This is an experimental checkpoint. A first physical Quest 3 combat run is now
recorded; arm geometry and HUD placement still block full acceptance.

## Physical test follow-up

Read [HEADSET_REVIEW.md](HEADSET_REVIEW.md) first when resuming. The user supplied a
153-second Oculus Mirror recording and reported that gameplay worked well, while
asking for coherent arms and personal HUD moved out of the main view. Keep the
enemy/location markers they find useful. The next slice is explicit rifle
hold/lower/reload states, corrected sleeves and hand contact, and real live
forearm status. The physical run used the installed 4B299FF8 build and exited
cleanly through Oculus. The preflight rebuild 6729001B passed five suites but was
not installed for that run. No runtime source change was made during the review.

## Saved implementation

- Native same-game-frame stereo and a player head-bone camera that remains first person with the weapon lowered.
- Player-owned head-model and body-group exclusion, retaining native arm groups; checked restoration when visibility or appearance changes.
- Tracking suspension and recovery without switching to a theatre quad or resetting the established head origin.
- Experimental UI worker/source matching and guarded projection changes. HUD pixel extraction and wrist display are not implemented.
- Default-off native controller rig: full render-pose arm IK and matching source pose for eyes. The rifle follows the right grip; native authored muzzle and projectile output were checked for AM MRS-4. Right grip readies, trigger fires, left grip supports, B reloads.
- All five local test suites passed, including GPU transfers, camera contracts, player ownership/replacement, and proxy/exit fixtures.

The last locally installed and live-tested DLL SHA256 is
`4B299FF8E47EB5EE7934F1FA3E57424913F5DE4B2102752114428A2896A8FB07`.
It targets only the executable baseline in the game profile. A CI build may have a
different DLL hash; this hash identifies the local observation, not a reproducible-build guarantee.

## Latest observation

Latest footage: `controller-grip-aim-left`, 35.234 seconds, 121 distinct captured
frames at 3.43 captures/second, on the current 4B299FF8 build. Right-grip readiness,
left-grip support, controller translation/rotation, native shots, reload and an
independent head lean/yaw completed. No fully black capture occurred. The silent
mobile file is `artifacts/MGS5VR-controller-grip-mobile.mp4` (35 seconds, 720x754).

The controller experiment and its limits are documented in [CONTROLLER_RIG.md](CONTROLLER_RIG.md).
The first 29.25-second left-eye controller capture has 101 distinct frames at 3.45
captures/second, no fully black captures, and completed translation/yaw/pitch/roll,
fire and reload actions. A private probe at native projectile creation confirmed
the controller-directed muzzle origin and shot direction. A separate head yaw and
6 cm translation left the controller poses and gun's world direction steady.
Walking retained first person. Releasing weapon-ready input exposes a stow/hand
animation defect; shoulder/sleeve intrusion and face HUD remain. This is failed
full-mod acceptance, not a finished hands/HUD demonstration.

The 59-second continuous left-eye capture `fps-head-visibility-movement-left`
contains 205 distinct frames at 3.47 captures/second. It includes head translations
and rotations, walking, stance changes, firing (29 to 27 rounds), reload (31/169),
and walking with the weapon lowered. No fully black frame was captured. It is not
a 60 FPS recording and does not qualify as the requested full-mod demonstration.
The footage and raw runtime evidence remain local and excluded from source.

Black dropouts initially coincided with the simulator's unused synthetic-room helper
occupying about 7 GB of dedicated GPU memory plus shared memory. Stopping that helper
reduced dedicated usage to roughly 5 GB and the subsequent capture showed no black
frames. This was a local session intervention, not an automatic launcher fix.

## Next work

1. Fix shoulder/sleeve intrusion, camera eye offset and rig attachment across stance and animation changes.
2. Finish HUD extraction and wrist attachment; correct world-marker projection and verify both eyes during motion.
3. Extend the ordinary-rifle muzzle binding to scoped/alternate modes; verify impacts, obstruction, weapon families, stow and tracking failures.
4. Diagnose remaining simulator frame stalls and runtime shutdown reliability. Verify cold startup without synthetic-room memory pressure.
5. Implement cinematic classification/skip coverage, then complete the continuous single-eye acceptance sequence and physical-headset checks.

Resume through the existing Continue/Resume save and equipped gear, using the windowed
1280x720 launcher. Use OpenXR actions for UI/gameplay, with no Windows key/mouse/focus
automation. Keep DOF, motion blur, post-processing and camera shake disabled. The
left-grip/left-stick-click chord toggles native VR; lower the weapon during verification
to ensure first person persists. Work stays in one task unless the user requests otherwise.
