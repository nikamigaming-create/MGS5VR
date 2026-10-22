# Field fixes — active checklist

Updated 21 October 2026. All community-reported defects are top priority.
An unchecked box means unresolved. Only check a box after its acceptance
evidence covers the reported behavior. Build/tests alone do not close a defect.
Detailed evidence and test requirements: [acceptance ledger](FREE_PLAY_ACCEPTANCE.json).
Live continuation: [CURRENT.md](CURRENT.md).

- [ ] **Y-action forced rotation — FP03.** Jeep entry/exit fixed in development6b560: retained entries at62/38 degrees and exits hold within0.03 degrees; deliberate seated look and iDroid stow/reopen retain continuity. Uses pre-Y heading and actual carrier motion. Four retained headings, driving turns, Fulton and table/sandbag climbing remain open; latest approach did not acquire vehicle.
- [ ] **Mortar operator viewpoint — FP15.** Reported inside the emplacement; establish native operator position and usable view of aiming/firing in both eyes.
- [ ] **Natural mounted-weapon controls — FP11/FP15.** Implement physical grip-and-aim and stick aiming for mortars and each turret type; verify native pivots/limits, contact, release, firing and exit. MountedGrip helper exists but has no production adapter.
- [ ] **Small-rock slipping and stops — FP16.** Reported; locate/repeat affected rocks, distinguish native collision/fall state from camera/body presentation.
- [ ] **Right-wrist shoulder/opposite-arm coupling — FP17.** Reported in physical VR. Reproduced: right wrist +/-30 degrees rotates chest and shifts fixed-input left palm8.5/12cm. Acquired-weapon-only torso correction removes wrist-driven heading in two Mother Base repeats. Remaining opposite-palm movement is at most 1.13cm and follows native idle animation. Physical headset and reach remain open.
- [ ] **Shortened arm reach — FP17.** Clavicle contribution and neutral head/neck fit reduce 50cm palm error to standing1.0mm/crouched1.6mm on56d5, from11.3/20.5cm. Both-eye continuity, native-scale legs, posture changes, walking, wrist isolation and supported rifle handling checked. Farther reach, broader interactions and physical Quest validation stay open.
- [ ] **Precision-weapon support loss — FP02.** Moving supported rifle and3 back-slot shots retain contact in082239, but open support fingers reproduced in both eyes. Read-only shared palettes match; candidate73fe7d0b521d cups acquired long-gun support outside native manipulation. Curled fingers now visible after moving aim and3 shots; native reload5->9 and reacquisition observed. Mid-reload pair capture failed on grip lease; broader verification remains open. Bolt-action and physical headset remain open.
- [ ] **Tutorial camera handoff and retained iDroid — FP07/FP12.** Exact retained-terminal timeout, cinematic return and ordinary stow/reopen pass onff0dd2c746a6 after fixing reproduced ownership loss. Scoped simulator subcase complete; physical Quest, alignment and remaining tutorial lessons stay open.
- [ ] **Idle/tracking-loss camera and weapon continuity — FP01/FP02.** Camera fallback not reproduced in simulator. Rifle disappearance reproduced in both eyes during tracking loss. Combined candidate 148d5ca51161 built/installed; 58 test groups and installer checks passed; scoped field loss/recovery and safe held-trigger checks passed on both primary slots (runs023544/023824/024003/024113 below). Other transitions and physical Quest remain open.
- [ ] **Floating in seated and standing modes — FP05.** Reproduced helicopter-exit latch fixed: initial crouched camera formerly1.18/1.369m above root versus native eye~0.76m; two fresh corrected exits0.775/0.788m. Standing/crouch/prone/walk retain native leg scale. Physical seated/standing report remains open.
- [ ] **Body size and tight-corridor sticking — FP04.** Exact reported corridor unresolved; compare full-body on/off, capsule movement and physical lean without weakening collision.
- [ ] **Natural cover and native benefits — FP04.** Reproduced automatic aim-out fixed in8e630: stationary-head camera position unchanged with native readiness and cover mode2; guarded3-round discharge retains cover/root position; physical lean/up/return and exit checked. Lean-right shows translucent body intrusion in both eyes. Off/immersive toggle, broader surfaces/postures, non-VR and headset remain open. Preserve native benefits only when physical crouch/lean/occlusion qualify, including ordinary geometry/posture effects with assistance off.
- [ ] **Helicopter/transit body presentation — FP06.** Open; verify body, view and hand continuity through native transit.
- [ ] **Handheld iDroid — FP07.** Left bionic hand; native contact/projector alignment, readable stereo screen, ordinary/forced stow and immediate reopening. Native projector endpoint fitting is enabled in development44f0 after reproducing a roughly20cm endpoint mismatch; front/turned stereo views and ordinary stow checked. Physical orientation, forced tutorial lifecycle and wider configuration acceptance remain open.
- [ ] **Rocket arm — FP08.** Native flight/live view and left-arm steering remain unverified.
- [ ] **Cabin paper/pen — FP09.** Marker pickup now seats its barrel in the palm. Combined run013111 completes keyboard caption, freehand ink and board pinning, visible in both eyes on371a. Shorter wrist guidance clears the sleeve in the reproduced camera-hold view on6c9889 (run013931, both eyes). Paper-edge pinch and both-hand carrying now meet visibly on58d2; combined typing, writing and pinning pass in020748. Exact pen contact, wider hand orientations and prompt readability remain open.
- [ ] **Cabin gallery/workbench/cassettes — FP10.** Front-slot print presentation fixed and visible in both eyes on371a; new copy remains until taken/replaced. Combined cassette handling, both-hand camera use and note editor completed. Flat-sheet stacking and field-paper ray corrected in source; their wider live acceptance and cassette presentation remain open.
- [ ] **ACC loadout holsters — FP11.** Native physical interaction and live acceptance incomplete.
- [x] **ACC free-roam destination entry — FP13.** Resolved route: Missions -> Mission List -> Free Roam. Existing VR bindings reach all three destination entries; Afghanistan selection, landing zone, sortie, flight and on-foot exit verified on c2b3. No unlock/save edits or forced visibility. Controls guide corrected; handset alignment remains FP07.
- [x] **Headless sortie preview — FP14.** Restored in development: complete head in both eyes on native sortie screens; clear first-person return after departure. Runs 134958388735Z and 135550994795Z.
- [ ] **Remaining mission/menu/weapon/control regressions — FP12.** Reconcile existing catalogs by exact build; retain explicit untested combinations.

## Current execution checkpoint

Current checked development is `58d2f6882464`, adding native-chain paper pinch/contact to the cabin batch.
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

## Closure requirements

- [ ] Reproduce each reported defect in the actual game before altering a working flow.
- [ ] Verify the changed interaction and adjacent transitions in seated/standing and applicable body/input modes.
- [ ] Review both final eyes and motion; retain exact build/run evidence and failures.
- [ ] Obtain physical Quest Link evidence for physical-headset claims; Snakebot does not substitute for it.
- [ ] Release test inputs, restore test poses and close test-opened menus.
- [ ] Preserve published play build, personal bindings/settings, saves and established VR interactions.

FP14 is fixed in development and FP13 routing is resolved; all unchecked defects remain open. Keep this checklist and the JSON ledger in
sync as evidence changes; retain the full scope across continuation turns.
