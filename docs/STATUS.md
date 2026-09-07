# Status: experimental native VR mod

Updated 2026-09-06. The mod source, native adapters, tests and build tooling are
open under the MIT license. The complete requested VR conversion remains unfinished.

The latest simulator slice implements real weapon/status UI on the **left
forearm**, removes the flat gameplay reticle and destination labels, stabilizes
the visible rig, and adds native equipment selection. Zoom is reserved so aiming
stays in stereo. Hidden body/head meshes now retain their native shadow casting;
the latest 14.811-second, 1.38 MB SIM clip shows the full person silhouette and
tracked arm movement in 44 reviewed frames at 2.903 captures/second.
The preceding clip covers centered sky projection and native audio-listener
tracking. Both clips are silent and do not establish perceived localization,
smoothness or full-mod acceptance.
Read the [current review](SIM_HUD_REVIEW.md) and [complete controls](CONTROLS.md).

The latest arm follow-up adds guarded native ground contacts. Both eyes now show
the hands above the slope in the reproduced prone failure. Its 14.785-second
SIM clip is 1.42 MB, with 51 reviewed frames at 3.386 captures/second. Other
terrain, close garment/hand presentation and physical fit remain open.

iDroid and pause now open on the native menu screen and return automatically to
tracked VR. Map zoom, tab switching, both-eye return and the manual override were
checked in SIM. The 14.879-second, 1.20 MB menu clip has 54 reviewed frames and no
black captures. This does not establish all cinematic/loading transitions.

An initial **physical Quest 3 combat run** is now recorded and reviewed. The user
reported good overall playability and requested better arms and a less intrusive
HUD. Movement, rifle fire/reload and manual theatre/native transitions are visible;
the rig and HUD still fail complete acceptance. See [headset review and next
slice](HEADSET_REVIEW.md) for timestamps, evidence limits and the implementation order.

| Area | Implemented and observed | Still required |
| --- | --- | --- |
| Rendering | Native D3D11 scene drawn twice inside one game render transaction; atomic two-eye OpenXR projection submission; centered render coverage removes the reproduced sky rectangle in SIM | Shadow/culling/temporal and physical stereo acceptance |
| Head motion | Native player head bone anchors the camera; simulator translations and rotations modify native eye matrices | Verified anatomical eye offset, world scale and all-state acceptance |
| First person | Player-owned head/body hidden from normal rendering while preserving their native shadow; anatomical palm binding, shoulder anchors, forearm roll and native reload support; named hip mount hidden in VR | Cuff/garment deformation, stowed-equipment shadows, physical alignment, full reach/motion/stance acceptance |
| HUD and markers | Native weapon/ammo UI on left forearm; flat reticle/destination labels suppressed; captured eye projection applied to native scene-camera cues | Spatial damage/subtitle/context feedback, interactive wrist iDroid, full motion acceptance |
| Weapons | AM MRS-4 and WU pistol ready/fire/reload and equipment categories exercised; ordinary shots use the authored muzzle | Impacts/obstruction, scoped and alternate modes, all-weapon and physical verification |
| Movement | Native walking, strafe, turn and stance inputs reach gameplay | Correct first-person behavior across every stance and locomotion state |
| Travel | D-Horse mount/walk/gallop/dismount and bounded mounted rifle shots in SIM; native travel input routing with neutral transition gates | Actual vehicle entry/driving/exit, mounted roles and physical riding comfort |
| Audio | Native primary and virtual listeners consume the source center-head pose; head rotation/lean and theatre restoration observed in SIM | Physical localization, output/HRTF and occlusion checks |
| Effects | Native graphics UI saved DOF Disable, motion blur Off and post-processing Off; camera shake Off persisted on reopening settings | Verify remaining cinematic effects and isolate any shared per-eye temporal resources |
| Save | Continue/Resume loads the existing checkpoint and equipped rifle | Automatic startup state adapter |
| Menus | iDroid/pause switch to the native screen; same-player return restores VR, with held-input release protection | Every menu branch, interactive wrist iDroid and player-replacement transitions |
| Cinematics | Default large-screen theatre preview and native game controls | Automatic cinematic classification and complete skip coverage |

## Build and native runtime evidence

All five local suites passed: core contracts including tracking suspension/recovery,
D3D11 checks across two hardware devices, system DirectInput proxy parity, synthetic
camera setter/getter calls and player visibility ownership/replacement checks, and
process-exit cleanup. These checks do not certify the full mod.
GitHub CI builds every target and runs four suites; the hardware D3D11 suite is
explicitly excluded on hosted runners. Game/headset tests require a separate local run.

Current simulator build SHA256:
`C367CC6FABCCA6254353D8B5F1571A9630A08ADC841F6CFE1BC8CC3CA60C6EC1`.
It adds automatic native menu return to the arm, shadow and projection/listener updates in the
[SIM review](SIM_HUD_REVIEW.md). The initial physical run used
`4B299FF8E47EB5EE7934F1FA3E57424913F5DE4B2102752114428A2896A8FB07`,
with the later physical rollback retained as
`70779290C3A4872A458E73C03111B79B012C69BDEE7222CD4AF66B84C1460C15`.
The initial controller/muzzle probe and first recording used
`B22157DBCF1F00060057E5D3581314476962BA03ADD9C378183604F94AF43830`;
later builds added right-grip readiness and bounded shot diagnostics to the
actual firing caller. See [controller rig](CONTROLLER_RIG.md).
The tested game is TPP 1.0.15.4, x64/D3D11, executable SHA256
`085c2f82d1c963c40b3d2d55786661dfee2b18cbbf388a710c00fa76c5e9bb45`.

