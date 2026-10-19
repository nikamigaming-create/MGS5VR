# Body-mounted gear: math and implementation contract

October 18, 2026. Design and source inspection; not an implemented or accepted
feature. User selected a one-second grace window after contact vibration and
return to the body mount on release. Existing physical optics, weapon handling,
left-hand iDroid and left-forearm status remain requirements.

## Source evidence and remaining native research

- `tools/scenario-state.lua` reads three named weapon slots, eight support slots
  and eight item slots: **3 + 8 + 8 = 19 inventory positions**. Empty slots and
  unavailable reads are distinct. This is not a count of renderable meshes.
  That file explicitly reports the selected-weapon-slot getter as unverified.
- `src/controller_rig.cpp` suppresses named bone 53, `SKL_300_ASRROOT`, hash
  `ec70c442`, parent 0. The earlier stock-clipping observation explains this
  safeguard; it does not establish the correct presentation for every loadout.
- The same file publishes final anatomical palms and body frames and joins
  studio props after the skin solve. This is the existing pattern to extend.
- `include/mgs5vr/idroid_native_hold.hpp` supplies the authored left-wrist
  handset transform. Its translation magnitude is 0.0699957183 m and its
  quaternion norm is 1.00000000014. Preserve it; a new belt mount must not
  replace the established held-contact or projector transform.
- `src/xr_runtime.cpp` mixes native and interaction haptics using maximum
  amplitude, clamped to 0.75, with 60 ms pulses. Contact acquisition needs a
  one-shot event in this mixer, not a vibration every frame for one second.

Still required for EACH item: actual loadout identity and revision; native
render instance/resource; skeleton and authored mount; grip/contact frame;
native equip/stow request and readback; duplicate-visibility ownership; behavior
in field, cabin and forced tutorials. A runtime resource handle alone is not
an equipment definition. Missing evidence must remain unavailable, not be
replaced by a generic gun, pouch or guessed position. Support/item slots may
represent stacks or effects rather than separate physical models.

## Coordinate math

Use `T_A_B` for the pose mapping coordinates in B into A. `compose(A,B)` is
matrix product A*B; its translation is pA + rotate(qA,pB). Core tracking poses
are right-handed metres, +X right, +Y up, -Z forward. Native skeleton axes need
the existing native-to-tracking conversion; do not relabel native +Z as -Z.

For body bone B, mount M, item root I, palm P, and item grip G:

```
T_world_item_mounted = T_world_bone * T_bone_mount * T_mount_item
T_world_item_held    = T_world_palm * T_palm_grip * inverse(T_item_grip)
T_world_projector    = T_world_item * T_item_projector
```

The grip equation satisfies `T_world_item_held * T_item_grip =
T_world_palm * T_palm_grip`, so the authored contact, not the mesh origin,
lands in the hand. If free inspection preserves initial contact instead:

```
T_palm_item_at_pickup = inverse(T_world_palm_at_pickup) * T_world_item_at_pickup
T_world_item_held    = T_world_palm_current * T_palm_item_at_pickup
```

Use the established authored hold for functional guns, binoculars and iDroid;
free-contact inspection must not silently replace those functional grips.
On release use the CURRENT mounted transform, never the pickup-time world
position. This makes the item return correctly after walking or turning.

Existing conversion in `controller_rig.cpp`:

```cpp
const auto trackingFromNative = compose(frame.headPose,
    compose(Pose{{0,1,0,0},{}}, inverse(frame.nativePose)));
const auto trackingFromPalm = compose(trackingFromNative, renderedPalms[side]);
```

All mount, hand, item and projector render poses must share the completed skin
transaction. Contact decisions can consume a fresh prior immutable publication;
rendering must rejoin the selected item to the current completed publication.
Carry player/activation, reference epoch, rig sequence and loadout revision.
Never use an old body pose with a new raw controller pose to render gear.

## Contact and the one-second window

Define an item-local contact volume from owned geometry/contact evidence. For
an oriented box with half extents b, transform palm/contact point p to box
space, then compute distance to its surface/volume:

```
p_local = inverse(T_world_contact) * p_world
q       = max(abs(p_local) - b, 0)       // componentwise
d       = sqrt(dot(q,q))
```

For a spherical hand contact radius r, contact occurs when d <= r. Separate
entry and exit margins provide hysteresis; their actual sizes need measured
hand/gear fit. Example arithmetic, NOT production dimensions: b=(.04,.08,.02),
p=(.07,0,0) gives d=.03 m; p=(.07,.12,.02) gives d=.05 m.

On a fresh contact episode, pulse once and latch exact item identity to that
hand. While touching, it remains eligible; after contact exit, preserve the
candidate until 1000 ms after the most recent contact sample. This gives a full
second after drifting away without repeatedly buzzing during sustained touch.
The grace is logical availability; do not fade the physical gear off Snake.

