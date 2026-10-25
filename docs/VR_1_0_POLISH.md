# VR 1.0 polish and acceptance

> Historical report/specification. Statements such as "current", "next" and "unfinished" below refer to the original checkpoint, not today's implementation. Use [CURRENT.md](CURRENT.md), [FIELD_FIXES.md](FIELD_FIXES.md) and [RELEASE_PLAN.md](RELEASE_PLAN.md) for effective status and next work. Preserve this record; do not restart its old queue without checking the later code audit.

Updated October 14, 2026. The target is playable TPP from start to finish:
all cutscenes, every playable body and outfit, native mission progression,
working VR interactions and comfortable physical-headset use. A successful
field scene or movie is one case within that target. No new body, cinematic
movement or live-settings candidate is accepted by this inventory.

[CURRENT.md](CURRENT.md) records dated results and exact build identities.
[NATIVE_CONTEXT_COVERAGE.json](NATIVE_CONTEXT_COVERAGE.json) supplies the
existing native-context obligations; its initial list is explicitly incomplete.
The groups below organize those obligations into practical sessions. Add newly
discovered native situations to the inventory rather than assuming a shared
input mode makes them covered. The existing GZ adapter has limited native-button
coverage; its completeness must be stated separately in any release claim.

## Status and immediate blockers

**October 14 resume point:** the source, tests and acceptance records are
preserved by the recovery checkpoint described in [CURRENT.md](CURRENT.md).
The prologue genuinely reached results and mission 1 under requested damage
protection; normal-difficulty completion remains open. The mission 1 checkpoint
is preserved. Stationary mounted body views were reviewed, while riding motion,
larger leans, outfits and vehicle/cabin bodies remain open. The current direct
regression is Mission 18 (`10100`): its static sortie/loadout repeat exposed
the complete authored preview, with canvas padding and motion still open.
Actual VR deployment reached the field and the adult objective became clear.
The retained mine approach has all five children alive; the rescue door,
extractions and reported results-to-scene handoff remain pending. The simulator
upgrade continuation genuinely reloaded a field entry with the adult objective
preserved, then verified existing VR wall-cover recovery and paired rifle
discharge. The ordinary cave gate still blocked a clear-alert interaction/
crossing repeat. Back-facing prone rifle views also exposed body/shoulder
intrusion in the left eye. Neither the last test pose nor complete rifle/body
acceptance is verified. The new tooling passed all 56 groups and installer
checks; source checkpoints are on the remote development branch. The owned
simulator session is closed with Meta Link restored.
[MISSION_ACCEPTANCE.json](MISSION_ACCEPTANCE.json) inventories
all 51 required missions; none has full-mission acceptance. Physical Quest
acceptance remains pending. The October 13 findings below retain their original
scope and are superseded only by the exact later results in CURRENT.md.

**October 13 fresh campaign:** agreements are authorized, and genuine cleared-save
startup has reached the hospital through the VR START GAME binding. The exact
hospital TitleMode/3D-opening/menu paths are corrected and reviewed on
`e0cd10465c37`. The first acted hospital scene retained its actors and world
through head turns, physical lean and independent viewer movement. That same
review exposed foreground geometry clipping. The first short head-look bridge failed to advance the lesson. The bounded
two-second HMD gesture then advanced the actual native lesson on `cc2182251e28`
and `0bc5c0cb8ddf`, reaching `Seq_Demo_FewDaysLater2`. Foreground near-head
geometry remains visible and uncorrected. The latter candidate also preserves
the name/birthday form's authored body and spatial panel after its instruction
popup closes. Candidate `cc52a0345ef4` entered AHAB through physical VR text
entry and reached the actual avatar checkpoint; its keyboard remained occluded.
Candidate `3056df4c8304` restores the complete native avatar preview and VR
preset Confirm, while the bed backdrop and editor canvas still needed correction.
Checked candidate `1043b4c09aa0` adds a fade-safe blurred stereo cache and native
16:9 editor canvas. The complete preview and VR confirmation were reviewed in
both eyes, but the bed surround stays head-locked during head turns and remains
unaccepted. Native avatar confirmation reached Quiet's appearance/exit and the
first hospital crawl. Candidate `b8fe59e6bbd9` removes the integrated head and
separate hospital face cover from the near view while preserving normal body
visibility and native shadow flags. Both-eye reload, head turn/down/lean and
35.1 cm of actual crawl displacement were reviewed with no obstruction.
The named-demo classification correction still needs its actual Quiet repeat.
The hospital runner now holds asynchronous movement for its actual requested
duration, follows the retail rail and native heading, and handles its witnessed
crawl/crouch/stand states with released recovery from delayed rig publication.
Genuine movement has reached `Seq_Demo_Heli`, `Seq_Demo_Volgin` and
`Seq_Demo_Cure`; both captured Volgin and Cure actor pairs were reviewed.
The five-second right-controller synthetic loss retained first person, a
connected hand/forearm and a valid body through loss and recovery in the actual
hospital. Physical controller inactivity remains unverified. A separate camera
reason-8 interruption during a rail turn is still open. The compositor keyboard
repeat, full prologue and physical headset acceptance remain open. Retained
runs and exact scoped results are in [CURRENT.md](CURRENT.md).

