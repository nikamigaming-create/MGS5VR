# Touch controls for the tracked VR experiment

![MGS5VR field manual — original Snake artwork](images/field-header.svg)

[Editable default config](../config/mgs5vr-controls.ini) · [All control modes](images/control-modes.svg) · [Install and play](../README.md)

![Current illustrated default controller reference](images/controls-quick.svg)

## Edit your controls

Choose **VR Settings / Controls** in the native launcher for editable settings,
controller mappings and runtime options, or open **Edit-Controls.cmd** in the extracted release and choose the
`mgs5vr-controls.ini` beside your **game's** `dinput8.dll`. The external editor
checks conflicts while you type, refuses invalid saves, and backs up the previous
file when saving. No game launch is needed to check a layout. This is not yet
an in-game rebinding screen.

Open **mgs5vr-controls.ini in your MGSV game folder** (beside `dinput8.dll`).
Save, release all buttons/grips/triggers and center both sticks for about two
seconds. Changes apply live; no restart is needed. Invalid edits keep the last
working layout, and deleting an override restores its default. This applies to
bindings, axes, turning mode, HUD mode and the wrist/binocular adjustments.
The installed file is the one the game reads;
the repository copy is a template. Every supported button action is listed,
with separate gameplay, binocular, wrist, menu, horse and vehicle sections.

The settings editor includes player height, each hand's local position/rotation,
iDroid width/depth, paused panel size/distance/tilt, wrist placement, weapon smoothing, support acquire/detach
radii and relaxed/touched finger curl. Missing settings show their defaults.
Saving checks ranges, preserves other settings and creates an exact backup.
Runtime settings in `mgs5vr.ini` require restarting the game.

For a window-free workflow, run `MGS5VR-Launcher.exe --headless -Action Settings
-GameExe "D:\Games\MGS_TPP\mgsvtpp.exe"`. Actions `Set` and `Validate` use the
same validation as the editor; `Detect`, `Apply`, `Launch`, `Install`, `Update`
and explicit `Remove -ConfirmRemove` use the launcher's normal operations.
For example, `-Action Set -Setting settings.support_detach_radius_cm=35` changes
only that setting. Add `-Runtime` to edit runtime options.
The bindings below describe the defaults, not a hard-coded layout.

`[settings] hud_mode = binoculars_only` is the default: enhanced world cues
appear only in the binocular scene while the device is aligned with an eye.
Holding binoculars down does not enable them, and firearm scopes do not reveal
recon cues. `full` explicitly enables world cues in normal VR too; `off` hides them.
An existing file with `hud_mode = full` retains that preference until you edit it.
In the current development build, recognized gameplay caption/HUD layers are
routed above the left wrist; weapon/status stays flat on the left forearm.
Native equipment categories and descriptions have passed a SIM motion check.
Not every notification or caption has been visually checked.
`binocular_actor_glow = 0` disables the native glowing body effect while keeping
target acquisition and labels. `1` permits it within the selected recon view;
it does not force the game's native glow to activate for every person.
The native game's marker preferences still apply. Acquired people and A-Z
waypoints use native world positions in each eye. Caption restoration is still
incomplete, and other mission/objective labels need their own world-space adapters.

For a shifted player center, **left grip + Menu** recenters without changing
your current facing. Snap-turn pivot offsets now follow native camera yaw;
this fixes one native/snap transition error, not collision-following roomscale.

`right_stick_click` means pressing the stick inward; `right_stick_down` means
pushing it downward. A plain input stays active while held. `press(a)`,
`release(a)`, `tap(b,300)` and `hold(b,300)` provide one-shot gestures.
Use `press(left_grip + x)` for a combination, `press(a) | press(x)` for either
input, or `disabled` to remove a binding. Tap/hold pairs use the same duration.
The longer chord consumes the simpler input; release held buttons after changing modes.

The normal VR layout follows the native Action Type prompts where practical:
**A** is stance, bare **B** is pickup/carry, **Y** is context, and **X** opens
Commands. The deliberate exceptions are explicit: **left grip + B** reloads,
**left grip + Y** equips binoculars, and **right grip + right-stick click**
quick-switches a ready weapon. **Left-stick click** sprints and **right-stick
click** dives. Right-stick up zooms a ready weapon's fitted scope; in binocular
mode it runs, while left-stick click zooms the binocular lens.

