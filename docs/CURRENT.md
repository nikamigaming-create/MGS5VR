# Current MGS5VR work — 21 October 2026

Work only in this checkout on `main`. Local and origin have only main; earlier
branch tips are archive tags. No open GitHub PR remained to merge. Previously
verified source work has been consolidated on main. No feature branch or
worktree is needed for the remaining field fixes.

## Current result

- Cabin continuation on `6c9889a537db` shortens forearm guidance to stay clear
  of the sleeve in the reproduced camera-hold view, checked in both eyes.
  The combined workbench still completes. Sheet support now includes ink-quad
  thickness. Paper pinch contact remains open: measured distal thumb/index
  origins are about6cm apart, while paper is attached at palm centre. Fix the
  pinch and socket together; do not apply a paper-only visual offset.
  Retained run: `20261022T013931320198Z`.

- Cabin batch on `371a33ec2baf`: printed photos now remain at the camera's
  authored front slot until taken/replaced, verified in both eyes. Loose-sheet
  support, marker barrel pickup and field-paper ray coordinates are corrected
  in source. The combined cassette/camera/note/pen/board sequence completed;
  caption, freehand ink and photo remain on the board in both eyes. Paper
  finger contact and wrist-prompt clipping still fail visual review. Flat-stack
  and field-ray changes have focused checks, not full live acceptance.
  FP09/FP10 remain open. Retained run: `20261022T013111456118Z`.

- Development `44f0ed657fab` enables native iDroid projector fitting by default.
  The matched native cone previously ended about20cm from the screen centre.
  Both-eye front/turned views and ordinary stow were checked with normal settings;
  native light texture/bright patches remain visible. FP07 stays open for forced
  tutorial lifecycle, wider configurations and physical Quest. The final run
  completed gameplay cases but its simulator pose-restore RPC timed out; the
  owned session was closed. See active_work.idroid_projector_fit in the ledger.

- Candidate `4447439fc8bc` fixes the reproduced crouched cover-exit wall
  penetration. Both eyes now retain the wall after exit and through the
  previously obscured left/right reach check. The shoulder reach correction
  also keeps the shoulder outside an18cm eye envelope, reducing near-eye sleeve
  intrusion without hiding the arm. Native cover lean/up/return and ordinary
  exit remain functional. Wider cover and physical Quest remain open.
- Full-body cover no longer places the camera through the reproduced Mother
  Base wall. Candidate `4060b9259cea` preserves the native wall-relative eye
  footprint instead of rotating it into view yaw. Both eyes retain the solid
  wall during 22cm lateral lean, 25cm upward peek and return; native cover stays
  active and walking away exits it. Right-shoulder intrusion during lateral
  lean remains visible. The earlier Afghan translucent figure is not identified.
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

Current candidate with scoped gameplay checks: `6c9889a537db`.
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

Latest cover continuation: before runs233250/233314 compare full-body on/off
at the Mother Base wall. They change camera position as well as visibility,
so they cannot identify the Afghan translucent figure. With full-body on,
the old camera crossed the wall; off retained it. Candidate4060b preserves
the cover eye offset in actor-root space and settles cover entry/exit even
when the crouch flag stays unchanged. Runs234550/234616/234646 retain both-eye
lean/peek/return and normal exit. See active_work.cover_wall_anchor in the
ledger. Next: right-shoulder/body intrusion and wider cover behavior. Neither
headset acceptance nor exposed-head/native-benefit eligibility is established.

Latest continuation supersedes that next step: read-only joint samples in
20261021T235748151162Z locate the lean intrusion at the shoulder, about13cm
from the eyes. Candidate d731962c1f2d adds a constrained clavicle reach and
improves clearance, but its exit/reach runs20261022T001435264845Z and
001507602216Z reveal the separate camera-through-wall failure. Do not use
their reported input pass as visual acceptance. The cover flag clears while
the native crouched head is still offset; retaining the native footprint for
crouched poses fixes that transition. Final4447 runs002654/002713/002733
retain lean/up/return, exit and both-eye reach results. Peripheral sleeve
visibility remains; the original Afghan translucent figure is still not
identified. Continue assistance settings, exposure/benefits, broader cover
and the rest of FIELD_FIXES. Game/simulator and read-only sampler are closed.

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
