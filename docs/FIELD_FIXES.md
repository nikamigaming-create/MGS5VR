# Field fixes — active checklist

Updated 21 October 2026. All community-reported defects are top priority.
An unchecked box means unresolved. Only check a box after its acceptance
evidence covers the reported behavior. Build/tests alone do not close a defect.
Detailed evidence and test requirements: [acceptance ledger](FREE_PLAY_ACCEPTANCE.json).
Live continuation: [CURRENT.md](CURRENT.md).

- [ ] **Y-action forced rotation — FP03.** Jeep entry/exit fixed in development6b560: retained entries at62/38 degrees and exits hold within0.03 degrees; deliberate seated look and iDroid stow/reopen retain continuity. Uses pre-Y heading and actual carrier motion. Four retained headings, driving turns, Fulton and table/sandbag climbing remain open; latest approach did not acquire vehicle.
- [ ] **Mortar operator viewpoint — FP15.** Reproduced stationary-pose mismatch fixed on33b693: old camera62.8cm above native operator eye; both aiming axes now retain native eye and active mounted body. Barrel-facing views and ordinary exit reviewed in both eyes in031244. Physical grip contact, translucent geometry, firing/impact, other emplacements and physical Quest remain open.
- [ ] **Natural mounted-weapon controls — FP11/FP15.** Implement physical grip-and-aim and stick aiming for mortars and each turret type; verify native pivots/limits, contact, release, firing and exit. Mortar action0x46 has a production adapter on a8c9ef; machine-gun action0x47 now uses the physical adapter on484c334480b4, with both-hand yaw/pitch, release, stick override and exit checked in231846/233242. Full contact visuals, firing and other types remain open. Retained040830 checks either-hand yaw, no unintended pitch command, fixed view, two-hand stick override/release and entry/trigger/exit. Earlier035529 pitch oscillation is corrected by physical-intent braking. Mortar physical pitch, full stereo contact, firing/impact and other types remain open; fixture destruction also reproduces before firing (native mount event and visible explosion in042545); its cause remains unknown.
- [ ] **Small-rock slipping and stops — FP16.** Reported; locate/repeat affected rocks, distinguish native collision/fall state from camera/body presentation.
- [ ] **Right-wrist shoulder/opposite-arm coupling — FP17.** Reported in physical VR. Reproduced: right wrist +/-30 degrees rotates chest and shifts fixed-input left palm8.5/12cm. Acquired-weapon-only torso correction removes wrist-driven heading in two Mother Base repeats. Remaining opposite-palm movement is at most 1.13cm and follows native idle animation. Physical headset and reach remain open.
- [ ] **Shortened arm reach — FP17.** Clavicle contribution and neutral head/neck fit reduce 50cm palm error to standing1.0mm/crouched1.6mm on56d5, from11.3/20.5cm. Both-eye continuity, native-scale legs, posture changes, walking, wrist isolation and supported rifle handling checked. Cross-head reach camera freeze fixed on144735d34e49: both-arm reach/neutral-return200406 retains fresh6-12ms rig samples. Cross-body segment clearance now checks the complete upper arm and forearm on bb66ebf47b0b (220859); reduced crowding in both eyes. Broader interactions and physical Quest validation stay open.
- [ ] **Precision-weapon support loss — FP02.** Moving supported rifle and3 back-slot shots retain contact in082239, but open support fingers reproduced in both eyes. Read-only shared palettes match; candidate73fe7d0b521d cups acquired long-gun support outside native manipulation. Curled fingers now visible after moving aim and3 shots; native reload5->9 and reacquisition observed. Reload contact loss subsequently reproduced in021519 and fixed onaaffbee70236: moving aim, three shots, native reload5->9, return and release pass in023957. Reload motion reviewed in native source video and return in both final eyes. Bolt-action, stereo reload motion and physical headset remain open.
- [ ] **Tutorial camera handoff and retained iDroid — FP07/FP12.** Exact retained-terminal timeout, cinematic return and ordinary stow/reopen pass onff0dd2c746a6 after fixing reproduced ownership loss. Challenge Tasks exit-notice camera retirement is also fixed on 428a0c0872fc: readable spatial notice in both eyes and immediate reopen/stow in064303. Physical Quest, alignment and remaining tutorial lessons stay open.
- [ ] **Idle/tracking-loss camera and weapon continuity — FP01/FP02.** Camera fallback not reproduced in simulator. Rifle disappearance reproduced in both eyes during tracking loss. Combined candidate 148d5ca51161 built/installed; 58 test groups and installer checks passed; scoped field loss/recovery and safe held-trigger checks passed on both primary slots (runs023544/023824/024003/024113 below). Other transitions and physical Quest remain open.
- [ ] **Floating in seated and standing modes — FP05.** Reproduced helicopter-exit latch fixed: initial crouched camera formerly1.18/1.369m above root versus native eye~0.76m; two fresh corrected exits0.775/0.788m. Standing/crouch/prone/walk retain native leg scale. Physical seated/standing report remains open.
- [ ] **Body size and tight-corridor sticking — FP04.** Exact reported corridor unresolved; compare full-body on/off, capsule movement and physical lean without weakening collision.
- [ ] **Natural cover and native benefits — FP04.** Reproduced automatic aim-out fixed in8e630: stationary-head camera position unchanged with native readiness and cover mode2; guarded3-round discharge retains cover/root position; physical lean/up/return and exit checked. Lean-right shows translucent body intrusion in both eyes. Off/immersive toggle now releases native wall attachment in standing and crouch and permits re-entry on304fe91f6b9c. Standing-cover control classification/readiness is corrected on3d64af394fb3 (084333/084404/084432), but those missing-gun views used a lethal loadout under native Mother Base KILLING_WEAPON restriction. Same-build Afghanistan comparison193143 switches and renders guns in both eyes; eligible crouched-barrier rifle presentation and lean/peek/return are now checked in212627; broader surfaces/postures, non-VR and headset remain open. Preserve native benefits only when physical crouch/lean/occlusion qualify, including ordinary geometry/posture effects with assistance off.
- [ ] **Helicopter/transit body presentation — FP06.** Open; verify body, view and hand continuity through native transit.
- [ ] **Handheld iDroid — FP07.** Left bionic hand; native contact/projector alignment, readable stereo screen, ordinary/forced stow and immediate reopening. Native projector endpoint fitting is enabled in development44f0 after reproducing a roughly20cm endpoint mismatch; front/turned stereo views and ordinary stow checked. Physical orientation, forced tutorial lifecycle and wider configuration acceptance remain open.
- [ ] **Rocket arm — FP08.** Left-hand steering and missing flight handset fixed on fb83026e1682. Native trajectory, flight pixels in both eyes, stick override, weapon return and iDroid reopen/stow checked in060632/061327. Target impact, Blast Arm, wider contact/poses and physical Quest remain open.
- [ ] **Cabin paper/pen — FP09.** Marker pickup now seats its barrel in the palm. Combined run013111 completes keyboard caption, freehand ink and board pinning, visible in both eyes on371a. Shorter wrist guidance clears the sleeve in the reproduced camera-hold view on6c9889 (run013931, both eyes). Paper-edge pinch and both-hand carrying now meet visibly on58d2; combined typing, writing and pinning pass in020748. Marker shaft/pinch contact is corrected on e64919 and both-hand writing reviewed in both eyes in045333. Wider hand orientations and prompt readability remain open.
- [ ] **Cabin gallery/workbench/cassettes — FP10.** Front-slot print presentation fixed and visible in both eyes on371a; new copy remains until taken/replaced. Combined cassette handling, both-hand camera use and note editor completed. Flat-sheet stacking and field-paper ray corrected in source; loose sheets now settle flat instead of balancing on edges (both-eye combined run045333). Wider stacking acceptance and cassette presentation remain open.
- [ ] **ACC loadout holsters — FP11.** Primary rifle physical pickup, translation/rotation and native mount return implemented on0d935edd7324; both hands and final eyes reviewed in203751/204105. Second parked rifle now carries/returns with either hand through a center-aisle approach (210545/210826); ordinary iDroid open/stow and right reacquisition210651 pass. First-rifle right-hand tracking interruption return and fresh-grip recovery checked in222214 on bb66ebf47b0b. First-rifle left-hand loss/recovery and right-hand menu interruption now checked in015418/015756 on1b64aed9530e. Other loadouts, second-rifle interruptions and physical Quest remain open.
- [x] **ACC free-roam destination entry — FP13.** Resolved route: Missions -> Mission List -> Free Roam. Existing VR bindings reach all three destination entries; Afghanistan selection, landing zone, sortie, flight and on-foot exit verified on c2b3. No unlock/save edits or forced visibility. Controls guide corrected; handset alignment remains FP07.
- [x] **Headless sortie preview — FP14.** Restored in development: complete head in both eyes on native sortie screens; clear first-person return after departure. Runs 134958388735Z and 135550994795Z.
- [ ] **Remaining mission/menu/weapon/control regressions — FP12.** Reconcile existing catalogs by exact build; retain explicit untested combinations. Native binocular intel now verified on484c334480b4 (001704): corrected physical hand aim produces accepted=1 and advances the lesson. The old inspection pose missed the target volume; no production change. Zoom and waypoint creation observed, but authored village marking completion remains open.