You can optionally check syntax/conflicts by running
`mgs5vr_controls.exe --check mgs5vr-controls.ini` from the game folder.
Invalid files use built-in defaults and report the errors in `mgs5vr.log`.
The file also includes movement/navigation axes, snap angle and physical-gesture
switches. It remaps implemented actions; it does not create new native game abilities.

Smooth turning is the default: `[settings] turn_mode = native_smooth`.
In the launcher's **VR SETTINGS / CONTROLS**, set `settings.turn_mode` to `snap` and
`settings.snap_turn_degrees` to the angle you want (5–90 degrees; default 30).
The same settings can be edited in `mgs5vr-controls.ini`. The selected angle
applies in the cabin and on foot; center the stick between turns.
Set `turn_mode = snap` to enable snap turning; `off` disables normal stick turning. Horses/vehicles use native
horizontal camera input even with `snap`, because a camera-only snap cannot steer
their native forward direction. This mounted change still needs headset feedback.
Head pitch remains tracked. Menu sticks are unaffected.

Missing keys do **not** remove old/default actions. A replacement must explicitly
disable or relocate the original owner. Shared-mode exceptions are deliberate:
weapon-ready/support may accompany longer chords; tap/hold durations must match;
Commands continuation preserves an already-held CQC input but cannot start one.
Send the exact rejected layout when reporting a conflict—the checker names the
two actions involved, including inherited defaults.

## Complete native-button mode

A connected Xbox/XInput pad works with the game's normal controls in immersive
3D and on the big screen. Any of the four XInput slots can supply it. Physical
pad input takes control; a fresh VR button or stick action can take over after
the pad returns to neutral. Idle VR input no longer masks the pad. In gamepad
mode the game's authored hand/weapon animation supplies the poses.

Hold **left Menu + right B for 0.55 seconds**, then release, to switch between
immersive VR and a large screen. The screen recenters in front of the headset
and defaults to 12 metres wide at 6 metres away. Screen mode automatically uses
the native-button layout below; returning to VR restores the previous input
mode. There is currently no physical Xbox shortcut for switching presentation.

Hold **Menu + right A for 0.55 seconds**, then release buttons and center sticks.
Repeat to return to normal VR controls. This changes input only, not VR/theatre.
The separate `[native]` section maps every game-facing native Xbox control and
works in either game, including menus and GZ without a tracked-rig adapter.

| Native control | Touch control |
| --- | --- |
| A / B / X / Y | Same named face button, including native holds |
| LB / RB | Left / right grip |
| LT / RT | Left / right trigger, including partial pressure |
| Left / right stick and clicks | Same sticks and clicks |
| Start / Back | Hold Menu first, then left / right grip |
| D-pad | Hold Menu, center right stick, then flick the desired direction |

After choosing D-pad, center the stick again: the selected native D-pad remains
held while the right stick can browse equipment. Release Menu to equip/close.
All bindings, triggers and stick assignments are editable. Plain bindings retain
holds and combinations; normal VR mappings are inactive, so they do not duplicate
these actions. Native B therefore retains the game's single-button pickup/carry
hold. Game unlock/context rules still apply. This does not implement missing GZ
first-person presentation or certify every gadget's native behavior.

In native-button mode Menu + left grip means native Start, not VR recenter;
switch back to VR controls before using the recenter chord.

Optional `[gameplay].native_dpad_up/down/left/right` bindings send native D-pad
actions directly, bypassing the wrist category chooser. They default to
`disabled`; native weapon/state rules still apply. They are not a universal
alternate-fire action. For a deliberate custom chord, for example,
`native_dpad_up = press(right_grip + x)` keeps the weapon ready without also
opening Commands. Keep these disabled unless your custom layout needs them.

These bindings target the game's **Action Type** controller layout. The left-arm
HUD shows the native weapon name, ammunition and status. Native context-action
icons appear beside that display when the game offers an action; the horse/Y
mount prompt has been checked through mount and dismount. There is no flat aiming
reticle or destination-letter overlay in gameplay. Binocular waypoints and
acquired-person distances appear inside the lens; native 3D glow routing still
needs a separate review.

## Weapons and hands