The campaign has now genuinely reloaded its hospital save, passed both stairs
again, advanced the native corridor movement prompts and naturally finished
the curtain scenes. It saved checkpoint `2401495818` and reached
`Seq_Game_AfterCurtainRoom`. The current bed-row exit remains obstructed;
the gun tutorial, hospital exit and subsequent reloads are still unverified.
The bot now rejects animation oscillation as route progress and recovers the
observed backwards prone heading through real VR stance and movement inputs.
These runs use the user's requested God mode, so normal-difficulty progression
and the earlier searchlight failure remain open. They do not establish 1.0.

**Current field body:** `ed5ce9e88d7b` retained a valid body through 96 standing/
prone head-turn samples, with at most 0.733 mm of anatomical eye component
error. All 32 corresponding motion/body eye images were reviewed. The prior
prone dropouts and headset-driven foot orbit are corrected in this field
fixture. Rightward prone overhead gear remains a visible defect. Slopes,
larger reach, seated/mounted bodies, outfits and physical Quest use still need
their own actual acceptance. Historical findings below retain their original
scope and do not override these newer results.

**Failed in the actual game:** body run `20261013T015218004549Z` on candidate
DLL `75cc3fbf262c` could not obtain the standing body and hands. Its published
body was invalid and both palms were untracked. The corrected measured spine
and face-eye placement were repeated in `20261013T023620914011Z` on `2bbe736a023a`.
Both eyes of all eight phases were reviewed: connected hands/sleeves and the
previous stretched cloth/near-face intrusion are resolved in these views.
The clearer hands-down `eye-boots/` view on `9c8ef2716159` failed: a near-169-degree
native pelvis/retargeted chest split remained. Coherent full-body render yaw and
final eye displacement diagnostics passed retained run `20261013T030910923796Z`
on `11c8ba4614c5`. Eighteen reviewed eye images show connected hands/sleeves,
no old cloth tears and two boot silhouettes in the deeper hands-down view.
The published eye diagnostics then exposed 7--10 cm of displacement from the
fixed-waist pivot. Complete eye-centered render placement and a 2 cm maximum
eye admission bound are implemented; their actual-game repeat is recorded in
the final verification receipt. Actual prone inspection isolated a 107.6 mm
cached-camera versus animated-head eye discrepancy; original-skin eye sourcing
and foot-specific terrain query starts are included in the final candidate.
Sole/floor contact and large-lean clipping
remain unaccepted;
seated, mounted and outfit coverage remains pending. The fresh-story attempt
`20261013T011650365936Z` stopped at Terms; it provides no prologue evidence.

**Implemented, scope-limited runtime review:** live gear settings, VR off/on in
the editor, posture-aware view stabilization, full-body IK and cinematic
spectator movement. The candidate passed 56 automated groups and installer
validation; those results do not establish whole-game play. Actual run
`20261013T023922503571Z` applied and restored boolean, numeric and enum settings,
and kept its panel available with VR off. Returning to VR reproduced a duplicate
automatic-entry toggle. Its corrected handoff passed retained actual run
`20261013T024522086164Z` on `9c8ef2716159`, together with remap/conflict and physical
binocular equip/stow. Both eyes of sixteen checkpoints were reviewed.

Native mission 10020's loading Resume started the actual authored arrival scene;
Snake and Ocelot are visible in both eyes in `cinematic-native-arrival/`. It
naturally returned to the iDroid tutorial. The interrupted capture harness does
not certify spectator movement or the entire scene. The later natural
`cinematic-native-immediate-viewer/` take on `11c8ba4614c5` passed exact scene-ID,
HMD look, fresh neutral native delivery during spectator input and natural
horseback control return. All fourteen eye images retain native actors/heads
and the world. The subsequent `cinematic-final-immediate/` take on `efeb56da218e` measured
6.18 cm of submitted viewer translation separately from the moving authored
base, with a fixed HMD and neutral native gamepad delivery. It naturally
returned to the horseback iDroid tutorial with both hands attached and no
host Pause or iDroid menu left open. Both eyes of all seven checkpoints were
reviewed; authored dust prevents crisp face acceptance in this take. Avatar-preview, death/retry
and result-camera actor presentation still need distinct native observations;
the cached Lua demo classifier alone cannot establish those cases.

