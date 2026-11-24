"""Render the credited Highland GLB: blender -b --python render_vehicle.py -- MODEL.glb."""
import argparse
import hashlib
import sys
import math
from pathlib import Path

import bpy
from mathutils import Vector

SOURCE_SHA256 = '6ef6933d93ee0812d4049446a38e9b46273cab03b21be1e2ef1d502eccdb684b'
ROOT = Path(__file__).resolve().parent


def aim(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat('-Z', 'Y').to_euler()


def softbox(name, location, power, size, target=(0, 0, .6)):
    light = bpy.data.lights.new(name, 'AREA')
    light.energy = power
    light.shape = 'DISK'
    light.size = size
    obj = bpy.data.objects.new(name, light)
    bpy.context.collection.objects.link(obj)
    obj.location = location
    aim(obj, target)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('model', type=Path)
    parser.add_argument('--states', help='Comma-separated open-panel masks, or all (0–63).')
    parser.add_argument('--samples', type=int, default=128)
    parser.add_argument('--output', type=Path, default=ROOT / 'assets')
    parser.add_argument('--motion', action='store_true', help='Perspective camera orbit and articulated end views.')
    parser.add_argument('--frames', default='all', help='Camera frame indices, or all (0–64).')
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    if hashlib.sha256(args.model.read_bytes()).hexdigest() != SOURCE_SHA256:
        parser.error('Model differs from the credited source; see assets/Model3-CREDITS.md.')
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(args.model.resolve()))
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    # Bake the imported transform before placing the car in a metre-scale studio.
    for obj in meshes:
        matrix = obj.matrix_world.copy()
        obj.parent = None
        obj.matrix_world = matrix
        obj.data.transform(matrix)
        obj.matrix_world.identity()
    points = [v.co for obj in meshes for v in obj.data.vertices]
    lower = Vector([min(v[i] for v in points) for i in range(3)])
    upper = Vector([max(v[i] for v in points) for i in range(3)])
    origin = Vector(((lower.x + upper.x) / 2, (lower.y + upper.y) / 2, lower.z))
    scale = 4.72 / (upper.x - lower.x)
    for obj in meshes:
        for vertex in obj.data.vertices:
            vertex.co = (vertex.co - origin) * scale
    # Preserve the source textures and geometry; finish the dark source paint in pearl silver.
    paint = bpy.data.materials['Geohoodsub00021Mtl'].node_tree.nodes.get('Principled BSDF')
    paint.inputs['Base Color'].default_value = (.62, .65, .70, 1)
    paint.inputs['Metallic'].default_value = .55
    paint.inputs['Roughness'].default_value = .24
    paint.inputs['Coat Weight'].default_value = .45
    paint.inputs['Coat Roughness'].default_value = .18
    # Opaque smoked glazing gives a clean silhouette without alpha-sorting artifacts.
    for name in ('Geodoorl2sub31Mtl', 'Geodoorr2sub31Mtl', 'Geoextwindow0021Mtl'):
        shader = bpy.data.materials[name].node_tree.nodes.get('Principled BSDF')
        shader.inputs['Base Color'].default_value = (.009, .014, .022, 1)
        shader.inputs['Alpha'].default_value = 1
        shader.inputs['Metallic'].default_value = .25
        shader.inputs['Roughness'].default_value = .19
    rig = None
    if args.states is not None or args.motion:
        sys.path.insert(0,str(ROOT))
        from vehicle_rig import build_rig,pose
        rig=build_rig(meshes)
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.render.use_persistent_data = True
    scene.cycles.samples = args.samples
    scene.cycles.use_denoising = True
    scene.cycles.seed = 17
    scene.world = bpy.data.worlds.new('Studio')
    scene.world.use_nodes = True
    scene.world.node_tree.nodes['Background'].inputs[0].default_value = (.16, .18, .22, 1)
    scene.world.node_tree.nodes['Background'].inputs[1].default_value = .35
    softbox('Key', (2, -4, 6), 1100, 5)
    softbox('Roof', (-1, 2, 6), 1400, 4)
    softbox('Front edge', (5, 3, 2.5), 850, 3)
    softbox('Rear edge', (-5, -2, 3), 950, 3)
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGBA'
    scene.view_settings.view_transform = 'AgX'
    camera = bpy.data.objects.new('Camera', bpy.data.cameras.new('Camera'))
    bpy.context.collection.objects.link(camera)
    scene.camera = camera
    camera.data.type = 'ORTHO'
    args.output.mkdir(parents=True, exist_ok=True)
    if args.motion:
        render_motion(scene, camera, rig, args)
        return
    masks=(range(64) if args.states=='all' else [int(n,0) for n in args.states.split(',')]) if rig else [0]
    for mask in masks:
        if not 0<=mask<=63:parser.error('Panel mask must be in 0..63')
        if rig:pose(rig,mask)
        render_views(scene,camera,args.output,mask if rig else None)


