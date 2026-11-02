"""Verify exported printing meshes, edge closure, volume and bed placement."""
from prepare_print import read_stl
import pathlib, collections, json, math

root=pathlib.Path(__file__).resolve().parent
report=[]
for folder in ['STL_print_nominal','STL_print_trial_XY_100.301']:
 files=sorted((root/folder).glob('*.stl'))
 assert len(files)==15,(folder,len(files))
 for path in files:
  triangles=read_stl(path);edges=collections.Counter();volume=0
  for tri in triangles:
   keys=[tuple(round(x,5) for x in v) for v in tri]
   for a,b in zip(keys,keys[1:]+keys[:1]):edges[tuple(sorted((a,b)))]+=1
   a,b,c=tri
   volume+=(a[0]*(b[1]*c[2]-b[2]*c[1])+a[1]*(b[2]*c[0]-b[0]*c[2])+a[2]*(b[0]*c[1]-b[1]*c[0]))/6
  minz=min(v[2] for t in triangles for v in t)
  dims=[max(v[i] for t in triangles for v in t)-min(v[i] for t in triangles for v in t) for i in range(3)]
  bad=sum(1 for n in edges.values() if n!=2)
  item={'file':folder+'/'+path.name,'triangles':len(triangles),'non_manifold_edges':bad,'signed_volume_mm3':volume,'min_z_mm':minz,'dimensions_mm':dims}
  report.append(item)
  assert bad==0 and volume>0 and abs(minz)<1e-5,item
  assert dims[0]<=300 and dims[1]<=300 and dims[2]<=300,item
(root/'mesh_verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('PASS:',len(report),'closed, outward-oriented STL meshes, all on bed, all within 300 mm build volume')
