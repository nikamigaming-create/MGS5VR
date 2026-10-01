# ACC menu regression — September 30, late session

The community report concerns helicopter upgrading in the ACC. It is still
open: the exact reported screen and transaction have not been identified.
Development through iDroid and the native helicopter customization selector
are separate paths. Neither a field-list pass nor an open-terminal flag closes
that report.

## October 1 native lifecycle investigation

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
`tools/gameplay_bot/suites/acc-helicopter-customization-back.json`. It remains
unrun on an actual helicopter selector. Purchase/upgrade confirmation,
full mission progression, other menu modes and physical-headset acceptance
remain open. The 127 focused bot checks and 38 build check groups do not
certify those paths.
