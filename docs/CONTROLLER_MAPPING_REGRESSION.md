# Controller mapping walkthrough and regression

The October 11 local review covers the complete launcher control catalog and a
bounded retail-game iDroid session. The narrated walkthrough is
`artifacts/dev/controller-mapping/controller-mapping-showcase.mp4`. It contains
actual launcher screens, a recorded save/reload operation, enlarged editor
callouts, arrows, side-by-side comparisons, chapter wipes, captions and a
separately labeled final-eye iDroid recording. Narration is synthetic.

The companion `regression.md` and `regression.json` in that directory record
the exact evidence and limits. `complete-control-index.csv` lists every saved
action and stick assignment; `video-chapters.txt` and `narration.txt` provide a
searchable reference. The MP4 also has chapter metadata.

## Results

- All 55 automated groups and the installer checks passed.
- The refreshed real launcher HTML and native settings bridge passed 217 checks:
  all 104 actions, 59 settings, 29 selectable inputs, 10 separate touch contacts, 10 axes and
  eight context references. Alternative bindings, five gesture types, all
  axis sources, conflicts, stale saves and the minimum launcher width were
  exercised. A disposable settings copy owns the tutorial's edits.
- Two entry cases and 18 supervised cases passed their recorded predicates in
  the actual retail ACC. The iDroid opened in the LEFT bionic hand, moved with
  its projection, accepted focused Next/Previous page inputs, stowed, reopened
  on the next tap and stowed again. Both final-eye outcome images were reviewed.
- Equipment/Commands ownership and release recovery, plus physical binocular
  latch/stow, passed in the ACC. These do not establish item selection, an
  available buddy command, enemy acquisition or marking in the field.
- A later ordinary field take on `3e6e3d6e9818` separately passed current
  RIGHT-stick tabs, the retained RIGHT-grip loop, focused LEFT-stick row
  navigation, LEFT movement with the open handset and grip released, stow and
  next-tap reopen. Physical binocular acquisition, A clear, look-away/return
  without reacquisition and explicit RIGHT-trigger re-marking passed in that
  same take. Both eyes were reviewed for all 14 outcomes. See
  `artifacts/dev/idroid-stick-dog-current/markers-tabs-final/visual-review.json`.
- The earlier candidate has retained Music checks in both spatial
  and handheld modes. See `artifacts/dev/music-stop-current/music-pause-review.md`.

The fresh runs are `20261011T134433036774Z` and
`20261011T134534399362Z`. The separate continuous iDroid source is
`artifacts/dev/controller-mapping/idroid-final-eye/simulator.mp4` with its
`capture.json`. Its 144 frames sample roughly 3.6 frames/second; the 30 fps
tutorial export does not measure game or headset frame rate. The source video
is left-eye only. Stereo outcome review uses the paired captures in the runs.

## Using the editor

Choose **Controller bindings**, search for the action and read its context.
Inputs within one option form a chord; separate alternatives are OR choices.
Select **Light touch** to bind a contact rather than its press/squeeze. Choose
the gesture, save, then release controls and center the sticks for about two
seconds. Lift touch contacts used by the old or new bindings as well. Refresh
the saved bindings, inspect the guide and verify the real action in game.

Thumb rests are currently unbound. The walkthrough demonstrates LEFT thumb-rest
contact as an optional **While held** alternative for the wrist equipment
picker, retaining LEFT trigger. It does not apply that example to personal
settings. Face contacts, stick-top contacts and trigger contacts are also
separate selectable inputs. Runtime profile support and physical sensor
acceptance still require the player's controller.

The current personal layout uses LEFT Menu tap for iDroid and LEFT grip for
handheld focus. LEFT stick moves Snake without focus and browses menu lists
with focus. LEFT grip + RIGHT stick left/right selects Previous/Next tab;
those chords consume horizontal map panning. RIGHT grip retains the Next page
loop and LEFT grip + LEFT stick click remains a Previous alternative. Menu tap
and hold share a 550 ms threshold. The installed personal file, rather than
the repository template, determines the player's effective layout.

## Fixes and remaining coverage

This task corrected the startup test runner: it now recognizes the exact
observed Online Information **Next/Close** selector, with at most eight bounded,
released confirmation edges for successive information pages. A first-run agreement has a
different native mode and is rejected by this guard. Regression tests cover
wrong ownership, selection, scene and stale samples. The revised runner
reached Continue and the ACC; no new Music/gameplay DLL change was needed.

An initial Help-close test falsely required the native Back button to remain
down after the Help card had already closed. Its failed result remains failed;
the corrected case asserts the real close outcome and passes in the retained
entry run.

Full missions, every item/weapon/vehicle, mounted horse actions, continuous
moving-vehicle handling, all forced tutorials and physical wired Meta Quest
Link acceptance remain open. Earlier handheld Music evidence also retains an
unresolved observation of a very fast close tap during native reopening; the
fresh settled reopen/stow sequence is a narrower pass. Do not promote this
mapping review to whole-game or headset acceptance.

The owned game/simulator session is closed, inputs and test menus are released,
Steam remains running, the simulator lease restored Meta OpenXR and the exact
personal controls bytes were restored. The pinned `play/` build is unchanged.

## Reproducing the local work

`tools/build.ps1 -SkipBootstrap` builds/checks development. The browser audit is
`tools/qa-controller-mapping.cjs`; it needs Playwright on `NODE_PATH` and the
installed Edge executable. `tools/record-controller-remap.cjs` records the
disposable browser example. Neither uses desktop/window automation.

`tools/run-controller-mapping-regression.py` owns a bounded development
deployment and temporary handheld setting, and restores/closes them in
`finally`. It requires the reviewed spatial starting preference. The retained
`live-showcase-plan.json` and `acc-context-plan.json` are observation-bound
plans; inspect fresh native state and both eyes before submitting a plan.

Prepare and render the tutorial with:

```powershell
python tools/controller-mapping-film.py --prepare `
  --editor-clip artifacts/dev/controller-mapping/controller-remap-browser.mp4 `
  --clip artifacts/dev/controller-mapping/idroid-final-eye/simulator.mp4
python tools/controller-mapping-film.py --render
```

The renderer refuses an early media EOF instead of freezing a clip. Gameplay
source clips must be separately reviewed and keep their original evidence and
capture metadata. These commands do not pin a build or certify hardware.
