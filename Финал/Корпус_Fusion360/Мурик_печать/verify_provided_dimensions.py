"""Check recovered user dimensions against the actual delivered meshes."""
from pathlib import Path
import sys,json
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'audit_deps'))
import numpy as np
import trimesh
from shapely.geometry import Polygon,GeometryCollection,box as rect

SOURCE=ROOT/'Проверено_2026-10-18'
meshes={}
for name,filename in [('base','Корпус_основание.stl'),('lid','Корпус_крышка.stl'),('fascia','Вставка_кнопки_LED_исправленная.stl')]:
    mesh=trimesh.load_mesh(SOURCE/filename)
    if name!='base':
        t=np.diag([1.,-1.,-1.,1.]);t[:3,3]=[0,180,67] if name=='lid' else [38,72,69.5]
        mesh.apply_transform(t)
    meshes[name]=mesh

hardware={}
for name,lo,size,evidence in [
    ('main_board',[25,20,8],[90,80,20],'27.09.2026: photo 80x70; correction +10 mm in both plan axes is the existing model interpretation; height 20 mm explicit'),
    ('relay_board',[35,114.5,8],[70,50,20],'27.09.2026: user photos explicitly annotated 7 cm x 5 cm x 2 cm')]:
    mesh=trimesh.creation.box(extents=size);mesh.apply_translation(np.array(lo)+np.array(size)/2)
    hardware[name]=mesh
    results=[]
    for part,m in meshes.items():
        intersection=trimesh.boolean.intersection([mesh,m],engine='manifold')
        volume=float(intersection.volume)
        assert abs(volume)<.01,(name,part,volume)
        results.append(dict(part=part,intersection_mm3=volume))
    hardware[name]=(mesh,dict(size_mm=size,position_min_mm=lo,evidence=evidence,checks=results))

overlap=float(trimesh.boolean.intersection([v[0] for v in hardware.values()],engine='manifold').volume)
assert overlap<.01
support_section=meshes['base'].section(plane_origin=[0,0,7.999],plane_normal=[0,0,1])
support=GeometryCollection()
for loop in support_section.discrete:
    support=support.symmetric_difference(Polygon(loop[:,:2]).buffer(0))
relay_footprint=rect(35,114.5,105,164.5)
contact=support.intersection(relay_footprint)
patches=[contact] if contact.geom_type=='Polygon' else list(contact.geoms)
assert len(patches)==4
result=dict(date='2026-10-18',source_thread='01a0e1b5-2336-75f1-ac25-19954ee948b9',
    source_photos_folder=str(ROOT.parent/'Фото_комплектующих'),
    boards={k:v[1] for k,v in hardware.items()},boards_overlap_mm3=overlap,
    relay_support_patches=[dict(bounds_mm=list(p.bounds),area_mm2=float(p.area)) for p in patches],
    controls_size_status='User authorized estimate from photo on 27.09.2026; 30x30 is not a separately supplied exact measurement',
    missing_in_recovered_six_photos=['case fan mounting drawing','separate toggle mounting drawing'])
(ROOT/'provided_dimensions_verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=False,indent=2))
