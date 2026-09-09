# Remaining gameplay access and physical melee

The public September 8 package remains the headset-tested baseline. A development
candidate adds stereo viewing zoom, motion strikes and left-hand wheel control.
Native strike contacts have been observed against a truck; this is not yet an
enemy reaction or health-delta check. Wrist Commands now supports native call
selection; later powered arm abilities and native binocular marking still need work.

## What is available now

| System | Current control and limit |
| --- | --- |
| Nearby D-Horse | Release right grip, approach the horse, press left Y at the native mount prompt. Left stick rides, left X increases speed, Y dismounts after lowering the weapon. These basics have SIM evidence. |
| Wrist Commands | Hold left trigger and tap X. After the commands appear, center the right stick, point toward an available command, and confirm with right trigger or right-stick click. Release left trigger to close. Native D-Horse call/Stay back and distraction knock were exercised in SIM. Contextual interrogation and other buddies still need target interactions. |
| Equipment and NVG | Left trigger opens wrist selection; first right-stick flick chooses a category. NVG is the upward item slot; selecting None turns it off. NVG is separate from binoculars. |
| iDroid/map | Tap left Menu. Native full menus appear on the large in-headset screen; triggers zoom the map and grips switch tabs. Close with B to return to tracked VR. |
| Ordinary CQC | Right trigger with right grip released reaches native attack/CQC. The game chooses the contextual action. This is a button route, not physical fist hit detection. |
| Physical fists | Candidate: motion alone submits native kick contacts from palms or the held firearm tip. No trigger pulse or native kick animation. Target damage/reaction acceptance remains open. |
| Binoculars/scopes | Candidate: left trigger + Y opens stereo 2× viewing; right-stick click switches 2×/4×, B exits. This does not invoke native marking, target analysis or intel calls. Those and per-weapon scopes remain unfinished. |
| Marked-person display | Saved marks remain native. Blue streaks/escaping triangles were visible in prior eye captures and reported again by the user. The faulty draw has not been isolated; no current shader-fix claim. |
| Vehicles | Candidate: left grip acquires the native wheel contact, controller rotation steers, release lets go. Truck entry, both steering directions and exit have SIM evidence. Sustained driving/braking, physical haptic feel and personal-gun fire from the driver seat remain unverified or unimplemented. |
| Other actions | Loot, carry, Fulton, interrogation, powered arm abilities and all weapon families need individual end-to-end checks; selection alone does not prove the action. |

## Physical melee contract

Basic fists require no equipment selection or squeeze. A deliberate motion
stroke supplies attack intent and closes the free fingers. The left bionic
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

The native Call Menu now unfolds at the left wrist, with a dedicated input owner.
Equipment selection cannot consume its right stick or confirmation. Walking
remains available. The candidate exercises horse calling, Stay back and the
distraction knock. Other buddy orders, powered-arm commands and contextual
interrogation still need complete target interactions.

Add an explicit Binoculars action with a visible close control and discrete zoom
steps. Preserve binocular stereo and head tracking while magnified. Marking/intel
must follow the binocular viewing direction and use native target data. Keep
NVG, map zoom, binocular magnification and an equipped weapon's authored scope as
different states. An iron-sight weapon must not silently become a scoped weapon.
Do not re-enable the old mono optical-screen prototype as a completed VR optic.

The next priority is usable reconnaissance: repair marked-person rendering,
connect stereo viewing to native marking/intel, and extend wrist Commands coverage.
Enemy reaction/damage for motion strikes, vehicle movement/braking and
physical wheel feedback remain open. The requested
drive-and-shoot mode would keep left-hand wheel steering, move the pedals to the
left stick and give right grip/trigger to the carried gun. Native driver controls
do not expose that weapon path; remapping triggers alone does not implement it.
Retain the accepted 1440p/native-AA profile for
headset play. SIM runtime overrides are process-local and use the existing save.

## Reconnaissance and important missing abilities

Native marking tracks enemies and objects held in view. The intended VR flow is
to open binocular viewing, center a visible target and let the native acquisition
rules mark it. A short confirmation and readable left-wrist target/intel details
should accompany the native mark. Viewing direction comes from head orientation;
this does not require eye tracking or a permanent screen reticle. Preserve native
visibility, range, target eligibility and campaign unlocks. The current 2×/4×
candidate changes the image projection only; it is not this complete flow.