**Scoped earlier simulator evidence:** the retained editor, physical cabin
props, camera/gallery, iDroid, binoculars and combat runs in CURRENT.md establish
their stated cases on their recorded DLLs. Preserve those interactions and
repeat affected cases after shared camera, skin or input changes. They do not
accept a later candidate automatically. **Physical Quest and whole-mission
acceptance remain pending.**

## Practical acceptance groups

| Group | Required behavior and variants | Current standing |
| --- | --- | --- |
| Body, stance and traversal | Connected hands, torso, legs and feet; correct native body/outfit and prosthetic; standing, crouching, prone, transitions, walking/running, strafing/backwards, slopes/stairs, leaning and reaching. IK preserves segment lengths, gait and contact without seeing through limbs or moving the actor through geometry. Repeat across playable Snake variants, avatar/disguise and staff bodies exposed by the owned game. | Current field hands/torso and boot silhouettes reviewed. Floor contact, larger lean, seated/mounted and outfit coverage pending. See [body/view contract](NATIVE_BODY_VIEW.md). |
| View and tracking | No footstep bob/sway or movement-stick orbit; sensible stance height and floor clearance. Direct HMD look/lean, snap/smooth turn, recenter, tracking/focus loss and recovery. Both eyes, hands, held props and native pixels use the same accepted source frame. | Reported bob/sway reproduced; new correction pending actual-game and headset review. |
| Live settings and mapping | Open the icon-based panel in each actual mode; edit/cancel/confirm toggles, numeric and enum settings; release to apply; switch VR off/on and return; record/remap controls with conflict handling. Native ACC, driving, horse, optics, equipment, Commands, iDroid, menus and cutscenes select the right actions. | Gear edits, VR off/on, physical remap/conflict and binocular mode reviewed on `9c8ef2716159`. Remaining actual modes pending; driving was inspected from ACC. See [editor controls](HEADSET_CONTROLS_EDITOR.md). |
| Weapons, optics and equipment | Physical sights, muzzle, impact and hands agree. Pickup/equip, reload, holster, two-hand support, weapon switching, scope zoom and firing work across eligible weapon families. Physical binocular hand aim, zoom, visible-target acquisition, clear/explicit mark and stow remain intact, including tutorials. Native NVG and equipment effects retain their real state. | Scoped weapon/binocular/NVG cases exist. All weapon families, grades, awkward reach/cover and mission tutorial variants remain open. See [owned item catalog](ITEM_CATALOG.md). |
| Wrist HUD, iDroid and menus | Ordinary status stays on the left bionic forearm. Native iDroid stays in the left hand with its emitter/projection, both-eye contact, movement, pages, stow and immediate reopen. Pause uses its stereo panel; nested menus, tutorials, forced stow, rewards, failure/retry and exit restore the correct controls. | Ordinary field/ACC paths and selected repairs reviewed. Forced tutorial stow, failure/retry, purchase/result and broader nested menu variants pending. |
| Cabin, camera and papers | Physical tapes rest on the seat; reachable pickup, weight/contact, insertion/ejection and native Continue. One embedded player, readable casing labels, connected hands. Live lens photos, optional owned Freeze, browse/reprint, confirmed deletion, independent prints, sticky-note disposal, captions/drawing and wall attachment survive reload. | Scoped cabin/ACC/field/Mother Base loops reviewed on earlier DLLs. Recheck after rig changes; native player-lid animation remains unidentified. See [studio contract](CABIN_STUDIO.md). |
| Horses, vehicles and transport | Mounted body/seat fit, hands, native steering/pedals/weapons and deliberate view motion. Verify vehicle subtypes, driver/passenger/turret roles, boarding, flight/landing, dismount and fresh on-foot control. Include field helicopter and ACC ownership separately. | Selected steering/flight/ACC cases exist. Full bodies, pickup/return, all roles and subtype transitions remain pending. |
| Native field interactions and buddies | CQC, carrying/placing bodies, hold-up/interrogation, cover/climb/hide, collection, Fulton, emplacement and buddy interactions use native eligibility and outcomes behind reachable VR controls. Held objects follow solved hands. | Bounded cases exist; complete eligible actions, target variants and recoveries remain open. |
| Campaign, saves and recovery | Genuine first-playthrough tutorials, mission entry/objectives/completion, checkpoint/death/retry, return to ACC, save/resume and repeated loading transitions. No fabricated tutorial or story completion. Sustain whole missions with combat, optics, menus and mounted transitions together. | Whole-game completion pending; native failure/retry and Mission 6 transition remain independent cases. |
| Physical headset and delivery | Wired Meta Quest Link/Meta OpenXR; correct runtime eye targets/FOVs, sustained readable stereo, scale, reach, comfort and stable performance. Keep desktop preview 1280 x 720. Both Play paths, configuration, installer/update/recovery and saved preferences remain usable. | Launcher/runtime discovery and automated checks exist. Sustained physical Quest gameplay and final release-candidate acceptance pending. |

