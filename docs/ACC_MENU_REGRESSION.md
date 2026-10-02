# ACC menu regression — October 1

The community report concerns helicopter upgrading in the ACC. It is still
open: the exact reported screen and transaction have not been identified.
Development through iDroid and the native helicopter customization selector
are separate paths. Neither a field-list pass nor an open-terminal flag closes
that report.

## Current repair and actual game result

The completed ACC **Tutorials / Choose a Rival** restriction is repaired on
candidate DLL `d893eb437a06`. Native observation found two separate leftovers:
the tutorial's own pause handle and the disabled bit on all 78 Mother Base menu
entries. Clearing the tutorial mode alone did not restore menu eligibility.
Making all entries active also failed because active and disabled differ.

The verified native guide begin transaction now records each entry's prior
disabled bit. After the player dismisses the reviewed Help card, recovery is
restricted to ACC mission 40010 / `Seq_Game_MainGame`, FOB state FINISH (127),
no live mission-list guidance and no popup. It restores only the recorded
disabled bits and releases only that tutorial's native pause handle. Other
menu flags, progression and tutorial completion remain game-owned. A later
independent disable invalidates that entry's restoration. Partial transactions,
replaced owners and mismatched native ABI cannot provide restoration evidence.
An unfinished guide cannot be cancelled by the held-Back recovery.

ACC root Back already asks the native terminal to close, but the cabin lacks
the ordinary on-foot stow task. The repair completes that requested native
closure in the main ACC only. The resulting cabin must have a live VR camera,
no suspension/waiting-for-player, neutral controls and restored rig input.

Retained `20261001T181154160585Z` reached these **actual reviewed pages** through
the effective physical VR bindings: Rival Help -> Missions -> Map -> Mother
Base -> Development -> Support Helicopter Armament. Development eligibility
returned to true. Three individual Back edges returned from the leaf through
Development and Mother Base to the live cabin; physical iDroid reopened with
Development still enabled. Both final eyes of the helicopter list, cabin and
reopened display were reviewed. Eleven scoped cases passed. The subsequent
Daily Bonus case failed before dispatch because its outcome already matched;
the overall run remains partial and does not certify all menus.

`20261001T183114243475Z` repeated the Help/eligibility repair on a second cold
boot. Root Back restored the cabin, but its twelve-consecutive-sample test hit
the two-second review deadline; that case remains failed. The reusable
`acc-completed-rival-roundtrip.json` retains every camera/control predicate and
uses three stable samples, matching cleanup's existing requirement. It is a
completed-guide lifecycle regression, not a generic menu walker.

`20261001T183825927585Z` proved a real physical Confirm dismissed **Daily Bonus
Received** and revealed the distinct **Skulls Attack** event card. The earlier
no-dispatch failure is not a game defect. The event card received no stale
confirmation. Automatic identification of those informational pages remains
open; `IsShowPopup()` alone does not identify them.

Final DLL `b6fd45533f6f` repeated the four maintained completed-guide lifecycle
cases in retained `20261001T190258541444Z`; all four observed outcomes passed.
Both final-eye pairs show Help gone, the ordinary cabin restored and iDroid
reopened. Cleanup also verified the live camera and rig input. These are
static scene checks, not continuous flicker or physical headset certification.

## New community reports and test-loop findings

The October 1 Development lock-up report has no confirmed release/build or
ACC/on-foot identity. The user says it may concern the last public release,
which has not received these local changes. Keep it open until current-build
Development entry/detail/Back/reopen is exercised in each eligible context.

The two supplied videos contain dark metal stairs, catwalks and masonry walls;
neither shows Development in the reviewed two-frame-per-second samples. A
crouched armed character is visible near the camera in the longer clip, but
its ownership and the exact location are unestablished. Native collision,
posture, cover and camera/root ownership need separate matching-scene checks.
Do not flatten floor identity, disable collision or infer a menu freeze from
these movement clips. Private originals and derived review pixels are not
release assets.

The warm run `20261001T191943458398Z` failed to open iDroid after a real 120 ms
Menu tap, although native XInput sampled Start. A 300 ms tap opened the same
ACC in `20261001T192821425533Z`, followed by actual Daily Bonus dismissal and
Map -> Mother Base. Rewards was now the first row; the earlier Customize-first
navigation is not reusable blindly. Two 160 ms sampled Down holds overshot
Development and reached Staff Management. These are test-loop defects rather
than proof of broken player bindings. The bot now uses one fresh stick edge
for cursor steps and requires an explicit mode for an iDroid scroll hold.
Its default iDroid tap is longer but capped below the user's effective hold
threshold. Native replay passed in cold `20261001T193945164460Z` and warm
`20261001T194125435098Z`: default physical opening works and two edges select
Development exactly. The focused bot contracts have 131 passing checks.

