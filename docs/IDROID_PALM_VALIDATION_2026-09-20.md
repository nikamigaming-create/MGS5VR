# iDroid palm alignment, 20 September 2026

Code slice: `8169263`.

The anatomical palm's +Y points toward the wrist. The display now uses
right = -Z, up = -Y (toward the fingers), and outward normal = -X. Its
center and orientation follow the rendered palm from the native skin
publication. Native iDroid finger contact is preserved instead of applying
the free-hand finger pose through the handset.

Paused native menus retain the last visible palm attachment rather than
moving the display to a fresh raw controller pose beside the frozen hand.
This preserves attachment to the paused skin; it does not animate a paused
native skin. Menu samples also clear an optic from an older input publication.

## Validation

The Release DLL and test executable built successfully. The working build
passed 7,185 contract checks. The new checks cover display axes, rigid palm
motion, pointer direction, and the paused-skin attachment. This workspace
also contains earlier uncommitted work; the installed DLL is not represented
by this one commit alone.

The final close-up is
`artifacts/showcase-20260920/20-idroid-palm-protected/idroid-palm-showcase.mp4`.
It shows palm translation and rotation, Mother Base/Map tab switching, map
zoom, close, and reopen. Both composited eyes were saved at those checkpoints.
The video is a continuous native right-eye capture, cropped to the runtime
display FOV and resampled to 840x880. It is silent. Source metadata reports
598 captured frames over 20.072 seconds, with 5 dropped capture frames.
Temporary native damage protection isolated the UI check from enemy fire;
it was cleared afterward and the input runner was stopped.

Installed DLL SHA-256 for this close-up:
`775930395E2798ABC9677180F48E0384DEE261BCBC1BDDC1AF29759E72A41C27`.

## Tactical video

`artifacts/showcase-20260920/19-tactical-showcase/tactical-showcase.mp4`
contains 64.533 seconds of continuous native right-eye footage with aligned
game process audio. Native interrogation delivered `_OnMapUpdate`; guard
1147's marker changed from `0x13` to `0x53`. The take then shows the iDroid
map and palm movement, binocular zoom, clearing that tag (`0x1`), and placing
a terrain waypoint. The attempted re-mark of guard 1147 was behind cover
and did not acquire him; the trigger placed a terrain waypoint instead.
This is not a successful enemy re-mark claim.

The tactical take used a native setup warp before recording, ordinary guard
inputs with damage enabled, then damage protection only for map/optic
inspection. Its source DLL was
`5FFD63438B2B9033B337D2A634FB8ABAA128360C67CBA10E02E2E1719AEFA907`:
it includes the palm orientation/contact correction, but predates the paused
attachment fix. Capture metadata and paired compositor checkpoints remain
beside the original footage. No still images were substituted for gameplay.

These simulator captures do not establish physical headset ergonomics,
every weapon/scope, every menu/submenu, or complete gameplay acceptance.
The separate attempted CQC knockout take and the first close-up interrupted
by enemy fire remain diagnostic recordings, not successful showcases.
