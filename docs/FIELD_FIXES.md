# Field fixes — active checklist

Updated 21 October 2026. All community-reported defects are top priority.
An unchecked box means unresolved. Only check a box after its acceptance
evidence covers the reported behavior. Build/tests alone do not close a defect.
Detailed evidence and test requirements: [acceptance ledger](FREE_PLAY_ACCEPTANCE.json).
Live continuation: [CURRENT.md](CURRENT.md).

- [ ] **Y-action forced rotation — FP03.** Fulton, vehicle entry, table and sandbag climbing. Reported; compare four starting headings and preserve physical view direction through entry/action/exit.
- [ ] **Mortar operator viewpoint — FP15.** Reported inside the emplacement; establish native operator position and usable view of aiming/firing in both eyes.
- [ ] **Natural mounted-weapon controls — FP11/FP15.** Implement physical grip-and-aim and stick aiming for mortars and each turret type; verify native pivots/limits, contact, release, firing and exit. MountedGrip helper exists but has no production adapter.
- [ ] **Small-rock slipping and stops — FP16.** Reported; locate/repeat affected rocks, distinguish native collision/fall state from camera/body presentation.
- [ ] **Right-wrist shoulder/opposite-arm coupling — FP17.** Reported in physical VR. Primary-aim torso steering is a source lead, not yet a verified cause. Hold HMD/left hand fixed while independently moving right wrist/hand.
- [ ] **Shortened arm reach — FP17.** Reported; compare native bone lengths/calibration and physical reach across modes. Physical Quest validation required.
- [ ] **Precision-weapon support loss — FP02.** Moving supported rifle and3 back-slot shots retain contact in082239, but open support fingers reproduced in both eyes. Read-only shared palettes match; candidate73fe7d0b521d cups acquired long-gun support outside native manipulation. Built/checked; moving/reload/free-hand actual-game verification in progress. Bolt-action and physical headset remain open.
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
- [ ] **ACC free-roam destination entry — FP13.** Entry absence reproduced in ACC40050; native cause/routing unresolved.
- [ ] **Headless sortie preview — FP14.** Visual failure reproduced; preserve ordinary first-person head exclusion while restoring preview.
- [ ] **Remaining mission/menu/weapon/control regressions — FP12.** Reconcile existing catalogs by exact build; retain explicit untested combinations.

## Current execution checkpoint

Checked development `ff0dd2c746a6`: exact retained-terminal native tutorial timeout,
cinematic and same-player return passed, including ordinary Back, immediate reopen
and final stow (`20261021T034930898656Z`). Same-open-terminal provenance fixes the
post-scene ownership loss reproduced on148d. All58 automated groups and installer
checks pass. Broad FP07/FP12 stay open for alignment, other lessons and headset.

Both primary-slot rifle tracking-loss, held-trigger suppression/release and positive
firing rearm passed on this binary (`20261021T040358781890Z`,
`20261021T040558709267Z`). Both loss/recovery eyes reviewed. Open support fingers
remain visible; other owners, movement, bolt-action/recoil and physical Quest are
unproven. Earlier evidence and failed harness assertions remain retained in JSON.

Session closed with neutral controls, faults0 and no menus; Steam retained.
Published pin unchanged. Per-game and both Play commands select Meta; global
Windows runtime is Virtual Desktop and registry restoration was denied. See
CURRENT.md for exact evidence and the launch check.

Next: FP02 moving support/contact and precision-weapon checks; FP07 native handset
alignment; FP03 Y-action rotation reproduction. Broader checklist stays active.

## Closure requirements

- [ ] Reproduce each reported defect in the actual game before altering a working flow.
- [ ] Verify the changed interaction and adjacent transitions in seated/standing and applicable body/input modes.
- [ ] Review both final eyes and motion; retain exact build/run evidence and failures.
- [ ] Obtain physical Quest Link evidence for physical-headset claims; Snakebot does not substitute for it.
- [ ] Release test inputs, restore test poses and close test-opened menus.
- [ ] Preserve published play build, personal bindings/settings, saves and established VR interactions.

FP14 is fixed in development; all unchecked defects remain open. Keep this checklist and the JSON ledger in
sync as evidence changes; retain the full scope across continuation turns.