| Action | Control |
| --- | --- |
| Draw/ready the selected weapon | Hold **right grip** |
| Aim | Point the weapon with your **right hand**; use its actual sights |
| Fire | **Right trigger**, while holding right grip |
| Lower the gun while keeping it in hand | Keep right grip held and lower your hand |
| Put the weapon away | Release right grip; the game performs its native stow animation |
| Add the support hand | Squeeze left grip near the weapon's support socket and hold briefly |
| Free the left hand | Release left grip or pull away from the support contact |
| Reload | **Tap left grip + B**; the native reload moves the support hand, then releases it |
| Pick up a dropped weapon / carry a person | **Hold B** at the native prompt; bare B remains a native hold |
| Switch weapon while aiming | **Right grip + right-stick click** while ready; the longer chord consumes Dive |
| Physical weapon-scope zoom | **Right-stick up** while ready; cycles the fitted scope's powers, while fixed-power sights stay fixed |
| Detonate placed C4 / inflate thrown decoys | Keep that gadget selected, **hold right grip, then Y**; right trigger places/throws it first |
| Read the HUD | Raise and turn your **left forearm** toward your eyes |

The right hand owns the weapon grip. With support engaged, the line from the
right controller to the left guides the authored barrel direction. The left
hand retains the native support contact; reload animations temporarily own it.
Reloads use a button rather than manual magazine grabbing.
Round weapon scopes show an independent magnified scene only through the eye
behind their rear lens; the surrounding world keeps its normal stereo view.
Set `settings.scope_eye_relief_cm` in the controls file for the preferred
eye-to-glass distance (10 cm by default). This does not move the rifle.
The native full-screen zoom is not triggered by this binding.
See [scope implementation and current coverage](WEAPON_SCOPES.md) for sight limits.
Pickup/carry and Dive temporarily lower native aim and suppress the attack trigger.
This prevents Dive from becoming the game's aiming-mode weapon switch. Stance
also lowers aim. These are separate configurable actions, not automatic scope
or equipment selection. Use Y for Fulton/context actions, not for native B holds.

Free fingers articulate on both hands: grip curls the lower fingers and thumb,
and trigger curls the index finger. Supported controller touch sensors also pose
the index and thumb. These are controller-driven poses, not individual finger
tracking. Hands in contact with a weapon retain its native grip animation.
Grip and trigger keep their gameplay actions while also animating the fingers.

Support requires the left palm to stay within 10 cm of the weapon's support grip
for 150 ms, then blends into contact. Pulling more than 30 cm away releases it.
Both distances are editable as `support_grip_radius_cm` and
`support_detach_radius_cm`; detach must exceed acquire by at least 2 cm.
For long-gun foregrips, moving the left hand beside or behind the firing hand
also releases support; guidance stays within 60 degrees of the right controller's
pointing direction. This prevents the weapon from following a withdrawn hand.
Pistol-style close cups use the native palm contact instead: the support hand
can sit beside/below the firing hand without passing a rifle's forward cone.
The firing hand alone owns aim in a close cup, so almost-touching controllers
cannot twist the sights. Weapon changes clear the previous weapon's acquired grip.
Clenching left grip in empty space only animates the free fingers; support also
requires contact at the weapon. Releasing it or lowering the weapon clears contact.
Wrist rotation retains an acquired support contact; release left grip or
withdraw from the socket to free the hand. Selection
does not leave a sticky support latch. The corrective headset run received
positive feedback; complete reach and pose coverage remains open.

## Equipment selection

Hold **left trigger** to open the left-wrist picker. The **left stick still
moves you**, so you can select while walking. Use the **right stick** to browse,
then **release left trigger** to confirm and close. Gameplay does not pause.

`settings.wrist_picker_width_cm` enlarges the unfolded native cards and text
(42 cm by default, 42–100 cm). The layout stays above the rendered forearm;
the small ammunition HUD keeps its own size on the wrist.

`settings.idroid_screen_width_cm` controls the hand-carried iDroid display
(30 cm by default, 20–60 cm). Its height is derived from the native 16:9
source, so changing the width cannot stretch the map. Keep the value near the
default first; larger values improve legibility but occupy more of the view.

| Right stick with left trigger held | Action |
| --- | --- |
| First flick up / down / right / left | Primary / secondary / support / items |
| After the cards appear and the stick is centered | Flick toward a card, including diagonals; center between choices |
| Right B, while keeping left trigger held | Return to category choice |
| Right A, on an item with a Use prompt | Use the selected item once |

