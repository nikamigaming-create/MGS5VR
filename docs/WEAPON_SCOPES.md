# Physical weapon scopes (development)

TPP's round-scope implementation uses the equipped weapon resource, its native
`CNP_REAR_SIGHT` and `CNP_FRONT_SIGHT` connection points, and the completed skin
publication. It does not attach a binocular model to the rifle or use the
headset pose as the scope camera.

The objective renders an independent narrow-angle scene. The ordinary left and
right eye poses and fields of view are unchanged. Only an eye behind the ocular
can see that scene through the measured glass aperture. Lowering the rifle
closes the aperture and does not display a mirrored image through its front.

`[gameplay].zoom` defaults to left-stick click while the weapon is ready.
It cycles the equipped sight's native powers; a fixed-power sight remains fixed.
One click advances exactly one power, including when the configured native-safe
press pulse spans several XR frames. Holding the click does not keep cycling.
`[settings].scope_eye_relief_cm` controls the comfortable eye-to-glass distance
(default 10 cm, range 3–20 cm) without moving the rifle or its sight.

## Native binding

On supported TPP 1.0.15.4, weapon initialization copies its 0x90-byte gun
parameters to the selected 0x610-byte weapon state at +0x1b0. The optical
three-power/UI record is at +0x208. Its accepted resource handle is at +0x240.
The native connection-point reader is weapon component +0x60, vtable +0x148;
the supported target is RVA 0xdbdc50. Initialization at RVA 0x1060d5f uses it
for the same weapon's muzzle and named sight points.

The reader, object type and resource are checked before use. The rear/front
poses are composed with the native wrist attachment and final solved wrist.
Their resulting LOCAL-space publication is carried with the same head/skin
sample as the submitted eyes. Missing, replaced or unknown sights remain closed.

The round-aperture calibrations cover native sight definitions 6, 7 and 9–20.
They are numerical measurements of the owned FMDL glass and FCNP connection
points, matched by connection-point separation and native optical powers.
No retail model, texture, UI file or save is distributed. Holographic/reflex,
flip-to-side, night-vision and launcher sight presentations are not covered by
these round-aperture calibrations.

## Current verification

Math checks cover socket direction, rigid motion, glass placement, unsupported
sights, separate eye visibility, zoom sequencing and fixed-power behavior.
The asset-free D3D11 fixture compiles the actual lens shaders, reads back the
image/aperture/reticle pixels, and checks native target restoration.

In the 7CD5079A SIM build, stock `EQP_WP_60204` (BAMBETOV SV, sight 17) has an
aligned right-eye 4× lens with ordinary left-eye surroundings. Two tracked shots,
partial reload (9/31 afterward), fixed-power click, coordinated head/hand motion,
and lowered-lens closure were exercised. Empty reload, impact/obstruction and
individual weapon/grade coverage are still in progress;
the calibration list is not a claim that every weapon has been exercised.
Ground Zeroes requires its own weapon/first-person adapter.

The subsequent C5F23C89 SIM run reproduced and fixed a variable-scope input bug:
the 100 ms press pulse was counted once per XR frame, sometimes completing an
entire 2×/4×/8× cycle in one click. `EQP_WP_60105` (M2000-D, sight 16) now visibly
cycles 2× → 4× → 8× → 2× one click at a time. Six tracked shots were issued at
3-second intervals, followed by the native automatic reload and a B-tap top-off.
The later "empty" still was taken after automatic reload had begun/completed;
its filename does not establish a manually initiated empty reload. Native
`EQP_WP_30305` (G44, sight 7) also has an aligned fixed 3× lens; its firing and
under-barrel behavior are not established by that static view.
