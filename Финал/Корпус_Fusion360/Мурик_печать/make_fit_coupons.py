"""Crop actual delivered mesh interfaces into small physical fit coupons."""
from pathlib import Path
import sys,json
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'audit_deps'))
import numpy as np
import trimesh
OUT=ROOT/'Пробники_посадок'
OUT.mkdir(exist_ok=True)
base=trimesh.load_mesh(Path(r'C:\Users\vladm\Desktop\мурик_STL')/'Корпус_основание.stl')
reports=[]
for filename,lo,size,rotation in [
    ('Проверка_крепления_вентилятора.stl',[45,0,7],[50,3,50],[[1,0,0],[0,0,-1],[0,1,0]]),
    ('Проверка_боковых_разъемов_и_тумблера.stl',[0,19,5],[3,75,22],[[0,1,0],[0,0,1],[1,0,0]])]:
    crop=trimesh.creation.box(extents=size)
    crop.apply_translation(np.array(lo)+np.array(size)/2)
    coupon=trimesh.boolean.intersection([base,crop],engine='manifold')
    transform=np.eye(4);transform[:3,:3]=rotation
    coupon.apply_transform(transform);coupon.apply_translation(-coupon.bounds[0])
    assert coupon.is_volume and len(coupon.split())==1
    coupon.export(OUT/filename)
    q=trimesh.load_mesh(OUT/filename)
    assert q.is_volume
    reports.append(dict(file=filename,size_mm=q.extents.tolist(),volume_mm3=float(q.volume),watertight=bool(q.is_watertight)))
(ROOT/'coupon_verification.json').write_text(json.dumps(reports,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(reports,ensure_ascii=False,indent=2))
