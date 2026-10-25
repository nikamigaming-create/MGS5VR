# Native body and view

Current implementation audit: [FIELD_FIXES.md](FIELD_FIXES.md). Native thigh/shin lengths remain fixed; pelvis fit and garment drivers are implemented. Forearm-only cropping admits identified sna5 geometry; rough cuffs, other outfits and physical acceptance remain. Follow [RELEASE_PLAN.md](RELEASE_PLAN.md); earlier measurements below are scoped evidence.

The headset gear tab exposes the complete gameplay tuning catalog, including
full body and cinematic movement. Menu + left stick click opens the panel.
Changes are validated and confirmed, saved into the personal controls file,
and acknowledged after the runtime applies them with neutral controls.

The native camera owner, source skeleton and submitted eye pose remain one
transaction. Headset motion is applied directly. The standing, crouched and
prone states come from the retail game's own double-buffered PlayerStatus
table. Live Lua registration confirmed STAND=4, SQUAT=5 and CRAWL=6. The view
holds a stable head offset within each state, follows a bounded stance
transition, and retains native root movement for terrain/stairs. Repeated
camera/skin resolves in the same millisecond no longer reset the filter.
Snake turning toward movement no longer rotates the eye position around his
root. On-foot full-body eyes use the anatomical fit instead of the older
35 cm rearward / 25 cm upward shoulder fit.

Owned face0, face3 and face4 agree on the anatomical eye midpoint: 87 mm above
and 106 mm in front of `SKL_004_HEAD` in its native +Y-up/+Z-forward basis.
The earlier provisional 60 mm up / 40 mm behind fit put the view 146 mm behind
the eyes. Camera placement shares the measured constant with body fitting and takes the
anatomical eye from the owned original skin transaction. The cached camera HEAD
basis agrees in standing but differed by 107.6 mm from the animated head in a
real prone snapshot. The inverse body fit uses the authored head orientation,
including prone pitch; cache orientation cannot replace that original bone.
These landmarks establish placement, not visual or whole-game acceptance.

The visible on-foot body is the player's actual loaded skin. Its verified
lumbar/chest/neck chain uses two-segment inverse kinematics, keeps native
segment lengths and retains native gait ownership. The complete on-foot render
pose receives torso yaw and translation around the measured anatomical eyes
before the limited spine/arm solve, including the rendered pelvis. A fixed
waist pivot made the eyes orbit during a turn and exceeded native spine reach.
Hips and legs now share the same initial body placement. Native game actor
movement and collision are unchanged.
The torso heading comes from the same accepted native camera and snap turn,
before physical headset look. Standing and crouched neck look has a 50-degree
range before the torso follows the excess. Prone neck look keeps the crawl
heading; headset-only turning no longer rotates the feet around the eyes.
Camera posture offsets use the root and lateral chest heading from that same
original skin. The lateral axis remains meaningful when the prone chest pitches
through vertical. Terrain checks compare original and retargeted foot contacts
and retain native gait lift on the new surface. The actual owned animation
skeleton measures 151 mm and 309 mm; the 3 mm waist/spine pivot remains native.
Chest descendants follow the solved torso; neck/head descendants follow the
solved neck. Existing tracked arm, elbow hinge, twist
helper, finger and weapon/contact solves remain native. Wrists and elbows
have camera clearance; native floor queries constrain feet and prone arms.
Prone shoulder contact bends the chest around a fixed neck endpoint instead
of lifting the head and rejecting the complete body for missed eye alignment.
When that fixed endpoint makes the spine almost straight, the rendered pelvis
advances only the minimum required clearance, bounded to 12 cm. This preserves
native segment lengths and the pre-adjustment foot targets; leg IK compensates
the small hip shift without dragging the planted feet. Native actor collision
and movement remain unchanged.
The native leg animation supplies gait, with constrained hip/knee/ankle solves
and downstream foot transforms. A head lean cannot move the game actor or
stretch limbs to unlimited reach.

Only a complete admitted solve can reveal the body. Unknown skeleton chains,
unavailable tracking or an unjoined source frame never authorize a visible
partial body. An unavailable optional body solve retains the established hands
and optics rather than abandoning their publication. Field-body support does not certify seated cabin, horse or
vehicle avatars; those contexts preserve the established native arm paths.
Head geometry/NVG concealment and native shadows retain their existing
ownership. Full body can be switched off in the gear tab.
Both the fitted spine and the final completed skin must place the anatomical
eyes within 2 cm of the submitted camera before body visibility or body-mounted
prop joins are admitted. Final head/eye poses and displacement are reported
from that same skin transaction. Terrain correction preserves the original
foot's clearance on the retargeted surface; sole contact, slopes and gait require
final-eye review independently of the eye bound. Rejected body frames retain
their cause and eye displacement for investigation while preserving hands.

An authored cinematic uses its original actor skeleton instead of moving
Snake's shoulders and hands to the spectator camera. Concealment is restored
at the native visual publication boundary even when the shot has no player
camera owner. A newly published authored player head within 30 cm of the
accepted viewer permits concealing only that player's modular head/NVG or
integrated head subtree in the
main view. Its torso, limbs, native animation and shadow flags are retained;
the complete head returns when the viewer leaves that volume. Both original
skin and accepted view must be fresh and share the activation. HMD looking
remains direct; left-stick spectator movement and
the configured turn axis are independent of actor movement in noninteractive
shots. Camera cuts clear the translation offset. Pause, input focus, loading
and interactive native look lessons retain their established precedence.

The two hospital look lessons consume the player's native look-input history.
Physical HMD look gestures now supply that history only while the exact
native lesson has a fresh heartbeat and VR owns the focused view. A held head
angle supplies a bounded two-second pulse so the one-second native history
window can recognize the gesture, then releases until the head returns to
neutral. A fixed authored eye anchor prevents the
resulting stock look from applying the same HMD turn twice. Menus, tracking
loss and leaving the lesson cancel this route. Native tutorial outcome/timers
remain in charge; no sequence or success flag is written by this bridge.
Actual fresh-campaign acceptance of this candidate is recorded separately.

`workspace.py bot --command body --seconds 120` is one bounded actual-game
inspection of standing/crouched/prone body, native foot clearance, head lean,
return to standing and sampled walking. The controls-editor case can include
`"settings": true` to inspect toggles, numeric/enum edits, cancel/confirm,
VR off/on and remapping in one session. Current evidence/cleanup receipts
belong in `artifacts/dev/live-vr-body-current/`; simulator checks alone do not
establish wired Quest acceptance, every outfit or every cinematic.
