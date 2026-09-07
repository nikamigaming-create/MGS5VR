# Grenade pointing prototype — failed visual acceptance

See the [focused follow-up](RIG_FOLLOWUP.md) for the subsequent support-release
and atomic throw-pair corrections, newer SIM samples and the user's instruction
to prioritize clarity. The original failure remains unresolved; this is a draft.

This branch preserves the 2026-09-07 prototype for repair. It is not the installed
candidate and must not be packaged as a passing release. The base branch retains
only the wrist-menu direction correction.

The prototype pairs the native grenade preview and actual-throw requests with a
published right-hand origin and pointing direction. SIM observations reached both
paths and two actual throws. Release build and six automated suites passed before
capture, but the final 15-second recording showed disappearing hands during part
of grenade readiness/throwing. That visual failure blocks acceptance.

The private candidate DLL SHA-256 was
`B631DA47A506FEF7A73D7325AE7F0B26398112480C55D81C01FC402F5F5A71FC`.
The diagnostic recording and sampling limits are described in
[the test review](MENU_GRENADE_REVIEW.md).

Before another runtime pass:

- Repair grenade-ready/throw hand visibility and verify the rig/animation state.
- Origin/velocity fallback is now atomic: both outputs are committed at the
  matched velocity call, leaving native origin intact on a mismatch.
- Check native throw-strength changes across stance and throw modes. The current
  adapter preserves native strength; fixed range from hand tilt alone is not
  established.
- Recheck preview and actual throw against the same source pose, then perform
  final-eye visual acceptance and a physical controller test.

No headset session was launched for this prototype. The SIM was stopped after
capture and the menu-only candidate was restored.
