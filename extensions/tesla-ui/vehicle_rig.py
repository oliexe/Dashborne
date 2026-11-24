"""Panel articulation for the credited Highland mesh, in normalized Blender coordinates."""
import math
import bpy
import bmesh
from mathutils import Vector

# Bit order is vehicle-left front/rear, vehicle-right front/rear, frunk, trunk.
PANELS = (
    ('front_left', (.91, .84, .65), 'Z', -62),
    ('rear_left', (-.22, .86, .65), 'Z', -62),
    ('front_right', (.91, -.84, .65), 'Z', 62),
    ('rear_right', (-.22, -.86, .65), 'Z', 62),
    ('frunk', (1.105, 0, .87), 'Y', -55),
    ('trunk', (-1.72, 0, 1.07), 'Y', 65),
)


FRONT = [(.94,.24),(-.205,.24),(-.315,1.325),(.22,1.325),(.78,1.025),(.94,.90)]
REAR = [(-.205,.24),(-1.28,.25),(-1.17,.91),(-1.18,1.325),(-.315,1.325)]


def cut_panel_boundaries(mesh):
    # Split intersected faces at panel boundaries before assigning them to hinges.
    # Selecting whole triangles by their centers leaves sawtooth edges on the skin.
    planes=[]
    for polygon in (FRONT,REAR):
        for a,b in zip(polygon,polygon[1:]+polygon[:1]):
            planes.append(((a[0],0,a[1]),(b[1]-a[1],0,a[0]-b[0])))
    for axis,values in ((0,(1.105,-1.73,-2.20)),(1,(-1.05,-.725,-.70,-.60,-.56,.56,.60,.70,.725,1.05)),(2,(.59,.87,.58))):
        for value in values:
            point=[0,0,0];point[axis]=value
            normal=[0,0,0];normal[axis]=1
            planes.append((point,normal))
    bm=bmesh.new();bm.from_mesh(mesh)
    for point,normal in planes:
        bmesh.ops.bisect_plane(bm,geom=list(bm.verts)+list(bm.edges)+list(bm.faces),
            plane_co=point,plane_no=normal,dist=.000001,clear_inner=False,clear_outer=False)
    bm.to_mesh(mesh);bm.free()


def inside_polygon(x, z, polygon):
    inside = False
    for a, b in zip(polygon, polygon[1:] + polygon[:1]):
        if (a[1] > z) != (b[1] > z) and x < (b[0]-a[0])*(z-a[1])/(b[1]-a[1])+a[0]:
            inside = not inside
    return inside


def panel_for(point, material):
    x, y, z = point
    # Hood and trunk are hinged lids; the rear windshield remains on the body.
    if 'extwindow' not in material:
        if x > 1.105 and abs(y) < .725 and z > .59 and material not in ('Ln7Mtl', 'Ln1Mtl'):
            return 4
        if ((x < -1.73 and abs(y) < .56 and z > .87) or
            (x < -2.20 and abs(y) < .70 and z > .58)):
            return 5
    if abs(y) < .60 or abs(y) > 1.05:
        return None
    # Tires and wheel faces never move with a door.
    if 'Tire' in material or 'sub021' in material:
        return None
    if inside_polygon(x,z,FRONT):
        return 0 if y > 0 else 2
    if inside_polygon(x,z,REAR):
        return 1 if y > 0 else 3
    return None


def build_rig(meshes):
    pivots=[]
    for name,origin,axis,angle in PANELS:
        pivot=bpy.data.objects.new(name,None)
        bpy.context.collection.objects.link(pivot)
        pivot.location=origin
        pivots.append(pivot)
    bpy.context.view_layer.update()
    for obj in meshes:
        mesh=obj.data
        cut_panel_boundaries(mesh)
        groups={None:[]}
        for face in mesh.polygons:
            center=sum((mesh.vertices[i].co for i in face.vertices),Vector())/len(face.vertices)
            part=panel_for(center,mesh.materials[face.material_index].name)
            groups.setdefault(part,[]).append(face.index)
        # Copy and delete faces with bmesh to retain UVs, materials and source topology.
        for part,faces in groups.items():
            if not faces:continue
            data=mesh.copy()
            bm=bmesh.new();bm.from_mesh(data);bm.faces.ensure_lookup_table()
            keep=set(faces)
            bmesh.ops.delete(bm,geom=[f for f in bm.faces if f.index not in keep],context='FACES')
            bm.to_mesh(data);bm.free()
            piece=bpy.data.objects.new(obj.name+'_'+str(part),data)
            bpy.context.collection.objects.link(piece)
            if part is not None:
                piece.parent=pivots[part]
                piece.matrix_parent_inverse=pivots[part].matrix_world.inverted()
        bpy.data.objects.remove(obj,do_unlink=True)
    return pivots


def pose(pivots, mask):
    for bit,(pivot,spec) in enumerate(zip(pivots,PANELS)):
        pivot.rotation_euler=(0,0,0)
        pivot.rotation_euler['XYZ'.index(spec[2])]=math.radians(spec[3]) if mask & (1<<bit) else 0
    bpy.context.view_layer.update()
