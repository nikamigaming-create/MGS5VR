# Current MGS5VR work — 24 October 2026

**Release completion is active on main.** The user requested all remaining fixes,
then combined testing, and an explicit release-readiness goal. The previously
checked candidate below remains available while the next batch is prepared.
Do not publish or advance the published pin during preparation.

The next batch flattens the forearm-only cut rim and separates cap normals/UVs
using dead native vertex slots. Resource sizes, skin weights and distal forearm
positions remain native; all three streams change only around admitted player
color draws. Build and final-eye review are pending. Additional mounted types
and terrain reports still need a diagnosed native cause before changing controls
or collision. The user clarified the location only as “terrain”; representative
slope/obstacle/passage review is useful but cannot establish an exact reproduction.

## Candidate and delivered batch

- Development DLL: `1a64bb965bc61e06423519d5940f6c5b5c7f018bc233e107098533a5b4325b7e`.
- Gameplay source: `439253632bd8aec2a5c78fe25359c8549a2608d6`; batch implementation
  is `c2caa68`. Later outcome documentation does not change this runtime binary.
- Published play remains `b627c7cacc9a` / October 19.1. No publication or pin change.
- Upper sleeves now use the existing 18 cm shoulder envelope along the upper arm,
  rather than the 12 cm forearm envelope. Forearm/hand clearance stays unchanged.
  Native arm/leg lengths, wrist isolation, garment drivers and hand owners remain.
- Forearm-only cuffs close supported native cut loops using existing vertices,
  skin weights and material. Only removed triangles exclusive to that LOD are
  reused; overlapping draw ranges and ambiguous boundaries are not rewritten.
  The game reports 55 added cuff triangles without the prior startup failure.

## Combined smoke and limits

All 58 check groups and installer checks pass. The first build's only failure was
an omitted source-inventory entry for the new helper; it was corrected before
this candidate was staged. No gameplay code changed between those build attempts.

One native simulator session produced three retained runs:

| Run | Observed result |
| --- | --- |
| `20261025T024142216582Z` | Full-body right/left cross-head reach and neutral return pass. First iDroid open reaches the existing native help screen, interrupting the ordinary-menu expectation. Run remains failed. |
| `20261025T024303487697Z` | Existing guarded help dismissal, stow, reopen and turned handset pass. Daily Bonus interrupts final stow expectation; cleanup returns to cabin. Run remains failed. |
| `20261025T024405362489Z` | Forearm-only right/left reach/return and left iDroid front/turned/stow: all seven cases pass. Both-eye cuff and handset captures reviewed. |

Caps close the exposed end; **the outer rim remains uneven and the reused native
texture stretches across the cap**. Extreme full-body reach still puts cloth
close in view. Other outfits and nonmanifold lower-LOD cap boundaries remain
unsupported. These are real visual limits, not a polished/full-body acceptance
claim. Physical fit, comfort and handset readability still need the user's test.
No release gate or sustained/headset acceptance counter was promoted.

## Next and cleanup

Test this candidate with full body on/off, cross-body reach and return, then
left-hand iDroid open/turn/stow on wired Meta Quest Link. Record feedback against
`1a64bb965bc6`. Continue the remaining defect batch from [RELEASE_PLAN.md](RELEASE_PLAN.md)
using [FIELD_FIXES.md](FIELD_FIXES.md); do not restart already implemented repairs.

Full-body preference restored to 1. Owned game/simulator and test runners are
closed; Steam retained. The per-game runtime is restored to Meta Quest Link.
Both Play paths remain ready. Evidence is retained in the structured ledger;
initial Continue scratch expired and is not promoted as acceptance evidence.

[Field implementation map](FIELD_FIXES.md) · [Durable plan](RELEASE_PLAN.md) ·
[Evidence ledger](FREE_PLAY_ACCEPTANCE.json) · [Documentation map](README.md).
