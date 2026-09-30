# Native handheld iDroid alignment

The right-hand iDroid uses its retail device and native pixels. The left bionic
weapon/status display and ordinary HUD keep their existing owner and layout.

The old screen mount guessed a projector frame from the anatomical palm. In the
actual game a normal controller grip presented that screen edge-on. Turning the
hand to make the screen readable put the device across the projection. Those
failures were reproduced before changing the rig.

The legally owned `idr0_main0_def.fcnp` has separate named sockets, both under
`SKL_000_ROOT`:

| Socket | Position in device coordinates, metres | Rotation |
| --- | --- | --- |
| `CNP_CONNECTOR` | (0.0000056, -0.092196204, -0.000741) | Identity |
| `CNP_HOLOGRAM` | (0, 0.043378498, 0.012349601) | Identity |

A bounded read-only comparison matched the actual player's iDroid model root
to native hand CNP `0x1c68632c5c53`, anchored at right wrist bone 12, variant 4.
The maximum difference between their wrist-relative matrices was less than
0.000001. The native anchor packs its variant in the upper **16** bits of its
64-bit value; `0x0004000038b1433c` is the observed match. Other variants, weapon
sockets, left-arm anchors and unverified matrices are rejected. Retail model,
texture and FCNP files remain private; none is added to the release.

The September 30 upright-simulator fix assumed identical grip and pointing
axes. The user's physical report and a later anatomical-palm experiment
showed why that did not establish a natural hold. In handheld mode, contact
position now comes from the controller grip and contact orientation from its
same-frame pointing basis. It does not use the pointing origin or a guessed
per-headset angle. Independent iDroid-only grip translation and rotation fit
that contact. General controller calibration is applied first.

The arm solver places the native wrist through `CNP_CONNECTOR` and keeps the
authored cupped fingers. Its final wrist and verified native CNP produce
`HeadCameraSample.idroidDevice`. Native-gamepad and authored movie presentation
keep their native wrist. The hologram inherits `CNP_HOLOGRAM` from that same
published device, never a newer tracking sample.

Retail introductory cutscene images and the locally extracted s10020 d01
animation/UI package show the hologram above its projector. The old fit put
the screen centre at projector height, leaving half the display across the
hand. The lower-edge anchor now owns the configured right/up/depth shift and
pitch/yaw/roll. Its centre is one half-height above that anchor. Width remains
16:9; resizing or rotating preserves the lower-edge pivot. Extracting the
native SAND tracks is not a claim that every animation channel was decoded.

The 16:9 source canvas, stereo projection, UI clipping and menu input routing
remain unchanged. Missing native attachment data rejects the handheld mount.
The overlay does not reuse a stowed device pose. Automated checks exercise the
packed native identity, controller grip/pointing separation, contact fit,
lower-edge pivot, shared motion, aim-ray mapping and invalid/stowed rejection.

The launcher shows every spatial setting in a rotatable reference fitting
view; see VR_FIT_SETTINGS.md. Its illustrated projection volume follows the
screen corners. The actual retail light cone is a separate native effect:
default attachment and ordinary reopening are reviewed, but the configured
offset/rotation run shows that effect does not retarget to every custom screen
fit. That remains an explicit defect, not a passed projection-cone claim.

Current final-eye and reopening results are recorded in `docs/CURRENT.md` and
`artifacts/dev/idroid-alignment-acceptance.json`. Ordinary closing/reopening is
separate from forced tutorial stow; physical headset and tutorial acceptance
must not be inferred from a simulator or unit result.
