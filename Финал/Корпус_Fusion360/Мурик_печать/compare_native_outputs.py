from pathlib import Path
import sys,json
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'audit_deps'))
import numpy as np
import trimesh
WORK=Path(r'C:\Users\vladm\AppData\Local\Temp\murik_native_fix_20261018')
READY=ROOT/'Проверено_2026-10-18'
results=[]
for name,filename,translation in [('base','Корпус_основание.stl',None),('lid','Корпус_крышка.stl',[0,180,67]),('fascia','Вставка_кнопки_LED_исправленная.stl',[38,72,69.5])]:
    native=trimesh.load_mesh(WORK/(name+'_native.stl'))
    reference=trimesh.load_mesh(READY/filename)
    if translation:
        t=np.diag([1.,-1.,-1.,1.]);t[:3,3]=translation
        reference.apply_transform(t)
    assert native.is_volume and reference.is_volume
    assert np.max(np.abs(native.bounds-reference.bounds))<.001
    removed=trimesh.boolean.difference([reference,native],engine='manifold')
    added=trimesh.boolean.difference([native,reference],engine='manifold')
    delta=abs(float(removed.volume))+abs(float(added.volume))
    assert delta<5,(name,delta)
    result=dict(part=name,watertight=bool(native.is_watertight),bounds_mm=native.bounds.tolist(),
        native_volume_mm3=float(native.volume),reference_volume_mm3=float(reference.volume),
        symmetric_difference_mm3=delta,native_triangles=len(native.faces))
    results.append(result)
    print(json.dumps(result,ensure_ascii=False),flush=True)
(ROOT/'Fusion_исправление'/'native_vs_verified_stl.json').write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf-8')
