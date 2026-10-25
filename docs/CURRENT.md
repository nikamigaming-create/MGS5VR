# Current MGS5VR work — 24 October 2026

**Body/arm batch is in progress on main; next is its combined build and smoke.**
The retained reproduction was reused. Upper sleeves now use the shoulder-sized
head-clearance envelope along the whole upper arm, while forearm/hand clearance
stays unchanged. Forearm-only cuffs close supported cut boundaries with existing
native vertices and LOD-exclusive removed triangles; no new vertex streams.
Runtime smoke and physical Quest review are pending; no acceptance is promoted.
See [RELEASE_PLAN.md](RELEASE_PLAN.md).

## Source and retained builds

| Item | Audited identity/status |
| --- | --- |
| Checkout | One local branch/worktree, `main`; gameplay source `5622052bd8799603ef8aae66235ec82d3c7bce6f` |
| Development | DLL `17b044cba72e`; existing October 23 report: 58 groups passed, zero failures |
| Published play | DLL `b627c7cacc9a`, source `f8cf03f949ae`; October 19.1 baseline |
| Integrity | Both slot manifests matched all listed files; full development source identity matched before this documentation pass |
| After cleanup | Documentation-only working changes; packaged docs/source identity need refresh in the next planned build. No runtime binary changed. |

Two cached remote-tracking refs, `contributions/silent` and `contributions/vinion`,
remain historical references, not extra local development branches. Their
worker/filter differences were reviewed: [fork review](FORK_REVIEW_2026-09-20.md).

## Already implemented

Fixed-length legs/shared pelvis fit; clavicle reach, wrist isolation and
cross-head fallback; tracked garment corrections; acquired support/reload
retention; tracking continuity; contextual yaw preservation; cover
aim-out/exit/control classification; transit body/exit height; left iDroid
contact/projection/lifecycle/scene handoff; Rocket Arm steering/display;
paper/marker/printing/settling; native ACC rifle carry/return; sortie head.
Physical binocular intel and village marking use the existing native-compatible
VR route. [FIELD_FIXES.md](FIELD_FIXES.md) maps every FP report to source.

## Remaining release decisions

- Body first: rough forearm cuff and extreme cross-head sleeve crowding.
  Forearm cropping admits identified sna5 geometry only. Never stretch legs or
  restore the removed startup-failing cuff reconstruction.
- Mounted aiming admits mortar `0x46` and machine gun `0x47`; additional types
  need verified identities/contacts. Latest local Mother Base handoff reaches
  the adjacent platform and fails entry at an intact AAG. Diagnose eligibility;
  do not repeat approach loops, infer `0x45` or override native restrictions.
  Destroyed hostile fixtures cannot prove a control defect.
- Exact rock/corridor reports remain undiagnosed. Separate native movement from
  presentation before changing either.
- Acceptance gaps include forced iDroid lessons/readability in motion,
  bolt-action support, mounted contact/impact, contextual headings, powered-arm
  impact, outfits/loadouts and physical Quest. These are not all missing code.
  FP13/FP14 remain scoped resolved.
- Five catalog gates remain conservatively blocked pending reconciliation of
  historical failures with later repairs/evidence. No gate was waived by docs.

## Resume and cleanup

Use one bounded investigation, one related correction batch and one combined
review, as specified in RELEASE_PLAN. Reuse passing evidence where code/scope
match. Do not repeat ordinary ACC toggles, both-rifle tracking recovery or village
marking by default.

Last recorded gameplay session was cleaned up. This audit/documentation pass
launched no game/harness and performed no build, deployment, pin or release.
Both Play commands retain Meta Quest Link. Published `play/` is unchanged.

Exact history: [previous current log](CURRENT_HISTORY_2026-10-23.md),
[field narrative](FIELD_FIXES_HISTORY_2026-10-23.md),
[structured ledger](FREE_PLAY_ACCEPTANCE.json). Use these for evidence, not
competing execution orders. [Documentation map](README.md).
