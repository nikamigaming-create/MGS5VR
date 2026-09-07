# Left-arm HUD and weapon controls: simulator review

September 6, 2026. This is a bounded development check, not full-mod or physical
headset acceptance. The complete [Touch control list](CONTROLS.md) describes the
current input path. Optical zoom is reserved: aiming stays in the native stereo
scene, using the equipped weapon's actual sights. No binocular/scope quad remains.

## Prone ground-contact follow-up

The native ground-IK collision query reproduced the hillside surface at the
existing checkpoint. Before the change, the tracked prone hands went beneath
that surface. The [guarded native adapter](GROUND_CONTACT.md) now supplies wrist
and elbow contacts during the same skin/eye publication. Both final composited
eyes showed the hands above ground; rifle support stayed attached. Crouch and
90/180/270/360-degree controller turns were inspected separately, including the
return to the neutral grip. The ground-free crouch pose reported zero contacts.

Local DLL SHA256:
`D9A4F63A771673AF4C64B71054E72E6D49FEB633E77F9F70E94C592DD906F30A`.
All five local suites passed, including nine rotated-slope arm regressions.
Temporary native collision/input probes were removed from the candidate.

Private `artifacts/ground-contact-15s/simulator.mp4` is 14.785 seconds and
1,418,871 bytes. All 51 distinct left-eye frames were reviewed. Measured capture
cadence is 3.386 images/second, with maximum response gap 344 ms, no black frame,
and no capture/action error. No raw frames were archived. The clip shows prone
hands, wrist inspection, rifle readiness, trigger/reload inputs and stowing.
The attempted final stance tap did not visibly establish a completed transition;
it is not counted as a pass. This silent clip does not establish physical fit,
impact accuracy, smooth display cadence, every stance/terrain, or complete arm
polish. Close garment edges and hand presentation still require work.

## Context-action forearm follow-up

The flat UI filter also removed useful context-action icons. The exact native
build draws those icons in artificial-camera layer 52; destination letters and
distances occupy layer 50. The action layer now uses a separate area beside the
left forearm's weapon/status display, with the same captured rig/eye transaction
and front-facing/tracking guards. Destination markers and reticles remain hidden.

Local DLL SHA256:
`F3A1D9E0336938360C05EE5789A053F48433CD0F61D54EB851ECC7B4EA5989AA`.
All five local suites passed. Private isolation probes and machine-specific
probe paths were removed before this build.

Both-eye SIM inspection covered the native horse/Y icon beside the ammo display,
close wrist movement, and hiding both displays when the forearm turned away.
The icon disappeared during mounting and returned after dismounting; native
travel flags independently changed on foot -> horse -> on foot. A short ride
also completed. This establishes the mount-context path, not every loot or CQC
target and not physical headset acceptance.

Private `artifacts/context-actions-15s/simulator.mp4` is 14.846 seconds and
3,214,127 bytes. All 43 distinct decoded left-eye frames were reviewed; capture
cadence was 2.817 images/second, maximum response gap 422 ms. The silent H.264
clip has no black frames, capture error or action errors. Native horse fading,
reins and close garment behavior remain visible development issues. Other
context layers, subtitles, damage feedback and ground contact remain open.

## Shoulder and sleeve placement follow-up

DLL `5090EA1879FAACBE74D283370484C4573E7371B55BDE2554813B1A66039D3E9B`
addresses the large open sleeve edges in the latest supplied standing screenshot.
A private native-animation comparison reproduced those edges with the arm solve
bypassed. The accepted candidate places the shoulder line 18 cm below and 16 cm
behind the head anchor, and moves spine/clavicle weights through the same rigid
transform. It also preserves the native corrective joints' animated translations.
Temporary diagnostic modes are absent from the final candidate.

The owned body's group hierarchy now keeps the arm subtree and its ancestors,
and excludes other body/garment branches from the normal pass. It uses the existing
normal-only helpers, retaining shadow membership. In process 39256, the changed
body, garment and head flags were 11 in VR and restored to 15 in theatre. Their
shadow membership remained present; another native group changed with weapon
stow, so these snapshots do not assert whole-list identity across that transition.
The standing person silhouette remained visible in a right-eye check. These are
bounded external observations, not coherent per-frame shadow traces.

