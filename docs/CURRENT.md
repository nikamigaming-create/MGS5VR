# Current MGS5VR work — 21 October 2026

Work only in this checkout on `main`. Local and origin have only main; earlier
branch tips are archive tags. No open GitHub PR remained to merge. Previously
verified source work has been consolidated on main. No feature branch or
worktree is needed for the remaining field fixes.

## Current result

- Raising the rifle at the reproduced concrete barrier no longer triggers the
  native automatic peek. Submitted camera position stays unchanged at recorded
  precision, with real weapon readiness and cover mode2. A separate guarded
  cover firing check discharged3 rounds with unchanged actor position and
  cover retained. Physical lean/up/return retained cover and actor position;
  walking away exited normally. Translucent body intrusion during lean remains
  visible in both eyes. Assistance settings, exposure/benefit eligibility,
  other surfaces/postures, non-VR fallback and physical Quest remain open.
- Native sortie previews show Snake's complete head in both eyes and return to
  clear first-person play. FP14 is fixed in development.
- Rifle support fingers curl through moving aim and three shots. Native reload
  and subsequent reacquisition retain the curled grip. Broader FP02 stays open.
- Bare-hand right-wrist rotation no longer steers torso heading. The previous
  8.5–12cm opposite-palm displacement fell to at most 1.13cm in two repeats;
  residual movement follows native idle animation. Arm reach/headset stay open.
- Free Roam is reachable through Missions -> Mission List -> Free Roam. Existing
  VR bindings completed Afghanistan destination/LZ selection, sortie, flight and
  on-foot helicopter exit. The controls guide now describes the native route.
- Helicopter exit no longer latches an intermediate crouched eye height. Two
  fresh exits settle at 0.775/0.788m above the native root instead of 1.18/1.369m.
  Native eyes are about 0.76m. Standing/crouch/prone/walking retain native leg
  scale. This fixes the reproduced exit floating; physical FP05 remains open.
- Authored clavicle contribution plus neutral head/neck fitting corrects the
  reproduced 50cm reach gap: standing palm error is 1.0mm and crouched 1.6mm,
  previously 11.3cm and 20.5cm. Both-eye arms, posture changes, a short walk,
  wrist isolation, moving rifle support and three shots were checked. Reload
  completed and support reacquired; mid-reload capture expired. Farther reach,
  wider weapon/interaction coverage and physical Quest validation remain open.
- Earlier tracking-loss weapon continuity and tutorial iDroid ownership repairs
  remain included. Their evidence is scoped, not full-game acceptance.

Current candidate with completed scoped gameplay checks: `8e63070500b9`.
Required build and installer checks passed. Exact runs, rejected experiments and
limits are in [FREE_PLAY_ACCEPTANCE.json](FREE_PLAY_ACCEPTANCE.json).
The complete unresolved list is [FIELD_FIXES.md](FIELD_FIXES.md).

## Cleanup and next work

Owned game/simulator closed with neutral input. No test menu, supervisor or
Python runner remains. Steam was retained. Published `play/` remains unchanged;
`build/dev/` is the candidate. Both Play paths retain Meta Quest Link selection.
Do not pin development or claim headset acceptance from these simulator runs.

The reproduced cover step-out is corrected in8e630. Candidate `ffdd224d59da` still entered native
aim-out mode3 at the original concrete barrier, with real weapon readiness.
Retained failure: `artifacts/bot/runs/20261021T223745313501Z/cover-review.json`.
Its freshness guard mixed GetTickCount64 with camera steadyMilliseconds; the
former was31ms behind, rejecting fresh frames. Candidate `8e63070500b9` uses the
camera clock and passed required build checks plus scoped barrier aim, firing,
lean and exit checks. Runs230508/231018/231215/231251 are retained in the ledger.
Assistance toggle, physical exposure/benefits, lean body visibility and wider
coverage remain open. Private investigation:
`private/cover-native-investigation.json`.
Other unresolved reports include Y-action rotation, arm reach, physical floating,
iDroid alignment, mounted weapons, small-rock traversal and cabin interactions.
Continue actual defect fixes; do not create another harness project or stop on
another narrow pass as if the whole release were complete.

The neutral head/neck correction is now implemented and checked with the fixed
camera height. Earlier camera-only candidate: `8483ea95dba9`, commit `1217caf`.
Exact current reach/body/wrist/weapon runs are recorded in the acceptance ledger.
Continue the unresolved field list, including Y-action/vehicle view ownership,
cover assistance/lean visibility, mounted weapons and native iDroid alignment. Do not interpret
50cm reach or one landing zone as whole-game or physical-headset acceptance.

## Vehicle rotation continuation

Development6b560 preserves the pre-Y view before native occupancy changes,
follows the actual TppVehicle2 carrier heading and retains the heading through
exit. Earlier9f86 still moved14 degrees on entry and was rejected. The final
candidate has retained settled entries at62/38 degrees and exits within0.03
degrees. Deliberate seated look changes62 to38 degrees; ordinary iDroid reopen
and stow preserve that view. Native tutorial notices interrupted the first
roundtrip and were reviewed and dismissed. The handset remains edge-on in the
neutral test pose: ownership continuity is not alignment acceptance.

Retained runs and limits are in FREE_PLAY_ACCEPTANCE.json. Earlier148-degree
and initial62-degree scratch runs expired and are excluded from retained
acceptance. A final entry approach did not acquire the jeep; it is a failed
interaction, not a yaw pass. Four retained headings, actual driving turns,
Fulton, table/sandbag climbing and physical Quest remain open. Do not certify
all Y actions from the jeep correction. Read-only native research remains in
private/current-vehicle-heading-contract.md; production now implements it.

Owned game/simulator closed after these checks; Steam retained. No test panel
or runner left open. Continue the unresolved gameplay list, especially native
action variants, cover step-out and handheld projection; do not expand harnesses.
