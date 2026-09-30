# Local field-guide prototype

Build the static guide from the repository root:

    python tools/field-guide/build_guide.py

The builder exports effective bindings with the compiled controls tool, reads
the 35-situation catalog and the separate 19-slot / 13-family TPP catalog, and
joins the current crouch run with its single-eye diagnostic capture. It writes
the browsable page and evidence snapshot to artifacts/field-guide-20260926.

To reflect the installed game settings, pass the installed controls file:

    python tools/field-guide/build_guide.py --controls-config "D:\SteamLibrary\steamapps\common\MGS_TPP\mgs5vr-controls.ini"

Use --controls-config PATH for another controls file. New posture capture
details can be joined with --posture-video PATH and --posture-capture PATH.
The page keeps effective controls, observed outcomes, and visual limits
distinct. The 35 situations are an incomplete catalog, not a completion claim.

The embedded launcher is now built from `tools/launcher-ui/` with
`tools/build-launcher-ui.py` and `tools/launcher-3d.cs`. It resolves semantic
lesson actions through the current installed bindings and displays the 3D models
beside unobstructed source gameplay. The older inset exporters below are retained
for historical artifacts; they are not the launcher lesson format.

The bounded binocular equip pass links from the TPP optics situation. It shows
equipping the binoculars after HOLD Y + L GRIP; it does not verify eye
alignment, zoom, marking, or analysis. The lesson renderer requires a complete
native-eye source segment, a case-matched held-input audit, acknowledged
controller press/release calls, and a transparent 3D inset asset. Controller
orientation is illustrative unless a synchronized pose source is supplied.
