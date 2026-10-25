# Release execution plan — 24 October 2026

This is the durable plan for finishing the user-reported repairs and preparing
the next TPP experimental release. Read [CURRENT.md](CURRENT.md), then execute
this plan. [FIELD_FIXES.md](FIELD_FIXES.md) separates implemented repairs from
remaining code work and acceptance. Do not restart the three-day investigation
from unchecked historical boxes.

The user has now explicitly set release readiness as an active goal: complete
all justified fixes on main, then test the batch and continue through remaining
field/progression checks. Do not stop merely because one cuff case passes.

Current execution produced checked candidate `a6dc5fb91151`; see
[CURRENT.md](CURRENT.md) for its smoke results and remaining visual limits.
The identities below record the original audit baseline.

## Starting point and authority

- Work in the existing checkout on `main`; no new branch or worktree.
- Audited gameplay source: `5622052bd8799603ef8aae66235ec82d3c7bce6f`.
- Checked development DLL: `17b044cba72e33e6d6eefc89be5f00bcb313ba0fee4bb8751da9d43e52b50e4f`.
- Published local play DLL: `b627c7cacc9acd540e04193a811d3ca14bd59c9be542cc587345f1cd5ca2b47c`,
  source `f8cf03f949aeb37203dccf67ad5ce7a306d7fe6d`.
- Before this documentation cleanup, development's complete source identity
  matched the checkout and both slots had zero manifest file mismatches.
  The existing October 23 report records 58 passing test groups, zero failures.
  These are existing results, not a new gameplay or headset pass.
- Documentation changes now make the checkout newer than the packaged docs.
  Do not rebuild merely to refresh prose; include them in the next planned batch.
- The source audit verified production callers and relevant test assertions.
  It did not launch the game, change bindings, or certify physical play.
- `AGENTS.md` contains the effective rules. Historical directions and reports
  retain provenance but cannot override newer rules or reopen a repaired subcase.

## Decisions already made

The fixed-length legs/pelvis solver, reach and wrist-isolation repairs, garment
drivers, reload support retention, tracking recovery, contextual-heading guard,
cover entry/exit fixes, transit body, left iDroid lifecycle/projection, Rocket Arm
steering, cabin paper/printing and ACC rifle grabbing are implemented on main.
Preserve them. FP13's free-roam route and FP14's sortie head are scoped resolved.

Concrete implementation limits: physical mounted aiming admits native actions
`0x46` and `0x47` only; forearm-only cropping admits the identified sna5 geometry
only. The corrected cuff has scoped stereo review; extreme full-body sleeve crowding remains a visible limitation.
The exact rock/corridor reports remain undiagnosed. Missing coverage for another
weapon, outfit or lesson does not establish a new defect.

Historical contribution refs are not outstanding feature branches to merge.
The asynchronous mailbox worker was withheld over native D3D11 synchronization;
the extra movement filter was not layered over the existing stabilizer. Preserve
those decisions and attribution; see [fork review](FORK_REVIEW_2026-09-20.md).

## Execution order

### 0. Documentation preparation — completed in this pass

Consolidate current rules and entry points; preserve old evidence as history;
record code state separately from acceptance; keep the release gate conservative.
No gameplay code, build, deployment, pin or publication is part of this step.

### 1. One bounded baseline investigation for the remaining unknowns

Use the existing checked development before editing working flows. Inspect the
effective personal bindings and current native state once, then select the
following scene groups. Reuse retained passing subcases; do not repeat ordinary
ACC toggles, both-rifle tracking recovery or binocular village marking just to
generate another pass count.

| Order | Scene group | Exact question to resolve | Required outcome before a code change |
| --- | --- | --- | --- |
| 1 | Body/arms, then eligible field cover | Does the current cuff/cross-head crowding obstruct ordinary use? Does full-body fallback retain hands/forearms without upper sleeves for the selected outfit? | Identify exact outfit, vertices/weights or admission failure; preserve native thigh/shin lengths and left bionic arm. Do not restore the startup-failing cuff experiment. |
| 2 | Mounted fixture | Why does native entry fail at the intact Mother Base gun, and what native action/contacts belong to each additional type? | Verify intact status, native interaction eligibility and actual mounting. The latest local handoff already reaches the adjacent platform and records failed rear-entry attempts. Do not repeat approach loops, infer `0x45`, or override Mother Base's lethal-weapon restriction. Use a genuinely usable alternative if the base gun is unavailable. |
| 3 | Reported terrain | Can the exact low-rock interruption and tight passage be repeated? | Record native actor/capsule/floor state versus camera/body in both travel directions and body modes. A bot displacement guard or blocked route is not itself a collision bug. If the location cannot be recovered, record that precise dependency and continue independent work. |
| 4 | Remaining progression/weapon gaps | Do forced iDroid lessons, bolt-action support, Fulton/climb headings or powered-arm impact actually fail on this candidate? | Identify native owner/state and visible failure before altering their working generic paths. No tutorial/save/progression bypass. |

