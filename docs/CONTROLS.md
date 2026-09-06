# Touch controls for the tracked VR experiment

These bindings target the game's **Action Type** controller layout. The left-arm
HUD shows the native weapon name, ammunition and status. There is no flat aiming
reticle or destination-letter overlay in gameplay. Native 3D people cues remain.

## Weapons and hands

| Action | Control |
| --- | --- |
| Draw/ready the selected weapon | Hold **right grip** |
| Aim | Point the weapon with your **right hand**; use its actual sights |
| Fire | **Right trigger**, while holding right grip |
| Lower the gun while keeping it in hand | Keep right grip held and lower your hand |
| Put the weapon away | Release right grip; the game performs its native stow animation |
| Add the support hand | Hold **left grip**; release it to move that hand freely |
| Reload | **Right B**; the native reload moves the support hand, then releases it |
| Read the HUD | Raise and turn your **left forearm** toward your eyes |

One-handed aim works. Two-handed use currently means the left hand follows the
weapon's native support pose; the right hand still controls its direction. It is
not a two-controller leverage solve. Reloads use a button, not manual magazine
grabbing. Native finger and bolt animations remain in use.

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
| Native call/radio | Left trigger + X |
| Toggle native VR / large game screen | Left grip + left-stick click |
| Open iDroid map | Tap left Menu |
| Pause | Hold left Menu for at least 0.55 seconds |
| Recenter the large screen | Both grips + right-stick click |

For the complete native menu controls, release right grip and toggle to the large
screen before opening the menu. Then A confirms, B goes back, sticks navigate, grips act as LB/RB, and
triggers retain LT/RT. On the map, triggers zoom, right-stick click changes zoom
step and Y switches MAP/NAV. Close with B, then use left grip + left-stick click
to return to tracked VR. This deliberate menu transition does not change aiming
into a screen view.

## Test scope

Rifle and WU pistol ready/fire/reload, support attach/release, equipment cards,
left-arm ammo updates and basic iDroid/pause navigation have simulator observations.
Physical controller fit still needs a headset check. Throws, CQC, mounted weapons,
all inventory items, contextual targets, every menu branch and every weapon are
not certified. Flat damage, subtitle and context prompts currently have no spatial
replacement. Sleeve/cuff polish remains open. This guide describes the controls
without claiming those gates pass; see the [current SIM review](SIM_HUD_REVIEW.md).