## Current execution checkpoint

Current checked development is `aaffbee70236`, adding stereo shoulder clearance to confirmed machine-gun physical aiming to full arm-segment head clearance to ACC primary-rifle pickup/carry/return and the cross-head reach freeze correction to native cover-assistance entry/exit, automatic ledge-heading continuity and tutorial exit-notice handoff, Rocket Arm steering and restored flight handset to seated helicopter body/eye continuity, marker pinch contact, loose-sheet settling and prior fixes. Physical pitch, contact, discharge and other mounted types remain open.
Prior gameplay-checked `6b560649c200` includes the jeep heading correction and helicopter-exit height and head/neck reach corrections plus the sortie-head, support-finger and bare-hand
wrist/torso corrections summarized in CURRENT.md. FP13's native free-roam route
was verified through on-foot arrival on 21 October; the earlier missing root-row
investigation did not identify the Mission List category. Guidance now describes
that route. The complete field list above remains the work queue.

- [x] **This pass: reproduced barrier automatic aim-out.** Native cover mode2 and
  camera position are retained while raising the rifle; guarded firing expends
  three rounds without moving the actor, and ordinary cover exit works. Runs
  230508/231018/231215/231251 retained. This does not close the broader FP04 item.
- [x] **This pass: full-body camera through Mother Base cover wall.** Preserve
  native wall-relative eye placement instead of rotating it toward view yaw.
  Both-eye wall visibility, lateral lean, upward peek, return and walking exit
  checked on4060b; runs234550/234616/234646 retained. Right-shoulder intrusion,
  original Afghan translucent figure, wider cover and physical Quest stay open.