The warm run reviewed Weapons/Items, the helicopter list, already-developed
body grades and an eligible weapon confirmation. Cleanup canceled the cost/time
dialog and restored the live cabin without changing development resources.
`20261001T195300890687Z` then reviewed Buddy Equipment and Security Devices.
Security Back returned to Development in both captures, but a later neutral
Trigger release RPC timed out after 15 seconds. That case remains failed.
No navigation was retried after unknown transport completion; the exact owned
game/simulator were closed with Steam retained. Native camera/stereo publication
continued, so this failure does not establish a native Development lock-up.

Retained fresh `20261001T200807859685Z` repeated the four completed-Rival
regressions and the Security entry/Back successfully. Physical Confirm dismissed
the separately reviewed Daily Bonus and Skulls Attack informational cards.
The same run entered Development and purchased the eligible AM D114 LA weapon
development through the actual cost confirmation. Before/after native reads
show GMP 29,956,290 -> 28,323,170, Common Metal 1,182,558 -> 1,151,258 and Minor
Metal 1,170,661 -> 1,140,861: exact displayed costs, not a dispatch-only claim.
The developed count remained 453; the native development timer was
not forced or accepted as complete. All nineteen scoped action outcomes passed,
and cleanup verified closed menus, live cabin camera and restored hand poses.
The sequential eyes include a native post-purchase display transition; they
do not establish continuous stereo/flicker acceptance.

Read-only native completion checks found all nineteen helicopter definition IDs
already completed on `acc-completed-fob-baseline`. Speaker and body-grade
Confirm did not open a purchase dialog. Requirements-met alone cannot identify
an eligible unpurchased grade. `tools/development-state.lua` now reads completion,
definition grade, requirements and resource facts without granting/spending
anything or pretending to identify the current menu page. A less-complete
paired-save fixture is required for helicopter purchase and lock variants.

Helicopter upgrade purchase, locked/unavailable grades, completion results, the helicopter
customization selector, unfinished/first-time FOB guides and physical-headset
acceptance remain open. Historical failures below stay historical.

## Earlier October 1 native lifecycle investigation

Current runtime DLL `0eae8b0d8322` cold-booted naturally through login and
physical Continue to ACC mission 40010 in retained run
`20261001T125301162970Z`. Intermittent startup remains open; one successful
boot does not repair the earlier login stall.

Retained run `20261001T125600146402Z` reopened iDroid and reviewed the
**Tutorials / Choose a Rival** informational Help card in both eyes. A closed
that actual card and restored the normal Missions presentation. Ordinary
four-edge cleanup still failed to close iDroid; the owned session was closed
with Steam retained. The declined stale tab decision dispatched no input.
This run does not add successful tab navigation or immediate reopening.

Read-only native binding/field checks found the pending open-condition flags
already zero, their three mode fields at the native cleared value, and
cancellation enabled. The game also reported FOB tutorial state FINISH while
its separate done-in-this-game flag remained false. These observations do not
establish an active forced-open request. Do not call this Help card a proven
forced FOB tutorial, clear progression, or retry the already-ruled-out
cancellation/player-pad experiments.

The immediate `StopMbDvcTerminal` recovery is a reproduced incomplete exit:
clearing the terminal bit did not restore the character/device/Help lifecycle.
The separate ordinary tab/Back failure after reviewed Help dismissal still
needs its exact input/update owner traced. No behavior repair is claimed by
these read-only observations.

## Confirmed changes

- Ordinary ACC cabin rendering no longer draws the title-only cassette rack.
  The real title rack and physical Continue interaction are preserved. Both
  final eyes were reviewed at the title and after native ACC arrival.
- Explicit Pause navigation uses one fast sampled stick edge, avoiding the
  repeated holds that selected Options instead of Return to ACC. The real
  Return to ACC confirmation and native arrival were reviewed.
- Menu navigation checks native prerequisites before input and after release.
  An overlapping iDroid/Help owner cannot receive ordinary navigation or
  automatic cleanup input.
- The state planner distinguishes ordinary iDroid, native Pause/Help overlap
  and native popup owners. The latter two have no executable edges; missing
  flags cannot authorize ordinary navigation or confirmation.
  Ordinary Pause also requires its own active flag and no popup, so an ACC
  selector cannot inherit its Back recipe.
- Nested iDroid cleanup allows at most four freshly guarded ordinary Back
  edges. Development -> Helicopter -> top -> cabin needs three. Cleanup is
  attempted once; an ambiguous failure cannot start another input chain.
- A startup prerequisite that changes during capture returns to observation
  only when admission dispatched no input. Transport/post-dispatch failures
  remain fatal. Unknown login dialogs receive no automatic A presses.

The fixed local installation remains `play/`; the desktop is 1280 × 720 and
the tested runtime recommendation is 2520 × 2640 per eye. Personal controls
and VR settings retain their recorded hashes. Steam is retained. Failed runs
close only their exact owned game/simulator generation.

## Native observations

