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

The exact authored screen distance and scale are still unresolved. The default
8 cm depth is an approximate, configurable fit, not a measured cutscene
distance. The matching cutscene contains animated device channels as well as
separate UI resources. Native canvas constants are 128 by 72; they are canvas
units, not metres. Neither those constants nor uninterpreted UI-layout floats
justify a new physical screen size.

The recovered device parts bind the native light to `SKL_002_Kamera` and a
lens-flare effect to `CNP_HOLOGRAM`. The flare's authored local offset is
(-0.08, -0.07, 0.02). That is an effect attachment offset, not the projection's
screen distance. Recover the UI/effect transform contract before retargeting
the visible native projection or replacing the current approximate fit.

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

## Separate native projection effect

The owned device model puts `SKL_002_Kamera` at
(0, 0.0434, -0.0019) metres under the device root. This is a different origin
from `CNP_HOLOGRAM`: their separation is approximately 14.25 mm. The camera
bone also influences 92 vertices of the physical device. Redirecting that
bone to fit the screen would deform the device, so it is not an eligible
projection writer.

A bounded offline parse of the owned Light VFX identifies graph name
`0x3db2fdbc61cd`, stored as native StrCode32 `0xfdbc61cd`. Its model primitive
uses a cone with near ring z=0.01 m, radius 0.004 m, and far ring z=2 m,
radius 0.8 m, before effect transforms. The graph supplies local translation
(0.0013, 0.0048, 0.0022), a scale vector with force 0.1, and a rotational
vector (15, -10, 15). The exact native Euler order and authored screen distance
are not yet established. Its exposed receive channels change colour and
light intensity; they do not expose a screen endpoint. The native lens flare
is a separate graph. These findings do not establish the configured screen
fit as the native cone endpoint.

The independent diagnostic adapter in `idroid_effect_probe.cpp` identifies the
Light graph by its verified native type and exact graph name, captures its
model primitive after native graph compilation finishes, and joins runtime
updates using that primitive's unchanged complete 0x70-byte property block.
Four compiled-node callbacks are checked as well as the graph type/name;
the shared cone mesh alone never identifies an iDroid. Twenty executable byte
guards cover the function ABIs, graph-name storage, property-buffer layout,
typed model resource and current particle draw records.
Any mismatch disables the optional probe without changing menu or device flow.

Compilation frees the temporary prototype before returning. The adapter copies
the graph name while that prototype is live, then validates the retained output
graph and relocated property block; it never reads the freed prototype. Bounded
factory reports include the copied graph name to distinguish an absent Light
graph from an unresolved runtime owner.

To collect native diagnostics through the existing Steam session, set
`idroid_effect_probe=1` under `[diagnostics]` in the game-local `mgs5vr.ini`
before starting the game. It uses the ordinary INI loader and requires the
controller rig. The shipped default is 0; restore the prior setting after
the diagnostic session. A directly launched game can also opt in through
`MGS5VR_IDROID_EFFECT_PROBE=1` in its own launch environment. Registration logs begin `iDroid Light probe registration`;
runtime samples begin `iDroid Light probe update`. Storage is limited to eight
registered primitives, 16 registration reports and 128 runtime reports, with
at least 500 ms between runtime reports. No native memory is read during that
report interval; unresolved runtime discovery is limited to 256 attempts per
500 ms and starts only after an exact Light primitive has registered.
With only `idroid_effect_probe=1`, the adapter observes existing native
callbacks and bounded memory reads only; it never writes a native effect,
bone, model, input, menu or save, and never invokes another native effect pass.

Each runtime report includes the native parent matrix, property and model
resource identities, particle bounds, and a fresh published iDroid device
camera/screen comparison when available. `native_graph_verified=1` identifies
the Light graph; `player_owner_verified=0` and `coherent_frame=0` deliberately
remain false. The update's attachment owner and pose generation have not been
joined to the player yet. Matrix proximity cannot substitute for that join.
No pointer recorded in a previous process is eligible for reuse.