Each case has a bounded deadline and neutral cleanup. A timeout without a new
diagnostic fact ends that attempt; resume only after a changed prerequisite or
specific diagnostic hypothesis. No supervisor loop or repeated destroyed-gun
entry. Missing hardware/scene is recorded as unproven, not a code defect.

### 2. One coherent correction batch

Prioritize body/head/arm presentation, then confirmed mounted/interaction issues
and any terrain or progression failure reproduced in step 1. Use the source
ownership map in FIELD_FIXES; extend existing implementations instead of adding
parallel rigs, optics or menu owners. Additional mounted types require confirmed
native action identity, authored contacts, native limit enforcement, release and
stick override. Broader outfit cropping requires source-bound geometry admission,
not arbitrary model hiding. Do not weaken collision to clear a route.

Keep every selected repair in one reviewed candidate. Run focused contracts
while editing where they test the failure; do not build/deploy after each small
subcase. If step 1 finds no justified gameplay change, use the existing fixes and
proceed to final source/docs packaging checks without inventing work.

### 3. Commit reviewed source/docs, build once, validate the combined candidate

Record actual outcomes in CURRENT/FIELD_FIXES/FREE_PLAY_ACCEPTANCE and reconcile
the five conservative catalog blockers with their exact evidence. Prepare the
release notes before the final source commit/build so the package includes them.
No source push or publication is implied by a local commit.

From the repository root:

```powershell
python tools/workspace.py status
.\tools\build.ps1
python tools/workspace.py launch-sim
python tools/workspace.py bot --command continue
```

`build.ps1` builds/checks development only. `launch-sim` explicitly deploys it;
do not call `pin-build`. Run the selected bounded cases through `workspace.py bot`
with reviewed existing suites/policies. Do not pretend a named suite exists for
an unimplemented fixture; select its actual guarded case before dispatch.

| Combined review | Required behavior and adjacent transitions |
| --- | --- |
| Body and movement — FP04/05/06/17 | Standing/seated calibration and recenter; crouch/prone/walk; both-arm reach and wrist isolation; cross-head return; cover lean/peek/exit; full-body on/off and fallback; boarding/transit/disembark. Native thigh/shin lengths remain unchanged. |
| Combat and recovery — FP01/02/08 | Moving supported rifle, fire/visible impact, reload, bolt cycle, release/reacquire and weapon swap; valid idle and invalid tracking with no held-trigger discharge on recovery; Rocket Arm flight/impact/return and eligible Blast Arm case. |
| Mounted and contextual actions — FP03/11/15 | Each claimed mounted type: actual entry, either-hand yaw/pitch, limits, contact, operating-hand transfer, stick override, release, discharge/impact, exit. Vehicle entry/exit, deliberate driving turn, Fulton and table/sandbag climb at four starting headings; fixed HMD heading must not inherit forced actor alignment. |
| iDroid/progression — FP07/12/13/14 | Left-hand contact/projector and readable both-eye motion; ordinary and forced stow/immediate reopen; authored-scene return and exit notice; Development and nested dialog navigation without unintended purchases; sortie preview and native free-roam route; physical binocular inspection/zoom/acquisition/explicit marking preserved. Complete a naturally available mission through its objective and return; do not call a lesson a mission pass. |
| Cabin — FP09/10/11 | Both-hand paper/marker contact, write/type/pin, front-slot printing, stacking and cassette presentation; both native rifle assemblies carry/return and reject held-grip reacquisition across interruptions. Recheck already-passed cases only when the changed code affects them. |

Review final composition in both eyes and through transitions, not only endpoint
stills. Preserve exact candidate/run identity and failed cases. For sustained
claims, the existing catalog requires at least 180 seconds/five cycles per
feature and a 600-second integrated soak; a short slice cannot replace these.
Measure fresh XR submissions, repeats, stalls and actual runtime period with
unchanged per-eye recommendation/FOV. Capture FPS is not runtime performance.

Retain only reviewed evidence using `python tools/workspace.py pin` (run
retention, not build promotion). End with `python tools/workspace.py stop-sim`:
release inputs, restore poses/settings, close test-opened menus and owned
processes, restore Meta runtime; keep Steam running. A failed test does not
authorize leaving Pause, a terminal or a background harness open.

