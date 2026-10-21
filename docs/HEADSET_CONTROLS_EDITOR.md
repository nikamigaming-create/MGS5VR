# Headset controller mapping

Hold the left Menu button and click the left thumbstick together for 650 ms.
The `system.controls` binding opens a spatial stereo panel in the game. Its
default chord is reserved before the hold threshold, so it does not also open
native Pause/iDroid or start running. The desktop preview stays 1280 x 720;
the editor has a separate 2048 x 1280 texture and both-eye OpenXR quad.

The gear icon opens every supported live VR gameplay setting, including
toggles, numeric tuning, turning/HUD modes, full body and cinematic movement.
Use the switch or +/- icons, then check to apply or X to cancel. Release the
controls for the validated file to reload; no restart is required. Ranges and
coupled hand/support constraints are checked before saving. The VR switch
changes immersive rendering to the spatial screen and keeps this panel open
so you can return immediately. It is a session presentation choice. OpenXR
runtime selection, hook diagnostics and owned asset paths remain launch setup.

The controller icon returns to mapping. The panel follows the current native mode: on foot, ACC, launch cabin, driving,
horseback, binoculars, equipment, Commands, iDroid, menus, cutscene or native-button mode.
It displays the effective personal bindings for that context. The mode arrows
let you inspect other contexts; Auto returns to the live mode. ACC omits on-foot
movement/combat actions. Driving shows vehicle actions and its pedal/turn axes.

Point either controller at an action and press Trigger. The actual licensed
Quest Touch Plus models show assigned controls in gold, presses in green and
light touches in blue. Grip/trigger assignments rotate the drawings to expose
their contact surfaces. The drawings are guides; they do not replace Snake's
physical hands, props, binoculars or left-hand iDroid.

Choose the red record icon, release controls, then press the new button or chord
and release it. Choose level, press, tap, hold or release before recording; tap
and hold duration can be adjusted. `+ ALT` adds an alternative while preserving
the current one. Touch capture is explicit because resting fingers should not
silently become modifiers. To map an axis, record a stick movement and center
the stick. Reset and Off also require confirmation.

The preview is checked by the same parser and conflict rules as the runtime.
Check saves the proposed binding; X cancels it. Conflicting assignments keep
the working layout. Only the selected override is patched; comments, other
bindings and tuning survive. A file changed by another editor is rejected until
this panel is reopened. One `.vr-backup` retains the preceding controls file.
Release buttons and center sticks to apply the saved layout live, then try it
on the controller drawings before closing.

Game input is consumed during editing and through the closing release gate.
Accepted gameplay/ACC uses the existing mod-owned world-pause lease while eyes,
hands and the editor continue rendering. Native menus retain their own pause;
the launch cabin stays live. Closing, focus loss, tracking loss and scene loss
release the editor request without opening stock Pause.

This editor maps the configured action/axis catalog. Proximity interactions
such as cassette insertion and physical paper contact retain their existing
physical behavior; those contacts are not arbitrary input bindings.

Use `Play-Dev.cmd` for the checked candidate. Build/unit results and simulator
captures do not certify physical-headset ergonomics or whole-game acceptance.
