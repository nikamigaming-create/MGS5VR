#!/usr/bin/env python3
"""Render the Quest 3 Touch Plus binocular-input inset from the official GLB.

The retail controller geometry and named button nodes come from the WebXR Input
Profiles repository. Camera projection and acknowledgement-driven highlights
are illustrative; the clip does not contain a sampled physical controller pose.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import shutil
import subprocess
import sys
from pathlib import Path

import bpy
from bpy_extras.object_utils import world_to_camera_view
from mathutils import Matrix, Quaternion, Vector


CLIP_DURATION_S = 4.6492166
ACKS = {
    "Y": {"press_s": 1.3358353, "release_s": 2.9619379},
    "L_GRIP": {"press_s": 1.3205165, "release_s": 3.0409},
}

MODEL_RELATIVE = Path("assets") / "meta-quest-touch-plus-left.glb"
MODEL_SOURCE = (
    "https://github.com/immersive-web/webxr-input-profiles/blob/"
    "4484a05e30bcd43fe86bb4e06b7a707861a26796/packages/assets/profiles/"
    "meta-quest-touch-plus/left.glb"
)
PROFILE_SOURCE = (
    "https://github.com/immersive-web/webxr-input-profiles/blob/"
    "4484a05e30bcd43fe86bb4e06b7a707861a26796/packages/registry/profiles/"
    "meta/meta-quest-touch-plus.json"
)
LICENSE_SOURCE = (
    "https://github.com/immersive-web/webxr-input-profiles/blob/"
    "4484a05e30bcd43fe86bb4e06b7a707861a26796/packages/assets/LICENSE.md"
)
MODEL_SHA256 = "41a35b80221398d37029cbfaa9e39084ccd43e0206be72a5e8460f52e7882cb4"

COLORS = {
    "cream": (0.91, 0.86, 0.73, 1.0),
    "white": (0.98, 0.95, 0.87, 1.0),
    "dark": (0.022, 0.032, 0.033, 1.0),
    "red": (0.92, 0.075, 0.045, 1.0),
}


def parse_args() -> argparse.Namespace:
    raw = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--model", type=Path)
    parser.add_argument("--mode", choices=("poster", "sequence"), default="poster")
    parser.add_argument("--width", type=int, default=720)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--fps", type=int, default=90)
    parser.add_argument("--samples", type=int, default=8)
    parser.add_argument("--threads", type=int, default=2)
    parser.add_argument("--engine", choices=("workbench", "eevee", "cycles"), default="cycles")
    parser.add_argument("--poster-time", type=float, default=1.7)
    parser.add_argument("--poster-azimuth", type=float)
    parser.add_argument("--make-movie", action="store_true")
    parser.add_argument("--cues", type=Path, help="Recording-derived timing manifest from prepare-controller-cues.py")
    return parser.parse_args(raw)


def clear_scene() -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for block in bpy.data.materials:
        bpy.data.materials.remove(block)
    for block in bpy.data.curves:
        if block.users == 0:
            bpy.data.curves.remove(block)


def transparent_principled(name: str, color, opacity: float):
    material = bpy.data.materials.new(name)
    material.diffuse_color = (color[0], color[1], color[2], opacity)
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    transparent = nodes.new("ShaderNodeBsdfTransparent")
    shader = nodes.new("ShaderNodeBsdfPrincipled")
    shader.inputs["Base Color"].default_value = color
    shader.inputs["Roughness"].default_value = 0.78
    mix = nodes.new("ShaderNodeMixShader")
    mix.inputs[0].default_value = opacity
    links.new(transparent.outputs["BSDF"], mix.inputs[1])
    links.new(shader.outputs["BSDF"], mix.inputs[2])
    links.new(mix.outputs["Shader"], output.inputs["Surface"])
    return material


def emission_material(name: str, color, strength: float):
    material = bpy.data.materials.new(name)
    material.diffuse_color = color
    material.use_nodes = True
    nodes = material.node_tree.nodes
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    emission = nodes.new("ShaderNodeEmission")
    emission.inputs["Color"].default_value = color
    emission.inputs["Strength"].default_value = strength
    material.node_tree.links.new(emission.outputs["Emission"], output.inputs["Surface"])
    return material


def set_emission(material, color, strength: float) -> None:
    node = next(
        (node for node in material.node_tree.nodes if node.type == "EMISSION"),
        None,
    )
    if node:
        node.inputs["Color"].default_value = color
        node.inputs["Strength"].default_value = strength


def make_highlight_copy(source, name: str):
    material = source.copy()
    material.name = name
    material["base_diffuse_color"] = list(material.diffuse_color)
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    output = nodes.get("Material Output")
    surface_link = next(
        (link for link in links if link.to_node == output and link.to_socket.name == "Surface"),
        None,
    )
    if surface_link is None:
        raise RuntimeError(f"Material {source.name} has no connected surface shader.")
    base_shader = surface_link.from_node
    links.remove(surface_link)
    emission = nodes.new("ShaderNodeEmission")
    emission.name = "Input highlight emission"
    emission.inputs["Color"].default_value = COLORS["red"]
    emission.inputs["Strength"].default_value = 1.3
    mix = nodes.new("ShaderNodeMixShader")
    mix.name = "Input highlight mix"
    mix.inputs[0].default_value = 0.0
    links.new(base_shader.outputs[0], mix.inputs[1])
    links.new(emission.outputs["Emission"], mix.inputs[2])
    links.new(mix.outputs["Shader"], output.inputs["Surface"])
    material["highlight_mix_node"] = mix.name
    material["highlight_emission_node"] = emission.name
    return material


def set_highlight(material, active: bool) -> None:
    base = material.get("base_diffuse_color")
    if base:
        material.diffuse_color = COLORS["red"] if active else tuple(base)
    mix = material.node_tree.nodes.get(material.get("highlight_mix_node", ""))
    emission = material.node_tree.nodes.get(material.get("highlight_emission_node", ""))
    if mix and emission:
        mix.inputs[0].default_value = 0.80 if active else 0.0
        emission.inputs["Strength"].default_value = 1.25 if active else 0.0


def create_box(name: str, dimensions, material, parent, location, bevel=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1.0)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.parent = parent
    obj.matrix_parent_inverse = Matrix.Identity(4)
    obj.location = location
    obj.data.materials.append(material)
    if bevel:
        modifier = obj.modifiers.new("Soft field-kit edge", "BEVEL")
        modifier.width = bevel
        modifier.segments = 5
        obj.modifiers.new("Weighted normals", "WEIGHTED_NORMAL")
    return obj


def create_text(name: str, body: str, material, parent, size: float, location,
                align: str = "LEFT", bold: bool = False):
    curve = bpy.data.curves.new(name + "Curve", "FONT")
    curve.body = body
    curve.size = size
    curve.space_character = 1.1
    curve.align_x = align
    curve.extrude = 0.0
    curve.bevel_depth = 0.0
    fonts = (
        [r"C:\Windows\Fonts\segoeuib.ttf", r"C:\Windows\Fonts\segoeui.ttf"]
        if bold else
        [r"C:\Windows\Fonts\segoeui.ttf"]
    )
    for candidate in fonts:
        if Path(candidate).exists():
            try:
                curve.font = bpy.data.fonts.load(candidate)
                break
            except Exception:
                pass
    obj = bpy.data.objects.new(name, curve)
    bpy.context.scene.collection.objects.link(obj)
    obj.parent = parent
    obj.matrix_parent_inverse = Matrix.Identity(4)
    obj.location = location
    obj.data.materials.append(material)
    return obj


def make_curve(name: str, material, bevel=0.00026):
    curve = bpy.data.curves.new(name + "Curve", "CURVE")
    curve.dimensions = "3D"
    curve.resolution_u = 1
    curve.bevel_depth = bevel
    curve.bevel_resolution = 3
    spline = curve.splines.new("POLY")
    spline.points.add(2)
    obj = bpy.data.objects.new(name, curve)
    bpy.context.scene.collection.objects.link(obj)
    curve.materials.append(material)
    return obj


def update_line(obj, a: Vector, b: Vector, c: Vector, amount: float) -> None:
    points = obj.data.splines[0].points
    points[0].co = (*a, 1.0)
    points[1].co = (*b, 1.0)
    points[2].co = (*c, 1.0)
    obj.data.bevel_depth = 0.00018 * amount


def add_area_light(scene, name: str, position, energy, color, size, target):
    data = bpy.data.lights.new(name, "AREA")
    data.energy = energy
    data.color = color
    data.shape = "DISK"
    data.size = size
    obj = bpy.data.objects.new(name, data)
    scene.collection.objects.link(obj)
    obj.location = position
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat("-Z", "Y").to_euler()
    return obj


def set_camera_pose(camera, target: Vector, face_normal: Vector,
                    side_axis: Vector, body_up: Vector, azimuth: float,
                    elevation: float, distance: float) -> None:
    view_axis = (face_normal * math.cos(azimuth) +
                 side_axis * math.sin(azimuth)).normalized()
    position = target + view_axis * distance + body_up * elevation
    direction = (target - position).normalized()
    # Keep the controller's body axis vertical while following the official
    # imported face normal and button layout.
    up_hint = body_up - direction * body_up.dot(direction)
    right = direction.cross(up_hint)
    if right.length < 1.0e-5:
        right = direction.cross(side_axis)
    right.normalize()
    up = right.cross(direction).normalized()
    rotation = Matrix((right, up, -direction)).transposed().to_quaternion()
    camera.location = position
    camera.rotation_euler = rotation.to_euler()


def projected_mesh_center(obj, depsgraph) -> Vector:
    evaluated = obj.evaluated_get(depsgraph)
    if evaluated.type != "MESH":
        return evaluated.matrix_world.translation.copy()
    bounds = [Vector(point) for point in evaluated.bound_box]
    center_local = sum(bounds, Vector()) / len(bounds)
    return evaluated.matrix_world @ center_local


def visible_mesh_surface_point(obj, body, camera, depsgraph,
                              prefer_projected_center=False):
    """Choose a camera-facing mesh face center not hidden by the controller shell."""
    evaluated = obj.evaluated_get(depsgraph)
    if evaluated.type != "MESH":
        return evaluated.matrix_world.translation.copy(), "node-origin"
    body_inverse = body.matrix_world.inverted()
    camera_position = camera.matrix_world.translation
    best_point = None
    best_score = -float("inf") if prefer_projected_center else -1.0
    projected_center = None
    if prefer_projected_center:
        projected_center = world_to_camera_view(
            bpy.context.scene, camera, projected_mesh_center(evaluated, depsgraph)
        )
    for polygon in evaluated.data.polygons:
        point = evaluated.matrix_world @ polygon.center
        toward_camera = camera_position - point
        distance = toward_camera.length
        if distance <= 1.0e-6:
            continue
        toward_camera /= distance
        normal = (evaluated.matrix_world.to_3x3() @ polygon.normal).normalized()
        facing = normal.dot(toward_camera)
        if facing <= 0.05:
            continue
        ray = -toward_camera
        local_origin = body_inverse @ camera_position
        local_direction = (body_inverse.to_3x3() @ ray).normalized()
        hit, hit_location, _hit_normal, _face_index = body.ray_cast(
            local_origin, local_direction
        )
        if hit:
            hit_world = body.matrix_world @ hit_location
            if (hit_world - camera_position).length < distance - 0.0005:
                continue
        if prefer_projected_center:
            projected = world_to_camera_view(bpy.context.scene, camera, point)
            screen_distance = (
                (float(projected.x) - float(projected_center.x)) ** 2
                + (float(projected.y) - float(projected_center.y)) ** 2
            )
            score = -screen_distance + 1.0e-6 * facing
        else:
            score = float(polygon.area) * facing
        if score > best_score:
            best_score = score
            best_point = point
    if best_point is not None:
        method = (
            "visible-unoccluded-face-nearest-projected-mesh-center"
            if prefer_projected_center else
            "visible-unoccluded-face-center"
        )
        return best_point, method
    return projected_mesh_center(obj, depsgraph), "mesh-bounds-center-fallback"


def frame_point(camera, scene, x_norm: float, y_top_norm: float,
                depth: float) -> Vector:
    frame = camera.data.view_frame(scene=scene)
    scale = depth / abs(frame[0].z)
    xs = [corner.x * scale for corner in frame]
    ys = [corner.y * scale for corner in frame]
    return Vector((
        min(xs) + x_norm * (max(xs) - min(xs)),
        max(ys) - y_top_norm * (max(ys) - min(ys)),
        -depth,
    ))


def smoothstep(value: float) -> float:
    value = max(0.0, min(1.0, value))
    return value * value * (3.0 - 2.0 * value)


def callout_progress(time_s: float, tag: str) -> float:
    start = ACKS[tag]["press_s"]
    end = ACKS[tag]["release_s"]
    if time_s < start:
        return 0.0
    if time_s <= end:
        return smoothstep((time_s - start) / 0.22)
    return 1.0 - smoothstep((time_s - end) / 0.15)


def create_scene(args):
    clear_scene()
    scene = bpy.context.scene
    model_path = (args.model or (Path(__file__).resolve().parent / MODEL_RELATIVE)).resolve()
    if not model_path.is_file():
        raise FileNotFoundError(f"Quest Touch Plus model not found: {model_path}")
    digest = hashlib.sha256(model_path.read_bytes()).hexdigest()
    if digest != MODEL_SHA256:
        raise ValueError(f"Unexpected left-model SHA-256: {digest}")
    bpy.ops.import_scene.gltf(filepath=str(model_path))
    profile_root = bpy.data.objects.get("root")
    controller_mesh = bpy.data.objects.get("controller_mesh")
    if profile_root is None:
        raise RuntimeError("Official left GLB is missing its root node.")
    y_button = bpy.data.objects.get("y_button")
    y_value = bpy.data.objects.get("y_button_pressed_value")
    y_min = bpy.data.objects.get("y_button_pressed_min")
    y_max = bpy.data.objects.get("y_button_pressed_max")
    x_button = bpy.data.objects.get("x_button")
    x_value = bpy.data.objects.get("x_button_pressed_value")
    squeeze = bpy.data.objects.get("squeeze")
    squeeze_value = bpy.data.objects.get("xr_standard_squeeze_pressed_value")
    thumbstick_value = bpy.data.objects.get("xr_standard_thumbstick_pressed_value")
    if not all((controller_mesh, y_button, y_value, y_min, y_max, x_button,
                x_value, squeeze, squeeze_value, thumbstick_value)):
        raise RuntimeError("Official left GLB is missing a Y or squeeze visual node.")

    # Derive the showcase axes from Blender's imported world transforms. The
    # GLTF root's children carry the Y-up/Z-up conversion, so keep those
    # matrices intact and animate a separate pivot around the evaluated body.
    body_vertices = [controller_mesh.matrix_world @ vertex.co
                     for vertex in controller_mesh.data.vertices]
    bounds_min = Vector(tuple(min(point[i] for point in body_vertices)
                              for i in range(3)))
    bounds_max = Vector(tuple(max(point[i] for point in body_vertices)
                              for i in range(3)))
    camera_target = (bounds_min + bounds_max) * 0.5
    extents = bounds_max - bounds_min
    long_axis = max(range(3), key=lambda axis: extents[axis])
    body_up = Vector(tuple(1.0 if axis == long_axis else 0.0
                           for axis in range(3)))
    button_up = y_value.matrix_world.translation - x_value.matrix_world.translation
    if button_up.length < 1.0e-6:
        raise RuntimeError("Official GLB has no usable X/Y button ordering.")
    if body_up.dot(button_up) < 0.0:
        body_up.negate()

    # The X/Y child meshes' largest polygons are their cap/back surfaces, not
    # the outward-facing control plate. The pinned GLB's UV-mapped black face
    # plate and printed X/Y legends identify the outward normal as this
    # imported-world direction. Keep the camera on that face hemisphere; the
    # button-side vector fixes its sign if the profile's node transforms change.
    face_normal = Vector((-0.025, 0.624, 0.779)).normalized()
    button_side = ((x_value.matrix_world.translation +
                    y_value.matrix_world.translation) * 0.5 - camera_target)
    if face_normal.dot(button_side) < 0.0:
        face_normal.negate()
    side_axis = body_up.cross(face_normal)
    if side_axis.length < 1.0e-5:
        side_axis = button_up.cross(face_normal)
    side_axis.normalize()

    root_world = profile_root.matrix_world.copy()
    pivot = bpy.data.objects.new("Quest Touch Plus composition pivot", None)
    scene.collection.objects.link(pivot)
    pivot.location = camera_target
    pivot.rotation_mode = "QUATERNION"
    pivot.rotation_quaternion = Quaternion((1.0, 0.0, 0.0, 0.0))
    bpy.context.view_layer.update()
    profile_root.parent = pivot
    profile_root.matrix_parent_inverse = pivot.matrix_world.inverted()
    profile_root.matrix_world = root_world
    rig = pivot

    if args.engine == "cycles":
        scene.render.engine = "CYCLES"
        scene.cycles.device = "CPU"
        scene.cycles.samples = max(4, args.samples)
        scene.cycles.use_denoising = True
        scene.render.threads_mode = "FIXED"
        scene.render.threads = max(1, args.threads)
    elif args.engine == "eevee":
        scene.render.engine = "BLENDER_EEVEE_NEXT"
        scene.eevee.taa_render_samples = max(16, args.samples * 4)
        scene.eevee.use_gtao = True
        scene.eevee.gtao_distance = 0.08
        scene.eevee.gtao_quality = 0.8
    else:
        scene.render.engine = "BLENDER_WORKBENCH"
        shading = scene.display.shading
        shading.light = "STUDIO"
        shading.color_type = "TEXTURE"
        shading.show_shadows = True
        shading.show_cavity = True
        shading.cavity_type = "BOTH"
        shading.curvature_ridge_factor = 1.15
        shading.curvature_valley_factor = 0.9
        shading.background_type = "WORLD"
    scene.render.resolution_x = args.width
    scene.render.resolution_y = args.height
    scene.render.resolution_percentage = 100
    scene.render.fps = args.fps
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.image_settings.color_depth = "8"
    scene.render.image_settings.compression = 9
    scene.render.use_file_extension = True
    scene.view_settings.view_transform = "AgX"
    scene.render.use_motion_blur = False
    scene.frame_start = 1
    scene.frame_end = max(2, math.ceil(CLIP_DURATION_S * args.fps))

    world = bpy.data.worlds.new("Transparent studio world")
    scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.10, 0.12, 0.12, 1.0)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.20

    # Keep the profile texture and geometry, tuning only roughness for a matte
    # studio response like the generated reference.
    for material in bpy.data.materials:
        if material.use_nodes:
            shader = material.node_tree.nodes.get("Principled BSDF")
            if shader:
                shader.inputs["Roughness"].default_value = max(
                    0.34, min(0.72, shader.inputs["Roughness"].default_value)
                )

    camera_data = bpy.data.cameras.new("Quest 3 Touch Plus inset camera")
    camera_data.type = "PERSP"
    camera_data.lens = 56.0
    camera_data.sensor_width = 36.0
    camera_data.sensor_fit = "AUTO"
    camera = bpy.data.objects.new("Quest 3 Touch Plus inset camera", camera_data)
    scene.collection.objects.link(camera)
    camera.data.clip_start = 0.01
    camera.data.clip_end = 20.0
    scene.camera = camera
    add_area_light(scene, "Warm softbox key",
                   camera_target + face_normal * 0.24 + body_up * 0.13 + side_axis * 0.10, 2.0,
                   (1.0, 0.84, 0.69), 0.13, camera_target)
    add_area_light(scene, "Cool face fill",
                   camera_target + face_normal * 0.19 - side_axis * 0.19, 0.70,
                   (0.66, 0.78, 1.0), 0.11, camera_target)
    add_area_light(scene, "Warm shell rim",
                   camera_target - face_normal * 0.13 + body_up * 0.17, 1.25,
                   (1.0, 0.56, 0.37), 0.12, camera_target)

    panel_mat = transparent_principled("Translucent charcoal field-kit glass",
                                       COLORS["dark"], 0.72)
    if args.engine == "eevee" and hasattr(panel_mat, "surface_render_method"):
        panel_mat.surface_render_method = "DITHERED"
    cream_ink = emission_material("Warm cream callout", COLORS["cream"], 1.35)
    white_ink = emission_material("Soft white title", COLORS["white"], 1.1)
    red_ink = emission_material("Signal red callout", COLORS["red"], 1.7)

    panel_depth = 0.43
    panel = create_box("Charcoal inset glass", (0.255, 0.255, 0.002),
                       panel_mat, camera, (0.0, 0.0, -panel_depth), 0.016)
    panel["purpose"] = "translucent visual-inset backing"
    frame_z = -panel_depth + 0.0018
    for y in (-0.121, 0.121):
        create_box("Cream frame rail", (0.238, 0.0012, 0.001),
                   cream_ink, camera, (0.0, y, frame_z), 0.0005)
    for x in (-0.119, 0.119):
        create_box("Cream frame rail", (0.0012, 0.238, 0.001),
                   cream_ink, camera, (x, 0.0, frame_z), 0.0005)
    for x in (-0.108, 0.108):
        create_box("Red corner mark", (0.012, 0.0015, 0.0012),
                   red_ink, camera, (x, 0.108, frame_z - 0.0005), 0.0005)

    title = create_text("Inset title", "FIELD KIT  /  BINOCULARS",
                        white_ink, camera, 0.0038,
                        (-0.02, 0.03, -0.155), bold=True)
    footer = create_text("Controller illustration footer",
                         "CONTROLLER ILLUSTRATION",
                         cream_ink, camera, 0.0031,
                         (-0.02, -0.03, -0.155))

    y_highlight = make_highlight_copy(
        y_button.data.materials[0], y_button.data.materials[0].name + " Y highlight"
    )
    y_button.data.materials.clear()
    y_button.data.materials.append(y_highlight)
    squeeze_highlight = make_highlight_copy(
        squeeze.data.materials[0], squeeze.data.materials[0].name + " squeeze highlight"
    )
    squeeze.data.materials.clear()
    squeeze.data.materials.append(squeeze_highlight)
    callouts = {
        "Y": {
            "anchor": y_button,
            "target": (0.65, 0.40),
            "bend": (0.58, 0.49),
            "label": "Y  ·  EQUIP\nBINOCULARS",
            "material": red_ink,
            "highlight": y_highlight,
        },
        "L_GRIP": {
            "anchor": squeeze,
            "target": (0.14, 0.68),
            "bend": (0.33, 0.40),
            "label": "HOLD LEFT\nGRIP",
            "material": cream_ink,
            "highlight": squeeze_highlight,
        },
    }
    lines = {}
    labels = {}
    for tag, info in callouts.items():
        lines[tag] = make_curve(f"{tag} projected leader", info["material"])
        labels[tag] = create_text(
            f"{tag} callout label", info["label"], info["material"],
            camera, 0.0032, (0.0, 0.0, -0.155), bold=True,
        )
    scene["source_profile"] = "meta-quest-touch-plus"
    scene["left_controls"] = "xr-standard-squeeze, x-button, y-button, menu"
    scene["visual_pose"] = "official static profile model; not sampled from runtime"
    return (scene, camera, rig, y_value, y_min, y_max, callouts, lines,
            labels, camera_target, face_normal, side_axis, body_up, model_path)


def set_frame(scene, camera, rig, y_value, y_min, y_max, callouts,
              lines, labels, camera_target, face_normal, side_axis, body_up,
              time_s, frame_number, azimuth_override_degrees=None):
    t = max(0.0, min(CLIP_DURATION_S, time_s))
    progress = t / CLIP_DURATION_S
    azimuth = (
        math.radians(azimuth_override_degrees)
        if azimuth_override_degrees is not None
        else math.radians(25.0 + 15.0 * progress)
    )
    distance = 0.255 - 0.010 * math.sin(math.pi * progress)
    elevation = 0.010 + 0.004 * math.sin(math.tau * progress)
    set_camera_pose(camera, camera_target, face_normal, side_axis, body_up,
                    azimuth, elevation, distance)
    camera.data.lens = 46.0 + 4.0 * math.sin(math.pi * progress)
    scene.objects["Inset title"].location = frame_point(
        camera, scene, 0.075, 0.095, 0.155
    )
    scene.objects["Controller illustration footer"].location = frame_point(
        camera, scene, 0.075, 0.90, 0.155
    )
    rig.rotation_quaternion = Quaternion(
        body_up, math.radians(2.0 * math.sin(progress * math.tau))
    )
    scene.frame_set(frame_number)
    bpy.context.view_layer.update()
    depsgraph = bpy.context.evaluated_depsgraph_get()

    y_progress = callout_progress(t, "Y")
    y_value.location = y_min.location.lerp(y_max.location, y_progress)
    bpy.context.view_layer.update()
    depsgraph = bpy.context.evaluated_depsgraph_get()

    anchors = {}
    for tag, info in callouts.items():
        interval = ACKS[tag]
        active = interval["press_s"] <= t < interval["release_s"]
        amount = callout_progress(t, tag)
        anchor_world, anchor_method = visible_mesh_surface_point(
            info["anchor"], scene.objects["controller_mesh"], camera, depsgraph,
            prefer_projected_center=(tag == "L_GRIP"),
        )
        projected = world_to_camera_view(scene, camera, anchor_world)
        anchor_x = float(projected.x)
        anchor_y_top = 1.0 - float(projected.y)
        target_x, target_y_top = info["target"]
        label_x = anchor_x + (target_x - anchor_x) * amount
        label_y_top = anchor_y_top + (target_y_top - anchor_y_top) * amount
        label_y_top = max(0.12, min(0.86, label_y_top))
        label_local = frame_point(camera, scene, label_x, label_y_top, 0.155)
        labels[tag].location = label_local
        labels[tag].scale = (amount, amount, amount)
        set_highlight(info["highlight"], active)

        anchor_y_bottom = 1.0 - anchor_y_top
        end_local = frame_point(camera, scene, label_x, label_y_top, 0.155)
        end_world = camera.matrix_world @ end_local
        bend_local = frame_point(
            camera, scene, info["bend"][0], info["bend"][1], 0.155
        )
        middle_world = camera.matrix_world @ bend_local
        update_line(lines[tag], anchor_world, middle_world, end_world, amount)
        anchors[tag] = {
            "model_node": info["anchor"].name,
            "anchor_method": anchor_method,
            "world": [round(float(v), 7) for v in anchor_world],
            "render_normalized_top_left": [round(anchor_x, 7), round(anchor_y_top, 7)],
            "render_pixels_top_left": [
                round(anchor_x * scene.render.resolution_x, 3),
                round(anchor_y_top * scene.render.resolution_y, 3),
            ],
            "in_front_of_camera": bool(projected.z > 0),
            "visible_in_frame": bool(
                projected.z > 0 and 0 <= anchor_x <= 1 and 0 <= anchor_y_top <= 1
            ),
            "ack_active": bool(active),
            "callout_pull_fraction": round(float(amount), 6),
        }
    bpy.context.view_layer.update()
    return anchors


def render_one(scene, camera, rig, y_value, y_min, y_max, callouts,
               lines, labels, camera_target, face_normal, side_axis,
               body_up, frame_number, time_s, output,
               azimuth_override_degrees=None):
    anchors = set_frame(
        scene, camera, rig, y_value, y_min, y_max, callouts,
        lines, labels, camera_target, face_normal, side_axis, body_up,
        time_s, frame_number, azimuth_override_degrees,
    )
    scene.render.filepath = str(output)
    bpy.ops.render.render(write_still=True)
    return {
        "frame": frame_number - 1,
        "time_s": round(time_s, 7),
        "anchors": anchors,
        "camera_world_matrix": [
            [round(float(value), 7) for value in row]
            for row in camera.matrix_world
        ],
    }


def write_metadata(path: Path, args, model_path: Path, frame_rows, frame_count: int):
    payload = {
        "schema": 2,
        "asset": "quest3_touch_plus_binoculars_input_inset",
        "render_engine": {
            "workbench": "Blender Workbench",
            "eevee": "Blender EEVEE Next",
            "cycles": "Blender Cycles CPU",
        }[args.engine],
        "transparent_background": True,
        "canvas_px": [args.width, args.height],
        "fps": args.fps,
        "frame_count": frame_count,
        "nominal_video_duration_s": frame_count / args.fps,
        "source_segment_duration_s": CLIP_DURATION_S,
        "duration_note": "The final ProRes MOV is trimmed to the source segment duration.",
        "source_take": args.cue_data.get("source_take") if args.cues else None,
        "case": args.cue_data.get("case") if args.cues else None,
        "cue_manifest_sha256": hashlib.sha256(args.cues.read_bytes()).hexdigest() if args.cues else None,
        "effective_inputs": ["left xr-standard-squeeze (L Grip)", "left y-button (Y)"],
        "acknowledgements_relative_to_clip_start_s": ACKS,
        "leader_lines_embedded": True,
        "cue_labels": ["HOLD LEFT GRIP", "Y · EQUIP BINOCULARS"],
        "pose_provenance": "illustrative orientation of official static WebXR controller model; not a sampled runtime pose",
        "controller_profile": {
            "profile_id": "meta-quest-touch-plus",
            "hand": "left",
            "model_file": str(model_path),
            "model_sha256": MODEL_SHA256,
            "model_source": MODEL_SOURCE,
            "registry_source": PROFILE_SOURCE,
            "license": "MIT",
            "copyright": "Copyright (c) 2019 Amazon",
            "license_source": LICENSE_SOURCE,
            "license_file": "tools/field-guide/assets/LICENSE-webxr-input-profiles.md",
            "webxr_components": [
                "xr-standard-squeeze", "x-button", "y-button", "menu",
                "xr-standard-thumbstick",
            ],
            "visual_responses": (
                "The registry profile declares no visualResponses. The GLB contains "
                "y_button_pressed_min/max/value nodes; squeeze is static and is "
                "highlighted without inventing a squeeze deformation."
            ),
        },
        "style_reference": {
            "image": "artifacts/field-guide-20260926/assets/quest3-touch-plus-generated-v1.png",
            "prompt": "artifacts/field-guide-20260926/assets/quest3-touch-plus-generated-v1.prompt.txt",
            "use": "lighting and material direction only; generated button order is not authoritative",
        },
        "anchors_are_projected_from": "camera-facing official GLB button/squeeze mesh faces that are not occluded by the controller body; mesh bounds are fallback only",
        "anchor_names": {"Y": "y_button mesh", "L_GRIP": "squeeze mesh"},
        "frames": frame_rows,
    }
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8")


def make_movie(out_dir: Path, fps: int, frame_count: int) -> Path:
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        raise RuntimeError("ffmpeg is not on PATH; the alpha PNG sequence is available.")
    movie = out_dir / "3d-preview-controller-alpha.mov"
    frame_pattern = out_dir / f"3d-preview-frames-{fps}fps" / "frame_%04d.png"
    command = [
        ffmpeg, "-y", "-hide_banner", "-loglevel", "error",
        "-framerate", str(fps), "-start_number", "0", "-i", str(frame_pattern),
        "-frames:v", str(frame_count), "-t", str(CLIP_DURATION_S),
        "-an", "-c:v", "prores_ks", "-profile:v", "4",
        "-pix_fmt", "yuva444p10le", "-alpha_bits", "16",
        "-fps_mode", "vfr", "-video_track_timescale", "90000", str(movie),
    ]
    subprocess.run(command, check=True)
    return movie


def main():
    global CLIP_DURATION_S, ACKS
    args = parse_args()
    args.cue_data = {}
    if args.mode == "sequence" and not args.cues:
        raise ValueError("A recording-derived --cues manifest is required for an instructional sequence")
    if args.cues:
        args.cue_data = json.loads(args.cues.read_text(encoding="utf-8-sig"))
        duration = args.cue_data["duration_seconds"]
        cues = args.cue_data["inputs"]
        if not isinstance(duration, (int, float)) or not math.isfinite(duration) or not 0 < duration <= 45:
            raise ValueError("Controller cue duration must be finite and within 45 seconds")
        if set(cues) != {"y", "left_grip"}:
            raise ValueError("This inset currently implements the left Grip + Y lesson; other controls need their model anchors")
        converted = {}
        for token, label in (("y", "Y"), ("left_grip", "L_GRIP")):
            start, end = cues[token]["press_s"], cues[token]["release_s"]
            if not 0 <= start < end <= duration:
                raise ValueError("Controller cue lies outside the actual action excerpt")
            converted[label] = {"press_s": start, "release_s": end}
        CLIP_DURATION_S, ACKS = duration, converted
    args.out_dir = args.out_dir.resolve()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    (scene, camera, rig, y_value, y_min, y_max, callouts, lines, labels,
     camera_target, face_normal, side_axis, body_up, model_path) = create_scene(args)
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"

    if args.mode == "poster":
        time_s = max(0.0, min(CLIP_DURATION_S, args.poster_time))
        frame_number = 1 + round(time_s * args.fps)
        output = args.out_dir / "3d-preview-poster.png"
        row = render_one(
            scene, camera, rig, y_value, y_min, y_max, callouts, lines,
            labels, camera_target, face_normal, side_axis, body_up,
            frame_number, time_s, output, args.poster_azimuth,
        )
        write_metadata(args.out_dir / "3d-preview-anchors.json", args, model_path, [row], 1)
        print(json.dumps({
            "mode": "poster", "poster": str(output),
            "anchors": str(args.out_dir / "3d-preview-anchors.json"),
            "frame": frame_number, "time_s": time_s,
        }))
        return

    frame_dir = args.out_dir / f"3d-preview-frames-{args.fps}fps"
    frame_dir.mkdir(parents=True, exist_ok=True)
    frame_count = math.ceil(CLIP_DURATION_S * args.fps)
    rows = []
    poster_index = max(0, min(frame_count - 1, round(args.poster_time * args.fps)))
    for index in range(frame_count):
        time_s = index / args.fps
        output = frame_dir / f"frame_{index:04d}.png"
        row = render_one(
            scene, camera, rig, y_value, y_min, y_max, callouts, lines,
            labels, camera_target, face_normal, side_axis, body_up,
            index + 1, time_s, output,
        )
        rows.append(row)
        if index == poster_index:
            shutil.copy2(output, args.out_dir / "3d-preview-poster.png")
        if index % max(1, args.fps // 2) == 0:
            print(f"rendered {index + 1}/{frame_count} frames ({time_s:.3f}s)", flush=True)
    metadata = args.out_dir / "3d-preview-anchors.json"
    write_metadata(metadata, args, model_path, rows, frame_count)
    movie = make_movie(args.out_dir, args.fps, frame_count) if args.make_movie else None
    print(json.dumps({
        "mode": "sequence", "frame_count": frame_count, "fps": args.fps,
        "source_duration_s": CLIP_DURATION_S,
        "poster": str(args.out_dir / "3d-preview-poster.png"),
        "frames": str(frame_dir), "anchors": str(metadata),
        "movie": str(movie) if movie else None,
    }))


if __name__ == "__main__":
    main()
