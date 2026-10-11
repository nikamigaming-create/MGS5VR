# October 11 field iDroid and combat settings

The checked development candidate recorded in `build/dev/BUILD.json` adds the requested launcher
toggles and left-grip tab chords, fixes two reproduced handheld iDroid defects,
and retains the published playable slot. Physical headset acceptance remains
open. The tests below ran in the retail game with Meta XR Simulator on an
isolated ON THE TRAIL mission save (10195); this is not the reported Rescue the
Intel Agents checkpoint.

## Launcher and controls

The shipping launcher passed 217 checks: all 59 validated settings, 104 actions,
10 axes and 29 bindable inputs are present. The three combat toggles were
actually saved and read back through the native bridge. Thumb rests, face-button
touch, stick touch and trigger touch remain bindable, including inputs without
a default action. Both local builds retain their matching launcher.

| Preference | Launcher section | Default |
| --- | --- | --- |
| Block firing near cover | Hands & weapons | Off |
| Enemy markers in the world | Wrist & HUD | Off |
| God mode | Movement & interaction | Off |

To apply a saved edit in game, release every button/grip and center both sticks
for two seconds. Held controls intentionally delay applying a changed layout.

Handheld iDroid focus remains left grip. While focused, flick the right stick
left for LB and right for RB; center it between flicks. LEFT stick moves Snake
without focus and browses menu lists while focused. Left stick click remains
an alternative LB chord. The existing right-grip RB page loop is retained.
Actual-game input samples verify the new LB/RB chords consume their horizontal
axes and do not move the native map cursor.
Earlier both-eye page captures on `e1dc46d11bc7` show MAP → MISSIONS → MOTHER BASE
→ MAP with the retained right-grip loop. That take tested the earlier left-stick
tab layout; the current RIGHT-stick layout has its own field evidence under
`artifacts/dev/idroid-stick-dog-current/`.

On `3e6e3d6e9818`, the fresh `markers-tabs-final/idroid-controls.json` field
take passed the current layout: LEFT movement covered 2.42 m with handheld
iDroid open and grip released; LEFT grip + RIGHT stick right changed MAP to
MISSIONS and left returned to MAP. The retained RIGHT-grip loop returned to
MISSIONS. Focused LEFT stick moved the row from Supply Drop to Buddy Support.
Native RB/LB packets were observed and the RIGHT horizontal axis was consumed.
Stow and the next short Menu tap reopened normally. All eight outcome pairs
were reviewed in both compositor eyes.

The same field take physically acquired native guard 1069 with the binoculars,
cleared it with A, looked away and returned without automatic reacquisition,
then explicitly restored its mark with RIGHT trigger. Fresh native acquired
records agree with the paired-eye outcomes. The compact world label is visible
after physical stow, with no oversized recon silhouette. The world marker
radius is 0.009 rather than 0.032; physical binocular/lens labels and letter
waypoints retain their established sizes. All six outcome pairs were reviewed.
Two earlier harness failures (held capture admission and exact-anchor log
rotation) remain recorded as failures; they do not qualify as passing cases.

## Reproduced iDroid defects

Before the camera correction, opening the handheld at HMD pitch +30 degrees
reset the view to level. Returning the headset to level then pointed the view
down 30 degrees while the native camera pitch was zero. The corrected handheld
keeps the gameplay orientation and tracks those same headset poses normally.
Spatial Pause/help presentation retains its existing anchor.

Before the stow correction, fresh movement reached native XInput while the mod's
own iDroid pad block remained active for more than 1.4 seconds. Player position
did not change. Afterward, the first fresh locomotion packet releases that owned
block after the native delivery acknowledgement; movement covered about 4.7 m
during the same window. Held menu buttons and old menu axes cannot acknowledge
the release. Native and cinematic controls retain their ownership.

## Combat proof

The combat and full page-loop takes used `e1dc46d11bc7`. The later development
candidate retains those combat and tab implementations and adds an exact native
handset pad-owner diagnostic. It passed all 55 automated groups, including real WARP
shader pixel checks for marker On/Off/HUD Off and per-eye projection, plus
installer checks. The Lua policy checks run inside the retail Lua VM separately.

