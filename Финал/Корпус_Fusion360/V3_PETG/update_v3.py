"""Fusion script: revise the saved V2 design, retaining its history. Dimensions mm."""
import adsk.core, adsk.fusion, os, json, traceback, importlib.util
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT=os.path.join(ROOT,'V3_PETG')
spec=importlib.util.spec_from_file_location('h2helpers',os.path.join(ROOT,'H2_Brain_Enclosure','H2_Brain_Enclosure.py'))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
p,val,box,posts,drilled,sketch,rect,coll,extrude=m.p,m.val,m.box,m.posts,m.drilled,m.sketch,m.rect,m.coll,m.extrude
NEW,JOIN,CUT=m.NEW,m.JOIN,m.CUT

def run(context):
 app=adsk.core.Application.get();log=[]
 def note(s):
  log.append(str(s));open(os.path.join(OUT,'build_log.txt'),'w',encoding='utf-8').write('\n'.join(log))
 # Import the verified local V2 into an editable document; the open read-only document is untouched.
 doc=app.importManager.importToNewDocument(app.importManager.createFusionArchiveImportOptions(os.path.join(ROOT,'H2_Brain_LCD2004_v2.f3d')))
 doc.name='H2 PRO V3 - PETG NIT - K1 Max 0.6'
 d=adsk.fusion.Design.cast(app.activeProduct);root=d.rootComponent;ex=d.exportManager
 occ=list(root.occurrences);assert len(occ)==19
 base,lid=occ[0].component,occ[1].component;bo,lo=occ[:2]
 b,lb=base.bRepBodies.item(0),lid.bRepBodies.item(0)
 pars=[('v3_lip_gap',0.5,'PETG removable cover lateral gap each side'),('v3_m3_clear',3.6,'M3 clearance for nozzle 0.6'),('v3_button_guide',6.3,'Guide for 5.5 mm plunger: 0.4 mm radial clearance'),('v3_switch_height',4.3,'Reference B3F-1000 height; actual unmarked switch requires fit check'),('v3_relay_w',75.5,'Envelope of documented Addicore variants; not positively identified'),('v3_relay_l',55,'Largest documented plan length'),('v3_lcd_standoff',12.8,'PCB front to lid underside; conservative 12 mm bezel plus 0.8 gap')]
 for name,v,comment in pars:d.userParameters.add(name,val(v),'mm',comment)
 d.userParameters.add('v3_shrink_xy',adsk.core.ValueInput.createByReal(0.003),'','TRIAL ONLY 0.3 percent; applied only by STL export utility, never to CAD')
 # Remove obsolete tiny engravings, then apply larger plain labels for a 0.6 mm nozzle.
 for c in [lid,occ[10].component]:
  for f in list(c.features.extrudeFeatures):
   if f.name.startswith('V2 engrave'):f.isSuppressed=True
 # Sliding lip: increase wall-to-lip clearance from 0.30 to 0.50 on all four sides.
 for x,y,w,l in [(3.2,15.9,0.3,148.2),(136.5,15.9,0.3,148.2),(15.9,3.2,108.2,0.3),(15.9,176.5,108.2,0.3)]:
  box(lid,x,y,59.9,w,l,4.2,'V3 PETG lip side relief',CUT,lb)
 corners=[(8,8),(132,8),(8,172),(132,172)]
 drilled(lid,corners,63.9,3.3,3.6,'V3 PETG M3 cover clearance',lb,'v3_m3_clear')
 drilled(lid,corners,65.6,1.5,6.8,'V3 M3 cover head recess',lb)
 # Revise LCD pillars in the existing lid to M2. Fill old bores before drilling smaller ones.
 lm=[(23.5,100.5),(116.5,100.5),(23.5,155.5),(116.5,155.5)]
 posts(lid,lm,51.2,12.8,7,'V3 LCD 12.8 mm pillars - filled M3 bores')
 drilled(lid,lm,51.1,7.3,1.7,'V3 LCD M2 tapping pilots',lb)
 box(lid,30,114,63.9,80,28,3.3,'V3 LCD aperture 80x28',CUT,lb)
 # Rework relay bay for the 75.5 x 55 envelope. Existing main-board mounting stays in place.
 box(base,22,111,3,96,65,30,'V3 clear old relay mounts',CUT,b)
 for o in reversed(occ):
  if o.component.name.startswith('V2 RELAY clamp') or o.component.name.startswith('REF four relay') or o.component.name.startswith('REF LCD2004'):o.deleteMe()
 parts=[('base',base,bo),('lid',lid,lo)]
 for i,(x,y) in enumerate([(20,48),(120,35),(20,85),(120,85)]):
  c=occ[2+i].component
  ss=sketch(c,9.5,'V3 main clamp PETG slot');m.rounded(ss,x-3.5,y-1.8,7,3.6,1.7)
  extrude(c,ss,2.8,CUT,'V3 main clamp M3 running clearance',c.bRepBodies.item(0))
  parts.append(('main_clamp_'+str(i+1),c,occ[2+i]))
 refs=[occ[15],occ[18]]
 def component(name,filename=None):
  o=root.occurrences.addNewComponent(adsk.core.Matrix3D.create());o.component.name=name
  if filename:parts.append((filename,o.component,o))
  else:refs.append(o)
  return o,o.component
 def color(body,name):
  a=d.appearances.itemByName(name)
  if a:body.appearance=a
 def cyl(c,x,y,z,diam,h,name,op=NEW):
  s=sketch(c,z,name);m.circles(s,[(x,y)],diam);a=extrude(c,s,h,op,name);a.name=name if op==NEW else a.name;return a
 def solid(c,x,y,z,w,l,h,name,appearance):
  q=box(c,x,y,z,w,l,h,name);q.name=name;color(q,appearance);return q
 # Four end pads support the PCB corners, avoiding unknown solder joints in the middle.
 cl=[(27.25,122),(112.75,122),(27.25,160),(112.75,160)]
 for x in [30.25,103.75]:
  for y in [117,155]:box(base,x,y,3,6,10,5,'V3 relay edge support',JOIN)
 posts(base,cl,3,6.6,8,'V3 relay clamp pedestals')
 drilled(base,cl,3.1,6.6,2.5,'V3 relay clamp pilots',b)
 for i,(x,y) in enumerate(cl):
  o,c=component('V3 RELAY adjustable clamp '+str(i+1),'relay_clamp_'+str(i+1))
  start=x-5 if i%2==0 else x-9
  cb=box(c,start,y-5,9.6,14,10,3,'V3 relay clamp',r=1)
  ss=sketch(c,9.5,'V3 8 mm adjustment slot');m.rounded(ss,x-4,y-1.8,8,3.6,1.7);extrude(c,ss,3.3,CUT,'V3 adjustable M3 clamp slot',cb)
  color(cb,'V2 graphite')
 for x in [37,99]:
  box(base,x,170,3,7,5,8,'V3 cable tie anchor',JOIN)
  box(base,x+1.5,169.9,5,4,5.2,3,'V3 cable tie tunnel',CUT,b)
 note('Existing shell revised: relay envelope, PETG lid fit, M2 LCD mounting')
 # The combined controls PCB remains intact. Recessed guide opens by 0.2 mm in diameter.
 po,co=occ[10],occ[11];pc,cc=po.component,co.component;pb=pc.bRepBodies.item(0)
 ph=[(43,32),(97,32),(43,62),(97,62)];ch=[(45,44),(95,44)]
 buttons=[(62.38,53),(70,53),(77.62,53)]
 drilled(pc,buttons,66.9,2.8,6.3,'V3 PETG plunger running clearance',pb,'v3_button_guide')
 drilled(pc,ph,66.9,2.8,3.6,'V3 fascia M3 clearance',pb,'v3_m3_clear')
 drilled(cc,ch,46.4,2.8,3.6,'V3 carrier M3 clearance',cc.bRepBodies.item(0),'v3_m3_clear')
 parts.extend([('control_fascia',pc,po),('control_carrier',cc,co)])
 # 4.3 mm is a documented switch envelope, not an identification of the photographed switch.
 uc=occ[18].component
 for a in uc.bRepBodies:
  if a.name.startswith('Switch actuator'):
   bb=a.boundingBox;x=(bb.minPoint.x+bb.maxPoint.x)*5;y=(bb.minPoint.y+bb.maxPoint.y)*5
   cyl(uc,x,y,58.3,3.3,0.6,'V3 switch 4.3 mm actuator extension',JOIN)
 for i,o in enumerate(occ[12:15]):
  c=o.component;x,y=buttons[i]
  box(c,x-4,y-4,58.4,8,8,0.85,'V3 0.35 mm idle button clearance',CUT,c.bRepBodies.item(0))
  parts.append(('plunger_'+str(i+1),c,o))
 def label(c,body,t,x,y,width,height,z):
  s=sketch(c,z,'V3 label '+t);ti=s.sketchTexts.createInput3("'"+t+"'",val(height));ti.fontName='Arial';ti.isBold=True
  ti.setAsMultiLine(p(x-width/2,y-height),p(x+width/2,y+height),adsk.core.HorizontalAlignments.CenterHorizontalAlignment,adsk.core.VerticalAlignments.MiddleVerticalAlignment,0)
  st=s.sketchTexts.add(ti);inp=c.features.extrudeFeatures.createInput(st,CUT);inp.setDistanceExtent(False,val(-0.6));inp.participantBodies=[body];c.features.extrudeFeatures.add(inp).name='V3 engraving '+t;s.isLightBulbOn=False
 label(lid,lb,'H2 PRO',70,166,100,7,67)
 label(lid,lb,'POWER  /  LOCK  /  PURGE',70,82,120,4,67)
 label(lid,lb,'CONTROL UNIT',70,15,100,4,67)
 # Larger 1 / 2 / 3 markings use slots of 0.8 mm minimum width instead of fine font strokes.
 for i,(x,y) in enumerate(buttons):
  for k in range(i+1):box(pc,x-(i*1.6)/2+k*1.6-0.4,60,68.9,0.8,3.2,0.8,'V3 tactile number bars',CUT,pb)
 # Conservative envelopes keep room for multiple board variants documented by the vendors.
 ro,rc=component('REF relay envelope 75.5x55x20 - variant not confirmed')
 rb=solid(rc,32.25,112,8,75.5,55,1.6,'Relay PCB envelope','V2 blue pcb')
 # Reference blocks sit inside the edge clamps; terminal location remains a visual estimate.
 for i in range(4):solid(rc,36.75+i*17.3,131,9.6,15.5,19,15.5,'SRD relay envelope '+str(i+1),'V2 relay blue')
 solid(rc,37,155,9.6,66,10,12,'Terminal strip envelope','V2 blue pcb')
 solid(rc,41,114,9.6,36,5,18.4,'Control connector reserve','V2 black')
 lco,lc=component('REF LCD2004 98x60 - 93x55 mounting - conservative thickness')
 q=solid(lc,21,98,49.6,98,60,1.6,'LCD PCB 98x60','V2 pcb');drilled(lc,lm,49.5,1.8,2.5,'LCD minimum published hole diameter',q)
 solid(lc,21.5,108.8,51.2,97,38.4,12,'LCD bezel depth envelope','V2 black')
 solid(lc,31.5,115.4,63.2,77,25.2,0.1,'LCD viewing area envelope','V2 lcd blue')
 solid(lc,32,100,39.2,42,18,10.4,'LCD I2C backpack reserve','V2 pcb')
 # LCD contact washers: modeled separately as optional standard hardware, not printable.
 # No unverified hole pattern is imposed on the relay or home-built boards.
 for _,c,_ in parts:c.isSketchFolderLightBulbOn=False;c.isConstructionFolderLightBulbOn=False
 for o in refs:o.component.isSketchFolderLightBulbOn=False;o.component.isConstructionFolderLightBulbOn=False
 color(b,'V2 graphite');color(lb,'V2 light silver')
 d.computeAll();adsk.doEvents();d.computeAll()
 # Create physical fit and shrinkage coupons in another document, outside the assembly.
 report={'version':3,'printer':'Creality K1 Max first revision','filament':'NIT PETG','nozzle_mm':0.6,'case_mm':[140,180,67],'cad_shrink_scale':1.0,'trial_stl_xy_shrink_fraction':0.003,'trial_stl_z_shrink_fraction':0.0,'lcd_mount_pitch_mm':[93,55],'lcd_screws':'M2 with washers','relay_envelope_mm':[75.5,55,20],'main_board_mm':[90,80,20],'controls_pcb_mm':[30,30],'controls_dimensions_status':'photo estimate, not exact','parts':[],'feature_problems':[]}
 for i in range(d.timeline.count):
  e=d.timeline.item(i).entity
  if e and hasattr(e,'healthState') and e.healthState not in (adsk.fusion.FeatureHealthStates.HealthyFeatureHealthState,adsk.fusion.FeatureHealthStates.SuppressedFeatureHealthState):report['feature_problems'].append({'name':e.name,'message':e.errorOrWarningMessage})
 ip=d.createInterferenceInput(coll([o for _,_,o in parts]+refs));ip.areCoincidentFacesIncluded=False;ir=d.analyzeInterference(ip)
 report['interferences']=[{'a':r.entityOne.name,'b':r.entityTwo.name,'volume_cm3':r.interferenceBody.volume} for r in ir] if ir else []
 for name,c,o in parts:
  assert c.bRepBodies.count==1 and c.bRepBodies.item(0).isSolid,name
  report['parts'].append({'name':name,'volume_cm3':c.bRepBodies.item(0).volume})
 json.dump(report,open(os.path.join(OUT,'verification_v3.json'),'w',encoding='utf-8'),indent=2)
 assert not report['interferences'],str(report['interferences'])
 # Health of the last text feature can require another event cycle in Fusion; export only clean models.
 assert not report['feature_problems'],str(report['feature_problems'])
 nominal=os.path.join(OUT,'STL_nominal');os.makedirs(nominal,exist_ok=True)
 for name,c,o in parts:
  opt=ex.createSTLExportOptions(o,os.path.join(nominal,name+'.stl'));opt.meshRefinement=adsk.fusion.MeshRefinementSettings.MeshRefinementHigh;assert ex.execute(opt)
 vp=app.activeViewport
 def view(eye,target,up):
  cam=vp.camera;cam.eye=p(*eye);cam.target=p(*target);cam.upVector=adsk.core.Vector3D.create(*up);cam.isFitView=True;cam.isSmoothTransition=False;vp.camera=cam;vp.fit();adsk.doEvents()
 view((280,-280,380),(70,90,30),(0,0,1));vp.saveAsImageFile(os.path.join(OUT,'assembled_v3.png'),1800,1500)
 view((70,90,500),(70,90,0),(0,1,0));vp.saveAsImageFile(os.path.join(OUT,'top_v3.png'),1400,1800)
 upper=[lo,po,co,lco,occ[18]]+occ[12:15]
 for o in upper:o.isLightBulbOn=False
 view((160,-180,480),(70,90,15),(0,0,1));vp.saveAsImageFile(os.path.join(OUT,'inside_v3.png'),1800,1600)
 for o in upper:o.isLightBulbOn=True
 view((280,-280,380),(70,90,30),(0,0,1))
 assert ex.execute(ex.createFusionArchiveExportOptions(os.path.join(OUT,'H2_Brain_V3_PETG.f3d')))
 assert ex.execute(ex.createSTEPExportOptions(os.path.join(OUT,'H2_Brain_V3_PETG.step')))
 note('ASSEMBLY VERIFIED AND EXPORTED: '+json.dumps(report))
 cd=app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType);cd.name='H2 PETG calibration and LCD fit'
 dd=adsk.fusion.Design.cast(app.activeProduct);cr=dd.rootComponent;ce=dd.exportManager
 co=cr.occurrences.addNewComponent(adsk.core.Matrix3D.create());c=co.component;c.name='XY 100mm and hole calibration'
 cb=box(c,0,0,0,100,12,3.2,'100 mm X arm');box(c,0,0,0,12,100,3.2,'100 mm Y arm',JOIN)
 for x,diam in [(22,3.2),(42,3.4),(62,3.6),(82,6.3)]:drilled(c,[(x,6)],-0.1,3.4,diam,'Hole '+str(diam),cb)
 # A second 40 mm span distinguishes a constant contour offset from a proportional error.
 box(c,0,42,0,40,8,3.2,'40 mm X reference',JOIN)
 box(c,42,0,0,8,40,3.2,'40 mm Y reference',JOIN)
 co2=cr.occurrences.addNewComponent(adsk.core.Matrix3D.create());c2=co2.component;c2.name='LCD 93x55 mounting fit coupon'
 q=box(c2,115,0,0,107,69,1.2,'LCD fit plate',r=3)
 box(c2,129,21, -0.1,79,27,1.4,'LCD test viewing window',CUT,q)
 drilled(c2,[(122,7),(215,7),(122,62),(215,62)],-0.1,1.4,2.5,'LCD pitch check 93x55',q)
 for c,name in [(c,'test_XY_100_40_holes'),(c2,'test_LCD_93x55')]:
  c.isSketchFolderLightBulbOn=False;c.isConstructionFolderLightBulbOn=False
  o=ce.createSTLExportOptions(c,os.path.join(OUT,name+'.stl'));o.meshRefinement=adsk.fusion.MeshRefinementSettings.MeshRefinementHigh;assert ce.execute(o)
 assert ce.execute(ce.createFusionArchiveExportOptions(os.path.join(OUT,'Fit_tests.f3d')))
 doc.activate();note('SUCCESS: V3 assembly and two calibration coupons exported')