- [x] **This pass: crouched cover exit through wall.** Native cover clears before
  its crouched animation settles. Preserve the native low-posture eye footprint
  through that transition. Both eyes retain the wall after exit and through
  left/right50cm reach on4447; retained20261022 runs002713/002733. Earlier
  d731 exit/reach failed visually despite passing input predicates.
- [x] **This pass: shoulder reach enters eye area during cover lean.** Constrain
  the clavicle's shoulder endpoint while retaining the hand target and native
  bone lengths. Near-eye sleeve intrusion is reduced; arms remain visible.
  Retained baseline235748 and final002654 joint samples and both-eye captures.
  This does not identify the Afghan translucent figure or close broader FP04.

- [x] **This pass: paper floats away from the holding fingers.** Fit thumb/index
  articulation and the paper-edge contact together. Both-hand note views
  reviewed in both eyes on58d2; combined typing, freehand writing and board
  pinning complete in020748. Broader FP09 and physical Quest remain open.

- [x] **This pass: stationary mortar retains standing camera and drops full body.**
  Native STAND remains set while the operator bends. Follow its current authored
  eye and preserve native mounted pelvis/legs. Baseline023132 had62.8cm vertical
  error and rejected the full body; final031244 has matching anchor/eye through
  both axes and active mounted body. Barrel-facing and exit pairs reviewed.
  This does not close physical grip aiming, firing/impact or broader FP15.

