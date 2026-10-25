# Documentation map

Updated 24 October 2026. Start here instead of reading dated reports as current
instructions.

| Document | Authority and purpose |
| --- | --- |
| [AGENTS.md](../AGENTS.md) | Effective local development and VR interaction rules. |
| [CURRENT.md](CURRENT.md) | Short live handoff, build identities and next action. |
| [RELEASE_PLAN.md](RELEASE_PLAN.md) | Ordered, durable release execution plan and decision gates. |
| [FIELD_FIXES.md](FIELD_FIXES.md) | User reports mapped to implemented repairs, actual limitations and remaining acceptance. |
| [FREE_PLAY_ACCEPTANCE.json](FREE_PLAY_ACCEPTANCE.json) | Exact retained field evidence; implementation state is separate from acceptance. |
| [REGRESSION_COVERAGE.md](REGRESSION_COVERAGE.md) | Catalog semantics, historical blockers and packaging gate. |
| [ACCEPTANCE.md](ACCEPTANCE.md) | Evidence requirements, not an implementation backlog. |
| [CONTROLS.md](CONTROLS.md) | Public defaults; the installed personal bindings remain authoritative. |
| [ADVANCED_SETUP.md](ADVANCED_SETUP.md) | Install, runtime and source-build workflow. |
| [GAMEPLAY_FEATURES.md](GAMEPLAY_FEATURES.md) / [VR_FEATURE_CHECKLIST.md](VR_FEATURE_CHECKLIST.md) | Broader feature inventory; absence of complete coverage is not absence of code. |
| [MISSION_ACCEPTANCE.json](MISSION_ACCEPTANCE.json) | Per-mission coverage; a scoped interaction cannot close a mission. |

Published release notes describe their named releases. They must not be rewritten
to claim later repairs. Dated reviews, capture reports and files labelled
historical retain the evidence and decisions of their recorded builds.
CURRENT_HISTORY, FIELD_FIXES_HISTORY, STATUS_HISTORY, REGRESSION_COVERAGE_HISTORY
and ACCEPTANCE_HISTORY preserve the pre-cleanup records. The chronological user
directions are retained in [RULES_HISTORY.md](RULES_HISTORY.md); only the effective
rules in AGENTS apply now.

When updating status, distinguish **implemented**, **partial implementation**,
**reproduced failure**, **unreproduced report**, **scoped verified**, and
**acceptance pending**. Do not count acceptance gaps as unfinished repairs.
Record exact build/run scope; preserve failures; never infer headset or whole-game
acceptance from source inspection, unit counts or simulator endpoints.
