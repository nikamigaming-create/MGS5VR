# Prologue bookmarks for local retesting

The private `prologue-escape-bookmark` profile returns to the first playable
hospital escape. This lets development resume after closing MGSV or rebuilding
without replaying the opening and avatar editor. It is a native section bookmark,
not an arbitrary-position save state or a feature in the public nightly.

## Verified destination

| Bookmark | Restored destination | Verification |
| --- | --- | --- |
| `hospital-avatar-current` | Avatar editor, through native saved LoadAvatarPlayer1 sequence27 | `20261111T192745220779Z`; avatar selection and acceptance subsequently advanced |
| `prologue-escape-bookmark` | First playable crawl, EscapeFromAwakeRoom sequence41 | New process9856, run `20261111T194752680054Z`; Ishmael present, rail active, movement unlocked; VR input moved4.051m and triggered Heli |

The escape archive's live capture state says AfterHeliDemo. That is **not** its
restore destination: the saved checkpoint remains EscapeFromAwakeRoom. During
both tested crawl sections, native current-position save eligibility is false.
No manual coordinate/sequence writes were used to reach or restore either section.

The first arrival capture still shows the loading-panel handoff. The later
`restored-hospital-escape-progress-before-*` pair shows settled stereo gameplay.
Its immediate after-capture catches a blank native fade and remains failed;
`supervisor-2566d0f2e2-*` shows the subsequent Heli scene in both eyes. The native
route event records27.828s of movement and the real scene transition.

## Return workflow

These files are private, account-specific game saves under
`artifacts/test-profiles/`. Do not redistribute them or apply them to another
Steam account. Use the account that created the profile; substitute its actual
directory below. `test-saves.py` verifies all six file hashes, requires MGSV to
be stopped and both TPP cloud mirrors disabled, and preserves a rollback before
activation. Steam stays running.

1. Close MGSV normally, or release the owned bot and use `workspace.py stop-sim`
   for a session launched by this checkout.
2. Activate the retained profile:

   ```powershell
   python tools/test-saves.py verify prologue-escape-bookmark
   python tools/test-saves.py activate prologue-escape-bookmark --account '<Steam>/userdata/<account-id>'
   ```

3. Launch `Play-Dev.cmd` for physical Meta Link testing, or
   `python tools/workspace.py launch-sim` for an explicitly owned simulator run.
4. On the hospital title, use the saved VR Start binding, select **Continue**,
   then confirm **Resume Game** after loading. Seeing the hospital title is not
   evidence that the bookmark failed. Do not choose Delete Save Data.
5. After retesting and closing MGSV, activate the automatic rollback profile
   recorded by step2 to restore the campaign that was present before the test.

The11November experiment restored `before-prologue-resume-20261111` and verified
all six original files byte-for-byte, personal settings and Meta64 runtime hashes.
Its cleanup receipt is `artifacts/prologue-resume-20261111/cleanup.json`.

## Remaining save-system work

The requested custom save system still needs a transactional, account/build-bound
bookmark flow, VR selection and validated manual saves where the game permits
them. Prologue rails must explicitly report section restart semantics unless an
exact rail/action restoration contract is proved. Later hospital sequences,
objectives/enemies/quests, PC reboot and physical-headset behavior are not covered
by this proof. The public nightly does not contain a new save feature.