- [x] **This pass: marker buried in the palm.** Fit its socket and thumb/index articulation together. Both-hand writing completes and both final eyes reviewed in retained045333. Wider orientations and physical Quest remain open.
- [x] **This pass: loose sheets balanced indefinitely on edges.** Settle toward the nearer face while preserving heading and support. Ten previously tilted sheets now stay flat with no displacement across the combined workbench run; both-eye seat views reviewed. Final-build restart050252 restores all32 papers, including the10 corrected sheets flat.

- [x] **This pass: torso and legs disappear during helicopter transit.** Admit the original native skin eye and keep the seated lower body attached to the carrier. The anchor follows the boarding-to-seat pose change without following small idle head movements. Both-eye seated and post-disembark body views reviewed on c412aa, retained052947/053027/053103. Extraction boarding and physical Quest remain open.

- [x] **This batch: left-hand motion did not steer Rocket Arm.** Physical pitch/yaw now change native trajectory; stick override and normal weapon return checked.
- [x] **This batch: Rocket Arm handset missing after sortie selection.** Flight ownership now survives the unrelated terminal closing flag. Live flight pixels reviewed in both eyes and ordinary iDroid reopen/stow retained. Broader FP08 remains open.

- [x] **This pass: Exit Tutorial freezes the old handset view.** Recover the immediately following paused notice using the same owned menu anchor; no stale skin is admitted as gameplay. Readable spatial notice reviewed in both eyes, dismissal and immediate iDroid reopening/stow pass in064303.

- [x] **This pass: automatic ledge catch turns the fixed headset view.** Baseline turns approximately180 degrees. Fresh normal-to-native-action heading capture retains the VR view through catch and ordinary Y climb-back; view-relative walking resumes. Both-eye held/return views reviewed on829065, retained075341/075452/075558/075640. Other contextual actions and physical Quest remain open.

- [x] **This batch: cover assistance Off leaves Snake attached.** Use native exit cleanup together with entry prevention. Standing/crouched Off clears mode2 to0 and restores normal action; On restores entry; wall collision and walking away retained. Both Off eye pairs reviewed. Retained082101/082139/082220/082303/082826/082855/082935. Attached-cover ready-weapon checks failed and remain open; this does not close FP04.

- [x] **This batch: standing cover incorrectly uses stationary-weapon controls.** Native BEHIND now retains on-foot VR control and weapon-component admission. Actual standing-cover mode/ready checks, lean/up/return, crouch/stand and Off/walk pass on3d64af394fb3. Final gun and right-hand presentation remain unaccepted; the gun is also absent in retained outside-cover085521. This fixes the control gate, not the whole weapon-presentation report.

## Closure requirements

- [ ] Reproduce each reported defect in the actual game before altering a working flow.
- [ ] Verify the changed interaction and adjacent transitions in seated/standing and applicable body/input modes.
- [ ] Review both final eyes and motion; retain exact build/run evidence and failures.
- [ ] Obtain physical Quest Link evidence for physical-headset claims; Snakebot does not substitute for it.
- [ ] Release test inputs, restore test poses and close test-opened menus.
- [ ] Preserve published play build, personal bindings/settings, saves and established VR interactions.