Holding left trigger shows the game's four-direction equipment overview without
equipping anything: primary above, secondary below, support right, items left.
The icons, names, counts and unavailable/None states come from the native game.
A stick already held when selection opens is ignored
until centered. A fresh flick opens one category. Once its cards appear, center the stick and flick toward a
card. Up is up, left is left, and diagonal cards accept diagonal flicks. Keep a
flick steady briefly; center between choices. Holding or wobbling one flick
cannot skip into a second selection. B returns to category choice while left
trigger stays held. Native equip/stow transitions can delay opening; navigation
is blocked until the expanded menu has rendered.

The cards and descriptions use real game data and unfold above the **left wrist**,
facing you; the small status display stays flat along the rendered left forearm.
Its canvas preserves the native aspect before the stereo eye replay; changing
the picker does not change that mount. `wrist_surface_lift_cm` is currently
reserved and does not move the status surface. `wrist_selector_height_cm`
sets the picker height (default: 15 cm, clearing the bottom card from the arm). Raise that
wrist into view to read the selection. Right-stick turning is consumed while
the picker is open and until the stick returns to neutral after closing. A held
fire trigger must be released before it can fire after selection.

For an item card with a **Use** prompt, keep the category open, select the card
with the right stick and press **A**. A held Use button
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
while right-stick left/right still turns. Left-stick click runs; **A** changes
stance, never the throw angle. Throw strength remains the native
equipment strength; this is point-and-trigger throwing, not a velocity gesture.
The latest SIM observations and remaining stance limits are in the
[interaction and visibility review](INTERACTION_VISIBILITY_REVIEW.md).

**Handheld binoculars:** hold **left grip + Y** for about 0.3 seconds to equip
the device. Release the chord: it stays equipped in the right hand.
Brief lost hand/aim tracking hides the presentation, not the equipped selection;
the device returns when tracking resumes. Equipping gives a short right-hand pulse.
Tap **B** again to stow it. When binoculars are not selected, a short B tap
remains the native pickup/carry action. Aim by moving the right hand. **Click the
left stick** to switch 2×/4×. **Right trigger** marks the person under
the optic's crosshair, or places a native waypoint on the visible surface when
no person is targeted. A short rumble confirms marking. Tap B to stow.
**Tap A** to remove the waypoint or acquired person under the
crosshair. Empty space clears nothing. Right-stick down remains the binocular
stance gesture, because A owns clear in this mode.
Bring the left hand to the opposite side and squeeze
**left grip** to cup it for support; release the grip or pull away to let go.
Zoom does not require support contact. Zoom starts at **2×** and each
left-click switches **2× → 4× → 2×**; lowering retains that selection.
Walking, running and turning remain available; **right-stick click dives** in this
mode, just as it does with binoculars stowed.
The device renders its own narrow-angle scene through its physical lens while
the surrounding world retains normal stereo. The lens stays on its physical
aperture at all distances; it never expands into a full-screen zoom. Its image
keeps the same left/right orientation as the unzoomed world. Waypoint letters and acquired-person distances are
drawn inside this view. Automatic identification by dwelling on a person and
intel analysis are not implemented; marking currently requires the trigger.
The default `binocular_pitch_degrees = -90` tilts the device down 90° around the
right palm position. Both palms face the housing sides, with the fingers curling
over its top, not pointing back at the eye. Housing, lens, aiming ray and cupped
hands share that rotation. Set this under `[settings]`
in your existing controls file to apply the new fit; `0` restores the old angle.
`binocular_max_eye_distance_cm` sets how far the eye may sit behind the ocular:
30 cm by default, editable from 15 to 50 cm. G2/WMR users can increase it to
keep controllers in their tracking volume. This changes activation and
stabilization tolerance, not lens size, scope geometry or world culling.
Tracked first-person hands remain fully opaque, including when a wrist, finger,
or binocular approaches the face. The shoulders follow physical head yaw so turning
does not leave the sleeve roots in the old game-camera direction.

**Motion melee (development candidate):** swing either free hand, or bash with a
readied firearm. No button arms a punch. Deliberate movement closes the free
fingers and submits contact at the visible palm or authored weapon tip using the
native kick attack entry. Slow movement, tracking jumps, wrist selection, optics,
reloads and seated movement cannot generate punches. One accepted target ends a
stroke; pull back before another strike. Native eligibility and damage rules
apply; not every object becomes destructible. Automatic swings ignore D-Dog,
puppies, companion horses, companion Quiet and non-hostile wildlife, including
small rodents. Explicit native attacks retain their game rules. Enemy reaction
and damage still need testing.

