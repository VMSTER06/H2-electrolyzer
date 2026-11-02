"""Orient native Fusion STL files onto the bed and optionally compensate measured XY scale.
No external packages. Never edits CAD or the source STL files.
"""
import argparse, struct, pathlib, math, json

def read_stl(path):
 data=path.read_bytes();n=struct.unpack_from('<I',data,80)[0]
 if len(data)!=84+n*50:raise ValueError('Expected binary STL: '+str(path))
 return [[list(struct.unpack_from('<3f',data,84+i*50+12+j*12)) for j in range(3)] for i in range(n)]

def write_stl(path,triangles):
 with path.open('wb') as f:
  f.write(b'H2 V3 PETG print orientation and explicit XY correction'.ljust(80,b' '));f.write(struct.pack('<I',len(triangles)))
  for a,b,c in triangles:
   u=[b[i]-a[i] for i in range(3)];v=[c[i]-a[i] for i in range(3)]
   n=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]]
   norm=math.sqrt(sum(x*x for x in n));n=[x/norm for x in n] if norm else [0,0,0]
   f.write(struct.pack('<12fH',*(n+a+b+c),0))

def prepare(source,dest,sx=1,sy=1):
 dest.mkdir(parents=True,exist_ok=True);report=[]
 for path in sorted(source.glob('*.stl')):
  t=read_stl(path);flip=path.stem in ['lid','control_fascia'] or path.stem.startswith('plunger_')
  for tri in t:
   for q in tri:
    if flip:q[1]*=-1;q[2]*=-1
    q[0]*=sx;q[1]*=sy
  low=[min(v[i] for tri in t for v in tri) for i in range(3)]
  high=[max(v[i] for tri in t for v in tri) for i in range(3)]
  for tri in t:
   for q in tri:
    for i in range(3):q[i]-=low[i]
  write_stl(dest/path.name,t)
  report.append({'file':path.name,'triangles':len(t),'bed_dimensions_mm':[round(high[i]-low[i],5) for i in range(3)],'xy_scale':[sx,sy],'z_scale':1.0,'flipped_about_x':flip})
 (dest/'dimensions.json').write_text(json.dumps(report,indent=2),encoding='utf-8');return report

if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--shrink-xy',type=float,default=0,help='Fraction, e.g. 0.003 for an UNVERIFIED trial 0.3 percent')
 ap.add_argument('--measured-x',type=float,help='Measured 100 mm coupon X span after cooling')
 ap.add_argument('--measured-y',type=float,help='Measured 100 mm coupon Y span after cooling')
 ap.add_argument('--output',default='STL_print_nominal');args=ap.parse_args()
 if not 0<=args.shrink_xy<0.02:ap.error('Shrink fraction must be between 0 and 0.02')
 if bool(args.measured_x)!=bool(args.measured_y):ap.error('Supply both measured axes')
 if args.measured_x and (not 97<args.measured_x<103 or not 97<args.measured_y<103):ap.error('Coupon outside expected range; inspect printer/calipers before scaling')
 sx=100/args.measured_x if args.measured_x else 1/(1-args.shrink_xy)
 sy=100/args.measured_y if args.measured_y else sx
 root=pathlib.Path(__file__).resolve().parent
 result=prepare(root/'STL_nominal',root/args.output,sx,sy)
 if not result:raise RuntimeError('Fusion STL export has not completed')
 print(json.dumps({'count':len(result),'xy_scale_percent':[sx*100,sy*100],'output':str(root/args.output)},ensure_ascii=False))
