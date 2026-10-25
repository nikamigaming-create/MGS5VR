# Regression coverage and release gates — 24 October 2026

The [catalog](../tests/vr_regression_catalog.json) inventories 48 families:
37 gameplay, six runtime and five tooling, mapped to 58 CTest groups.
Inventory is not gameplay completion. [FIELD_FIXES.md](FIELD_FIXES.md) records
the code audit; [RELEASE_PLAN.md](RELEASE_PLAN.md) owns execution.

## Historical failures versus implemented repairs

The prior catalog called an October 2 investigation the current candidate.
Main now contains the later rig, left iDroid contact/projection, stow,
authored-scene handoff and tutorial-notice repairs. Development `17b044cba72e`
comes from `5622052`; published play is `b627c7cacc9a` / October 19.1.

Five release gates remain conservatively blocked: hand-arm-rig, idroid-mount,
idroid-lifecycle, idroid-native-guides and performance-pacing. Historical
failure scopes are retained with explicit audit/reconciliation annotations.
**Blocked here does not mean missing repair code or prove the old failure
persists on the audited candidate.** No acceptance record, gate or sustained
counter is promoted by cleanup. Reconcile retained later evidence and the
combined candidate review; source cannot certify ergonomics or performance.

## Gate operation

`python -B tools/regression_catalog.py` validates mappings and evidence.
`python -B tools/regression_catalog.py --release` additionally refuses blocked
features. Packaging uses that gate. Do not remove a failure to make it green or
use `--allow-known-issues` as routine cleanup. A reviewed explicit experimental
release decision may disclose specific blockers; integrity checks still apply.

Accepted scoped/continuous/headset/soak claims need matching candidate and
retained acceptance records plus public report markers. Existing thresholds:
180 seconds/five cycles per sustained feature and a 600-second soak. No promoted
continuous/headset/soak passes are currently recorded. This does not erase
scoped gameplay repairs.

[REGRESSION_COVERAGE_HISTORY.md](REGRESSION_COVERAGE_HISTORY.md) preserves older
measurements and decisions on their original builds.