| Actual-game check | Observed result |
| --- | --- |
| God mode On, native guards shooting Snake for eight seconds | Health stayed 7,801 |
| God mode Off, the same native enemy combat | Health fell to 7,330 |
| Enemy markers On → Off → On | Native acquired guard labels appeared, disappeared and reappeared in both eyes |
| Marked world at yaw ±20 degrees and positional lean | Both-eye labels followed world positions; native world recon silhouettes stayed absent |
| Close-cover block On, stable positive native tracked-barrel obstruction, VR trigger | Pistol ammunition stayed 10; all seven during-trigger samples retained a positive applied obstruction |
| Close-cover block Off, same requested controller pose, VR trigger | Native consumer selected ignore-proximity mode; pistol ammunition fell 10 → 9 |

The combat fixture uses real native guards, positions Snake on an observed
actor floor, sets native time to midday and seeds four native acquired tags.
It does not write health or ammunition or complete a mission. God mode owns
only `MGS5VR_GodMode`'s native damage status and releases that owner when disabled;
it never resets retail cinematic/tutorial owners. World labels use each eye's
projection, independently of native silhouette shaders inside binoculars.

The cover test uses a prone firearm aimed into native ground. Its first attempts
are retained as rejected fixtures: one lost obstruction before firing; another
attempted to apply an edit with ready grip held. The passing take requires a
stable fresh obstruction immediately before and throughout the blocked trigger.
The Off take retains the controller pose after releasing controls to apply the
edit. Native bullet collision and ammunition/reload rules are unchanged. Outcome
images are taken after releasing ready and do not show the weapon during firing.
Other weapons, cover shapes, vehicle weapons and physical comfort remain open.

The retained combat MP4 contains 100 real composited left-eye frames over about
35 seconds (about 2.86 capture fps). This is capture cadence, not game frame rate.
Both-eye outcome images and head-motion images were reviewed separately, and
the MP4 was decoded and reviewed. No desktop preview or generated frame is used
to establish stereo acceptance.

## Native failure and checkpoint scope

Before the new cleanup, a native target-failure fixture left the mod-owned
UI-only terminal open, its stale handset/map visible and its input block active.
The correction closes only that mod-owned terminal when retail
`TppMission.IsGameOver()` is affirmatively true. It does not reset unrelated game
statuses or force a mission transition. Actual cleanup takes closed the terminal
and removed its UI-only owner, but their failure panel was black and retry did
not enter loading. They do not establish native failure-menu or retry acceptance.
The old Lua `vr_idroid_player_pad_block` diagnostic is aggregate protection; a
true value during Game Over does not identify a mod leak. A proposed extra pad
handoff was removed after this was established. The new read-only fast
`idroid_player_pad_owner` checks the native registration for the exact mod owner.
Ordinary stow and native retail protection remain unchanged.

The supplied failure video ends on a loading screen. It establishes loading,
but does not establish an infinite hang by itself. Named checkpoints in the
isolated test profile are disabled and have no native locator; those fixture
attempts were rejected before creating a failure. The intermittent reported
checkpoint hang remains open until reproduced on a matching save/state.

## Retained evidence

All paths below are relative to
`artifacts/dev/field-idroid-checkpoint-current/`:

- `source-review.json`: source clips, hashes and decoded contact sheets.
- `field-before/result.json`: pitch reset and delayed movement reproduction.
- `field-settings-candidate/result.json`: corrected pitch and movement replay.
- `field-final-candidate/result.json`: later camera/stow replay on `e2842416cf48`.
- `tab-after/result.json`: new left-stick LB/RB and retained right-grip input.
- `tab-full-loop/result.json` and `review-tabs.jpg`: complete retained page loop,
  with both-eye outcomes reviewed.
- `launcher-settings-proof/launcher-regression.json`: 216 launcher checks.
- `combat-settings-proof-v2/result.json`: damage and marker transactions.
- `combat-settings-proof-v2/compositor/simulator.mp4`: actual combat video.
- `combat-settings-proof-v2/review-eyes-*.jpg`: both-eye combat/head-motion review.
- `close-cover-proof-v3/result.json`: stable obstruction, VR trigger and ammo proof.
- `close-cover-proof-v3/review-eyes.jpg`: released outcome eyes.

These are scoped simulator observations, not whole-mission or physical headset
certification. The published pin remains `experimental-2026-10-10.1`, DLL
`ba5c0ccbf406`. Use `Play-Dev.cmd` to test the candidate with wired Meta Quest Link;
`Play.cmd` restores and plays the published build. New personal preferences are
retained safely when switching to the older published parser and restored on
return to development. Building/testing does not advance the pin or publish.

The unresolved native failure-menu/retry fixture and the intermittent field
checkpoint report prevent claiming complete mission or release acceptance.
