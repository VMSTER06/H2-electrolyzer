import adsk.core, adsk.fusion, os, json, traceback, importlib.util
OUT=r'S:\1Electrolizer\Финал\Корпус_Fusion360'
spec=importlib.util.spec_from_file_location('helpers',os.path.join(OUT,'H2_Brain_Enclosure','H2_Brain_Enclosure.py'))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
p,box,posts,drilled,coll=m.p,m.box,m.posts,m.drilled,m.coll
def run(context):
 app=adsk.core.Application.get()
 try:
  d=adsk.fusion.Design.cast(app.activeProduct);root=d.rootComponent;ex=d.exportManager
  occ=list(root.occurrences);base,lid=occ[0].component,occ[1].component;b=base.bRepBodies.item(0)
  for name,dx,dy in [('V2 MAIN clamp 1',0,1.3),('V2 RELAY clamp 3',-0.08,0)]:
   o=next(o for o in occ if o.component.name==name);o.transform2=adsk.core.Matrix3D.create()
   t=adsk.core.Matrix3D.create();t.translation=adsk.core.Vector3D.create(dx,dy,0)
   mv=o.component.features.moveFeatures
   mv.add(mv.createInput(coll([o.component.bRepBodies.item(0)]),t)).name='V2 permanent clamp clearance correction'
  d.computeAll();adsk.doEvents();d.computeAll()
  names=['base','lid']+['main_clamp_'+str(i) for i in range(1,5)]+['relay_clamp_'+str(i) for i in range(1,5)]+['control_fascia','control_carrier']+['plunger_'+str(i) for i in range(1,4)]
  parts=[(name,o.component,o) for name,o in zip(names,occ[:15])];refs=occ[15:]
  issues=[]
  for i in range(d.timeline.count):
   e=d.timeline.item(i).entity
   if e and hasattr(e,'healthState') and e.healthState!=adsk.fusion.FeatureHealthStates.HealthyFeatureHealthState:issues.append({'name':e.name,'message':e.errorOrWarningMessage})
  ip=d.createInterferenceInput(coll(occ));ip.areCoincidentFacesIncluded=False;ir=d.analyzeInterference(ip)
  report=json.load(open(os.path.join(OUT,'verification_v2.json'),encoding='utf-8'))
  report['feature_problems']=issues;report['interference_count']=ir.count if ir else 0
  report['interferences']=[{'a':r.entityOne.name,'b':r.entityTwo.name,'volume_cm3':r.interferenceBody.volume} for r in ir] if ir else []
  report['parts']=[]
  for name,c,o in parts:
   assert c.bRepBodies.count==1 and c.bRepBodies.item(0).isSolid,name
   report['parts'].append({'name':name,'volume_cm3':c.bRepBodies.item(0).volume})
   opt=ex.createSTLExportOptions(o,os.path.join(OUT,name+'.stl'));opt.meshRefinement=adsk.fusion.MeshRefinementSettings.MeshRefinementHigh;assert ex.execute(opt)
  json.dump(report,open(os.path.join(OUT,'verification_v2.json'),'w',encoding='utf-8'),indent=2)
  assert not issues and report['interference_count']==0,str(report)
  vp=app.activeViewport
  def view(eye,target,up):
   cam=vp.camera;cam.eye=p(*eye);cam.target=p(*target);cam.upVector=adsk.core.Vector3D.create(*up);cam.isFitView=True;cam.isSmoothTransition=False;vp.camera=cam;vp.fit();adsk.doEvents()
  for o in occ:o.isLightBulbOn=True;o.component.isSketchFolderLightBulbOn=False;o.component.isConstructionFolderLightBulbOn=False
  view((280,-280,380),(70,90,30),(0,0,1));vp.saveAsImageFile(os.path.join(OUT,'assembled_v2.png'),1800,1500)
  view((70,90,500),(70,90,0),(0,1,0));vp.saveAsImageFile(os.path.join(OUT,'top_v2.png'),1400,1800)
  topocc=[occ[1]]+occ[10:15]+occ[17:19]
  for o in topocc:o.isLightBulbOn=False
  view((190,-210,460),(70,90,15),(0,0,1));vp.saveAsImageFile(os.path.join(OUT,'internals_v2.png'),1800,1500)
  for o in topocc:o.isLightBulbOn=True
  origins=[o.transform2 for o in topocc]
  for o in topocc:
   t=adsk.core.Matrix3D.create();t.translation=adsk.core.Vector3D.create(0,0,9);o.transform2=t
  view((300,-300,400),(70,90,65),(0,0,1));vp.saveAsImageFile(os.path.join(OUT,'exploded_v2.png'),1800,1700)
  for o,t in zip(topocc,origins):o.transform2=t
  view((280,-280,380),(70,90,30),(0,0,1))
  assert ex.execute(ex.createFusionArchiveExportOptions(os.path.join(OUT,'H2_Brain_LCD2004_v2.f3d')))
  assert ex.execute(ex.createSTEPExportOptions(os.path.join(OUT,'H2_Brain_LCD2004_v2.step')))
  open(os.path.join(OUT,'repair_log.txt'),'w',encoding='utf-8').write('SUCCESS: zero interference, all features healthy, 15 solid printable parts.\n'+json.dumps(report))
 except:
  open(os.path.join(OUT,'repair_log.txt'),'w',encoding='utf-8').write(traceback.format_exc());app.userInterface.messageBox(traceback.format_exc())