The disposable campaign returned naturally from Mission 6 to ACC mission
40010, `Seq_Game_MainGame`, helicopter-space=true. No save replacement,
upgrade purchase, forced mission sequence or tutorial-completion write was
performed in this session.

In the ACC Mother Base tab, **Customize is the first row and Development is
the second**. The field recipe assumed Development was first. The reviewed
Development submenu contains Weapons/Items, Buddy Equipment, Helicopter and
Security Devices. The actual Support Helicopter Armament list was reached.
The old two-edge cleanup failed at this depth; its later duplicate cleanup
closed the remaining level. That run stays failed. The corrected loop still
needs a complete native leaf -> Back -> stow -> immediate reopen regression.

Cold ACC iDroid opening also shows a native **Choose a Rival** Help card.
That owner overlaps the terminal and native Pause flags while the ordinary
popup and tutorial-pause flags are false. The reviewed A closes the card.
In the latest cold fixture, subsequent tab, Back and terminal-toggle inputs
did not change/close the underlying Missions page. Sampling those buttons
does not establish a successful page transition.

Held-Back's existing direct terminal stop cleared the terminal-open flag but
left an overlapping Pause/player-device state. One ordinary Back returned
the visible cabin, but immediate iDroid reopening failed. Deferred close,
temporary cancellation permission and an ACC-only player-pad exclusion
experiment did not repair this. All three experiments were reverted.

Native startup is also intermittent: cold runs progressed automatically
through login, while another stopped on **Logged in to server**. The login
progress/result dialogs can replace each other without a distinct popup-open
boundary. Reliable dialog identity and unattended cold startup remain open.

## Retained evidence

Paths are relative to this checkout. Runs used different DLLs; exact executable,
DLL, INI and controller-tool hashes are in each run's `identity.json`. Read the
run result and visual scope before reusing it.

| Run under artifacts/bot/runs/ | Scope / result |
| --- | --- |
| 20261001T032140052459Z | Physical title Continue and native ACC cabin without the stray title rack; both eyes reviewed. |
| 20261001T043949650460Z | ACC Development is the second Mother Base row; both eyes reviewed. |
| 20261001T044109505951Z | Actual Development submenu; ordinary cleanup returned to cabin. |
| 20261001T044233760816Z | Actual helicopter armament list; failed old two-edge cleanup. |
| 20261001T044809338209Z | Held-Back direct stop leaves an overlapping native menu; failed. |
| 20261001T045358925081Z | Reviewed ordinary Back closes the leftover Pause and reveals the cabin. |
| 20261001T045952125713Z | Immediate reopening after that forced stop fails. |
| 20261001T053812211021Z | Cancellation already enabled; deferred close still fails. Candidate reverted. |
| 20261001T054554672951Z | Cold startup and physical Continue reach ACC on the cabin-rendering fix. |
| 20261001T054659634674Z | Help card closes, but the reviewed tab does not change Missions; cleanup fails. |
| 20261001T055804851427Z | One guarded autosave acknowledgment, then login completes without bot login confirmation. |
| 20261001T055940218625Z | ACC pad-exclusion experiment: tabs, ordinary Back and terminal toggle still fail. Candidate reverted. |
| 20261001T062129083685Z | Final runtime DLL: autosave acknowledgment progresses to login, then startup times out. Owned game/simulator closed; Steam retained. |

The maintained selector probe is
`tools/gameplay_bot/suites/acc-helicopter-customization-back.json`.
Native reproduction rejected its former expectation of an immediate cabin
return: Back instead opens **Cancel customization?**, with Cancel selected.
The corrected two-case recipe checks that dialog, moves left through the
effective VR stick, confirms discard and requires the live cabin camera and
control owner. Exact ACC selector/target/popup guards prevent it from operating
on field iDroid, another selector or an unknown target.

Retained `20261002T040239079392Z`, DLL `431d6cb9a5ad`, passes ten scoped action
outcomes including selector entry, the two-case discard path and immediate
physical iDroid reopening. Both eyes of the selector, dialog, cabin and reopened
iDroid were reviewed. Its reopening stills span a queued Daily Bonus card; this
does not certify continuous flicker-free or a stable common menu image.
The scoped record is `artifacts/bot/acceptance/acc-helicopter-customization.json`.
The retained dialog shows Cancel selected by default. Its separate Cancel
round trip still needs retained acceptance; the earlier scratch investigation
was not promoted and is no longer retained.
The bot still refuses to automatically acknowledge an unidentified popup.

Purchase/upgrade confirmation, selector part changes, full mission progression,
other menu modes and physical-headset acceptance remain open. The 138 focused
bot checks and 38 build check groups do not certify those paths.

The current default-exposure DLL `431d6cb9a5ad` also passes all four maintained
completed-Rival cases after a fresh Steam-client launch, retained in
`20261002T025907840443Z`. Both-eye review covers Help dismissal, live cabin
return and immediate physical iDroid reopening. This extends the cold-start
regression evidence; it does not certify an upgrade transaction or selector.
