# Left-arm HUD and weapon controls: simulator review

September 6, 2026. This is a bounded development check, not full-mod or physical
headset acceptance. The complete [Touch control list](CONTROLS.md) describes the
current input path. Optical zoom is reserved: aiming stays in the native stereo
scene, using the equipped weapon's actual sights. No binocular/scope quad remains.

## Build and scope

Tested local DLL SHA256:
`70779290C3A4872A458E73C03111B79B012C69BDEE7222CD4AF66B84C1460C15`.
Game: the profile's TPP 1.0.15.4 executable, SHA256
`085c2f82d1c963c40b3d2d55786661dfee2b18cbbf388a710c00fa76c5e9bb45`.
Meta XR Simulator v205, simulated Quest 3, per-process runtime selection; the
system Oculus runtime selection was preserved. Native input and all capture used
OpenXR semantic APIs. No Windows input or desktop capture was used.

The existing Continue/Resume checkpoint supplied the outfit, AM MRS-4, WU pistol,
bionic arm, grenades and item categories. Only ordinary rifle/pistol firing was
exercised. All five local test suites passed; GitHub CI runs four non-GPU suites.

## Observed behavior

| Check | Result in the observed states |
| --- | --- |
| Left forearm status | Actual native weapon/ammo UI follows the solved arm in both eye checks; turning its back toward the eye hides mirrored text |
| Main view | Flat reticle, destination letters/distances and flat red cues suppressed; native scene-camera people cues remain |
| Rifle and pistol | Right grip readies, right trigger fires, release stows; held weapon returns after selection |
| Hand support | Left grip attaches the native support pose; release returns the hand to tracking; right hand remains the aim source |
| Reload / bolt cycle | B reloads; native manipulation state temporarily owns left-hand contact, then releases it |
| Hip holster | Guarded suppression of the named player hip mount removes the stowed rifle stock in the tested pistol and prone views |
| Equipment | Left trigger plus stick category changes native selection; cards appear around the forearm; categories include primary, secondary, support and items |
| Head and movement | Independent head lean/yaw with explicitly retained local controller poses; forward movement, native turn and prone/stance changes exercised |
| Menus | Manual screen toggle, iDroid open, map trigger zoom, grip tab switch, B close, pause hold and return to native VR exercised |
| Reserved optics | Left trigger + Y left gameplay in stereo and did not open native binoculars or a scope |

The source pose now carries the wrist panel through the same native scene/UI job
as each eye. The UI worker uses its captured eye view even when the shared camera
has already been restored. The rig restores native animation inputs after skin
publication, uses anatomical palm frames, stabilizes shoulders, and transports
forearm roll and garment descendants with the solved limbs.

Large above-head sleeve spikes from the supplied headset image were absent in
the reviewed sequence. Angular cuff/garment shapes remain visible, especially
around stow and selection. This does not establish polished skinning in every
pose. A transient free-hand transition in the wider live checks also needs
higher-cadence scrutiny. Do not mark the complete arm gate passed.

## Requested short recording

Private local artifact: `artifacts/sim-polish-15s/simulator.mp4`, accompanied by
capture/action metadata and a small review sheet. Started at 17:22:00 UTC.

- 14.973-second H.264 video, 688 x 720, silent, 561,573 bytes.
- 56 captured and decoded frames, all distinct; 3.676 captured images/second.
- Strictly increasing decoded timestamps; largest interval 0.304 seconds.
- No fully black decoded frame. Every decoded frame was visually reviewed.
- PNGs streamed directly to the encoder; no raw frame sequence was written.
- Actions: support, rifle fire/reload, arm ammo read, stow, pistol selection,
  pistol fire/bolt cycle, independent head lean/yaw.

Rifle status changes from 31/172 to 31/170 after firing and reloading; pistol
status changes from 8/12 to 7/12. The three actual shots in the capture interval
logged barrel/direction dot 1. These checks do not certify target impacts or
near-wall obstruction. The capture cadence is separate from game/runtime cadence
and cannot demonstrate smooth physical tracking. No interpolation or generated
frames were used.

The recorder's `--mp4` mode caps duration at 30 seconds and media at approximately
12 MiB, rejects existing output directories, and stores request/response bounds.
The requested run used 15 seconds. One known physical-test rollback is retained
locally; private media and game data are excluded from the public source.

## Still open

- Physical controller fit, sleeve/cuff polish, full reach and rapid-motion coverage.
- Live loss/recovery of each controller and runtime focus; synthetic contracts
  pass, but a live controller-loss gate was unavailable in this simulator API.
- Every weapon/item, tracked throws, CQC, scoped modes and mounted weapons.
- Damage feedback, subtitles and contextual prompts: flat gameplay UI is
  suppressed, and these do not yet have a spatial replacement.
- Interactive wrist iDroid, pointers and automatic menu/cinematic transitions.
- Full stereo/culling/temporal acceptance and reliable native runtime shutdown.

The new HUD/control path is ready for a bounded experimental headset comparison;
neither this report nor the CI badge claims that the conversion is 100% complete.
