"""Resolve the verified atlas Menu location onto its actual controller mesh."""
import hashlib
import json
import math
from pathlib import Path
import bpy
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view

here=Path(__file__).resolve().parent
atlas=json.loads((here/'assets/controller-callouts.json').read_text())['left-face.png']
model=here/'assets/meta-quest-touch-plus-left.glb'
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.gltf(filepath=str(model))
body=bpy.data.objects['controller_mesh']
vertices=[body.matrix_world@v.co for v in body.data.vertices]
lo=Vector([min(v[i] for v in vertices) for i in range(3)])
hi=Vector([max(v[i] for v in vertices) for i in range(3)])
center=(lo+hi)*.5;face=Vector((-.025,.624,.779)).normalized()
scene=bpy.context.scene
scene.render.resolution_x=scene.render.resolution_y=1000;scene.render.resolution_percentage=100
camera=bpy.data.cameras.new('atlas');obj=bpy.data.objects.new('atlas',camera)
scene.collection.objects.link(obj);scene.camera=obj;camera.type='ORTHO';camera.ortho_scale=max(hi-lo)*1.50
obj.location=center+face*.5;obj.rotation_euler=(center-obj.location).to_track_quat('-Z','Y').to_euler()
obj.rotation_euler.rotate_axis('Z',math.pi);bpy.context.view_layer.update()
projected=[world_to_camera_view(scene,obj,v) for v in vertices]
span=max(max(p.x for p in projected)-min(p.x for p in projected),max(p.y for p in projected)-min(p.y for p in projected))
camera.ortho_scale*=span/.84;bpy.context.view_layer.update()
anchor=atlas['anchors']['7'];rotation=obj.matrix_world.to_quaternion()
origin=obj.location+rotation@Vector(((anchor['x']-.5)*camera.ortho_scale,(.5-anchor['y'])*camera.ortho_scale,0))
direction=rotation@Vector((0,0,-1))
hit,position,normal,face_id,mesh,matrix=scene.ray_cast(bpy.context.evaluated_depsgraph_get(),origin,direction,distance=2)
if not hit or mesh.name!='controller_mesh':raise RuntimeError('Verified atlas Menu point did not intersect the controller body')
back=world_to_camera_view(scene,obj,position)
if abs(back.x-anchor['x'])>1e-5 or abs((1-back.y)-anchor['y'])>1e-5:raise RuntimeError('Menu anchor reprojection differs')
result={'left_menu':{'gltf_world_position':[position.x,position.z,-position.y],
    'gltf_world_normal':[normal.x,normal.z,-normal.y],
    'model_sha256':hashlib.sha256(model.read_bytes()).hexdigest(),
    'atlas_image_sha256':atlas['image_sha256'],'atlas_coordinate':[anchor['x'],anchor['y']],
    'mesh':mesh.name,'triangle':face_id,
    'provenance':'Ray intersection of the visually verified Menu center in the pinned 1000x1000 atlas; Blender Z-up converted to glTF Y-up.'}}
(here/'assets/controller-spatial-anchors.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result))
