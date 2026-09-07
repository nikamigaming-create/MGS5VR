# Touch controls for the tracked VR experiment

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
| After centering the stick | Point toward the displayed card: up is up, left is left |
| Right B, while keeping left trigger held | Return to category choice |
| Click, on an item with a Use prompt | Use the selected item |

The first flick selects a category and is consumed. Center the stick once, then
browse its cards in their displayed directions. B lets you choose another
category without releasing left trigger. Native equip/stow transitions can
delay opening; let the picker appear before browsing.

The cards and descriptions use real game data and unfold above the **left wrist**,
facing you; the small status display stays flat along the forearm. Raise that
wrist into view to read the selection. Right-stick turning is consumed while
the picker is open and until the stick returns to neutral after closing. A held
fire trigger must be released before it can fire after selection.

For an item card with a **Use** prompt, keep the category open, select the card
with the right stick and click the **right stick**. Phantom Cigar use was exercised
in SIM; **B** ended its time passage. This does not certify the other item actions.

**Optical zoom is unavailable in this VR test.** Left trigger + Y is reserved and
does nothing. It cannot force a 2D scope or binocular screen. The checkpoint's
AM MRS-4 has iron sights; no magnification is invented for it. Future optical work
must identify the equipped optic and retain a stereo view. Selecting a grenade,
bionic arm or item does not establish tracked throwing, CQC or item-use support.

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
| Native call/radio | Left trigger + X |
| Toggle native VR / large game screen | Left grip + left-stick click |
| Open iDroid map | Tap left Menu |
| Pause | Hold left Menu for at least 0.55 seconds |
| Recenter the large screen | Both grips + right-stick click |

iDroid and pause automatically use the large screen. **A** confirms, **B** goes
back, sticks navigate, grips act as LB/RB, and triggers retain LT/RT. On the map,
triggers zoom, right-stick click changes zoom step and Y switches MAP/NAV.
Close iDroid with **B**, or hold **Menu** again to unpause. Tracked VR returns
automatically when the same player camera resumes. Release held controls once
after returning before moving or firing. The manual VR toggle remains available;
switching VR off inside a menu also disables automatic return. Other cinematic,
loading and player-replacement transitions still need acceptance.

## Vehicle mapping under simulator development

Native vehicle state changes the controls to **right trigger: accelerator**,
**left trigger: brake/reverse**, **left stick: steering**, and **Y: enter/exit**.
**Left grip** retains the native mounted attack/call action. **Right grip** becomes
the equipment modifier while seated, freeing both triggers for driving. Release
held controls after entry/exit or regained focus before resuming input. These
mappings have contract tests; actual vehicle entry/driving/exit remains unproven.
They are not a claim of physical steering-wheel or mounted-gun hand interaction.

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
