# Native title cabin — 2026-09-19

## Proven defects and repairs

- The below-floor screenshot is reproducible with LOCAL HMD position
  `(0.15, -1.66, -0.1)`. The original room producer always returned invalid:
  its validator required `valid=true` before construction, and rear Z had the
  wrong sign. Both are fixed, with production-constructor tests.
- Six native collision samples at the tested title camera returned positive
  hits, including a floor 0.865845 m below the anchor and ceiling 0.505249 m
  above. The corrected low-pose capture remains above the floor. These samples
  are a point envelope, NOT a capsule sweep or certification of the whole cabin.
- The synchronous action pipe previously blocked native Lua on a response
  write while the pipe worker was reading the next request. Only the worker
  now performs pipe I/O; Lua publishes replies to a bounded response queue.
  The client has bounded nonblocking response reads and sequential batching.
- A fixed native cassette anchor is retained across camera constraints. Its
  inverse tracking-space transform is used for hand targeting. Unit tests
  reject the displaced phantom target and stale publications.

## Real D-Dog, not a replacement mesh

`src/native_cabin.lua` is embedded in the DLL and runs on the native Lua
transaction. It requires title helicopter state, an already obtained/sortieable
dog, the authored `HelispaceLocatorIdentifier/BuddyDDogLocator`, and the native
buddy block. It uses native `Load`, `HeliSpaceSetting`, and `CallBuddy`.

The essential missing step was removing **only**
`heli_common_sequence.lua/S_DISABLE_NPC`. The title sets that stop even when
the buddy block is loaded. After resetting it, real dog bones were published
and the animated native model became visible in both eye checkpoints.

`GetPosition` returning nil is not sufficient evidence that D-Dog is absent:
that command still returned nil while the native bone response and final-eye
images positively showed him. Use `inspect-animal-touch` together with pixels.

The original buddy selection and NPC stop are retained in mission-local state.
The `title_sequence.ClearTitleMode` wrapper restores them before native title
exit. An isolated mock fixture exercises load/wait/activation, idempotence,
cancellation, cleanup order, unavailable dog, and scene-mismatch refusal:

```powershell
python tools/test-native-cabin.py
```

This fixture uses the live Lua VM but mocks all game state and mutating APIs.
It does not itself prove a successful in-game Continue transition.

## Fresh-launch and physical-stroke verification

Installed and cold-launched DLL SHA-256:
`D7AEE732D9116C9A7FAE29276BCB188E445164A7DF675B23575BE4C06E392AE3`.
The fresh session reached mission 40050 / title=true / lifecycle=active, with
original buddy selection 1 preserved. No manual dog load/placement command was
used in this session. The native locator rotation is passed through the game's
`Tpp.GetRotationY`, not nonexistent quaternion `GetY`/`GetW` methods.

`artifacts/cabin-final-floor-and-dog-20260919.json` repeated the exact failing
HMD pose, returned to neutral, and looked at D-Dog. The final low and neutral
captures show the floor and fixed native cassette anchor. Both-eye low-pose
and dog images were inspected. This is not whole-cabin navigation acceptance.

Continuous dog interaction: `artifacts/cabin-native-pet-20260919-02/simulator.mp4`.
The sequence only sends tracked-controller poses and neutral grip/trigger
values. It does not directly request a native pet response. The native log
inside the capture window reports:

```text
1789832782468 Physical D-Dog pet response native_result=1
1789832782468 Physical D-Dog stroke hand=1 travel=0.072934
```

Start, all captured contact frames (2–12 seconds), and the withdrawal/end were
reviewed. The right hand contacts the visible dog's head and the native dog
responds; both hands are visible during the contact portion. The captured
view does not certify headset ergonomics or stereo correctness.

The earlier `cabin-native-pet-20260919-01` take is **rejected as petting proof**.
The operator returned `Error: Cannot query current right controller pose for
animation start.` without MCP `isError`, and the old recorder ignored it.
The recorder now fails on those textual errors, retains each input result,
and fails capture when its sequence fails. Four isolated recorder tests pass.
The replacement take uses sampled instantaneous controller updates, the same
working input mechanism as the gameplay runner, not operator-side animation.

Validation: 14/14 CTest tests; 22 isolated native Lua lifecycle checks; 4/4
recorder tests. These counts do not certify the remaining gameplay claims.

## Acceptance remains incomplete

| Claim | Evidence / state |
| --- | --- |
| Reproduced below-floor pose stays above floor | Fresh-launch `cabin-final-floor-low-20260919` both-eye captures |
| Native D-Dog visible and animated in cabin | Automatic native lifecycle on the installed DLL, both-eye checkpoints and continuous pet take |
| Pet clip provenance | Actual Meta composited right eye, 17.881 s MP4, 71 arrivals / 18.203 s, 3.900 capture fps, 481652 bytes; no audio or stereo claim |
| Two real cabin rats | **Fail:** no rat objects/models, rat lists, or native animal script block in tested title scene |
| Physical dog stroke | Visible tracked hand contact plus native response at the matching capture time; simulator only |
| Whole-cabin walking / containment | Not certified by six axial rays |
| Spatial Continue into saved gameplay | Not yet captured in the requested animal-interaction sequence |
| Multiplayer | Never entered; the spatial MGO action remains blocked |

Do not label the dog-visibility clip as petting. Do not label either dog clip
as rat pickup or end-to-end acceptance. No save files or progression flags
were changed by these repairs. The full menu-focus/submenu state reader and
title-to-game cleanup still require live verification; Continue currently
relies on the initial title focus, and all other spatial actions are blocked.
