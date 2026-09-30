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

In handheld mode, the raw controller holds `CNP_CONNECTOR` with normal upright
controller axes. The existing arm solver places the native wrist through that
contact and keeps the authored cupped fingers. Its final wrist and native CNP
produce `HeadCameraSample.idroidDevice`. Native-gamepad presentation keeps the
authored native wrist. The hologram inherits `CNP_HOLOGRAM` from that same
published device, with the existing size, depth and fit settings. It never
substitutes a newer raw grip or an anatomical palm for the rendered mount.

The 16:9 source canvas, stereo projection, UI clipping and menu input routing
remain unchanged. Missing native attachment data rejects the handheld mount.
The overlay does not reuse a stowed device pose. Automated checks exercise the
packed native identity, connector offset, projection centre, shared motion,
aim-ray mapping and invalid/stowed-device rejection.

Current final-eye and reopening results are recorded in `docs/CURRENT.md` and
`artifacts/dev/idroid-alignment-acceptance.json`. Ordinary closing/reopening is
separate from forced tutorial stow; physical headset and tutorial acceptance
must not be inferred from a simulator or unit result.