Proposed logic, not existing API:

```cpp
if (!sameOwnerEpochLoadout || !freshTracking || inputBlocked) clearCandidate();
else if (contact) {
    if (newContactEpisode) emitContactPulse(hand);
    latchExactItem(itemKey);
    lastContactMs = nowMs;
} else if (nowMs < lastContactMs || nowMs - lastContactMs >= 1000) {
    clearCandidate();
}
if (candidate && freshGripPress && withinGraceReach && handAvailable)
    requestNativeAcquire(candidate); // await exact native acknowledgement
```

A modest spatial grace bound is also required so a one-second latch cannot
pull an item across the body or through another item. Derive its limit from
measured reach/contact trials; no centimetre value has been accepted yet.
If two items overlap, choose nearest valid contact deterministically, then lock
that candidate until exit/expiry; do not switch silently during the grace.
Only one hand may acquire an item. A consumed grip requires release before
rearming; arriving at a mount with grip already held must not draw another gun.

Use monotonic time, not a frame counter. Eligibility is [0,1000) ms. At 72,
90 and 120 Hz this spans 72, 90 and 120 nominal frames respectively, with at
most one sampling interval of event observation delay (13.889/11.111/8.333 ms).
Arithmetic spot checks passed for box distance and 999/1000/1001 ms boundaries;
these are not runtime tests or physical haptic acceptance.

## Input and native lifecycle

The retained effective binding snapshot from `20261018T224746514907Z` has left
grip for support and iDroid focus, B+left-grip for reload, Y+left-grip for
binocular equip, and left-grip chords for iDroid tabs. Left trigger opens
equipment. Re-read current effective bindings before implementation/testing.
Do not hardcode a new raw-grip consumer after mapping native actions.

Order ownership before action evaluation: blocking UI/tutorial/context;
already-held item interaction; eligible body-grab candidate; ordinary gameplay.
Existing device-focus, support and deliberate configured chords must remain
usable. A successful grab consumes only its selected binding inputs, with
release gating through native acknowledgement. Idle proximity never consumes
buttons or changes the active item. Add remappable body-grab actions and audit
their effective conflicts rather than rewriting personal settings.

State flow: Mounted -> Candidate -> AcquirePending -> Held -> ReturnPending
-> Mounted. Expired candidates return to Mounted without native action.
AcquirePending must confirm the exact item before exposing functional held
geometry. A failed or timed-out acquire restores the mounted presentation.
Release returns presentation to the current mount while the guarded native
stow completes; block re-acquisition until acknowledgement. Never fire or use
an item merely because it was grabbed. Throwables need a separate explicit
use/release policy so inspect-and-return does not break intentional throwing.
Consumed items cannot reappear on the mount. Scene/owner/loadout changes cancel
requests; invalid tracking must not issue a throw or reuse stale world poses.

## Concrete code work and checks

1. Add `body_gear.hpp/.cpp`: immutable source/identity records, contact math,
   per-hand grace and exclusive ownership state machine. Unit-check deadline
   boundaries, already-held grip, overlapping mounts, both hands, stale epochs,
   tracking loss, changed/empty loadouts, failed acquire, consumption and return.
2. Extend `HeadCameraSample` with source-bound mounted gear poses/contact
   frames. Populate after the final rig solve in `controller_rig.cpp`; reuse
   existing authored attachment/held chains and final-palm joins. Gate the
   existing hip suppression only after its restored native mount is verified.
3. Integrate arbitration in `xr_runtime.cpp` before conflicting actions are
   evaluated; mix one-shot contact haptics through its existing mixer. Use
   per-item native adapters for acknowledgement and lifecycle, not synthetic
   inventory edits or a blanket native gamepad packet.
4. Add a bounded `workspace.py bot` body-gear inspection that captures torso,
   mounts and hands without requiring the unrelated feet-clearance assertion.
   Keep the existing full-body floor test intact. The prior floor failure does
   not establish that gear inspection is impossible.
5. Record each supported loadout item's native identity and both-eye appearance;
   test touch, 0/500/999 ms grab, expiry, release after body movement, regrab,
   item swap and menu/tracking interruptions. Check sights/muzzle/hand alignment,
   binocular aim/zoom/acquire/mark, and iDroid projector/stow/immediate reopen.
6. Run `tools/build.ps1`, deploy only checked development for bounded tests,
   review both final eyes and retain reviewed evidence. Wired Meta Link haptic
   feel and physical reach remain headset checks. Published `play/` stays pinned.

The algorithm and integration points are specified. Native per-item mounting,
render ownership and equip/stow acknowledgements are still research work;
this document does not claim 100% research or implementation completion.
