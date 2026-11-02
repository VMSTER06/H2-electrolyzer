from pathlib import Path
import sys,json
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'audit_deps'))
import numpy as np
import trimesh
from shapely.geometry import Polygon,GeometryCollection
SOURCE=Path(r'C:\Users\vladm\Desktop\мурик_STL')
meshes={}
for name,fn in [('base','Корпус_основание.stl'),('lid','Корпус_крышка.stl'),('fascia','Вставка_кнопки_LED.stl')]:
    m=trimesh.load_mesh(SOURCE/fn)
    if name!='base':
        t=np.diag([1.,-1.,-1.,1.]);t[:3,3]=[0,180,67] if name=='lid' else [38,72,69.5]
        m.apply_transform(t)
    meshes[name]=m

def crossings(mesh,origin,direction):
    xyz,_,_=mesh.ray.intersects_location([origin],[direction],multiple_hits=True)
    return sorted(set(round(float(np.dot(p,direction)),5) for p in xyz))

measurements={}
for key,name,point in [('cover_pilot_bottom','base',[8,8,-10]),
    ('cover_head_pocket_floor','lid',[10.5,8,-10]),
    ('fascia_pilot_bottom','lid',[43,32,-10]),
    ('lcd_pilot_back','lid',[23.5,100.5,-10]),
    ('carrier_pilot_back','fascia',[45,44,-10])]:
    measurements[key]=crossings(meshes[name],point,[0,0,1])
measurements['fan_corner_web_y_crossings']=crossings(meshes['base'],[88.85,-10,49.646],[0,1,0])

for x in [.05,.5,1,1.5,2,2.5,2.95]:
    section=meshes['base'].section(plane_origin=[x,0,0],plane_normal=[1,0,0])
    shape=GeometryCollection()
    for loop in section.discrete:
        shape=shape.symmetric_difference(Polygon(loop[:,[1,2]]).buffer(0))
    polys=[shape] if shape.geom_type=='Polygon' else list(shape.geoms)
    measurements[f'side_wall_x_{x:g}']=[dict(center=list(Polygon(r).centroid.coords[0]),bounds=list(Polygon(r).bounds))
        for p in polys for r in p.interiors]

# Verify the mating profiles directly in a horizontal section, excluding axial contact.
def plane_shape(mesh,z):
    path=mesh.section(plane_origin=[0,0,z],plane_normal=[0,0,1]);shape=GeometryCollection()
    for loop in path.discrete:
        shape=shape.symmetric_difference(Polygon(loop[:,:2]).buffer(0))
    return shape
measurements['lid_to_base_min_lateral_clearance_mm']=plane_shape(meshes['base'],62).distance(plane_shape(meshes['lid'],62))
measurements['fascia_posts_to_lid_min_clearance_mm']=plane_shape(meshes['lid'],65).distance(plane_shape(meshes['fascia'],65))
(ROOT/'mount_measurements.json').write_text(json.dumps(measurements,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(measurements,ensure_ascii=False,indent=2))
