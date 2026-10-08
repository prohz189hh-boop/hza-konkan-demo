"""Original two-tier rack, metres in Blender; no existing assets modified."""
import bpy, bmesh, json, math
from pathlib import Path
from mathutils import Vector, Quaternion

ROOT = Path(__file__).resolve().parent
if bpy.data.objects.get('SM_HZA_Rack_Main'):
    raise RuntimeError('Rack already exists; inspect before rebuilding')
scene = bpy.data.scenes.new('HZA_REVAMP_RackWorkshop')
collection = bpy.data.collections.new('HZA_REVAMP_Racks')
scene.collection.children.link(collection)
bpy.context.window.scene = scene

def material(name, color, rough, metal):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    shader = next(n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    shader.inputs['Base Color'].default_value = (*color, 1)
    shader.inputs['Roughness'].default_value = rough
    shader.inputs['Metallic'].default_value = metal
    mat.diffuse_color = (*color, 1)
    return mat

mats = [material('M_HZA_Rack_Navy', (.012, .027, .052), .36, .12),
        material('M_HZA_Rack_Bronze', (.32, .18, .065), .34, .72),
        material('M_HZA_Rack_Charcoal', (.017, .021, .028), .7, 0)]
parts = []

def finish(name, verts, faces, mat, bevel):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    bm = bmesh.new(); bm.from_mesh(mesh)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    assert all(e.is_manifold for e in bm.edges), name
    bm.to_mesh(mesh); bm.free()
    obj = bpy.data.objects.new(name, mesh); collection.objects.link(obj)
    obj.data.materials.append(mats[mat])
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    mod = obj.modifiers.new('Machined soft edges', 'BEVEL')
    mod.width = bevel; mod.segments = 3
    bpy.ops.object.modifier_apply(modifier=mod.name)
    # Per-face projection; no textures or lightmap dependency. All UVs finite in 0..1.
    uv = obj.data.uv_layers.new(name='UVMap')
    for p in obj.data.polygons:
        axis = max(range(3), key=lambda i: abs(p.normal[i]))
        axes = [i for i in range(3) if i != axis]
        for li in p.loop_indices:
            co = obj.data.vertices[obj.data.loops[li].vertex_index].co
            uv.data[li].uv = (co[axes[0]]/.7+.5, co[axes[1]]/.7+.5)
    obj.select_set(False); parts.append(obj)
    return obj

def box(name, lo, hi, mat, bevel=.0005):
    verts = [(x,y,z) for x in [lo[0],hi[0]] for y in [lo[1],hi[1]] for z in [lo[2],hi[2]]]
    return finish(name, verts, [(0,4,6,2),(1,3,7,5),(0,1,5,4),(2,6,7,3),(0,2,3,1),(4,5,7,6)], mat, bevel)

# Sculpted stepped cross-section: two open channels, uninterrupted slide surfaces.
# Lower tile centre (0,0,.048) matches the existing Unreal placement exactly.
profile = [(-.018,0),(.064,0),(.064,.104),(.050,.104),(.050,.065),
           (.024,.065),(.024,.052),(.009,.052),(.009,.027),(-.018,.027)]
n = len(profile)
verts = [(x,y,z) for x in [-.303,.303] for y,z in profile]
faces = [tuple(range(n-1,-1,-1)),tuple(range(n,n*2))]
faces += [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
finish('Rack_SculptedBody', verts, faces, 0, .0012)
for label,y,z in [('Lower',0,.028),('Upper',.040,.066)]:
    box('Rack_'+label+'Liner',(-.299,y-.008,z-.001),(.299,y+.0075,z),2,.00025)
    box('Rack_'+label+'Lip',(-.302,y-.012,z-.003),(.302,y-.008,z+.0025),0,.0006)
    box('Rack_'+label+'Bronze',(-.301,y-.0124,z-.0015),(.301,y-.0117,z-.0003),1,.0002)
# Thin charcoal end caps and a single restrained bronze crown detail.
for side in [-1,1]:
    x=side*.3065
    box('Rack_EndCap', (x-.0035,-.018,0),(x+.0035,.064,.104),2,.0015)
    box('Rack_EndInlay',(x-.001,-.019,.009),(x+.001,-.018,.024),1,.0003)
box('Rack_Crown',(-.30,.050,.102),(.30,.052,.104),1,.0004)
# Four soft feet are part of the same mesh and do not add draw calls.
for x in [-.245,.245]:
    for y in [-.008,.052]:
        box('Rack_Foot',(x-.017,y-.006,-.001),(x+.017,y+.006,.001),2,.0005)

bpy.ops.object.select_all(action='DESELECT')
for obj in parts: obj.select_set(True)
bpy.context.view_layer.objects.active = parts[0]
bpy.ops.object.join()
rack = bpy.context.object; rack.name = 'SM_HZA_Rack_Main'; rack.data.name = rack.name
rack['HZA_slots_per_row']=15; rack['HZA_rows']=2; rack['HZA_pitch_cm']=3.3
rack['HZA_lower_tile_center_m']=[0,0,.048]
rack['HZA_upper_tile_center_m']=[0,.040,.086]
rack['HZA_pivot']='Centre of lower row in XY; base at Z=0'
bpy.ops.export_scene.gltf(filepath=str(ROOT/'HZA_Rack_Revamp.glb'),use_selection=True,use_active_scene=True)

preview = bpy.data.scenes.new('HZA_REVAMP_RackPreview')
preview.collection.objects.link(rack)
tiles = bpy.data.scenes.get('HZA_REVAMP_TileWorkshop')
if tiles is None:
    with bpy.data.libraries.load(str(ROOT.parent/'tiles/HZA_Tiles_Revamp.blend'),link=False) as (src,dst):
        dst.scenes=['HZA_REVAMP_TileWorkshop']
    tiles=dst.scenes[0]
# Reference-style two-row sample: 15 tile hand, 8 below and 7 above.
for i in range(15):
    row=0 if i<8 else 1; col=i if row==0 else i-8
    source=tiles.objects['SM_HZA_Tile_'+['red','black','blue','yellow'][i%4]+'_%02d'%(i%13+1)]
    obj=source.copy(); obj.name='RackPreview_Tile_%02d'%i
    preview.collection.objects.link(obj)
    obj.location=((col-3.5)*.033,row*.040,.048+row*.038)

# Full-copy save avoids a Blender 5.2.2 partial-library write crash with shared scenes.
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'HZA_Rack_Revamp.blend'),copy=True)
mesh=rack.data; mesh.calc_loop_triangles()
report={'mesh':rack.name,'triangles':len(mesh.loop_triangles),'materials':len(mesh.materials),
        'dimensions_m':list(rack.dimensions),'rows':2,'slots_per_row':15,'slot_pitch_m':.033,
        'lower_center_m':[0,0,.048],'upper_center_m':[0,.040,.086],
        'tile_dimensions_m':[.028,.00904,.040],'end_clearance_m':.054,
        'source':'Original reference-inspired HZA design; old rack not reused'}
(ROOT/'mesh-report.json').write_text(json.dumps(report,indent=2))
bpy.context.window.scene=preview
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':
        space=area.spaces.active
        space.region_3d.view_rotation=Quaternion((1,0,0),math.radians(68))
        space.region_3d.view_location=(0,.02,.045)
        space.region_3d.view_distance=.85
        space.shading.type='MATERIAL'; space.overlay.show_overlays=False
print(json.dumps(report))
