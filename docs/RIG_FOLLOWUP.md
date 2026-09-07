# Focused rig follow-up — 2026-09-07

The user reports that normal headset arm behavior was mostly good and asked to
prioritize distant image clarity. Broad pose and ground-placement changes are
not retained from this follow-up.

One targeted correction keeps a support-release blend anchored to the last
presented contact instead of chasing a newly changing native stow animation.
Menu entry, lowering, non-firearm selection and tracking loss release contact
immediately. Native reload contact remains available while already attached.
Regression cases cover acquisition, animation changes, reload, release and reset.

The grenade adapter now writes origin and velocity together at the verified
velocity call. An unmatched request leaves the original origin unchanged; there
is no timeout path that mixes a modified origin with native velocity.

The diagnostic DLL was
`3496A4E2AD700CCAFC059F8B5A574BA998F7EB27DF1D182EB2CE879887776C28`.
Release build and all six automated suites passed. Sampled final eyes retained
both hands during grenade readiness and hand-directed aiming; a sampled actual
throw also retained them. This did not reproduce or resolve the older low-pose
failure, and it is not continuous or physical acceptance of the new changes.
The installed menu-only baseline was restored after the SIM stopped.

The earlier disappearing-arm clip remains diagnostic evidence. A possible
uphill ground-query issue was investigated, but its proposed change was removed
before another runtime test. Do not claim that hypothesis was confirmed.
