# Acceptance requirements

Updated 24 October 2026. These are evidence requirements, not a list of missing
implementations. [FIELD_FIXES.md](FIELD_FIXES.md) owns code state;
[RELEASE_PLAN.md](RELEASE_PLAN.md) owns the ordered combined review.

## Record claims separately

1. Implementation: production path/caller and relevant isolated contracts.
2. Native behavior: real game state, inputs and visible result on an exact build.
3. Final output: both submitted eyes, including motion and transition intervals.
4. Physical headset: actual Quest Link feedback for scale, reach, readability,
   stereo comfort, tracking and controller fit.

Mark pass, fail or unproven within tested scope. A helper, input dispatch or
source observation does not prove visible contact. Missing headset coverage
does not mean a repair is absent. Preserve failures, scene limitations,
candidate/configuration identities and cleanup.

## Combined review

Exercise RELEASE_PLAN's body/movement, combat/recovery, mounted/contextual,
iDroid/progression and cabin groups. Preserve native leg lengths, left
bionic arm/HUD/iDroid, physical binoculars/sights, native restrictions and
personal controls. Review opposing head/hand movement, owner changes,
stow/reopen, tracking interruptions and neutral returns, not just still frames.

Existing catalog thresholds remain 180 seconds/five cycles for sustained
feature claims and a 600-second integrated soak. A compact single-eye demo
cannot replace these, both-eye internal review, full missions or physical
feedback. Record capture cadence/drops separately from native frame times and
fresh XR submissions/repeats; no interpolation or performance inference from
recording FPS. Keep desktop 1280 x 720 and runtime per-eye dimensions/FOVs.

Mission acceptance needs native objective completion and reliable next-scene
control under [MISSION_ACCEPTANCE.json](MISSION_ACCEPTANCE.json). Unvisited
missions and weapon/configuration combinations remain unproven. Debug
progression bypass cannot count as ordinary mission completion.

Release inputs, restore temporary settings/poses, close test-opened menus and
owned sessions/readers. Leave no background runner; keep Steam running. Retain
reviewed evidence before relying on it; run retention is not build promotion.

## Historical matrix

[ACCEPTANCE_HISTORY.md](ACCEPTANCE_HISTORY.md) preserves the older mixed-date
matrix. Its old "not implemented"/"current result" cells are not today's code
audit. Update active ledgers with actual new evidence rather than rewriting
historical results.