The next native case is an ordinary open/close/reopen followed by changed screen
width, translation and rotation, collecting the registered Light update and
same-session player attachment. A transform fix must first establish that exact
attachment and the output transform's lifetime, then show both final eyes with
the cone terminating on the configured screen and native stow/reopen preserved.
This probe is diagnostic progress; the projection overshoot remains open.

### Opt-in current-draw fitting candidate

The later discovery path also handles Light graphs compiled before the probe
was installed. It reads the current native model-primitive callback and exact
typed model resource, without treating geometry counts as an effect identity.
Retained ACC run `20261002T125243253232Z`, DLL `88295529388d`, contains two
independent physical hand poses and both final eyes. The installed native trace
contains two users of the canonical cone resource; only one follows the entire
published player camera-bone frame. The distant second user is not eligible.
These observations establish a candidate attachment, not a fitted-screen pass.

The separate `[diagnostics] idroid_effect_retarget=1` switch admits an
experimental writer. It defaults off. It requires the canonical model resource,
single bone identity, complete current emitter matrix, stable native callback
properties and particle pool, and a complete active range of at most three
coincident draw records. A fresh matching
published player/activation/rig generation is required immediately before the
fit. The native render callback runs first; the adapter then fits only that
current draw's transform and its corresponding culling bounds. It preserves the
authored cone topology, fixes the near-ring centre at the native camera emitter,
and puts the far ring in the configured screen plane. It never redirects the
camera bone, changes the device model or alters ordinary HUD ownership.

Every writable transform/bounds pair is checked and snapshotted before mutation,
then the complete native source and draw range are checked again. A failed write
attempts to restore all original pairs independently. The executable contract
suite checks endpoint fitting, rotated-screen bounds, source/pool replacement,
unwritable bounds and partial-write rollback, including failures in the third
particle and a changed source generation during preflight. Those checks do not establish
native presentation or successful rollback after an inaccessible process page.

Native acceptance is still required with both diagnostic switches enabled:
ordinary Map open, independently translated/rotated hand poses, configured
screen width/offset/rotation, ordinary close and immediate reopen. Require fresh
`iDroid Light retarget` records with `current_draw_and_bounds_fitted=1`, then
review both final eyes for the actual cone endpoint and intact device/world.
Restore the prior diagnostic settings afterward. Forced tutorial stow and
physical headset acceptance remain separate cases. Do not enable this candidate
by default or describe projection retargeting as fixed before that replay.

Retained field run `20261002T140550163961Z` on candidate `0d6de5c01263`
reproduced why the original opt-in writer did nothing: native Light render
callbacks published three coincident particles, while the writer required one.
Its ordinary open, two modest hand poses, Back, immediate reopen and final Back
passed six scoped cases; all twelve outcome eyes were reviewed. The retained
`idroid-light-native.log` joins the exact canonical source, callback range and
complete emitter frame. The revised writer now admits that bounded complete
batch with one transaction and rejects partial ranges, divergent transforms
and a fourth particle. Retained `20261002T142717882392Z` on `77ade9c8950d`
passes all six ordinary lifecycle cases with twelve reviewed outcome eyes.
Its retained native log records sixteen verified three-particle fits during
an earlier Map opening in the same owned session. That proves the bounded
writer executes; it does not prove the precise visible cone endpoint. A
separate custom-fit replay was reviewed but lost its scratch pixels before
promotion, so it is excluded from retained visual acceptance. The writer
remains off by default pending exact endpoint and forced-tutorial acceptance.

Format research used the original [VfxTool source](https://github.com/youarebritish/VfxTool)
at commit `c777fa14f5549205458c1ce8bd09d1e5c6784ba0` and its referenced
[CityHash implementation](https://github.com/Atvaark/CityHash), Legacy v1.0.3.
The executable guards and offsets were recovered from the legally owned
1.0.15.4 native view; local static records and owned binary assets remain in
ignored private research storage. No game asset is shipped with the adapter.