FP14 is fixed in development and FP13 routing is resolved; all unchecked defects remain open. Keep this checklist and the JSON ledger in
sync as evidence changes; retain the full scope across continuation turns.

### Weapon restriction correction, 22 October

The Mother Base missing-gun investigation was a restricted-loadout comparison, not proof of broken weapon publication. Retained native flags524289 include KILLING_WEAPON (0x80000); all three saved definitions use lethal ammunition. On the same unchanged candidate3d64af394fb3, native Afghanistan sortie193143 clears that restriction, switches resources 93476 ->159074 ->27873, and renders the rifle and pistol in both final eyes. No loadout/save edits or visibility override. Do not patch native weapon restrictions or repeat Mother Base lethal-weapon selection as a rendering test. Cover weapon presentation still requires an eligible weapon/context; this does not close broader FP04/FP02.

- [x] **This pass: cross-head reach freezes the view.** Cross-head reach freeze fixed on144735d34e49 (source6da316e). Captured ACC baseline195850 rejects the right elbow head-clearance solve and stops fresh rig publication. If the requested wrist has no feasible elbow bend, limit the reach at the head/bone sphere intersections while preserving both native bone lengths and wrist orientation. Combined200406 right/left cross-head and neutral-return cases retain active camera and fresh6/12/9/11ms rig samples; both final eyes reviewed. Close sleeve/forearm crowding remains visible, so broader FP17 and physical Quest remain open. Adjacent200731 closes Choose a Rival help, stows, reopens and turns the left iDroid; its last stow step times out at a Daily Bonus notice. Cleanup then closes the terminal, confirmed in200843. Do not call the entire combined suite a pass. No ACC pickup implementation. Owned sessions closed; published play unchanged.

- [x] **This batch: physical ACC primary-rifle carry and return.** ACC primary rifle pickup/carry/return implemented on0d935edd7324 (source2018df5). Touch the actual visible receiver and squeeze either hand; the complete native assembly follows that rendered palm and returns to its original mount on release. The initial a3e760 candidate acquired the grip but the later native equipment update restored the parked pose; the verified native assembly publication hook corrects that overwrite. Right-hand translation/20-degree turn203751 and left-hand translation/turn204105 show the complete rifle in both final eyes. Read-only final census restores assembly0 to[-0.728,1198.420044,0.1975] and leaves assembly1/hidden assembly2 unchanged. No copied model, inventory/save change or visibility override. Earlier204813 second-rifle approach remained0.510m short; the center-aisle follow-up below now verifies both-hand pickup. Earlier204231 operator timeout occurred before grip; the game stayed fresh and204436 resumed from the confirmed pose. Other loadouts, menu/tracking interruptions and physical Quest remain open. Owned sessions closed; published play unchanged.


### Second ACC rifle approach, 22 October

Second ACC rifle carry/return verified on unchanged0d935edd7324: use the center aisle, then face and reach the receiver. Right-hand translation/turn210545 and left-hand translation/turn210826 show the native assembly in both final eyes. Combined210651 opens/stows iDroid and reacquires with the right hand. Its first left-hand cross-body attempt did not acquire despite the physical-input predicate passing; facing the receiver resolves contact. Final read-only census restores both original mounts and leaves hidden assembly2 unchanged. The earlier straight/low approaches stopped at native clearance; no collision, arm-length or contact-radius change was needed. Other loadouts, held-menu/tracking interruptions and physical Quest remain open. Owned sessions closed; published play unchanged.


### Eligible field cover, 22 October