**Pet D-Dog:** stow the weapon, open your hand and gently stroke his head or
muzzle. Keep contact for at least half a second and move the palm a few
centimetres. D-Dog performs his native pet
response while you keep control of your head and hands. Pull away before another
pet. This interaction has been demonstrated with native game audio in SIM;
other animal pet responses are still being implemented.

**Hold a rat:** crouch with **A**, then offer an open palm close to the ground
beside a live rat. Keep the palm facing up and bring it under the animal briefly;
lift your hand to carry it. Lower the palm to the ground and pause to release,
then withdraw. Grip and trigger should remain released. The interaction follows
active native rat instances, including rats encountered outside the test scene.
A brief tracking interruption preserves the last hand pose; sustained tracking
loss returns the rat to the last safe ground position. Headset testing remains
outstanding.

Basic left-arm melee needs no special arm selected. Powered prosthetic abilities
retain campaign unlocks and equipment selection. After selecting an available
ability, use **right grip to ready + right trigger to activate**, following its
native charge/release behavior. Ordinary punches cannot select or discharge an
ability. Later powered arms are not verified on the 1% checkpoint.

## Movement, actions and menus

| Action | Control |
| --- | --- |
| Move, including during wrist selection | Left stick; on foot, forward follows your view heading |
| Turn | Right stick left/right: native smooth turn by default; enable optional 30° snap in controls. Mounted view/turret uses both axes |
| Sprint / run | Left-stick click; with binoculars, right-stick up also runs |
| Crouch / stand / prone | Tap **A** for crouch/stand; hold **A** for prone. The weapon lowers for the stance action |
| Quick dive | Right-stick click on foot, including with binoculars equipped. No face-button duplicate |
| Right-stick context | Up/down never pitch the gameplay camera. Equipment, Commands and native menus keep navigation |
| Context action / Fulton | Left Y without the equipment modifier; hold when the native prompt requires it |
| Pick up weapon / carry person | Hold **B** at the native prompt |
| Native attack / CQC / carried-body throw | Right trigger with right grip released; the native game state chooses the action |
| Wrist Commands / buddy orders | Hold X; point the right stick toward an available command and press A; release X to close |
| Toggle native VR / large game screen | Hold **left Menu + right B** for 0.55 seconds, then release |
| Open iDroid map | Tap left Menu |
| Pause | Hold left Menu for at least 0.55 seconds |
| Main menu / options | Hold left Menu on foot; choose Return to Title Menu or Options with the left stick and A |
| Cutscene skip | Hold left Menu; choose Skip and press A when the native game offers it |
| Recenter the large screen | Left grip + left Menu |
| Recenter in game, including while holding binoculars | Hold left grip and tap left Menu; keeps your current facing and brings the body under your head |

By default, **iDroid and Pause use a stable tilted panel in the 3D scene**.
Gameplay pauses while iDroid is open. Head tracking and stereo stay active;
the panel stays where it opened, rather than following your face or hand.
It defaults to 1.2 metres wide, 1.3 metres away, slightly below eye level and
tilted up by 10 degrees. Width, distance and tilt are editable in VR Settings.
Closing the menu releases only the mod's own pause; other native pauses remain.

Set **`settings.handheld_menus = 1`** to opt into the live handheld iDroid and
wrist Pause panel. The handset and projection then follow the cupped right
palm, and iDroid leaves the world running. This experimental mode still needs
headset feedback. Equipment and Commands remain on the left wrist in both modes.

**Left stick** selects menu rows; **A** confirms and **B** goes back.
The **right stick** moves the live map. **Left/right grip** change tabs as
**LB/RB**, and triggers retain **LT/RT**. On the map, triggers zoom, right-stick click changes
zoom step and **Y** switches MAP/NAV. **B** closes iDroid; hold **Menu** again to
unpause. Holding Menu *inside iDroid* opens its native Help, so close iDroid before
opening Pause. If a native tutorial traps B, hold **B for 0.75 seconds** to
request its exit, then answer the game's confirmation. If the handset remains
out after the panel closes, tap left Menu to stow it before reopening.
Release held controls once after returning before moving or firing.
The handheld projection uses native stick/button selection; it does not show
a decorative touch pointer that cannot select anything.