The earlier 8CAAB92A baseline produced over 78,000 native scene pairs with zero reported scene
capture failures/rejections. A verified duplicate desktop-present append is removed
for the second eye while preserving the native target setup. A read-only 400-sample
check found no duplicate entries in that vector. Measured native cadence was 57.75
pairs/second over one eight-second interval without recording, and approximately
50.7 pairs/second over an 88-second simulator PNG recording interval. These are
bounded measurements, not a minimum frame-rate guarantee.

Both images and their camera metadata share one native source/tracking/activation
family. Copies are associated with their actual D3D11 command lists and published
only after both lists execute. The consumer submits those source eye poses/FOVs.
There is no alternate-eye rendering, generated eye, depth reconstruction or mono
fallback substituted for an active stereo frame.

Two head-camera cancellations were recorded in that earlier run: one stale-tracking event
and one deliberate toggle off. This prevents claiming uninterrupted acceptance for
all earlier footage. Native camera history is provisionally set to the current eye;
other temporal resources are not yet isolated.

The current camera keeps its activation and origin during tracking stalls, suspends
eye submission, and resumes on fresh matching tracking. It no longer selects a
third-person theatre quad because of a tracking timeout. Player visibility resolves
through the camera's player appearance collection and verifies its ownership backlink;
it does not hide other actors by scanning the global model list.

An earlier test initially suffered black dropouts while the simulator's synthetic
room helper occupied about 7 GB of dedicated GPU memory plus shared memory. Stopping
that helper reduced total dedicated usage from roughly 11.7 GB to 5 GB. The following
59-second recording contained 205 distinct captured frames and no fully black captures.
This sampled observation does not establish a minimum frame rate or rule out shorter
dropouts. No head-camera cancellation occurred in that session through this check.

The simulator is Meta XR Simulator v205 with simulated Quest 3. Game UI and gameplay
were driven through OpenXR actions, with all test launches windowed at 1280x720.
The system runtime registry selection is preserved. Elliott's simulator passed a
bounded headless OpenXR probe; its game-rendering path remains unverified. A physical
runtime initially reported no available headset. On September 6, the Quest 3
connected and the installed controller build completed an initial physical combat
run with a clean Oculus shutdown. Full physical headset acceptance is outstanding.

## Earlier recordings and failed acceptance

Private local artifacts retain actual composited left-eye PNGs, action timestamps,
capture metadata and variable-rate MP4s. They are development evidence, not a finished
mod demo, and are excluded from the public source.

- `controller-aim-left`: 29.25 seconds, 101 distinct captured frames, 3.45 captures/second.
  Controller translations, yaw/pitch/roll, firing and reload with the head fixed.
  The rifle/hands move in the native scene. No fully black frame was captured.
  Sleeve intrusion, unheld weapon animation and face HUD remain defects. A separate
  projectile-spawn probe confirmed the modified native origin/direction.
- `controller-grip-aim-left`: 35.234 seconds, 121 distinct captured frames, 3.43 captures/second.
  Uses the earlier 4B299FF8 build and right-grip weapon-ready mapping with the legacy
  left trigger released. Controller movement, shots, reload and independent head
  lean/yaw completed. Native firing diagnostics matched the rendered barrel; no
  fully black captures occurred. Mobile encoding is a silent 35-second 720x754
  H.264 Baseline MP4 with measured variable frame timing.
- `fps-head-visibility-movement-left`: 59 seconds, 205 captured frames, 3.47 captures/second.
  Head translation/rotation, walking, stance changes, firing (29 to 27 rounds), reload
  (31/169), and walking with the weapon lowered completed. The hair/face obstruction
  is removed. Shoulder/sleeve geometry still intrudes during some downward views,
  and HUD/marker distortion is unresolved. This remains failed full-mod acceptance.
- `native-stereo-six-dof-left`: 90.05 seconds, 441 captured frames, 4.90 captures/second.
  The rotation/fire/reload actions happened after this recording ended; it only
  contains the recorded translation portion of that attempted sequence.
- `native-stereo-complete-sequence-left`: 60.14 seconds, 281 captured frames,
  4.67 captures/second. The scripted sequence completed; complete visual acceptance
  and correlation with the tracking cancellation remain outstanding.
- `native-stereo-movement-effects-off-left`: 75.20 seconds, 238 captured frames,
  3.16 captures/second. Movement reached the game, but lowering the gun visibly
  returned to third person and the sequence ended prone. This fails persistent
  first-person acceptance. HUD warping and body clipping remain visible defects.

Recording cadence is separate from native game cadence. None of these recordings
is a 60 FPS video. The earlier controller recordings show the tracked rifle, but
none of these older clips contains the functioning forearm HUD shown in the
new [SIM review](SIM_HUD_REVIEW.md). The required full-mod demonstration and
acceptance remain incomplete. The user subsequently chose to test the
experimental build on a Quest 3; that initial run is documented in
[HEADSET_REVIEW.md](HEADSET_REVIEW.md). No zero-bug or all-weapons claim is made.

See [acceptance](ACCEPTANCE.md) for the remaining observable gates and
[architecture](ARCHITECTURE.md) for the rendering contracts. Private logs and raw
analysis stay outside public source and distributions.
