"""Create a separate insert compatible with the existing V3 control mechanism."""
from pathlib import Path
import sys,json,hashlib,shutil
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'audit_deps'))
import numpy as np
import trimesh
from shapely.geometry import Polygon,GeometryCollection
from shapely.ops import unary_union
OUT=ROOT/'Проверено_2026-10-18'
OUT.mkdir(exist_ok=True)
INPUT=Path(r'C:\Users\vladm\Desktop\мурик_STL')

def box(lo,size):
    mesh=trimesh.creation.box(extents=size)
    mesh.apply_translation(np.asarray(lo)+np.asarray(size)/2)
    return mesh

def cyl(x,y,z,diameter,height):
    mesh=trimesh.creation.cylinder(radius=diameter/2,height=height,sections=128)
    mesh.apply_translation([x,y,z+height/2])
    return mesh

original=trimesh.load_mesh(INPUT/'Вставка_кнопки_LED.stl')
T=np.diag([1.,-1.,-1.,1.]);T[:3,3]=[38,72,69.5]
original.apply_transform(T)
# Restore the plate material in the two modified openings.
filled=trimesh.boolean.union([original,
    box([61.2499,51.4999,67],[17.0002,3.5002,2.5]),
    box([60.4999,37.9999,67],[19.5002,6.5002,2.5])],engine='manifold')
# 6.3 mm guide holes give 0.4 mm radial clearance for the existing 5.5 mm tips.
tools=[cyl(x,53,66.9,6.3,2.7) for x in [62.38,70,77.62]]
# Restore the 22 x 8 mm common LED window, centred over the reference LEDs.
led=trimesh.boolean.union([
    box([61,35,66.9],[18,8,2.7]),box([59,37,66.9],[22,4,2.7]),
    *[cyl(x,y,66.9,4,2.7) for x in [61,79] for y in [37,41]]
],engine='manifold')
fixed=trimesh.boolean.difference([filled,*tools,led],engine='manifold')
assert fixed.is_watertight and fixed.is_winding_consistent
assert len(fixed.split())==1
checks=[]
reference=ROOT.parent/'V3_PETG'/'STL_nominal'
for name in ['control_carrier','plunger_1','plunger_2','plunger_3']:
    mesh=trimesh.load_mesh(reference/(name+'.stl'))
    old_overlap=float(trimesh.boolean.intersection([original,mesh],engine='manifold').volume)
    new_overlap=float(trimesh.boolean.intersection([fixed,mesh],engine='manifold').volume)
    assert new_overlap<.01,(name,new_overlap)
    checks.append(dict(part=name,old_overlap_mm3=old_overlap,new_overlap_mm3=new_overlap))

# Keep the delivered case orientation; copy these files byte-for-byte.
for filename in ['Корпус_основание.stl','Корпус_крышка.stl']:
    shutil.copyfile(INPUT/filename,OUT/filename)
for filename,translation in [('Корпус_крышка.stl',[0,180,67]),('Корпус_основание.stl',None)]:
    mesh=trimesh.load_mesh(INPUT/filename)
    if translation:
        tr=np.diag([1.,-1.,-1.,1.]);tr[:3,3]=translation;mesh.apply_transform(tr)
    vol=float(trimesh.boolean.intersection([fixed,mesh],engine='manifold').volume)
    assert abs(vol)<.01,(filename,vol)
    checks.append(dict(part=filename,new_overlap_mm3=vol))

path=fixed.section(plane_origin=[0,0,68],plane_normal=[0,0,1])
regions=GeometryCollection()
for loop in path.discrete:
    regions=regions.symmetric_difference(Polygon(loop[:,:2]).buffer(0))
holes=[Polygon(r) for r in regions.interiors]
guides=[]
for x in [62.38,70,77.62]:
    hole=min(holes,key=lambda p:p.centroid.distance(__import__('shapely').geometry.Point(x,53)))
    center=np.array(hole.centroid.coords[0]);bounds=np.array(hole.bounds)
    assert np.linalg.norm(center-[x,53])<.002
    assert np.max(np.abs((bounds[2:]-bounds[:2])-6.3))<.002
    guides.append(dict(center_mm=center.tolist(),size_mm=(bounds[2:]-bounds[:2]).tolist()))

# Inspect the light opening and nominal projected LEDs at z=68.
led_hole=min(holes,key=lambda p:p.centroid.distance(__import__('shapely').geometry.Point(70,39)))
led_clearance=[]
for x in [63.65,70,76.35]:
    p=__import__('shapely').geometry.Point(x,39)
    clearance=float(led_hole.boundary.distance(p)-1.5)
    assert led_hole.contains(p) and clearance>0
    led_clearance.append(clearance)

# Export face down, with the exact same 64 x 50 x 20.5 mm print envelope.
fixed_print=fixed.copy(); inv=np.linalg.inv(T);fixed_print.apply_transform(inv)
filename='Вставка_кнопки_LED_исправленная.stl'
fixed_print.export(OUT/filename)
reloaded=trimesh.load_mesh(OUT/filename)
assert reloaded.is_volume and len(reloaded.split())==1
assert np.max(np.abs(reloaded.extents-[64,50,20.5]))<.002
report=dict(date='2026-10-18',scope='Fits V3 nominal carrier and plungers; physical boards and fan unmeasured',
    output=str(OUT/filename),sha256=hashlib.sha256((OUT/filename).read_bytes()).hexdigest(),
    size_mm=reloaded.extents.tolist(),watertight=bool(reloaded.is_watertight),
    volume_mm3=float(reloaded.volume),guide_holes=guides,led_window_bounds_mm=list(led_hole.bounds),
    projected_led_edge_clearance_mm=led_clearance,checks=checks)
(ROOT/'insert_repair_verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2),flush=True)

# A focused before/after drawing, using the measured plate sections.
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
fig,axes=plt.subplots(1,2,figsize=(11,5))
for ax,mesh,title in [(axes[0],original,'До: прорезь 3,5 мм'),(axes[1],fixed,'После: три направляющих Ø6,3 мм')]:
    sec=mesh.section(plane_origin=[0,0,68],plane_normal=[0,0,1])
    for loop in sec.discrete:
        ax.plot(loop[:,0],loop[:,1],color='#2563eb',linewidth=1.2)
    for x in [62.38,70,77.62]:
        ax.add_patch(plt.Circle((x,53),2.75,facecolor='#ea580c',alpha=.5))
    ax.set(xlim=(37,103),ylim=(21,73),aspect='equal',title=title,xlabel='X, мм',ylabel='Y, мм')
    ax.grid(alpha=.15)
fig.tight_layout();fig.savefig(ROOT/'insert_before_after.png',dpi=160)
