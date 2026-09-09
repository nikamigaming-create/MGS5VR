# CQC with tracked hands

The target is simple: strike with a hand or weapon, grip a person to control
them, then release, throw or choose a contextual action. Native game rules own
eligibility, damage, knockouts, alerts and progression. This document describes
the intended interaction; it does not claim that physical grabbing is installed.

| Intent | Intended VR action | Current state |
| --- | --- | --- |
| Punch | Deliberate forward or downward hand strike. No mode button. | Candidate submits native kick contacts; enemy reactions still need work. |
| Bionic fist | Punch with the left hand while any ordinary equipment is selected. | Uses the same motion contact path; no powered ability is activated. |
| Weapon bash | Strike with the held weapon. | Candidate uses its native tip contact; firearm trigger stays separate. |
| Grab a standing enemy | Reach to the upper body and squeeze grip; keep holding to restrain. | Physical acquisition is not implemented. Native right-trigger CQC remains available. |
| Interrogate | While restraining or holding someone up, open wrist Commands and choose an available question. | Commands input and wrist display implemented; interrogation needs a target interaction. |
| Throw | While holding a valid grab, deliberate push and release. | Planned. Must enter a native throw, not teleport a ragdoll. |
| Knock out a restrained enemy | Deliberate free-hand strike or explicit wrist action. | Planned. Ordinary grip movement must not injure the person. |
| Lethal knife finish | Explicit labeled choice while restraining. | Native contextual knife action only; no free-hand knife system. |
| Move a body | Keep the existing native pickup/carry/set-down controls. | User reports these work in the headset; preserve this route. |
| Hand-drag a body | Grip near a body, pull while holding, release to let go. | Planned native ragdoll constraint integration. A pickup-button gesture is not physical dragging. |
| Shoulder carry | Lift a held body toward the shoulder to transition into native carrying; lower deliberately to set down. | Planned. The existing native carry remains the fallback. |
| Powered arm ability | Deliberately select the unlocked/equipped ability and use its native activation/charge action. | Completed campaign imported for testing; individual abilities remain unverified. |

## Ownership rules

A grab needs a nearby eligible target and an actual native acquisition result.
Squeezing empty air cannot pull in a distant person. While grabbed, the native
restraint or carry owns the body, hand contacts and contextual controls. Ordinary
repositioning must not become a punch, gunshot or throw. Releasing grip ends a
hand grab; shoulder carry is an explicit separate state.

Tracking loss cancels gesture intent and leaves the native actor in a valid
release/carry state. It must never fling a body or invent a high-speed strike.
Menus, reloads, weapon support and grabbing each have one input owner. Movement
with the left stick remains available where the native action allows it.

There is no need to render legs to obtain the native kick's target rules for a
hand strike. That does not turn scenery into destructible objects or make every
enemy state eligible. Preserve native obstructions and target reactions.

Native reference: [Konami's CQC, hold-up, interrogation and extraction guide](https://mgstpp-app.konamionline.com/manual/pc/na/en/pc_12.html).
