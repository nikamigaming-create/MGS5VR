# Current MGS5VR work — 24 October 2026

**Release readiness is the active goal on main.** The user requested the whole
remaining fix batch before the final combined smoke. Do not stop after another
narrow body case or redo repairs already implemented in [FIELD_FIXES.md](FIELD_FIXES.md).

## Checked development and published play

- Development DLL: `250884f4c15699c2c7307e8bf9db87dd949e16bf24aa60641b36a2c8440e0b5b`.
- Gameplay source: `21799fd3462463d7f29be136cfa7ba1c4914712c`; implementation `3cccc52`.
- All 58 check groups and installer checks pass. One integer-width compiler
  warning in the new helper was corrected before this checked candidate.
- Published play remains `b627c7cacc9a` / October 19.1. No publication or pin change.
- Current documentation may be newer than the checked gameplay source. Do not
  rebuild for prose alone; include it with the next justified code batch.

## Current correction batch

Forearm-only cut rims fit the native elbow plane. Caps get separate normals and
one native UV sample, reusing vertex slots absent from retained triangles in
all LODs. Original skin weights, buffer sizes and distal forearms remain native.
Overlapping LOD ranges retain exclusive cap triangle slots; touching closed
boundaries are split into simple loops. Position, attribute and index variants
are selected together only around the admitted player's color draw and restored
afterward. Native shadows and full-body geometry keep their original buffers.

The live game creates 107 cuff triangles without a startup failure. **Final-eye
acceptance of this new cuff is still pending.** The earlier 18 cm upper-sleeve
clearance and fixed native arm/leg lengths remain unchanged. Other outfits are
still outside the exact geometry admission; do not claim a universal outfit fix.

## Remaining diagnosis before final smoke

- Terrain: the user clarified the location only as “terrain.” The current native
  Afghanistan approach traversed descending terrain without reproducing the
  reported small-rock stop. A route deadline and combat interruption are not
  evidence of a collision defect. The exact rock/corridor reports remain open.
- Mounted: the field anti-air gun is loaded and intact. Existing code admits
  mortar `0x46` and machine gun `0x47`; Gatling `0x45` remains unconfirmed live.
  Address nearby threats and observe actual native entry/contact before changing
  admission. Do not repeat the old broken-gun attempt or guess an action ID.
- Existing firing works in the current field session: two patrol soldiers were
  incapacitated after physical VR discharge. Rifle discharge also occurred;
  blocked terrain rays and moving-target aiming deadlines are not new gun defects.
- Native online sign-out interrupted one route; its reviewed notice was closed.
  This was not a movement/control repair.

All runs remain scoped diagnosis until reviewed and retained. The final combined
smoke is deferred until the remaining justified fixes have been batched. Preserve
native restrictions, physical optics, left forearm HUD and left-hand iDroid.

## Active session and handoff

The owned simulator/game session is currently active for bounded field diagnosis.
Do not describe it as cleaned up yet. Every run releases its inputs; finish by
restoring temporary preferences, closing test-opened menus and owned processes
with `workspace.py stop-sim`, and restoring Meta Quest Link. Keep Steam running.
The published pin must remain unchanged. Physical Quest acceptance is pending.

Current working summary: `artifacts/release-completion-progress.json`.
Previous candidate `1a64bb965bc6` and its retained reach/iDroid outcomes remain in
`artifacts/body-arm-batch-review.json` and the evidence ledger; those observations
must not be relabeled as acceptance of the new cuff.

[Durable plan](RELEASE_PLAN.md) · [Field implementation map](FIELD_FIXES.md) ·
[Evidence ledger](FREE_PLAY_ACCEPTANCE.json) · [Documentation map](README.md).