Treat model highlighting separately from mark acquisition and saved knowledge.
The owned binary exposes model-marker effect controls separately from marker
management, but a live isolation check is still needed to associate them with the
reported blue streaks. Compare the same already-marked actor with the effect
enabled/disabled, then inspect its geometry, camera and depth inputs per eye.
Correct the responsible draw, or replace that visual with a stable cue attached
to the native marked actor. Do not erase saved marks or restore flat destination
letters as a substitute. Cover moving and occluded actors, near/far distances,
head turns and a newly acquired mark in both eyes.

| Important system | Remaining access or verification |
| --- | --- |
| Buddy and arm Commands | Dedicated wrist input/display implemented. D-Horse calling, Stay back and distraction knock exercised; other buddies, abilities and interrogation remain to be exercised. |
| Powered arms | Native stun, sonar and rocket capabilities depend on developed/equipped gear. Ordinary motion punches must not activate them. Their activation and any guided-camera behavior need individual VR integration. |
| Stealth and recovery | Hold-up, grab, interrogation, carry/set down, weapon pickup and Fulton need complete target interactions. A native button route is not an end-to-end pass. |
| Special equipment | Placement/detonation, decoys, cardboard-box actions and item-specific alternate controls need coverage beyond merely selecting the item. NVG on/off already has bounded SIM evidence. |
| Support and progression | iDroid is accessible in the headset, but helicopter requests, support drops/strikes, Mother Base development and deployment transitions are not fully tested. |
| Transport | Vehicles, Walker Gear and mounted weapons need their own state/control coverage. Driving and shooting a carried gun remains a custom feature, not an available binding. |
| UAVs and remote weapons | FOB UAVs are defence equipment, not a general scout drone piloted from Snake's wrist. Rocket Arm has guided Missile Cam behavior; its controls and camera transition need VR integration after the arm is unlocked. Walker Gear/D-Walker is a separate piloted platform. None of these later systems has passed this checkpoint's VR tests. |

For the intended VR controls, ordinary punches stay motion-driven. Deliberately
equip a powered arm to use its special attack; do not bind a normal punch to a
remote launch. Keep available buddy orders in wrist Commands and support/development
in iDroid. A guided projectile needs its own stereo camera and explicit return
to Snake before it can be called supported.

Native references: Konami's [FOB UAV update](https://www.konami.com/mg/mgs5/tpp/jp/news/update201605.php),
[Rocket Arm / Missile Cam demonstration](https://www.konami.com/games/eu/fr/topics/308/),
and [Walker Gear controls](https://mgstpp-app.konamionline.com/manual/pc/na/en/pc_06.html).

### September 9 reconnaissance check

The SIM exposed a cross-hand input problem: pressing left X produced both native
X and B, immediately cancelling a proposed Commands mode. Reading face actions
through their owning hand removed that extra B. The simple-controller profile
retains its intentional left-hand Cancel binding. This correction is in the
development candidate; physical controller-profile coverage remains open.

The original checkpoint was prone, where the native Call menu was unavailable.
After changing stance, the native menu appeared. Its choices and descriptions
now render on a larger left-wrist panel. Selection retains the chosen direction
through the native confirmation pulse, then consumes a held stick so it cannot
turn the camera after the menu disappears. The native horse arrived when called;
Stay back and the distraction knock were also exercised.

Temporarily holding native binocular input beneath the stereo camera entered and
exited binocular state, but head-directed acquisition and usable per-eye viewing
were not accepted. That experiment was also rolled back. The candidate's original
2x/4x viewing zoom remains separate from native marking. The model-marker switch
probe did not establish a fix for the reported blue streaks; it was restored and
no marker-effect patch is enabled. These features are a development candidate,
separate from the public headset-tested baseline. A downloaded 100% campaign
loaded in SIM, but its FOB onboarding interrupted gadget testing. The original
checkpoint was restored and used for the current showcase. Save files are
private test data and are not distributed.

Native references: [marking and action icons](https://mgstpp-app.konamionline.com/manual/pc/na/en/pc_02.html),
[prosthetic arm capabilities](https://www.konami.com/games/eu/en/topics/13601/),
and [iDroid support and progression](https://mgstpp-app.konamionline.com/manual/pc/na/en/pc_08.html).

Native reference: Konami's [Call Menu](https://eu-support.konami.com/hc/en-gb/articles/9667024720151-Metal-Gear-Solid-V-Call-Menu)
describes buddy orders and prosthetic-arm abilities; its [tactics manual](https://mgstpp-app.konamionline.com/manual/pc/na/en/pc_12.html)
describes CQC, interrogation, pickup and Fulton. Source bindings and SIM evidence
determine which of those functions are actually accessible through this mod.
