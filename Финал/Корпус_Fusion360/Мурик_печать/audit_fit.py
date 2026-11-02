"""Independent dimensional audit of the delivered STL files, in millimetres."""
from pathlib import Path
import sys, json, hashlib, itertools, time
ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / 'audit_deps'))
import numpy as np
import trimesh
from shapely.geometry import Polygon, GeometryCollection, Point
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

INPUT = Path(r'C:\Users\vladm\Desktop\мурик_STL')
REPORT = {'date': '2026-10-18', 'units': 'mm', 'files': {}, 'sections': {}, 'intersections': [], 'estimated_hardware_checks': []}
meshes = {}
for name, filename in [('base', 'Корпус_основание.stl'), ('lid', 'Корпус_крышка.stl'), ('fascia', 'Вставка_кнопки_LED.stl')]:
    path = INPUT / filename
    mesh = trimesh.load_mesh(path, process=True)
    REPORT['files'][name] = dict(file=str(path), sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        print_bounds=mesh.bounds.tolist(), size_mm=mesh.extents.tolist(), volume_mm3=float(mesh.volume),
        watertight=bool(mesh.is_watertight), winding_consistent=bool(mesh.is_winding_consistent),
        connected_components=len(mesh.split(only_watertight=False)), euler_number=int(mesh.euler_number))
    if name != 'base':
        transform = np.diag([1., -1., -1., 1.])
        transform[:3, 3] = [0, 180, 67] if name == 'lid' else [38, 72, 69.5]
        mesh.apply_transform(transform)
    meshes[name] = mesh
    REPORT['files'][name]['assembly_bounds'] = mesh.bounds.tolist()
    print(name, REPORT['files'][name], flush=True)

def section(mesh, axis, value):
    normal = np.eye(3)[axis]
    path = mesh.section(plane_origin=normal*value, plane_normal=normal)
    if path is None:
        return GeometryCollection()
    remaining = [i for i in range(3) if i != axis]
    result = GeometryCollection()
    for loop in path.discrete:
        if len(loop) < 4 or np.linalg.norm(loop[0]-loop[-1]) > 1e-5:
            continue
        polygon = Polygon(loop[:, remaining]).buffer(0)
        if not polygon.is_empty:
            result = result.symmetric_difference(polygon)
    return result

def polygons(geometry):
    if geometry.is_empty:
        return []
    if geometry.geom_type == 'Polygon':
        return [geometry]
    return [p for g in geometry.geoms for p in polygons(g)]

def measure_section(name, axis, value):
    geometry = section(meshes[name], axis, value)
    items = []
    for poly in polygons(geometry):
        holes = []
        for ring in poly.interiors:
            hp = Polygon(ring)
            holes.append(dict(center=list(hp.centroid.coords)[0], bounds=list(hp.bounds),
                              area=float(hp.area), equivalent_diameter=float(np.sqrt(hp.area*4/np.pi))))
        items.append(dict(bounds=list(poly.bounds), area=float(poly.area), holes=holes))
    key=f'{name}_{"xyz"[axis]}_{value:g}'
    REPORT['sections'][key] = items
    return geometry

levels={'base':[1.5, 4, 8.5, 20, 53, 60, 63], 'lid':[52, 56, 60.5, 62, 64.8, 66.5], 'fascia':[50, 54, 65, 68]}
for name, heights in levels.items():
    for z in heights:
        measure_section(name, 2, z)
for axis,value in [(0,1.5),(0,138.5),(1,1.5),(1,178.5)]:
    measure_section('base', axis, value)
for value in [0.05,0.25,0.5,1,2,2.75,2.95,3.05,5,10,12,15]:
    measure_section('base',1,value)

reference_parts={}
for part in ['control_carrier','plunger_1','plunger_2','plunger_3']:
    reference_parts[part]=trimesh.load_mesh(ROOT.parent/'V3_PETG'/'STL_nominal'/(part+'.stl'))
    q=reference_parts[part]
    print('V3 reference',part,'bounds',q.bounds.tolist(),flush=True)
    for p,m in meshes.items():
        overlap=trimesh.boolean.intersection([m,q],engine='manifold')
        item=dict(reference='V3_'+part,part=p,overlap_mm3=float(overlap.volume))
        REPORT['estimated_hardware_checks'].append(item)
        print('V3 reference intersection',item,flush=True)

