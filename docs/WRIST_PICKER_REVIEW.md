# Left-wrist equipment picker — 2026-09-07

The equipment carousel used a separate native UI camera from the ammo/status
display. Its cards and descriptions were bypassing the wrist projection and
remaining in front of the player's face. The version-gated adapter now maps that
camera's equipment layers to a readable popup above the left forearm. Status
stays flat along the forearm. The popup follows the wrist and faces the source
head pose; both eyes use the same source transaction and physical plane.

The observed equipment layout camera is Z=150 with draw orders 133–136. The
similarly numbered layers at Z=100 contain unrelated overlays and remain hidden.
Destination labels and the flat reticle are not added to the picker. The small
status display makes one binocular facing decision, avoiding opposite visibility
results when its plane falls between the eyes.

Candidate SHA256:
`53BC9813864BEC0AEC932AC531A6178102FBB9DA60331545472A5B6808F16DBE`.
All five local suites passed. On the existing 1% checkpoint, final composited
left/right captures showed the primary rifle, secondary pistol/bionic-arm,
support equipment and item cards above the left wrist. Right-stick browsing
changed the primary selection to None. These were actual native menus and data.
The full side-category layout can exceed an eye's visible area when the wrist
is held near the edge of view; physical placement and readability remain open.

The same candidate rejects VR activation in the title scene, where a decorative
Snake model previously captured menu input. The title toggle was rejected and
Continue/Resume remained usable; activation then worked on the playable hillside.
The first native stereo scene also waits for a tracked rig publication rather
than exposing the unmodified native arm pose during entry. This does not repair
all sleeve geometry or prove first-person body acceptance.

Native pause/iDroid still use the existing large spatial screen. Keeping a
paused stereo world behind that screen and interactive wrist iDroid are unfinished.
No physical headset acceptance or complete-mod acceptance is claimed here.
