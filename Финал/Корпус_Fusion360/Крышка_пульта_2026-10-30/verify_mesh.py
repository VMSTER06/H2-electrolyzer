from pathlib import Path
import sys,json,hashlib
sys.path.insert(0,r'S:\Projects\1Electrolizer\Финал\Корпус_Fusion360\Мурик_печать\audit_deps')
import trimesh,numpy as np
out=Path(r'S:\Projects\1Electrolizer\Финал\Корпус_Fusion360\Крышка_пульта_2026-10-30')
desktop=Path(r'C:\Users\vladm\Desktop\Крышка_пульта_клипсы_1.6_без_стоек_2026-10-30.stl')
m=trimesh.load_mesh(desktop)
native=trimesh.load_mesh(out/'lid_native_coordinates.stl')
assert m.is_watertight and m.is_winding_consistent and m.is_volume
assert len(m.split())==1
assert np.allclose(m.bounds[0],[0,0,0],atol=1e-5),m.bounds
assert np.allclose(m.extents,[76.8632450962782,65.52,8.8],atol=1e-4),m.extents
sections=native.section(plane_origin=[0,0,33],plane_normal=[0,0,1]).discrete
clips=[]
for xc,yc in [(-8,-35.3),(35,-35.3),(-8,24.82),(43,24.82)]:
    paths=[p for p in sections if abs((p[:,0].min()+p[:,0].max())/2-xc)<.01 and abs((p[:,1].min()+p[:,1].max())/2-yc)<.01]
    assert len(paths)==1,(xc,yc,len(paths))
    p=paths[0];size=np.ptp(p,axis=0)
    assert abs(size[1]-1.6)<1e-4,size
    clips.append({'center_mm':[xc,yc],'width_mm':float(size[0]),'thickness_mm':float(size[1])})
cad=json.loads((out/'CAD_проверка.json').read_text(encoding='utf-8'))
# Fix the unit label of the native box report (API coordinates are centimeters).
cad['native_bbox_mm']=(native.bounds).tolist()
(out/'CAD_проверка.json').write_text(json.dumps(cad,ensure_ascii=False,indent=2),encoding='utf-8')
# Analytic cylinders/fillets are tessellated in STL; allow 0.1% volume approximation.
deviation=abs(m.volume-cad['volume_after_mm3'])/m.volume
assert deviation<.001
report={'desktop_file':str(desktop),'sha256':hashlib.sha256(desktop.read_bytes()).hexdigest(),
 'watertight':bool(m.is_watertight),'single_closed_body':True,'winding_consistent':bool(m.is_winding_consistent),
 'dimensions_mm':m.extents.tolist(),'volume_mm3':float(m.volume),'triangles':len(m.faces),'clips':clips,
 'orientation':'outer face flat at Z=0; interior and clips face up','supports':0,
 'volume_deviation_from_CAD_percent':deviation*100,
 'physical_snap_test_performed':False,'sliced':(out/'Нарезка/Проверка_нарезки.json').exists()}
(out/'STL_проверка.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))
