# Touch controls for the tracked VR experiment

[Quick controller infographic](images/controls.png) · [All control modes](images/control-modes.svg) · [Install and play](../README.md)

These bindings target the game's **Action Type** controller layout. The left-arm
HUD shows the native weapon name, ammunition and status. Native context-action
icons appear beside that display when the game offers an action; the horse/Y
mount prompt has been checked through mount and dismount. There is no flat aiming
reticle or destination-letter overlay in gameplay. Native 3D people cues remain.

## Weapons and hands

| Action | Control |
| --- | --- |
| Draw/ready the selected weapon | Hold **right grip** |
| Aim | Point the weapon with your **right hand**; use its actual sights |
| Fire | **Right trigger**, while holding right grip |
| Lower the gun while keeping it in hand | Keep right grip held and lower your hand |
| Put the weapon away | Release right grip; the game performs its native stow animation |
| Add the support hand | Bring the left palm to the weapon's support grip and hold it there briefly |
| Free the left hand | Pull the hand away, or turn the wrist HUD toward your eyes |
| Reload | **Right B**; the native reload moves the support hand, then releases it |
| Read the HUD | Raise and turn your **left forearm** toward your eyes |

The right hand owns the weapon grip. With support engaged, the line from the
right controller to the left guides the authored barrel direction. The left
hand retains the native support contact; reload animations temporarily own it.
Reloads use a button rather than manual magazine grabbing.

Free fingers articulate on both hands: grip curls the lower fingers and thumb,
and trigger curls the index finger. Supported controller touch sensors also pose
the index and thumb. These are controller-driven poses, not individual finger
tracking. Hands in contact with a weapon retain its native grip animation.
Grip and trigger keep their gameplay actions while also animating the fingers.

Support requires the left palm to stay within 10 cm of the weapon's support grip
for 150 ms, then blends into contact. Pulling more than 20 cm away releases it.
Moving the left hand beside or behind the firing hand also releases support;
guidance stays within 60 degrees of the right controller's pointing direction.
This prevents the weapon from following a withdrawn hand and keeping it latched.
Clenching left grip only animates the free fingers; it cannot force two-handed
mode. Lowering the weapon or inspecting the wrist clears contact, so selection
does not leave a sticky support latch. The corrective headset run received
positive feedback; complete reach and pose coverage remains open.

## Equipment selection

Hold **left trigger** to open the left-wrist picker. The **left stick still
moves you**, so you can select while walking. Use the **right stick** to browse,
then **release left trigger** to confirm and close. Gameplay does not pause.

| Right stick with left trigger held | Action |
| --- | --- |
| First flick up / down / right / left | Primary / secondary / support / items |
| After the cards appear and the stick is centered | Flick toward a card, including diagonals; center between choices |
| Right B, while keeping left trigger held | Return to category choice |
| Right A, on an item with a Use prompt | Use the selected item once; right-stick click also works |

Holding left trigger alone does not equip or toggle anything. The first flick
opens one category. Once its cards appear, center the stick and flick toward a
card. Up is up, left is left, and diagonal cards accept diagonal flicks. Keep a
flick steady briefly; center between choices. Holding or wobbling one flick
cannot skip into a second selection. B returns to category choice while left
trigger stays held. Native equip/stow transitions can delay opening; navigation
is blocked until the expanded menu has rendered.

The cards and descriptions use real game data and unfold above the **left wrist**,
facing you; the small status display stays flat along the forearm. Raise that
wrist into view to read the selection. Right-stick turning is consumed while
the picker is open and until the stick returns to neutral after closing. A held
fire trigger must be released before it can fire after selection.

For an item card with a **Use** prompt, keep the category open, select the card
with the right stick and press **A** (or click the right stick). A held Use button
produces one press, and cannot become crouch when the picker closes. Phantom Cigar use was exercised
in SIM; **B** ended its time passage. This does not certify the other item actions.

On the current checkpoint, **NVG is the upward item slot**. Hold left trigger,
flick left for Items, wait for the cards and center, then flick up to NVG and
release left trigger. To turn it off, select **None** in the item picker and
release. This is night vision, separate from optical binocular zoom. Its native
upward slot has a small NVG label rather than a large rectangular card.

For a grenade, choose **Support** with a first flick right and finish selection.
Hold **right grip** to ready it. Move and tilt the **right hand** to position and
direct the trajectory; **right trigger** throws. Hand elevation controls the arc,
while right-stick left/right still turns your body. Right-stick up/down is
suppressed while a tracked grenade is readied. Throw strength remains the native
equipment strength; this is point-and-trigger throwing, not a velocity gesture.
The latest SIM observations and remaining stance limits are in the
[interaction and visibility review](INTERACTION_VISIBILITY_REVIEW.md).

**Stereo zoom (development candidate):** hold **left trigger + tap Y** for 2×.
**Click the right stick** for 4×/2×; **B** closes it. The world remains stereo with
live head tracking. Walking and horizontal turning remain available. This is a
separate viewing mode, not an added scope on an iron-sight gun. Native binocular
marking/intel and authored scope integration remain unfinished.

**Motion melee (development candidate):** swing either free hand, or bash with a
readied firearm. No button arms a punch. Deliberate movement closes the free
fingers and submits contact at the visible palm or authored weapon tip using the
native kick attack entry. Slow movement, tracking jumps, wrist selection, optics,
reloads and seated movement cannot generate punches. One accepted target ends a
stroke; pull back before another strike. Native eligibility and damage rules
apply; not every object becomes destructible. Enemy reaction/damage testing is
still required; a collision log alone does not prove damage.

