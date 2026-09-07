# Sky crop and wrist SIM review — 2026-09-07

This records the SIM review before physical launch. A subsequent Quest 3 run
received positive basic-experience feedback with low resolution/jaggies remaining;
see [HEADSET_SHARPNESS_REVIEW.md](HEADSET_SHARPNESS_REVIEW.md). Unproven physical
claims below describe the state at the time of this SIM review.

The user's capture showed a large rectangular lighting boundary in the sky.
Rendering a centered enclosing field removed that sampled boundary. Submitting
the whole enlarged field had previously failed physical stereo acceptance, so
this candidate retains the runtime's requested optics separately: each eye uses
its own pixel crop and the exact angular bounds of that rounded crop. Original
source poses and transaction identifiers remain attached to the pixels. There
is no duplicate-eye or depth-reconstructed substitute.

Installed DLL SHA256:
`7BF543BE8E4EC882295AD8491DA6CCEB88B98BB878120DDBC54C0D6C371B170A`.
All five local suites passed, including 1,685 core checks and 33 real D3D11 checks
across two devices. New contracts cover source-to-submitted ray equality, opposite
eye crops, coverage/size rejection, and the bounded age/generation gate. These
checks are not physical headset acceptance.

## Observed final images

Testing used Meta XR Simulator 205, the user's existing 1% checkpoint, native
1920x1080 rendering and clouds On. The large sky rectangle was absent in the
sampled forward, yaw, roll, pitch/lean and return views. Both eye images were
captured sequentially; these are not simultaneous stereo video. Low-light feature
matching produced only 17–26 correspondences per pose, insufficient for the
existing 100-match numerical gate. This is visual evidence for the sampled sky
boundary, not a numeric or physical stereo pass.

Private evidence: `artifacts/sky-crop-7bf543-review` and
`artifacts/wrist-sky-15s/simulator.mp4`. The latter is a real final-left-eye recording:

- 14.714 seconds, 670,771 bytes, silent.
- All 54 decoded frames were distinct and reviewed; no fully blank frames.
- 3.6 captures/second, maximum capture response gap 313 ms; this does not measure
  headset frame rate or prove motion smoothness.
- Primary, secondary, support and item cards open above the left wrist. The popup
  follows wrist movement and closes; native weapon/status UI stays on the forearm.
- A head turn and lean retain the native world. Both closed-menu eye endpoints
  were also inspected. No capture or semantic-input errors were reported.

Inputs came only through the simulator's semantic OpenXR controller/pose APIs.
The game executable, archives and save were not patched. The recording streams
directly into a small MP4 rather than storing an unbounded frame sequence.

## Runtime stalls

Stage timing isolated long delays inside the simulator's frame wait/submission,
not texture uploads. A separate unused Meta Bedroom environment consumed about
7.2 GB of dedicated GPU memory. Stopping that helper reduced total measured GPU
memory use from about 11.45 GB to 4.51 GB and restored the observed 72 Hz XR loop
and roughly 60 native pairs/second. That is an environment correction, not proof
that code alone fixed the stalls.

The runtime now waits nonblockingly for both eye swapchains before publishing
either. New sources still need a complete pair from the active generation and
must be at most 150 ms old. During a stall, an already accepted complete pair can
be presented for up to 500 ms with its original poses and FOVs, while current head
tracking remains valid. Missing/invalid generations still fail closed. Empty and
retained submissions and slow runtime stages are logged. This is bounded recovery,
not acceptance of frozen or delayed hands.

## Open failures

Blue marker streaks and a dotted patch remain visible. Close/extreme sleeve and
arm poses, all weapon families, actual vehicle driving and the full interaction
set remain unaccepted. Pause/iDroid still use the native large screen; a paused
stereo world behind that screen is unfinished. The prior physical builds C367CC6F
and 70779290 both failed. This candidate has not been tested in the physical
headset and is not a polished release.
