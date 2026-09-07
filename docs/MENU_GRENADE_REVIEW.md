# Wrist directions and grenade pointing — 2026-09-07

The wrist direction correction is retained. The grenade pointing candidate is
held after its final recording showed missing arms during part of grenade use.
No passing grenade release or full-mod acceptance is claimed.

## Wrist selection

The previous mapping sent right-stick Y into native horizontal browsing. The
first flick now selects the matching D-pad category: up primary, down secondary,
right support, left items. After centering, both axes keep their original signs
and directions for native card selection. B returns to category choice; releasing
left trigger closes. Left-stick movement, item Use and held-fire protection remain.

SIM final-eye observations confirmed rifle-to-right-card and return-left, plus
up/down/right positions in the item grid. The native picker can open late during
equip/stow transitions; early browsing was observed turning the camera before
the picker appeared. That transition case remains open and must not be described
as fully polished. Physical acceptance of this new mapping is pending.

## Grenade candidate — withheld

The prototype pairs native preview and actual-throw origin/velocity requests
with one published right-palm/aim pose. Both native paths were observed: right-hand
yaw and pitch changed the preview, and two actual throws consumed the adapter.
The retained local candidate DLL SHA-256 is
`B631DA47A506FEF7A73D7325AE7F0B26398112480C55D81C01FC402F5F5A71FC`.

Its final silent, left-eye recording is 14.796 seconds, 561,973 bytes and 40
encoded frames. Acquisition lasted 15.156 seconds; capture cadence is approximately
2.64 FPS, not game FPS. All encoded frames were reviewed. Menu directions and arc
changes are visible, but hands disappear for part of grenade readiness/throwing.
The recording is diagnostic evidence, not a passing presentation demo.

Additional sampled right-eye images were reviewed for the picker and grenade
preview. Those sequential samples do not establish stereo timing or physical
comfort. All six automated suites passed on the prototype; that does not override
the visible failure. The grenade changes are excluded from the installed menu
candidate and retained separately for repair.

After removing the grenade changes, the menu-only Release build and all six
automated suites passed. The installed DLL SHA-256 is
`A719B8EAC88EDDEE12B3AA4EE69B1F1E2475CC93C2968DACF683A5676F1389D5`.
This identifies the retained candidate; it is not a new headset acceptance.

The MGSV SIM process and its simulator child were stopped after recording, and
the game's 1920×1080 physical-test resolution was restored. No physical session
was launched. Remaining work: picker readiness/input gating during native equip
transitions, grenade hand visibility, and a fresh physical controller test.
