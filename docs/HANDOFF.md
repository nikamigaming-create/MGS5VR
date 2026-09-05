# Development handoff — 2026-09-05

This is an experimental checkpoint, not a finished VR mod or an invitation to user testing.

## Saved implementation

- Native same-game-frame stereo and a player head-bone camera that remains first person with the weapon lowered.
- Player-owned head-model and body-group exclusion, retaining native arm groups; checked restoration when visibility or appearance changes.
- Tracking suspension and recovery without switching to a theatre quad or resetting the established head origin.
- Experimental UI worker/source matching and guarded projection changes. HUD pixel extraction and wrist display are not implemented.
- All five local test suites passed, including GPU transfers, camera contracts, player ownership/replacement, and proxy/exit fixtures.

The last locally installed and live-tested DLL SHA256 is
`5906E0D2C52C3B95AB220CAFEAC47A897C458C79EE470883D175B8F5F35E3CBF`.
It targets only the executable baseline in the game profile. A CI build may have a
different DLL hash; this hash identifies the local observation, not a reproducible-build guarantee.

## Latest observation

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
3. Bind tracked hands and weapon aim to the native muzzle/ballistics and verify weapon families.
4. Diagnose remaining simulator frame stalls and runtime shutdown reliability. Verify cold startup without synthetic-room memory pressure.
5. Implement cinematic classification/skip coverage, then complete the continuous single-eye acceptance sequence and physical-headset checks.

Resume through the existing Continue/Resume save and equipped gear, using the windowed
1280x720 launcher. Use OpenXR actions for UI/gameplay, with no Windows key/mouse/focus
automation. Keep DOF, motion blur, post-processing and camera shake disabled. The
left-grip/left-stick-click chord toggles native VR; lower the weapon during verification
to ensure first person persists. Work stays in one task unless the user requests otherwise.