Eligible Afghanistan cover checked on unchanged0d935edd7324. Retained211522 completes native landing and shows all three saved guns in both eyes outside cover. At the roadside concrete barrier near[151.884,329.196,51.683], standing contact does not attach; crouched entry212552 sets native BEHIND. Combined212627 shows the rifle in both eyes while ready, with native root unchanged through center,22cm right lean,25cm upward peek and return. The previous Mother Base missing-gun views are not reproduced in this eligible context. This is scoped visibility/pose evidence, not continuous-video or physical-headset acceptance. The planned exit212728 was rejected before input because BEHIND had already cleared. Ordinary rifle readiness212827 passes; later re-entry212935 does not attach. The intervening native transition/movement remains unexplained; no exit success or collision defect is inferred. No production changes or rebuild were needed. Owned game/simulator closed; published play unchanged.


- [x] **This batch: forearm enters the head envelope between clear joints.** Cross-body arm-segment clearance fixed on bb66ebf47b0b. Baseline220020 has right/left forearms only10.2/10.6cm from the eye midpoint despite joint-only clearance. Check both complete bone segments and constrain the fallback direction at the head-sphere tangent, preserving native lengths and wrist orientation. First candidate220527 still penetrates because boundary rounding skips the tangent constraint; retained as a failed candidate. Corrected220859 samples12.0/12.4cm minimum, with reduced sleeve crowding in both final eyes, active camera, both neutral returns and ordinary left iDroid open/stow. The handset is viewed obliquely here; no new projector-readability claim. These are bounded simulator poses, not continuous-motion or physical-headset acceptance. Broader FP17 stays open. Owned sessions closed; published play unchanged.


### Alternate mounted fixture, 22 October

On unchanged bb66ebf47b0b, direct native route223742 reaches the second authored
machinegun at[72.7759,333.7447,84.7871]. Both final eyes224040 show the native
gun and rear handles. Entry attempts never establish a mounted action; the
retained224339 pre-action view already lacks the gun, and backward movement
224649 does not restore it. The last view shows nearby enemy gunfire. Native
lookup identifies locator hw03_gim_n0001|srt_hw03_tpod0_def_v00 as type82,
ID41992, broken=false; this does not prove it remains rendered or unrecovered.
The disappearance cause is unconfirmed. Do not repeat entry or change
visibility/action admission based on this attempt. Static action writes0x45
and0x47 remain semantically unconfirmed;0x44 is searchlight. No production
change, rebuild, mounted-controls delivery or broad checklist closure. Owned
game/simulator closed, no background bot/watchers, published play unchanged.


### Machine-gun physical aiming, 22 October

Machine-gun physical aiming enabled on484c334480b4. Native OnPlayerMachineGun(0,41992) and the live action table identify action0x47; baseline225829 has104 mounted samples without the physical adapter. Admit that confirmed action alongside mortar0x46. Retained231846 verifies right/left six-degree yaw requests settle at5.36/5.42 degrees, neutral pitch and unchanged view orientation. Retained233242 verifies right/left pitch5.39/5.40 degrees with unchanged view, both-grip stick override, neutral release and ordinary exit. Deliberate stick input changes native aim and view by3.25 degrees; only physical-hand aiming is claimed to hold the view fixed. Right yaw contact reviewed in both final eyes; left yaw contact reviewed in the recorded native right-eye source, not final composition. Pitch/both-hand final-eye pairs look away from the handles because the diagnostic reads native aiming yaw before entry settles; they do not close full contact visibility. Firing/impact, wider poses, Gatling/other emplacements and physical Quest remain open. Fresh native BreakGimmick identifies actual fixture destruction in225829; the responsible attacker is unconfirmed. After native BreakGimmick, Gimmick.IsBrokenGimmick(...,1) is true while the default query remains false; default false must not be used to declare a live fixture intact. Owned sessions closed, temporary procedure removed, published play unchanged.

- [x] **This pass: machine-gun physical grip/aim was not admitted.** Confirmed native action0x47 now uses authored hand contacts and native aiming controls. Both-hand yaw/pitch, neutral release, both-grip stick override and exit checked on484c334480b4 in231846/233242. Full contact visibility, firing/impact, Gatling and physical Quest remain open.

