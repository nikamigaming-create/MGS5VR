# Native body-gear findings

October 18 investigation. These are bounded findings, not complete equipment
or ACC acceptance. Retail assets and disassembly remain in private evidence.

The local catalog contains 780 equipment definitions, including ammunition,
projectiles, NPC/context variants and grades. It is not 780 wearable objects.
The Item category has 62 definitions referring to 31 distinct model paths;
Throwing has 33 definitions/9 paths and Placed has 31 definitions/7 paths.
Loadout positions, item counts, native resources and model instances must remain
separate identities. In particular the three customized firearm IDs in the
observed save were 871/873/875; their installed parts also matter.

The named hip bone is parented to the pelvis and the current full-body rig
already propagates its completed pelvis transform to descendants. The old
unconditional scale suppression was therefore hiding this properly placed
mount as well as the arms-only fallback. The development change retains the
native mount when the full body is admitted. A retained field comparison shows
the stock returning in both eyes. This does not establish every outfit, stance,
weapon, ACC transition or physical-headset clearance.

The registered native iDroid resource has a finite lifetime. The current
implementation has a separate, typed lease for the UI-only handheld path and
explicitly avoids duplicating the native player-owned handset. UI closure
hides/releases that lease. Persistent body-mounted iDroid needs a new explicit
stowed-presentation lifetime joined to the same native ownership rules; simply
changing its held transform cannot keep it on the body after closing. Preserve
the existing authored left-hand contact and projector as one chain.

Native slot APIs exist in the running player table: SetCurrentSlot,
GetEquipTypeIdBySlot, IsNonActiveBySlot, GetAmmoStockBySlot, ChangeEquip,
UnsetEquip and SetVisibilityToEquip. Availability is not an argument contract.
Retail scripts use SetCurrentSlot for ITEM/subIndex 0. Runtime enum reads
confirmed SECONDARY=2, SUPPORT=3 and ITEM=4. Names PRIMARY, PRIMARY_HIP and
PRIMARY_BACK returned nil; do not invent those constants from inventory names.

Earlier retained NVG work establishes ChangeEquip with the already-owned NVG
and toActive=true for that item only. SetVisibilityToEquip ignored ITEM in that
test. The normal NVG mesh is intentionally suppressed near the eyes while the
native effect remains active; body-mounted goggles must preserve that active
presentation distinction.

Fresh static inspection of the retained UnsetEquip wrapper found slotType,
subIndex, dropPrevEquip and dropOnGround arguments, with true-valued temporary
initialization before the boolean reads. It is not a proven stow operation and
must not be called as generic release-to-mount. Several callback decodes stop
partway through; no complete contract is claimed from them. The item-index
setter reached its return. Runtime selection acknowledgement, inventory
preservation, customization identity and transition behavior remain to prove.

The field camera is mod-authored and already has a body-owned mount. It is the
first integration of the new contact candidate logic: one cue on contact,
one-second grace with bounded spatial reach, fresh squeeze, and return to the
current mount on release. Native weapons and iDroid are not implicitly covered
by that integration. Live camera pickup must be evaluated using its published
candidate mask; timing against an old mount pose is insufficient.

Acceptance remains the user's full loop: select the real loadout, inspect and
handle it in the ACC, depart, use it in the field, return and handle it again.
Native cabin weapon inspection must not introduce cabin firing.