## Every cinematic class stays in scope

| Native situation | Required session | Present gap |
| --- | --- | --- |
| Noninteractive multi-shot scenes | Full native movie with visible Snake/other actors, HMD motion, viewer translation/turn, close and wide shots, cuts and return to play. | New authored-skin restoration and viewer movement unaccepted. Small cuts can enter the motion filter; viewer movement has no geometry clearance. |
| Interactive demos/tutorials | Hospital `Seq_Game_FewDaysLater0/1` and each other required-input scene; preserve visible bodies, native lesson progress and VR interaction. | Authored-skin protection now includes the hospital lessons in source; actual-game acceptance is pending. They still exclude spectator movement. Other interactive subtype/step coverage is incomplete. |
| Scripted gameplay transitions | Naturally reach Mission 6 bridge `p31_020020_000` and other camera/control handoffs; playable background demos must retain gameplay. | Source classification exists; the bridge itself remains unverified. |
| Scripted seated rides | Skull Face jeep and other authored passenger scenes, including body/seat, camera changes and exit. | Distinct mission/sequence/seat fixtures and actual acceptance pending. |
| Clear, abort, death and retry | Native end camera, visible result choice, legitimate retry/return, loading and correct restored on-foot/mounted body. | Separate subtype and outcome coverage pending; do not leave Resume/Skip or a failed test-opened menu behind. |

Restoring the mod's hidden groups does not prove native actor opacity or draw
inclusion. Preserve each actor's authored animation and native scene state;
verify final pixels before changing native visibility. Recorded media has its
own presentation path and must retain comfortable headset viewing and return
to control. It supplies no authored 3D actor or free-camera evidence.

## Existing Terms-free native reference route

For an isolated owned middle-campaign save, first observe neutral mission
30010 / `Seq_Game_FreePlay`, no demo, saving, popup or menu. The guarded private
[reference transfer](../private/idroid-current/replay-native-cinematic.py)
requests 10020 once with `isNoSave=true` and waits at most 45 seconds.
[Load the exact arrival pack](../private/idroid-current/load-reference-block.py),
then [replay the loaded scene](../private/idroid-current/play-loaded-reference.py)
only after observing 10020 / `Seq_Game_RescueMiller`, the owned `p31_010010`
body and neutral ownership. The replay uses no mission callback.

October 5 retained receipts establish that this route prepared the native
scene and returned with demo/menu/iDroid/Pause false. It avoids first-run Terms
and provides a real multi-shot actor/horse/iDroid reference. It does not certify
natural mission progression, the hospital, jeep or every cutscene. A live 10195
session does not satisfy its guards. Never bypass those prerequisites or retry
an ambiguous mutation. Preserve and restore the exact six user save files;
replay and `isNoSave` alone are not a substitute for that protection.

## Completion order and evidence

1. Recover the current body/hands failure, then inspect one continuous stance,
   lean, reach and walking session in both eyes. Broaden to outfits and seated/
   mounted bodies rather than declaring the first field pass complete.
2. Run live gear/VR off-on/remapping and affected established physical
   interactions on the same identified candidate.
3. Exercise the five cinematic classes above, starting with the guarded owned
   arrival reference. Record actual shot and player/control handoffs; complete
   native tutorial routes independently.
4. Run practical combat/equipment, field interaction and mounted sessions;
   then sustained whole missions, save/resume and checkpoint recovery.
5. Review the final candidate in the physical Quest, complete release checks
   and publish only when explicitly authorized.

Use bounded `tools/workspace.py bot` cases and the required `tools/build.ps1`
checks. Retain meaningful regression evidence and review both final eyes through
motion and transitions; telemetry and favorable stills cannot certify visible
correctness. Record each case as failed, pending or reviewed on its exact DLL,
native situation and effective personal bindings. Earlier footage, simulator
success and automated checks remain separate from physical-headset acceptance.

Release test inputs, close test-opened menus and clean the owned session. Restore
saves/settings and the saved Meta runtime; preserve Steam. `build/dev/` is the
checked candidate and `play/` the last successfully published release. Building,
testing, preparing a package or pushing source does not advance the play pin.