def render_views(scene,camera,output,mask):
    for name, size, location, target, framing in (
        ('park', (500, 300), (7, -8, 4.5), (0, 0, .7), 5.65),
        ('drive', (230, 260), (-8, 0, 4.8), (0, 0, .65), 3.65),
    ):
        if mask is not None and name=='drive':
            size=(400,300)
            target=(0,0,.9)
            framing=5.1
        camera.location = location
        aim(camera, target)
        camera.data.ortho_scale = framing
        scene.render.resolution_x = size[0] * 3
        scene.render.resolution_y = size[1] * 3
        scene.render.resolution_percentage = 100
        path = output / (f'{name}-{mask:02x}.png' if mask is not None else name+'.png')
        scene.render.filepath = str(path)
        bpy.ops.render.render(write_still=True)
        rendered = bpy.data.images.load(str(path), check_existing=False)
        rendered.scale(*size)
        rendered.save()
        bpy.data.images.remove(rendered)


def camera_pose(camera, t):
    # Bake easing into the frames: playback keeps a uniform 60 Hz time grid.
    # Front three-quarter Park -> rear elevated Drive, without image stretching.
    t = t*t*(3-2*t)
    angle = math.radians(48 + 132*t)
    distance = 7.2 + 13.8*t
    elevation = math.radians(22 + 2*t)
    target = Vector((0, 0, .68))
    camera.location = target + Vector((math.cos(angle)*math.cos(elevation),
        math.sin(angle)*math.cos(elevation), math.sin(elevation)))*distance
    aim(camera, target)
    camera.data.type = 'PERSP'
    camera.data.lens = 50
    camera.data.sensor_width = 36
    camera.data.shift_y = -.045 + .225*t


def render_motion(scene, camera, rig, args):
    from vehicle_rig import pose, PANELS
    scene.render.resolution_x = 1200
    scene.render.resolution_y = 720
    def save(name):
        path = args.output / (name+'.png')
        if path.exists(): return
        scene.render.filepath = str(path)
        bpy.ops.render.render(write_still=True)
        rendered = bpy.data.images.load(str(path), check_existing=False)
        rendered.scale(600,360);rendered.save();bpy.data.images.remove(rendered)
    frames = range(65) if args.frames=='all' else [int(n) for n in args.frames.split(',')]
    for index in frames:
        pose(rig,0);camera_pose(camera,index/64);save(f'camera-{index:02}')
    if args.frames!='all': return
    for view,t in [('park',0),('drive',1)]:
        camera_pose(camera,t)
        for mask in range(64):
            pose(rig,mask);save(f'{view}-{mask:02x}')
        # Intermediate hinge poses for independent doors/lids and all panels.
        for mask in [1,2,4,8,16,32,63]:
            for frame in range(1,12):
                pose(rig,mask)
                for bit,pivot in enumerate(rig):
                    if mask & (1<<bit):
                        pivot.rotation_euler['XYZ'.index(PANELS[bit][2])] *= frame/12
                bpy.context.view_layer.update()
                save(f'{view}-hinge-{mask:02x}-{frame:02}')


if __name__ == '__main__':
    main()
