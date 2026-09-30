# Weapon fit coverage

The release objective covers every player weapon and physical optic. A common
rig calculation or a pass on the current loadout does not certify that arsenal.

The owned equipment worklist currently contains 418 firearm definitions and
83 model entries. It includes variants and definitions whose player eligibility
still needs checking. Retail definitions and extracted models remain private.

| Worklist family | Definitions |
| --- | ---: |
| Handguns | 72 |
| Submachine guns | 41 |
| Shotguns | 63 |
| Assault rifles | 86 |
| Sniper rifles | 64 |
| Machine guns | 29 |
| Missile launchers | 30 |
| Grenade launchers | 33 |

For each eligible weapon, record its exact equipment/model identity and check
one-hand tilt, firing-hand contact, support-hand contact, physical sight and
muzzle direction, actual impacts, reload, tracking loss and both-eye motion.
Variants may share a model, but a shared model is not automatically a gameplay
pass. Turret travel and mounted sights require separate mounted-state cases.

The current local default adds -30 degrees of firearm-only pointing pitch.
The common native firing wrist owns that correction; no detached sight or
floating reticle is introduced. Two-hand aim continues to follow the support
grip. The adjustable setting and older-INI migration are documented in
VR_FIT_SETTINGS.md.

The retained stationary one-hand run
`artifacts/bot/runs/20260930T210559180122Z` covers the current FAKEL (SLEEP),
AM MRS-71 and URAGAN-5 AIR-S on DLL `df3d571a56ea76fb`. Both scoped guns publish
the requested -30 degree optic axis. The sidearm has no scope and is not
reported as a missing optic. These sequential final-eye stills do not prove
impacts, continuous stereo refresh, reloads, two-hand contact or physical
headset ergonomics. The per-definition queue is
`private/weapon-fit-audit.json`; unrun entries retain `not_run`.

The final local-DLL run `artifacts/bot/runs/20260930T213627283000Z` on
`4173f9187821` repeats those three inspections and verifies support acquisition
on both long guns. Both final eyes show the attached support hand. Acquisition
is a scoped mechanism pass; the reported FAKEL grip feel, reload contact and
weapon-transition coverage remain open. All other definitions keep their
existing unrun status.

Use `tools/workspace.py bot` with effective personal bindings for bounded
inspections, retain reviewed runs with `tools/workspace.py pin`, and record the
DLL and both INI hashes with any accepted result. Keep the fixed `play/` tree;
do not create a local distribution folder for each weapon.