Recenter uses only a horizontal tracking origin. Native camera pitch/roll is
excluded from the gameplay tracking frame, so physical turning keeps gravity
upright. On-foot movement uses the game's published camera basis directly;
applying another stick rotation would turn movement twice after a snap.

For the main menu, open Pause, select **RETURN TO TITLE MENU**, press **A**, then
answer the native confirmation. **OPTIONS** and **CONTROLS & MANUAL** are also in
Pause. At the initial title prompt, press **Enter** or tap **left Menu** to
enter tracked stereo in the helicopter. The title choices appear on the
cabin panel. Use the **left stick** to move the highlight and **A** to select.
Tracked arms remain in the stereo scene. The native-camera pass that supplies
the panel excludes the owned player's normal geometry, then restores it; this
does not globally hide arms or restore camera-obstruction fading.

With `opening.interactive_cabin=1`, the title cabin uses the physical cassette
rack. **Left stick** walks through the cabin; **right stick** turns according
to `settings.turn_mode` (smooth by default, snap only when selected). Sticks
do not move the hidden title-menu highlight. Movement checks native swept head
and shoulder clearance and floor support; the initial six-ray actor envelope
does not restrict walking to a small box. This is camera clearance, not a full
native player capsule. The cabin keeps the game's original world scale.

Hands follow the controllers when reaching toward D-Dog. An open-hand stroke
in contact with the native dog triggers his response. There is no dog-specific
hand snapping or pushback, and merely holding a hand still is not a pet stroke.
`system.toggle_vr` switches between immersive VR and the large
quad, including at the title menu. That manual choice persists through the
title-to-game transition. Its default is **Menu + B held for 0.55 seconds**.
Existing configurations with an explicit `disabled` retain that choice.
Loading can temporarily use
the quad while the next player camera is created; immersive play resumes with that character.
Cutscene Skip is available only when the native game offers it. Loading,
cutscenes and player-replacement transitions still need broader VR integration;
the floating gameplay menus do not establish support for every transition.

Wrist Commands has its own input mode, separate from equipment selection.
For interrogation, first hold up or restrain an eligible guard using the native
gameplay action. Keep the existing right-grip aim or lowered-weapon CQC trigger
held while opening Commands. That already-active input now continues through the
menu and closing transition. Releasing it stops it; Commands cannot start a new
attack, and confirmation cannot fire the weapon. This repairs an input-routing
defect, not a completed guard-interaction or CQC-animation implementation.
Keep X held, center the right stick after the commands appear, then
point toward a command and confirm. Left-stick walking remains available.
A confirms once. B cancels; release X and center the stick before returning to
gameplay. On horseback, X alone remains gallop: hold left trigger and tap X,
keep left trigger held, then use the same stick/A selection. The native game
chooses which commands are available; stand or crouch to call D-Horse. Calling
D-Horse, Stay back and the arm's distraction knock were exercised in SIM.
The motion-melee candidate now
reaches native contacts; enemy reaction and damage remain separate acceptance
steps. See [system access and the physical-melee contract](SYSTEMS_ACCESS.md).

D-Horse and D-Dog share the game's active-buddy slot; calling the deployed buddy
does not deploy the other one as well. Switch through iDroid → Missions → Buddy
Support. The call menu uses the native buddy and stance restrictions.

## Vehicle mapping under simulator development

Native vehicle state changes the controls to **right trigger: accelerator**,
**left trigger: brake/reverse**, **left stick: steering**, and **Y: enter/exit**.
In the development candidate, bring the **left hand to the wheel and squeeze
left grip**. Turn the controller clockwise/right or counterclockwise/left;
release grip to let go. The native driver's hand keeps its authored contact.
The left stick remains available when the wheel is released. **Left X** invokes
the native vehicle weapon/call action; grabbing the wheel cannot fire it.
**Right grip** is the equipment modifier while seated. The **right stick**
continues to send both native camera/turret axes, including vertical aim for
armored vehicles and helicopter views. Release
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
and other target-specific prompts still need individual checks. General layout
layers are suppressed in tracked first-person VR rather than projected onto the
player's face. Captions did not appear in the current radio fixture and are not
marked fixed. Sleeve/cuff polish remains open. This guide describes the controls
without claiming those gates pass; see the [current SIM review](SIM_HUD_REVIEW.md).