### Current cover batch, 23 October

Current cover batch on unchanged 484c334480b4: retained 20261023T003608337759Z checks crouched native cover, 22cm right lean, 25cm upward peek, rifle readiness and return at the roadside barrier. Both final eyes reviewed for lean, peek and ready rifle; no overhead translucent silhouette appears at this fixture. Native root remains fixed and full-body fit stays valid. Retained 20261023T003827007826Z exits cover and returns to native standing. The old 20261021T231018067219Z silhouette occurred at a different distant fixture and remains unresolved; this comparison does not establish its repair. No production changes or rebuild. Owned game/simulator and runners are closed; published play unchanged.

### Original barrier follow-up, 23 October

Original Afghan barrier revisited on unchanged 484c334480b4. Retained 20261023T011152744090Z repeats center rifle-ready/release, right lean, upward peek and return at [1636.700,323.969,1030.177], about1.3m along the wall from the old sample. Native BEHIND and root remain stable through all four phases. Rifle-ready and post-release lean reviewed in both final eyes: the old overhead translucent figure does not recur, but the right sleeve crowds the right-eye lean view with head_contacts=0. No solver change is justified until native sleeve geometry and per-eye clearance are measured. The old figure visually differs from Snake, making a nearby NPC a hypothesis, not confirmed identity. Read-only nearby-actors.jsonl is retained. Subsequent 20261023T011325074947Z dispatches no cover sequence because BEHIND is already false; nearby alerted soldiers and player displacement are recorded, without assigning causality. No production fix or rebuild. Owned game/simulator and read-only watcher closed; Steam and published play retained.

### Stereo shoulder clearance, 23 October

Stereo shoulder-clearance correction implemented on449707db6634. Retained native cover samples from002654 show a shoulder18.0cm from the eye midpoint but only15.4cm from the right eye. The clavicle constraint now uses the capsule joining the current runtime eye positions, retaining its18cm radius and native bone lengths; wrist targets and weapon sockets are unchanged. Build and required checks pass. Combined ACC run20261023T012345674933Z completes both-arm cross-head reach/return, both22cm lean/return directions and ordinary left iDroid open/stow. Sampled minimum shoulder-to-eye distance is18.38cm. Both final eyes reviewed for both cross-head poses, right lean and iDroid; the extreme right cross-head pose still crowds the view, and the handset is oblique. Do not claim full rig or projector readability acceptance. Exact crouched-cover visual improvement and physical Quest remain open. Owned game/simulator and bounded bone reader closed; published play unchanged.


### Stereo wrist and arm clearance, 23 October

Stereo limb clearance corrected on1b64aed9530e. The previous shoulder-only stereo fix left wrist and complete arm-segment checks centered between the eyes. Retained012345 samples show right/left arm-to-eye minima9.13/11.12cm despite the12cm rule. Wrist targets and bone contacts now use the actual eye capsule; only infeasible reaches use its containing sphere, retaining native bone lengths. Required build checks pass. Retained014423 checks both cross-head reaches and neutral returns; sampled minima improve to13.44/14.61cm. Both cross-head final-eye pairs reviewed; extreme right-arm sleeve crowding remains visible, so broad FP17 is not closed. Operator timeout interrupts the following lean case, with cleanup initially unable to release. Fresh014544 observation confirms neutral head and active camera;014622 completes both lean/return directions and ordinary left iDroid open/stow. Both lean pairs and oblique iDroid pair reviewed; no projector-readability claim. Owned game/simulator and bounded reader closed. Published play unchanged; physical Quest and original cover visual acceptance remain open.


### Cabin interruption follow-up, 23 October