Basic left-arm melee needs no special arm selected. Powered prosthetic abilities
retain campaign unlocks and equipment selection. After selecting an available
ability, use **right grip to ready + right trigger to activate**, following its
native charge/release behavior. Ordinary punches cannot select or discharge an
ability. Later powered arms are not verified on the 1% checkpoint.

## Movement, actions and menus

| Action | Control |
| --- | --- |
| Move, including during wrist selection | Left stick |
| Turn / native camera look | Right stick; currently smooth native turning |
| Sprint | Left-stick click without left grip |
| Crouch / change stance | Right A; hold A for prone; release weapon-ready grip first |
| Quick dive | Left X without the equipment modifier |
| Context action / pickup | Left Y without the equipment modifier; follow the native action when available |
| Native attack / CQC / carried-body throw | Right trigger with right grip released; the native game state chooses the action |
| Wrist Commands / buddy orders | Hold left trigger, tap X; point the right stick toward an available command and press right trigger or right-stick click; release left trigger to close |
| Toggle native VR / large game screen | Left grip + left-stick click |
| Open iDroid map | Tap left Menu |
| Pause | Hold left Menu for at least 0.55 seconds |
| Main menu / options | Hold left Menu on foot; choose Return to Title Menu or Options with the left stick and A |
| Cutscene skip | Hold left Menu; choose Skip and press A when the native game offers it |
| Recenter the large screen | Both grips + right-stick click |

In the development build, opening iDroid or Pause during tracked gameplay puts
the native menu on a floating panel in the stereo world. Head movement keeps
working during Pause. The iDroid handset remains attached to the right hand while
iDroid is open; closing iDroid stows it. Equipment and Commands remain on the left wrist.

**A** confirms, **B** goes back, the left stick navigates, grips act as **LB/RB**,
and triggers retain **LT/RT**. On the map, triggers zoom, right-stick click changes
zoom step and **Y** switches MAP/NAV. **B** closes iDroid; hold **Menu** again to
unpause. Holding Menu *inside iDroid* opens its native Help, so close iDroid before
opening Pause. Release held controls once after returning before moving or firing.

For the main menu, open Pause, select **RETURN TO TITLE MENU**, press **A**, then
answer the native confirmation. **OPTIONS** and **CONTROLS & MANUAL** are also in
Pause. The title screen uses the large in-headset screen and native controls.
Cutscene Skip is available only when the native game offers it. Loading,
cutscenes and player-replacement transitions still need broader VR integration;
the floating gameplay menus do not establish support for every transition.

Wrist Commands has its own input mode, separate from equipment selection.
Keep left trigger held, center the right stick after the commands appear, then
point toward a command and confirm. Left-stick walking remains available.
Right trigger confirms once and cannot fire the gun. B cancels; release left
trigger and center the stick before returning to gameplay. The native game
chooses which commands are available; stand or crouch to call D-Horse. Calling
D-Horse, Stay back and the arm's distraction knock were exercised in SIM.
The motion-melee candidate now
reaches native contacts; enemy reaction and damage remain separate acceptance
steps. See [system access and the physical-melee contract](SYSTEMS_ACCESS.md).

## Vehicle mapping under simulator development

Native vehicle state changes the controls to **right trigger: accelerator**,
**left trigger: brake/reverse**, **left stick: steering**, and **Y: enter/exit**.
In the development candidate, bring the **left hand to the wheel and squeeze
left grip**. Turn the controller clockwise/right or counterclockwise/left;
release grip to let go. The native driver's hand keeps its authored contact.
The left stick remains available when the wheel is released. **Left X** invokes
the native vehicle weapon/call action; grabbing the wheel cannot fire it.
**Right grip** is the equipment modifier while seated. Release
held controls after entry/exit or regained focus before resuming input. Truck
entry, left-hand wheel acquisition, both steering directions and exit have SIM
evidence. The short recording does not establish sustained driving or braking.
Wheel acquisition gives a short haptic pulse. Native game rumble also reaches the
controllers; actual feel needs a headset check. This mapping does not implement
a personal gun through a driver's window. Vehicle weapons retain their native
availability. Mounted-gun hand aiming remains unfinished.

The requested next behavior is left-hand steering with the carried rifle or
pistol aimed and fired by the right hand. It needs a native driver weapon path,
plus left-stick forward/back for the pedals while gripping the wheel so the
right trigger can fire. This is a design target, not a current control mode.

## Test scope

D-Horse mounting, walking, galloping and dismounting have now been exercised in
the simulator. **Y** mounts/dismounts, **left stick** rides, and **X** increases
speed. Release right grip and let the native stow animation finish before dismounting. The native horse and gallop flags changed with those actions and cleared
after dismount. Bounded ordinary rifle shots were also observed while mounted. Hiding, all mounted
weapons, vehicles and physical riding comfort remain separate acceptance gates.
Driving is not ready for headset testing.

Rifle and WU pistol ready/fire/reload, support attach/release, equipment cards,
left-arm ammo updates and basic iDroid/pause navigation have simulator observations.
Physical controller fit still needs a headset check. Throws, CQC, mounted weapons,
all inventory items, contextual targets, every menu branch and every weapon are
not certified. The lowered-weapon trigger now reaches the native action path;
its input contract passes, but a target interaction has not been accepted.
The native context-action icon layer now follows the left forearm; pickup, carry
and other target-specific prompts still need individual checks. Flat damage and
subtitles currently have no spatial replacement. Sleeve/cuff polish remains open. This guide describes the controls
without claiming those gates pass; see the [current SIM review](SIM_HUD_REVIEW.md).
