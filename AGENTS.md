# MGS5VR — effective local rules

Consolidated 24 October 2026 from the user's directions. This is the operative
rulesheet. [Chronological directions](docs/RULES_HISTORY.md) preserve provenance,
including superseded instructions; they are not a second instruction set.

## Start and continue

Read [docs/CURRENT.md](docs/CURRENT.md), then [docs/RELEASE_PLAN.md](docs/RELEASE_PLAN.md).
Use [docs/FIELD_FIXES.md](docs/FIELD_FIXES.md) for the complete report list and
[docs/FREE_PLAY_ACCEPTANCE.json](docs/FREE_PLAY_ACCEPTANCE.json) for retained evidence.
Work from this checkout on `main`, the only local development branch. Do not
create feature branches or worktrees. Historical contribution refs are not an
instruction to merge old prototypes over the current rig.

The release objective is full-game playability with established VR interactions
preserved. Progress means reported gameplay defects repaired, not harness changes,
test totals or evidence collection. Review the whole list, group related
reproduced fixes, then build/test the batch and adjacent interactions together.
Do not spend a separate build/deploy/test cycle on each narrow subcase.

Inspect existing implementation and callers before proposing a fix. An unchecked
acceptance box does not mean missing code. Separate implemented repairs, partial
implementation, reproduced failures, unreproduced reports and acceptance gaps.
Do not restart completed investigations. Reproduce unknown failures in the actual
game before changing a working flow. Untested combinations are not diagnosed
defects. Keep failures explicit; no blanket "100 percent working" claims.

## Preserve the VR game

- Keep desktop preview **1280 x 720**. VR uses the active runtime's per-eye
  recommendation and correct FOVs; desktop size cannot certify VR sharpness/stereo.
- Use native game controls only when necessary, behind a retained VR interaction
  wherever possible. Do not replace physical interactions with stock gamepad play.
- Binoculars remain physical: equip/hold/inspect, hand aim, zoom, visible-enemy
  acquisition and explicit marking with existing VR bindings. Preserve tutorial
  compatibility without stick-aim substitution or a second binocular display.
- Keep floating aiming reticles off. Preserve sights/reticles inside actual
  optics and verify sight, muzzle, hit and hand alignment. No detached zoom window.
- **iDroid belongs in the LEFT bionic hand**, using owned native cinematic contact
  and emitter/projection evidence. Verify handset and screen together in both
  eyes, including ordinary/forced stow and immediate reopening.
- Ordinary status, weapon HUD and popups stay on the **LEFT forearm**. Pause and
  explicitly selected off-wrist presentation use a spatial stereo panel.
- Never lengthen legs. Preserve native thigh/shin lengths through calibration,
  HMD motion, posture and floor fitting. Fit pelvis/joints within those lengths.
  Prioritize body/head IK and arm presentation. With full body off/unavailable,
  show hands and forearms without shoulders/upper sleeves. Preserve the left
  bionic/rocket arm, left forearm HUD and left-hand iDroid interactions.
- Audit effective personal bindings before changing chords. Preserve settings,
  saves, native restrictions and tutorial progression. Owned data stays private.

## Exactly two retained build slots

- `play/` contains the last successfully published release and matching launcher.
  `Play.cmd` verifies/restores this pin for physical play.
- `build/dev/` contains the latest completed checked candidate. `Play-Dev.cmd`
  selects it for testing. Keep both launch paths ready.
- `tools/build.ps1` builds/checks development only. `-Deploy`, `Play-Dev.cmd`
  or `python tools/workspace.py launch-sim` explicitly installs development.
  Building/debugging never replaces the published pin.
- An authorized release is packaged from checked development. Only after
  publication to the requested server/GitHub destinations succeeds and uploaded
  payloads verify may `python tools/workspace.py pin-build` promote that exact
  reviewed candidate. A source push, package or draft does not advance the pin.
  Publication/pinning never implies physical-headset acceptance.
- No dated local distribution folders, old binary snapshots or persistent
  rollback builds. ZIPs/public assets are for explicitly requested releases.
  Compiler output belongs to development; installed game files deploy one slot,
  not a third retained version.
- Staging/rollback exists only during updates; retain a failed transaction only
  until recovery. Remove recognized obsolete builds with
  `tools/clean-local-builds.ps1 -Apply` after both slots verify. Keep source,
  personal settings, saves and test evidence.

## Runtime, bounded tests and cleanup

Physical play uses **wired Meta Quest Link / Meta OpenXR**. Honor saved
`HeadsetRuntime` before the Windows default, preserve the user's requested
Windows 64-bit Meta runtime and both Play commands' explicit `-MetaLink`.
Never switch physical play to SteamVR. Simulator leases restore per-game Meta.

Do not shut down or restart Steam. No desktop, focus or window-control automation.
Use `tools/workspace.py bot` for bounded local runs. Default is observation;
Pause requires an explicitly selected case. Use existing cases and retained
routes; do not expand the harness as a substitute for defect work.

Review both final eyes through motion/transitions. Source/tests/simulator passes
do not certify whole missions, every weapon or physical-headset behavior.
Retain reviewed evidence before citing it: `workspace.py pin` retains a run;
`workspace.py pin-build` promotes the published build under separate authority.
Scratch runs expire. Preserve failures and exact scope in the ledger.

Always release test inputs, restore temporary poses/settings, close test-opened
menus and owned sessions/readers, and leave no background harness. Never
automatically open Pause and leave a failed test at Resume/Skip. Retain Steam.

## Durable handoff

Update CURRENT, FIELD_FIXES and the evidence ledger after a meaningful batch;
synchronize RELEASE_PLAN's next action. Historical reports keep their original
build scope. Do not use old "current candidate" text as a new failure diagnosis,
erase evidence or promote acceptance during documentation cleanup.

## Latest delivery direction — 25 October 2026

Batch remaining justified gameplay fixes on main, then build/check/deploy one
candidate for the user. Do not end after one small source edit. Do not restart
per-case simulator or soak loops; the user will test the deployed candidate.
Keep unresolved or undiagnosed reports explicit rather than inventing repairs.
