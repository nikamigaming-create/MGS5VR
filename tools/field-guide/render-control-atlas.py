"""Render source-model controller plates and project the model's named anchors.

Run with Blender --background --python this-file -- --out-dir PATH.
These are explanatory model views, never claimed to be recorded gameplay poses.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view

p = argparse.ArgumentParser()
p.add_argument("--out-dir", type=Path, required=True)
args = p.parse_args(sys.argv[sys.argv.index("--")+1:])
args.out_dir.mkdir(parents=True, exist_ok=True)
manifest = {"source_commit": "4484a05e30bcd43fe86bb4e06b7a707861a26796",
            "pose_provenance": "Illustrative source-model views; no runtime pose claim", "plates": []}
for hand in ("left", "right"):
    bpy.ops.object.select_all(action="SELECT"); bpy.ops.object.delete(use_global=False)
    model = Path(__file__).parent / "assets" / ("meta-quest-touch-plus-"+hand+".glb")
    bpy.ops.import_scene.gltf(filepath=str(model))
    body = bpy.data.objects["controller_mesh"]
    vertices = [body.matrix_world @ v.co for v in body.data.vertices]
    lo = Vector([min(v[i] for v in vertices) for i in range(3)])
    hi = Vector([max(v[i] for v in vertices) for i in range(3)])
    center = (lo+hi)*.5
    face = Vector((-.025 if hand == "left" else .025, .624, .779)).normalized()
    side = Vector((1,0,0)) if hand == "left" else Vector((-1,0,0))
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"; scene.cycles.device = "CPU"; scene.cycles.samples = 12
    scene.cycles.use_denoising = True
    scene.render.threads_mode = "FIXED"; scene.render.threads = 2
    scene.render.resolution_x = 1000; scene.render.resolution_y = 1000; scene.render.resolution_percentage = 100
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = "PNG"; scene.render.image_settings.color_mode = "RGBA"
    scene.world.color = (.25,.25,.25)
    scene.view_settings.view_transform = "AgX"
    for loc, energy, size in [(center+face*.5+Vector((-.35,0,.25)),45,.45),
                               (center+Vector((.4,-.1,.4)),25,.3)]:
        lamp = bpy.data.lights.new("studio", "AREA"); lamp.energy=energy; lamp.shape="DISK"; lamp.size=size
        obj = bpy.data.objects.new("studio", lamp); scene.collection.objects.link(obj); obj.location=loc
        obj.rotation_euler=(center-loc).to_track_quat('-Z','Y').to_euler()
    cam = bpy.data.cameras.new("atlas"); obj=bpy.data.objects.new("atlas",cam)
    scene.collection.objects.link(obj); scene.camera=obj; cam.type="ORTHO"; cam.ortho_scale=max(hi-lo)*1.50
    for view, direction in (("face", face), ("side", (face*.35+side*.94).normalized())):
        obj.location=center+direction*.5; obj.rotation_euler=(center-obj.location).to_track_quat('-Z','Y').to_euler()
        obj.rotation_euler.rotate_axis('Z',math.pi)
        bpy.context.view_layer.update()
        projected_body=[world_to_camera_view(scene,obj,v) for v in vertices]
        span=max(max(p.x for p in projected_body)-min(p.x for p in projected_body),
                 max(p.y for p in projected_body)-min(p.y for p in projected_body))
        cam.ortho_scale*=span/.84
        bpy.context.view_layer.update()
        anchors = {}
        nodes = {"1": "x_button" if hand == "left" else "a_button",
                 "2": "y_button" if hand == "left" else "b_button",
                 "3": "thumbstick", "4": "trigger", "5": "squeeze", "6": "thumbrest_pressed_value"}
        for number, name in nodes.items():
            part=bpy.data.objects[name]
            if part.type == "MESH":
                point=sum((part.matrix_world@Vector(c) for c in part.bound_box),Vector())/8
            else:point=part.matrix_world.translation
            projected=world_to_camera_view(scene,obj,point)
            anchors[number]={"node":name,"x":projected.x,"y":1-projected.y,"depth":projected.z}
        output=args.out_dir/(hand+"-"+view+".png");scene.render.filepath=str(output)
        bpy.ops.render.render(write_still=True)
        manifest["plates"].append({"hand":hand,"view":view,"image":output.name,"anchors":anchors,
            "model_sha256":hashlib.sha256(model.read_bytes()).hexdigest(),
            "image_sha256":hashlib.sha256(output.read_bytes()).hexdigest()})
(args.out_dir/"atlas.json").write_text(json.dumps(manifest,indent=2)+"\n")
