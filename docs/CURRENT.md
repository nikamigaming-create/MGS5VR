# Current MGS5VR work — 21 October 2026

Work only in this checkout on `main`. Local and origin have only main; earlier
branch tips are archive tags. No open GitHub PR remained to merge. All current
source work has been consolidated and pushed.

## Current result

- Native sortie previews show Snake's complete head in both eyes and return to
  clear first-person play. FP14 is fixed in development.
- Rifle support fingers curl through moving aim and three shots. Native reload
  and subsequent reacquisition retain the curled grip. Broader FP02 stays open.
- Bare-hand right-wrist rotation no longer steers torso heading. The previous
  8.5–12cm opposite-palm displacement fell to at most 1.13cm in two repeats;
  residual movement follows native idle animation. Arm reach/headset stay open.
- Earlier tracking-loss weapon continuity and tutorial iDroid ownership repairs
  remain included. Their evidence is scoped, not full-game acceptance.

Production candidate checked in game: `c2b3f553cbb8`. Recorded runs and precise
limits are in [FREE_PLAY_ACCEPTANCE.json](FREE_PLAY_ACCEPTANCE.json).
The complete unresolved list is [FIELD_FIXES.md](FIELD_FIXES.md).

## Cleanup and next work

Owned game/simulator closed with neutral input. No test menu, supervisor or
Python runner remains. Steam was retained. Published `play/` remains unchanged;
`build/dev/` is the candidate. Both Play paths retain Meta Quest Link selection.
Do not pin development or claim headset acceptance from these simulator runs.

Cover step-out remains reproduced and unfixed: raising the rifle moves the
native actor/view about 0.92m. Native state-transition ownership still needs
resolution; neither a camera counter-offset nor blanket cover disabling is a
completed fix. Private static investigation: `private/cover-native-investigation.json`.
Other unresolved reports include Y-action rotation, arm reach, physical floating,
iDroid alignment, mounted weapons, small-rock traversal and cabin interactions.
Continue actual defect fixes; do not create another harness project or stop on
another narrow pass as if the whole release were complete.
