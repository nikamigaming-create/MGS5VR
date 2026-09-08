# Remaining gameplay access and physical melee

The September 7 physical run of EB89CE69 received positive general gameplay
feedback. It does not establish acceptance of every interaction. The next
requested work is direct fist/bionic-arm melee, buddy commands, binoculars and
zoom. These are implementation gaps; no additional grip mode unlocks them in the
installed build.

## What is available now

| System | Current control and limit |
| --- | --- |
| Nearby D-Horse | Release right grip, approach the horse, press left Y at the native mount prompt. Left stick rides, left X increases speed, Y dismounts after lowering the weapon. These basics have SIM evidence. |
| Call/radio | Left trigger + X sends native Call/LB. A quick radio call has an input route, but the held buddy-command menu is incomplete: wrist selection consumes its right-stick navigation and click. Do not describe horse summoning or commands as verified. |
| Equipment and NVG | Left trigger opens wrist selection; first right-stick flick chooses a category. NVG is the upward item slot; selecting None turns it off. NVG is separate from binoculars. |
| iDroid/map | Tap left Menu. Native full menus appear on the large in-headset screen; triggers zoom the map and grips switch tabs. Close with B to return to tracked VR. |
| Ordinary CQC | Right trigger with right grip released reaches native attack/CQC. The game chooses the contextual action. This is a button route, not physical fist hit detection. |
| Physical fists | Finger articulation and tracked fists exist. Hand motion does not yet generate an accepted melee strike or contact damage. |
| Binoculars/scopes | Optical input is explicitly disabled in tracked VR. Left trigger + Y is reserved. No grip chord enables stereo magnification. |
| Vehicles and other actions | Vehicle input routing exists, but driving is unverified. Loot, carry, Fulton, interrogation, powered arm abilities and all weapon families need individual end-to-end checks; selection alone does not prove the action. |

## Physical melee contract

Basic fists should require no equipment selection. Squeezing a free controller
closes the fist; a deliberate punch supplies the attack intent. The left bionic
fist remains available while another item is selected. The right fist is available
when that hand is free. A left punch must release weapon-support ownership cleanly
and must never be interpreted as firearm discharge.

Movement detection must subtract locomotion/head translation and reject tracking
jumps, wrist-menu gestures, reload motion, support acquisition and ordinary slow
hand movement. Rearming requires a new deliberate stroke; a resting fist or one
continuous swing must not produce repeated hits.

Strike intent, native target acceptance and actual contact are separate gates.
Use the native close-combat rules for enemy eligibility, obstruction, damage/stun
and reactions. The accepted hit must agree with the visible punching hand. Merely
emitting the existing attack trigger is insufficient: that input can also fire,
grab or throw in another native state. Do not mark physical punching complete
until a final-eye capture shows the hand strike and the intended enemy reaction.

Powered bionic-arm abilities remain deliberate selections and retain native
unlock/charge behavior. A normal punch must not launch a rocket arm or discharge
an unrelated special ability. The native campaign exposes a CQC knife-kill
action; the mod has no free-hand knife combat system. A physical knife would need
its own owned presentation, deliberate draw/stow and contact validation.

## Commands and optics

Add a visible Commands page to the left-wrist interface. Route the right stick
and confirm/back actions to exactly one active page, so equipment selection
cannot consume a buddy order. Preserve walking. Exercise the real Call Menu's
horse call, available buddy orders, arm commands and contextual interrogation,
including opening, selection, cancellation and returning to equipment.

Add an explicit Binoculars action with a visible close control and discrete zoom
steps. Preserve binocular stereo and head tracking while magnified. Marking/intel
must follow the binocular viewing direction and use native target data. Keep
NVG, map zoom, binocular magnification and an equipped weapon's authored scope as
different states. An iron-sight weapon must not silently become a scoped weapon.
Do not re-enable the old mono optical-screen prototype as a completed VR optic.

Implementation order: ordinary physical punches and their native target/contact
gate; wrist Commands and horse calling; binocular marking and stereo zoom; then
remaining powered arm, loot/carry/Fulton and vehicle interactions. Retain the
accepted 1440p/native-AA profile throughout interaction work. Keep the installed
candidate unchanged until each replacement has bounded SIM evidence.

Native reference: Konami's [Call Menu](https://eu-support.konami.com/hc/en-gb/articles/9667024720151-Metal-Gear-Solid-V-Call-Menu)
describes buddy orders and prosthetic-arm abilities; its [tactics manual](https://mgstpp-app.konamionline.com/manual/pc/na/en/pc_12.html)
describes CQC, interrogation, pickup and Fulton. Source bindings and SIM evidence
determine which of those functions are actually accessible through this mod.