### 4. Physical Quest Link review and release decision

Launch the checked candidate through `Play-Dev.cmd` on wired Meta Quest Link.
The user performs physical reach/scale, seated/standing, wrist isolation,
handset readability, sight/contact, tracking recovery and comfort checks during
the combined gameplay flow. Simulator observations cannot close these reports.
Record the actual feedback against the exact candidate; do not repeat old
hardware feedback as acceptance of the new build.

Any new reproducible crash, progression lock, forced view rotation, lost hands,
stretched legs or broken established interaction blocks a claim that it is fixed.
Repair affected code and rerun its affected group; do not restart every unrelated
case. No promise of a same-day full-game certification: the mission ledger still
has no full-mission accepted passes across its 51 required missions/variants.

Present one release decision: delivered fixes, actual remaining defects,
unproven combinations, physical feedback and exact candidate. If unknowns remain,
the experimental release must disclose them. The existing `--allow-known-issues`
packaging option is not a default way to bypass review; a release with those
specific blockers needs an explicit decision on the concrete candidate.

### 5. Authorized packaging, publication and pin

Confirm the requested destinations and unused tag at release time. Proposed tag:
`experimental-2026-10-24`, only if it is still October 24 and that tag is unused.
Do not reuse the date automatically on a later day.

```powershell
python -B tools/regression_catalog.py --release
python tools/package-release.py --tag experimental-2026-10-24
```

Packaging requires clean committed source matching BUILD.json, passing checks,
matching public defaults, notes and complete payload hashes. If post-review docs
change the source identity, refresh/check development once before packaging and
verify whether runtime binaries changed; do not substitute a different DLL for
the reviewed one without appropriate revalidation.

Verify ZIP contents, source archive, launcher, licenses, default settings and
checksum; exclude owned assets, saves and private diagnostics. Publish only when
authorized to the confirmed server/GitHub destinations. Verify uploaded payloads
against the local checksum. A draft or source push is not publication completion.
Only then run `python tools/workspace.py pin-build` for that exact candidate.
Verify both Play paths and retain exactly `play/` and `build/dev/`; cleanup of
recognized obsolete builds uses `tools/clean-local-builds.ps1 -Apply` only after
both slots verify. Never remove settings, saves, source or retained evidence.

## Resume record

- Completed on main: documentation/source audit; previous body/head, garment,
  reach and interaction repairs; flat native cuffs with independent shading and
  correct repeated attribute-slot binding. Gameplay source `5ba674f`.
- Checked candidate `351d84672f0e`: 58 groups and installer checks pass. Both
  slots verify; published play remains October 19.1 (`b627c7cacc9a`).
- Combined affected review completed: nine cases in each body mode, including
  left iDroid stow/immediate reopen. Both final eyes reviewed. The rejected
  intermediate selector regression remains recorded; see CURRENT and the ledger.
- Native Afghanistan travel/fire diagnosis did not establish a terrain repair
  or actual intact anti-air mounting. Do not repeat combat-blocked approaches.
- Remaining: physical Quest feedback, extreme cloth crowding/other outfits,
  exact terrain and mounted/progression cases, sustained performance. Keep
  implemented fixes distinct from missing coverage; five conservative gates remain.
- Cleanup complete: full_body1, owned game/simulator/readers closed, Steam
  retained. Both Play commands/saved headset preference select Meta. Windows'
  global OpenXR registration is Virtual Desktop; an attempted Meta restoration
  was denied by registry permissions. See CURRENT for this precise limitation.
- Extreme-reach follow-up: the captured fixture requests106cm from a53.9cm
  native arm chain; dense comparison does not justify replacing its fallback.
  Current mesh penetration remains unproven. Do not repeat that impossible-pose
  test as evidence of a new tracking defect; see CURRENT.
- Next: process physical feedback on this candidate and diagnose a newly
  reproducible remaining failure before editing. No package/publication/pin or
  release-gate waiver. Outcome documentation may be newer than packaged prose.

- Sustained follow-up found a reproducible handheld stale-skin camera publication
  failure in cycle six. Source correction and stall/recovery regression are in
  this batch. Build completed: all 58 groups and installer checks pass. Retained
  combined run `20261025T044511277442Z` passes all 27 cases, including Pause and
  immediate reopening. The prior run stays failed. Candidate `a6dc5fb91151`
  is ready through Play-Dev. Next: physical Quest feedback and a newly
  reproducible field case; frame repeats remain measured and unresolved.
