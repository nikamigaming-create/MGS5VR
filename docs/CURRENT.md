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
- Free Roam is reachable through Missions -> Mission List -> Free Roam. Existing
  VR bindings completed Afghanistan destination/LZ selection, sortie, flight and
  on-foot helicopter exit. The controls guide now describes the native route.
- The authored clavicle now contributes to full-body arm reach. At 50cm forward,
  standing palm error fell from 11.3cm to 1.1mm; crouched error fell from 20.5cm
  to 5.4cm. Continuous arms, posture changes, a short walk, moving rifle support
  and three shots were checked. Crouched/farther reach and headset remain open.
- Earlier tracking-loss weapon continuity and tutorial iDroid ownership repairs
  remain included. Their evidence is scoped, not full-game acceptance.

Production candidate checked in game and installed: `ca16f0e84503`.
Required build and installer checks passed. Exact runs, rejected experiments and
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

Next reach investigation: `private/reach-next-investigation.json` and its native
standing/crouched skeleton snapshots. The native head remains tilted relative to
the tracked view; an offline head/neck fit suggests a remaining correction but
has not been implemented or verified. Do not ship that calculation as a fix.