ACC carry interruption gaps checked on unchanged1b64aed9530e. Retained015418 verifies first-rifle left-hand carry, tracking loss(mask3), exact native mount return, recovery at the receiver with squeeze still held, no pickup on subsequent held-hand movement, fresh-squeeze reacquisition and final return. Root joins are within11ms; before-loss and held-recovery movement reviewed in both final eyes. Retained015756 verifies right-hand carry, ordinary Menu opening while grip remains held, native mount return and ordinary stow; carry/return reviewed in both final eyes. Effective Menu+left-grip recenter chord is preserved and not substituted. Native Choose a Rival notice reviewed and closed before the menu case. No production change or rebuild; this is closure of two scoped verification gaps, not a new defect fix or whole FP11 acceptance. Other loadouts, second-rifle interruptions and physical Quest remain open. Owned game/simulator and bounded readers closed; temporary dispatch removed; published play unchanged.


### Field firing and reload batch, 23 October

Machine-gun discharge verified on unchanged1b64aed9530e before the reload fix. Retained021053 mounts the exact second slopedWest type82 fixture, sends RT, releases and exits; live broken(...,1) remains false before/after and native source-eye video shows a muzzle flash. Both final mounted views reviewed. The immediate exit snapshot precedes animation completion; later read-only native observation restores NORMAL_ACTION at[73.438,334.558,83.962]. Impact and full contact/headset/other-emplacement acceptance remain open. Route020845 arrived without an interruption; this does not reproduce or close the unspecified rock/corridor reports. No mounted production change was required.

- [x] **This batch: acquired rifle support drops during native reload.** Supported-rifle reload loss fixed onaaffbee70236. Baseline021519 loses acquired support0.484s into native reload with both physical grips held and unchanged weapon identity. Native magazine-hand motion was overwriting the acquired contact, and missing reload muzzle publication dropped firing-wrist calibration. Preserve the acquired distance-check offset and same-weapon local barrel calibration for the primary wrist; leave native reload animation and two-hand guidance policy intact. Final023957 passes moving aim, three native back-slot discharges, reload5->9 and return, with support retained in all17 reload samples including14 native manipulation samples. Native source-eye sequence reviewed for magazine-hand motion; both final post-reload eyes reviewed. Final observation has zero physical inputs and support detached after ordinary release. Required build/installer checks pass. The intermediatefa894 candidate was narrowed before acceptance so cached calibration cannot enable animation-driven two-hand aiming. No whole FP02, bolt-action or physical Quest claim. Owned sessions closed; temporary test dispatch removed; published play unchanged.


### Sleeve geometry diagnosis, 23 October

Fresh unchanged aaffbee70236 combined run025053 completes both cross-head reaches/neutral returns, both lean/return directions and ordinary left iDroid open/stow. Right cross-head sleeve crowding remains visible in both final eyes. Read-only full-palette samples identify all14 named garment correction joints with expected parents; their sampled right-cross-head centers remain at least16.05cm from either eye. This does not measure skinned vertices or prove correct garment deformation. Next diagnosis must inspect the rendered garment envelope/weights, not infer a missing helper or enlarge bone clearance from the picture alone. Samples are non-atomic with observation joins limited to100ms. No production change, rebuild or defect closure. Owned game/simulator and reader closed; published play unchanged.


### Stable garment binding check, 23 October

Unchanged aaffbee70236 run030337 confirms live model code84a033c711e29aed exactly matches the owned sna5_main0_def.fmdl path hash. Both reaches/returns, both leans/returns and iDroid open/stow complete; cross-head sleeve crowding remains in both reviewed final eyes.1550 whole-palette samples passed repeated palette/root equality; the suspected helper jump with nearly stationary arm joints does not recur. The earlier bone-by-bone transition sample does not justify changing corrective rotations. Neither topology nor this check proves a skin deformation bug; crowding remains a visual acceptance concern. No production fix or rebuild. Stop repeating this cross-head batch by default; resume remaining gameplay interaction work unless new garment evidence identifies a defect. Owned session and reader closed; published play unchanged.