for a,b in itertools.combinations(meshes,2):
    start=time.monotonic()
    overlap=trimesh.boolean.intersection([meshes[a],meshes[b]],engine='manifold')
    item=dict(a=a,b=b,overlap_mm3=float(overlap.volume),bounds=overlap.bounds.tolist() if len(overlap.faces) else None)
    REPORT['intersections'].append(item)
    print('intersection',item,'seconds',round(time.monotonic()-start,2),flush=True)

def box(lo, size):
    mesh=trimesh.creation.box(extents=size)
    mesh.apply_translation(np.asarray(lo)+np.asarray(size)/2)
    return mesh

refs={
    'main_board_estimate_90x80x20': box([25,20,8],[90,80,20]),
    'relay_board_estimate_75.5x55x20': box([32.25,112,8],[75.5,55,20]),
    'controls_pcb_estimate_30x30':box([55,30,53],[30,30,1.6]),
    'lcd_pcb_estimate_98x60':box([21,98,49.6],[98,60,1.6]),
    'lcd_bezel_estimate':box([21.5,108.8,51.2],[97,38.4,12]),
    'lcd_backpack_estimate':box([32,100,39.2],[42,18,10.4]),
}
for i,x in enumerate([62.38,70,77.62],1):
    refs[f'button_{i}_body_estimate']=box([x-3,50,54.6],[6,6,3.3])
    q=trimesh.creation.cylinder(radius=1.65,height=1,sections=64)
    q.apply_translation([x,53,58.4])
    refs[f'button_{i}_actuator_estimate']=q
for refname,ref in refs.items():
    for part,m in meshes.items():
        if np.any(np.minimum(ref.bounds[1],m.bounds[1])-np.maximum(ref.bounds[0],m.bounds[0])<=1e-5):
            volume=0
        else:
            overlap=trimesh.boolean.intersection([m,ref],engine='manifold')
            volume=float(overlap.volume)
        REPORT['estimated_hardware_checks'].append(dict(reference=refname,part=part,overlap_mm3=volume))
        if volume>0.05:
            print('REFERENCE INTERFERENCE',refname,part,volume,flush=True)

def draw(ax, geometry, color, alpha=.5, label=None):
    for i,poly in enumerate(polygons(geometry)):
        x,y=poly.exterior.xy
        ax.fill(x,y,facecolor=color,edgecolor=color,alpha=alpha,label=label if i==0 else None)
        for ring in poly.interiors:
            x,y=ring.xy
            ax.fill(x,y,facecolor='white',edgecolor=color,alpha=1)
    ax.set_aspect('equal'); ax.grid(alpha=.15)

fig,axs=plt.subplots(2,2,figsize=(12,12))
draw(axs[0,0],section(meshes['base'],2,62),'#475569',label='Корпус')
draw(axs[0,0],section(meshes['lid'],2,62),'#ea580c',label='Бурт крышки')
axs[0,0].set_title('Посадка крышки, сечение Z = 62 мм'); axs[0,0].legend()
draw(axs[0,1],section(meshes['lid'],2,65),'#94a3b8',label='Крышка')
draw(axs[0,1],section(meshes['fascia'],2,65),'#2563eb',label='Стойки вставки')
axs[0,1].set_title('Проход стоек вставки через крышку'); axs[0,1].legend()
draw(axs[1,0],section(meshes['fascia'],2,68),'#2563eb')
axs[1,0].set_title('Вставка: кнопки, LED и крепления')
draw(axs[1,1],section(meshes['base'],0,1.5),'#475569')
axs[1,1].set_title('Левая стенка корпуса, X = 1,5 мм')
for ax in axs.flat:
    ax.set_xlabel('Y, мм' if ax==axs[1,1] else 'X, мм')
    ax.set_ylabel('Z, мм' if ax==axs[1,1] else 'Y, мм')
fig.tight_layout(); fig.savefig(ROOT/'fit_sections.png',dpi=160); plt.close(fig)
(ROOT/'fit_audit.json').write_text(json.dumps(REPORT,ensure_ascii=False,indent=2),encoding='utf-8')
print('SAVED fit_audit.json and fit_sections.png',flush=True)
