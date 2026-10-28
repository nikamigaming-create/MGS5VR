# Nightly builds

[Download nightlies](https://github.com/nikamigaming-create/MGS5VR/releases?q=nightly-&expanded=true)
· [Reviewed release](https://github.com/nikamigaming-create/MGS5VR/releases/tag/experimental-2026-10-19.1)

Nightlies are opt-in experimental snapshots of committed `main`. They make recent
fixes and features available between reviewed releases without a separate
announcement for each build. They can contain regressions. A successful build is
not physical-headset, full-mission or complete gameplay acceptance.

Open the newest nightly and download its **MGS5VR-nightly-… ZIP**. Extract the
complete folder and run **Install.cmd** or **MGS5VR-Launcher.exe**. The source ZIP
is for developers, not installation. Your legally owned game data is imported
locally; public packages contain defaults, not personal settings or saves.

Each nightly includes the exact commit, downloadable source, SHA-256 checksums,
file manifest, Windows test results, change comparison and known limitations.
Include the nightly tag, headset/runtime and reproduction steps in bug reports.

## Cadence and checks

GitHub Actions checks daily at **10:37 UTC** (03:37 Pacific daylight time / 02:37
Pacific standard time). GitHub can delay scheduled jobs; this is not an exact
delivery deadline. Maintainers can also run **Actions → Nightly release → Run
workflow** on `main`. No local PC, game or Codex session needs to remain open.

The workflow skips a commit already published as a nightly. It does not commit or
push local work automatically. Commit and push complete development batches to
`main` to make them eligible. A failed check leaves the previous download intact.

The build runs the Windows CTest suite except `d3d11_mailbox` and
`independent_render_window`, which require real GPU hardware. Python, inventory,
nightly failure-path and packaged-installer checks also run. Packaging checks the
clean source identity, complete install inventory, public defaults, licenses,
source archive and every payload hash. These hosted checks do not run MGSV,
Meta XR Simulator or a physical headset.

Publication first creates a draft, uploads the payload and downloads it again to
compare every asset hash. Only then does it publish a prerelease with
`latest=false`; public assets are verified again. Technical tags have the form
`nightly-YYYY-MM-DD-<12-character-commit>`. Published nightlies are not overwritten
or assigned formal version numbers. Historical nightly assets remain on GitHub;
CI staging is temporary and does not add a local retained build slot.

## Known issues and stopping publication

`config/nightly-policy.json` records the exact existing blocker descriptions
accepted for experimental distribution on 27 October 2026. New or changed
blockers stop publication until reviewed and explicitly accepted in that policy;
resolved blockers can disappear without a waiver. The release notes disclose the
remaining blockers and link to the full field-fix record. This policy does not
clear acceptance boxes or alter the reviewed-release gate.

Set `enabled` to `false` in that policy, or disable the **Nightly release** Actions
workflow, to stop publication. Record newly found severe regressions in the
catalog and hold publication while repairing them. Automation cannot detect an
unreported gameplay defect. Re-run a failed publication job to recover its exact
draft; an existing published release is verified without replacing its assets.

Nightlies never call `pin-build`, deploy to a local game or change `play/`.
`Play.cmd` keeps the reviewed pin and `Play-Dev.cmd` keeps the checked development
slot. Promotion of a reviewed release remains an explicit separate decision.

## Announcement

MGS5VR nightly builds are now available! If you want the latest fixes and features
between regular releases, you can grab an experimental nightly. New builds are
published daily when there are changes and the automated checks pass. These are
work-in-progress builds, so expect rough edges and check the known issues. Please
include your nightly version when reporting bugs.

https://github.com/nikamigaming-create/MGS5VR/releases?q=nightly-&expanded=true
