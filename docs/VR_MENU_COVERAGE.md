# All-menu VR walkthrough

The target is every discovered TPP menu page and legal branch in every eligible
native game state, using the player's effective VR bindings. Test each branch
where it is actually available; do not count an open-menu flag as a menu pass.
Menu discovery, complete automation and physical-headset acceptance are open.
The connected map/state planner is described in GAME_STATE_MODEL.md; menu paths
are one part of that graph, alongside native roles and authored mission states.

## Walkthrough order

| Native state | Menu paths to discover and exercise |
| --- | --- |
| Title / cabin | Every visible title choice, Continue, setup and options branches. |
| ACC | iDroid tabs, customization, mission selection, character/buddy/loadout and sortie preparation. |
| Mother Base | Map, staff/resources, development categories and grades, helicopter upgrades, tapes/intel and available contextual menus. |
| Afghanistan / Africa free roam, missions and Side Ops | Each available iDroid branch, support requests, equipment, buddy commands, interrogation, Pause and options. Keep these states separate. |
| Horse, vehicle driver/passenger, turret/mortar, Walker and helicopter passenger | Discover which menus are available per native role, then test entry, choices, exit and control restoration. Split vehicle/station variants. |
| Cinematics / forced tutorials | Allowed menus, required acknowledgments, forced stow, immediate reopening and return to the correct player/camera. |
| Loading / results / death | Visible start confirmations, clear/abort/death screens, retry and return branches. |
| Online / FOB menu entry | Discover eligibility and entry/cancel branches separately. TPP online gameplay remains unaccepted; MGO and Ground Zeroes need their own game adapters. |

Each row must expand into the actual child pages found in-game, including locked
choices, progression-dependent choices, buddy/ability variants and confirmation
dialogs. The catalog is a starting queue, not an exhaustive list of game screens.
Unexpected pages become new paths; they do not get generic Confirm or repeated Back.

## What happens on each path

1. Enter a verified scene/checkpoint. Record mission, sequence, native role,
   progression, page, parent page, selected row and legal choices.
2. Open through the existing VR interaction. Visit every child page, scroll/list
   boundary, tab and applicable map/pointer control. Read the actual choices.
3. Check safe selection, disabled-choice behavior, confirmation, cancellation
   and Back from every depth. Verify the resulting native action or page change.
4. Close, immediately reopen, then close again. Confirm neutral inputs and
   ordinary movement/combat ownership recover, with no menu or pose left behind.
5. Review both final-eye sequences through head/hand motion: crisp readable text,
   complete choices, correct attachment, stable stereo and no blinking. Review
   physical-headset readability and usability separately.

Ordinary status/HUD remains on the **left bionic arm**. Handheld iDroid remains
in the **right hand**. Pause and selected off-wrist presentation use a spatial
stereo panel. Handheld and spatial variants are separate obligations. Native
integration stays behind VR actions; stock gamepad-only operation is not acceptance.
VR uses runtime-recommended eyes/FOVs independently of the 1280 × 720 desktop.

Read-only discovery goes first. Purchases, upgrades, resource use, deployment,
abort/retry and progression-changing actions need an isolated verified save or
checkpoint fixture. The ordinary play save must not become the test fixture.
Settings changes must restore the saved value, including cleanup after failure.

## Current tools and gaps

The fixed review view is generated without launching or controlling the game:

```powershell
python tools/workspace.py coverage --menus

# Add reviewed evidence only after pinning it. The latest retained baseline:
python tools/workspace.py coverage --menus --run artifacts/bot/runs/20260930T215049265217Z
```

It overwrites `artifacts/dev/coverage/index.html` and `coverage.json` in the same
folder. Filter/search by menu family, native state or presentation. Planned
menu/state pairs start with availability and page discovery unestablished;
missing adapters do not disappear. These counts are not a percent of the game.
Evidence from another DLL/configuration stays historical. Evidence for one mode
or presentation does not certify any other mode or presentation.

Available automated recipes cover bounded open/close and iDroid fitting; they do
not yet walk every page. Run only an explicitly selected recipe in an owned test
session through `tools/workspace.py bot`. For example:

```powershell
python tools/workspace.py launch-sim
python tools/workspace.py bot --command session --suite tools/gameplay_bot/suites/idroid-fit.json
python tools/workspace.py stop-sim
```

Pause is selected explicitly with `pause-roundtrip.json`; it is never opened by
the default observation command. Release controls, restore test poses/settings,
close test-opened menus and stop owned test sessions on exit. Steam stays running.

The next implementation is exact page/focus/choice observation and guarded
recipes for the discovered branches. Current fast telemetry normally knows
menu/iDroid ownership, not the exact native page or enabled selected choice.
The existing supervisor can review actual final-eye captures and dispatch bounded
semantic VR actions; a broad menu flag cannot safely drive a whole menu tree.
After repeatable simulator traversal, each group receives a physical-headset pass.

The helicopter list/Back baseline is partial field Mission 6 evidence only. Its
selected grades were already developed, so purchase completion remains open.
Another player reported the upgrade lock-up in the ACC; the user did not
personally experience it. The field baseline does not reproduce that report.
Keep ACC iDroid development and ACC helicopter customization as separate
candidate paths until the exact screen is established. Progression-changing
tests require a verified isolated save/checkpoint fixture.

The targeted customization cancel probe is
`tools/gameplay_bot/suites/acc-helicopter-customization-back.json`. It has not
run in game. On a visually reviewed, already-open ACC helicopter selector,
run it with `tools/workspace.py bot --command run --suite <that path>`.
It opens no menu and sends one ordinary Back through the effective VR binding;
exact selector, target, popup/save and input-context guards are checked before
dispatch. Return must reach the native main cabin, not merely clear iDroid's
open bit. Failed outcomes stop; no repeated Back or native state forcing is
used as the test recipe. Development through iDroid remains a separate path.
