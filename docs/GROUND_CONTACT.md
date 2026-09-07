# Native arm ground contact

This adapter is specific to the profiled TPP 1.0.15.4 executable. It queries the
game's collision scene during the same player skin publication that supplies the
tracked eye frame. It does not infer ground height from the player root or bones.

The native `HumanGroundIk` update uses constructor RVA `0xa0fb50`, synchronous
ray query `0x1b9b130`, and hit point/normal getters `0x1b9a5d0` / `0x1b99910`.
The player IK factory sets inclusion layer `0x800`; the ground path uses filter
`0x80000006` and query flags `0x04000700`. The query owns its internal scene read
lock. Its caller supplies aligned `0x250`-byte storage: count/index at `0x60` /
`0x64`, followed by five `0x60`-byte records at `0x70`. The getters convert
shape-local records to world coordinates. No game collision state is written.

All four function signatures must match before use. Existing player/model/skin
ownership checks still apply. Queries are bounded below the current head, and
returned positions, upward normals, segment bounds and record indexes are
validated. A miss or invalid result supplies no contact; there is no invented
flat floor. There is no collision-plane cache or extra render-pose prediction.

The shared shoulder frame lifts as one unit when necessary. Wrists stop above
the returned surface. An elbow contact chooses a feasible point on the two-bone
solve's elbow circle, preserving bone lengths. Fully attached support moves
with the primary weapon. Standing poses without contact keep their previous
placement. This handles ground support; it is not a swept hand/weapon collider
and does not establish wall obstruction, all terrain, outfits or physical fit.

Nine rotated-slope regressions cover buried elbows, preserved bone lengths,
unchanged clear wrists and invalid normals. In SIM, the reproduced hillside
prone pose previously buried the hands; both final eyes now show them above the
native surface. Crouch, wrist turns, support and return to neutral were also
inspected. See [the visual review](SIM_HUD_REVIEW.md) for exact build and clip
limits. Close garment edges and the complete rig remain open acceptance work.
