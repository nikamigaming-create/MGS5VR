# Stereo viewing, motion melee and wheel candidate

Development candidate, 2026-09-08 Pacific / September 9 UTC. This is separate
from the public September 8 headset-tested package. Release DLL SHA-256:
`373154587D1F2A1C17F91188F8E1A321C81525FDD4FA4447C435240A348F230A`.

The Release build and all six CTest suites pass: contracts, D3D11 mailbox,
system DirectInput forwarding, camera observer passthrough, process exit cleanup
and native frame-rate adapter. The installed candidate matched that hash.

## Observed in the simulator

- Both final eyes were inspected at ordinary, 2× and 4× viewing magnification.
  Each scene keeps its own tracked eye origin; no mono optical quad is used.
  This is viewing magnification, not native binocular marking or a weapon scope.
- Left and right palm strokes reached the native collision query with attack
  ID 7 (`ATK_Kick`). The native filter accepted a truck target. A ready pistol's
  authored muzzle also submitted a motion strike. Enemy reaction, actual damage
  and the full target set were not established by those observations.
- The player entered and exited an unarmed truck. A fresh left squeeze near the
  authored wheel contact acquired steering. Right/left controller rotations
  produced +0.5/-0.5 native steering input; release cleared the grip.
- The dynamic native XInput rumble adapter installed. The runtime routes native
  vibration and a wheel-acquisition pulse to tracked controllers. Simulator
  output does not establish the physical feel.

A single small clip captures wheel interaction, exit and the carried pistol on
foot. It is 14.696 seconds, 1,004,772 bytes and 52 real composited left-eye frames.
Capture averaged 3.44 frames per second; that is the image capture rate, not game
performance. No raw frame sequence is retained. Four sampled frames were reviewed.
The private capture metadata records no tool or encoder error.

The truck did not translate during the recorded accelerator segment. Therefore
this clip does not establish driving distance or braking. Those need an actual
movement check, rather than counting successful input calls as gameplay proof.

## Remaining work

Physical headset stereo comfort, enemy hit reactions/damage, wheel feel,
sustained driving/braking, and later unlocked powered-arm abilities remain open.
The existing 1% checkpoint was retained. No native special-arm ability or unlock
was added to ordinary punches.

The requested left-hand steering/right-hand carried-gun firing is not implemented.
The intended mapping uses left-stick forward/back for pedals while gripping the
wheel, with right grip to ready and right trigger to fire. That requires a native
driver weapon path, weapon attachment and animation handling, not just a new
button assignment. Native mounted vehicle weapons are a separate system.

The SIM run used a process-local runtime override at 1280×720. The SIM was closed
after recording and the accepted 2560×1440 profile restored. The registered
headset runtime was not changed. The local public baseline DLL was restored for
ordinary play; this candidate remains available in the development build.
