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
| Add the support hand | Bring the left hand near the weapon, or hold **left grip** |
| Free the left hand | Release left grip and move the hand away, or turn the wrist HUD toward your eyes |
| Reload | **Right B**; the native reload moves the support hand, then releases it |
| Read the HUD | Raise and turn your **left forearm** toward your eyes |

One-handed aim works. Two-handed use currently means the left hand follows the
weapon's native support pose; the right hand still controls its direction. It is
not a two-controller leverage solve. Reloads use a button, not manual magazine
grabbing. Native finger and bolt animations remain in use.

Automatic support acquires when the tracked hands are within 30 cm and releases
beyond 45 cm. It retains contact intent while changing weapons, then checks the
tracked hand distance again. Turning the left wrist toward the eyes frees automatic
support for reading; holding left grip still explicitly requests support.

## Equipment selection

Release right grip before switching equipment. Hold **left trigger**, push the
**left stick** in a category direction, then release the stick and trigger.
Keep the direction held to open that category's native selection cards; use the
right stick to select within the category before releasing.

| Left-stick direction with left trigger held | Category |
| --- | --- |
| Up | Primary weapon |
| Down | Secondary weapon / bionic arm |
| Right | Support equipment, such as grenades |
| Left | Items |

The cards use real game data and appear around the forearm HUD. Selection does
not move the player. Release held stick/button inputs before resuming movement.

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
| Move | Left stick without left trigger |
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