All five local suites passed, including shared shoulder placement, native yaw and
chest-pitch removal, invalid poses, group ancestry, invalid hierarchy refusal and
normal/shadow restoration. Both-eye SIM checks covered the reported standing
pose, forearm reading, close wrist reach, automatic rifle/pistol support and return
from theatre. Sampled right-controller turns at 90/180/270/360 degrees changed the
actual pose and returned without accumulated arm knots. Rifle and pistol firing
logged authored barrel alignment of 1; target impacts were not established.

The private `artifacts/arm-placement-15s/simulator.mp4`, starting at 23:44:51 UTC,
is 14.722 seconds and 736,726 bytes, silent H.264 of the actual right eye. All 51
decoded frames were reviewed chronologically; each is distinct and none is fully
black. Measured capture cadence was 3.389 images/second, maximum response gap
329 ms, with no capture/action/encoder errors and no raw frame sequence. It shows
forearm reading, automatic support, rifle fire/reload, stow, pistol selection and
support recovery, a full wrist turn and head lean. The large sleeve spikes did
not recur in this sequence. This capture rate cannot establish headset smoothness.

This is a bounded standing-arm improvement. The full rig gate remains open:
extreme inverted rifle poses can intersect the forearm; prone controllers below
the terrain hide the hands while the wrist UI remains visible. Raising the hands
above ground restores visible arms, but close-up sleeve clipping remains. Hand/
weapon ground collision, all poses/outfits and physical anatomical fit still need
work. The user's existing checkpoint was retained. No executable, clock or save
patch supplied the daylight; the native Phantom Cigar item was used.

## Player shadow preservation follow-up

DLL `02B38141BC4E444D2A73FFCF2ECBFFCFE376C7B3FC1ACEDD8D3EF0178DDC9137`
keeps the first-person-hidden body and head in the native shadow draw list. The
previous body helper removed both normal and shadow membership, and the head's
global mask excluded every pass. The replacement uses the exact native normal-only
hide/show functions, identified independently through the SHADOW_ONLY draw-mode
caller. It leaves the global mask and native-disabled shadows alone.

All five local suites passed, including named-group shadow preservation, native
state changes, appearance replacement, ownership refusal and mismatched signatures.
Read-only SIM observations in process 15052 confirmed unchanged shadow lists across
VR activation, mounted/dismounted states and theatre restoration. The body/head
normal flags changed from 15 to 11; disabling VR restored their original flags and
normal membership. The observations are external snapshots, not coherent frame traces.

Both composited eyes showed head, torso and leg shadows in daylight while those
meshes remained hidden from the first-person view. Raising a tracked arm changed
its shadow; crouch and the subsequent confirmed standing state retained the body
shadow. D-Horse remained visible. Native Phantom Cigar item use supplied daylight;
no game clock, executable or save was patched.

The private `artifacts/shadow-followup-15s/simulator.mp4` is a 14.811-second,
1,382,117-byte silent H.264 recording of the actual right eye. All 44 decoded
frames were reviewed chronologically; all are distinct and none is fully black.
Capture cadence was 2.903 images/second, maximum response gap 704 ms, with no
capture/action/encoder error or raw frame files. The clip shows the standing
silhouette, each arm raised, bounded head lean, rifle ready/stow and a sideways
step. The preceding tightly framed crouch take was replaced, not retained.

This passes the bounded missing-body/head-shadow check. Sleeve and loose garment
deformation remain visible, and the existing zeroed hip-mount matrix still removes
its stowed weapon from all passes. Complete equipment shadows, all outfits,
shadow-camera/culling accuracy and physical stereo acceptance remain open.

## Lighting and listener check

DLL `6A1EC3044DFAC1EE473C5ED9C2EEE62BD31D54F3B4548CE543BF997F90E49AA4`
adds a guarded native audio-listener update to the centered-projection build below.
All five local suites pass. The actual primary and virtual setter outputs were
checked in SIM process 29748: neutral head, 60-degree yaw with 12/4/8 cm lean,
45-degree pitch, horse-to-foot transition and rifle/pistol operation. A bounded
sample reached 21,434 accepted updates for each listener with zero failures.
Submitted and native-consumed poses match within each recorded publication.
Toggling to theatre restored the original primary and virtual camera poses;
reactivation used the new activation's center-head pose. Raw camera storage and
read-only observers remain unchanged. This is listener data-flow proof, not a
headset output, HRTF, occlusion or perceived-localization pass.

