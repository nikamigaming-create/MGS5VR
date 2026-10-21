- [ ] **Floating in seated and standing modes — FP05.** Reproduced helicopter-exit latch fixed: initial crouched camera formerly1.18/1.369m above root versus native eye~0.76m; two fresh corrected exits0.775/0.788m. Standing/crouch/prone/walk retain native leg scale. Physical seated/standing report remains open.

Updated 21 October 2026. All community-reported defects are top priority.
An unchecked box means unresolved. Only check a box after its acceptance
evidence covers the reported behavior. Build/tests alone do not close a defect.
Detailed evidence and test requirements: [acceptance ledger](FREE_PLAY_ACCEPTANCE.json).
Live continuation: [CURRENT.md](CURRENT.md).

- [ ] **Y-action forced rotation — FP03.** Fulton, vehicle entry, table and sandbag climbing. Reported; compare four starting headings and preserve physical view direction through entry/action/exit.
- [ ] **Mortar operator viewpoint — FP15.** Reported inside the emplacement; establish native operator position and usable view of aiming/firing in both eyes.
- [ ] **Natural mounted-weapon controls — FP11/FP15.** Implement physical grip-and-aim and stick aiming for mortars and each turret type; verify native pivots/limits, contact, release, firing and exit. MountedGrip helper exists but has no production adapter.
- [ ] **Small-rock slipping and stops — FP16.** Reported; locate/repeat affected rocks, distinguish native collision/fall state from camera/body presentation.
- [ ] **Right-wrist shoulder/opposite-arm coupling — FP17.** Reported in physical VR. Reproduced: right wrist +/-30 degrees rotates chest and shifts fixed-input left palm8.5/12cm. Acquired-weapon-only torso correction removes wrist-driven heading in two Mother Base repeats. Remaining opposite-palm movement is at most 1.13cm and follows native idle animation. Physical headset and reach remain open.
- [ ] **Shortened arm reach — FP17.** Native clavicle contribution restored in development ca16. At50cm tracked reach, standing palm error fell11.3cm ->1.1mm and crouched20.5cm ->5.4cm. Both-eye arm continuity, posture transitions, short walk and supported rifle handling checked. Remaining crouched gap, farther reach and physical Quest validation stay open. Two earlier pelvis-fit regressions were removed.
- [ ] **Precision-weapon support loss — FP02.** Moving supported rifle and3 back-slot shots retain contact in082239, but open support fingers reproduced in both eyes. Read-only shared palettes match; candidate73fe7d0b521d cups acquired long-gun support outside native manipulation. Curled fingers now visible after moving aim and3 shots; native reload5->9 and reacquisition observed. Mid-reload pair capture failed on grip lease; broader verification remains open. Bolt-action and physical headset remain open.
- [ ] **Tutorial camera handoff and retained iDroid — FP07/FP12.** Exact retained-terminal timeout, cinematic return and ordinary stow/reopen pass onff0dd2c746a6 after fixing reproduced ownership loss. Scoped simulator subcase complete; physical Quest, alignment and remaining tutorial lessons stay open.
- [ ] **Idle/tracking-loss camera and weapon continuity — FP01/FP02.** Camera fallback not reproduced in simulator. Rifle disappearance reproduced in both eyes during tracking loss. Combined candidate 148d5ca51161 built/installed; 58 test groups and installer checks passed; scoped field loss/recovery and safe held-trigger checks passed on both primary slots (runs023544/023824/024003/024113 below). Other transitions and physical Quest remain open.
- [ ] **Floating in seated and standing modes — FP05.** Earlier candidate changes and simulator posture checks exist; reported physical behavior not closed.
- [ ] **Body size and tight-corridor sticking — FP04.** Exact reported corridor unresolved; compare full-body on/off, capsule movement and physical lean without weakening collision.
- [ ] **Natural cover and native benefits — FP04.** Automatic aim-out moves actual actor/view ~0.924m: reproduced. Off/immersive toggle required; no automatic aim-out movement in either mode. Preserve native benefits when physical crouch/lean/occlusion qualify, including ordinary geometry/posture effects with assistance off.
- [ ] **Helicopter/transit body presentation — FP06.** Open; verify body, view and hand continuity through native transit.
- [ ] **Handheld iDroid — FP07.** Left bionic hand; native contact/projector alignment, readable stereo screen, ordinary/forced stow and immediate reopening. Edge-on presentation remains under investigation.
- [ ] **Rocket arm — FP08.** Native flight/live view and left-arm steering remain unverified.
- [ ] **Cabin paper/pen — FP09.** Grip/contact, writing/pinning and prompt clipping remain open.
- [ ] **Cabin gallery/workbench/cassettes — FP10.** Note editor, printing/stacking and cassette presentation remain open.
- [ ] **ACC loadout holsters — FP11.** Native physical interaction and live acceptance incomplete.
- [x] **ACC free-roam destination entry — FP13.** Resolved route: Missions -> Mission List -> Free Roam. Existing VR bindings reach all three destination entries; Afghanistan selection, landing zone, sortie, flight and on-foot exit verified on c2b3. No unlock/save edits or forced visibility. Controls guide corrected; handset alignment remains FP07.
- [x] **Headless sortie preview — FP14.** Restored in development: complete head in both eyes on native sortie screens; clear first-person return after departure. Runs 134958388735Z and 135550994795Z.
- [ ] **Remaining mission/menu/weapon/control regressions — FP12.** Reconcile existing catalogs by exact build; retain explicit untested combinations.

## Current execution checkpoint

Development `56d5a6b01274` includes helicopter-exit height and head/neck reach corrections plus the sortie-head, support-finger and bare-hand
wrist/torso corrections summarized in CURRENT.md. FP13's native free-roam route
was verified through on-foot arrival on 21 October; the earlier missing root-row
investigation did not identify the Mission List category. Guidance now describes
that route. The complete field list above remains the work queue.

## Closure requirements

- [ ] Reproduce each reported defect in the actual game before altering a working flow.
- [ ] Verify the changed interaction and adjacent transitions in seated/standing and applicable body/input modes.
- [ ] Review both final eyes and motion; retain exact build/run evidence and failures.
- [ ] Obtain physical Quest Link evidence for physical-headset claims; Snakebot does not substitute for it.
- [ ] Release test inputs, restore test poses and close test-opened menus.
- [ ] Preserve published play build, personal bindings/settings, saves and established VR interactions.

FP14 is fixed in development and FP13 routing is resolved; all unchecked defects remain open. Keep this checklist and the JSON ledger in
sync as evidence changes; retain the full scope across continuation turns.