The latest private `artifacts/lighting-followup-15s/simulator.mp4` starts at
20:43:57 UTC and is 14.803 seconds, 687,646 bytes, silent H.264 at 688 x 720.
All 58 decoded frames were reviewed chronologically; each is distinct and none
is fully black. Measured capture cadence is 3.850 images/second, maximum response
gap 282 ms, with no capture/action/encoder error. No raw frame sequence was saved.
It shows sky yaw, left forearm reading, automatic rifle support, fire/reload,
rifle-to-pistol selection and final head lean. Both-eye live checks supplement the
single-eye clip. The sky rectangle is absent in these captures; garment/body
intrusion and full physical/shadow/culling/temporal acceptance remain open.

## Forearm and support follow-up

Local DLL `102CEF7F7F0F3CCF665D05C8252DA83CFB7CDD484582780CB9324EFE30621C2E` passed all five suites.
The arm solver now retains an authored elbow hinge and recomputes the native
corrective channels, including partial forearm twist. The HUD's long edge follows
the left forearm and its normal follows the distal forearm helper. Automatic
support uses tracked hand distance, keeps its intent through selection, and
releases for deliberate wrist inspection.

SIM observations include both-eye wrist reading, automatic rifle-to-pistol
support, inspection release, pistol fire/reload, and sampled right-wrist rotations
at 0/90/180/270/360 degrees. The new compact recording is 14.883 seconds, 580,622
bytes, silent H.264, 688 x 720. Its 60 decoded frames were reviewed in chronological
sheets, with enlarged inspection of the selection and pistol poses. All frames
are distinct; none is fully black. Measured capture cadence is 3.967 images per
second, maximum response gap 282 ms. No raw frame sequence was written. The clip
shows rifle fire/reload, automatic support across selection, forearm reading and
wrist/head movement. Two rifle shots logged authored barrel alignment of 1.

Support and HUD placement improved, but full rig acceptance remains open:
body/garment geometry still intrudes beside the left arm in the pistol support
pose. The sky-lighting rectangle is visible in this older recording; see the
subsequent projection check below.
The clip is not a physical comfort, smoothness, lighting or audio pass.

D-Horse mount, walk, gallop and dismount were exercised through native input;
native horse/gallop flags corroborated those actions. Two ordinary rifle shots
were also observed while mounted. Riding comfort, hiding and all mounted weapons
remain unproven. Native travel-state routing now preserves vehicle accelerator
and brake triggers, with neutral-input gates across mode/focus transitions. Its
input contracts pass; **no vehicle has yet been entered or driven in SIM**.
A sprint toward the village ended in native player death and Mission Failed;
checkpoint recovery worked. The cause of that death was not established.

## Centered projection follow-up

DLL `D28A741FEA73152A3E77F7BA5819C4D2463370007CE8241E7B2D781194E216D7`
passed all five local suites. The native eye render now encloses the runtime's
requested FOV in a centered frustum. That exact rendered FOV travels beside its
pixels through the existing command-list/mailbox join and is submitted to OpenXR.
This preserves the ray geometry; it does not add zoom. Enlarging coverage at the
same texture resolution costs some pixel density. OpenXR explicitly supports
mapping a submitted FOV different from the located view to the display
([projection-view contract](https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrCompositionLayerProjectionView.html)).

In SIM process 17444, the screen-fixed sky rectangle disappeared in both eyes.
Volumetric Clouds off did not remove it in the preceding build; the centered
candidate was checked with clouds off and then restored to On. Reopening graphics
settings confirmed On. Sampled head yaw +40/-50 degrees, pitch 25/30/45/65 degrees,
and a 12 cm sideways lean retained continuous sky coverage. Both-eye left forearm
reading, automatic rifle support, two ordinary shots (barrel alignment 1), reload,
and horse-to-foot transition still worked. At the bounded diagnostic sample there
were 3,912 native eye pairs, zero capture rejections and no scene failure.

The 15-second forearm recording above is from the preceding DLL, not this build.
This removes the reproduced rectangle in the checked states; it does not certify
all shadows, distant-object culling, temporal effects or physical headset optics.
The native source/culling projection and shared temporal resources are unchanged.

## Earlier HUD build and scope

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
