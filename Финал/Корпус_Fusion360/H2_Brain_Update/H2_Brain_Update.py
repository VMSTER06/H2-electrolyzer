import adsk.core, adsk.fusion, os, json, traceback, importlib.util
OUT=r'S:\1Electrolizer\Финал\Корпус_Fusion360'
spec=importlib.util.spec_from_file_location('h2_helpers',os.path.join(OUT,'H2_Brain_Enclosure','H2_Brain_Enclosure.py'))
m=importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
p,val,coll,box,sketch,rect,circles,extrude,drilled,posts=m.p,m.val,m.coll,m.box,m.sketch,m.rect,m.circles,m.extrude,m.drilled,m.posts
NEW,JOIN,CUT=m.NEW,m.JOIN,m.CUT

def run(context):
 app=adsk.core.Application.get(); log=[]
 def note(s):
  log.append(str(s)); open(os.path.join(OUT,'update_log.txt'),'w',encoding='utf-8').write('\n'.join(log))
 try:
  d=adsk.fusion.Design.cast(app.activeProduct); root=d.rootComponent
  old=list(root.occurrences)
  assert len(old)==7 and old[0].component.name.startswith('01 BASE'),'Open the original H2 enclosure before running update'
  ex=d.exportManager
  assert ex.execute(ex.createFusionArchiveExportOptions(os.path.join(OUT,'Архив_v1','before_v2_edit.f3d')))
  base,lid=old[0].component,old[1].component
  # Keep and modify the two existing enclosure components and their history.
  for o in reversed(old[2:]):o.deleteMe()
  bo,lo=old[0],old[1]
  parts=[('base',base,bo),('lid',lid,lo)]; refs=[]
  def comp(name,filename=None):
   o=root.occurrences.addNewComponent(adsk.core.Matrix3D.create()); c=o.component;c.name=name
   if filename:parts.append((filename,c,o))
   else:refs.append(o)
   return o,c
  source=None
  for lib in app.materialLibraries:
   source=next((a for a in lib.appearances if a.name=='Plastic - Matte (Black)'),None)
   if source:break
  def paint(b,name,rgb):
   a=d.appearances.itemByName(name)
   if not a:
    a=d.appearances.addByCopy(source,name)
    cp=next((q for q in a.appearanceProperties if adsk.core.ColorProperty.cast(q) and q.id in ('opaque_albedo','generic_diffuse')),None)
    if cp:cp.value=adsk.core.Color.create(*rgb,255)
   b.appearance=a
  def cyl(c,x,y,z,diam,h,name,op=NEW):
   s=sketch(c,z,name);circles(s,[(x,y)],diam);b=extrude(c,s,h,op,name)
   if op==NEW:b.name=name
   return b
  def text(c,b,t,x,y,w,h=3,z=67):
   s=sketch(c,z,'V2 text '+t); ti=s.sketchTexts.createInput3("'"+t+"'",val(h));ti.fontName='Arial'
   ti.setAsMultiLine(p(x-w/2,y-3),p(x+w/2,y+3),adsk.core.HorizontalAlignments.CenterHorizontalAlignment,adsk.core.VerticalAlignments.MiddleVerticalAlignment,0)
   st=s.sketchTexts.add(ti);ei=c.features.extrudeFeatures.createInput(st,CUT);ei.setDistanceExtent(False,val(-0.35));ei.participantBodies=[b]
   c.features.extrudeFeatures.add(ei).name='V2 engrave '+t;s.isLightBulbOn=False
  def slot_side(c,y,x,z,w,h,name,b):
   pi=c.constructionPlanes.createInput();pi.setByOffset(c.xZConstructionPlane,val(-y));pl=c.constructionPlanes.add(pi);pl.isLightBulbOn=False
   s=c.sketches.add(pl);s.name=name;m.rounded(s,x,z,w,h,2);extrude(c,s,-4,CUT,name,b)
  b=box(base,0,0,0,140,180,64,'V2 extend existing base for relay bay',JOIN,r=6)
  b=box(base,3,3,3,134,174,62,'V2 clear obsolete interior mounts',CUT,b,r=3);b.name='BASE_V2_PRINT'
  corners=[(8,8),(132,8),(8,172),(132,172)]
  s=sketch(base,3,'V2 lid pillars')
  for x,y in corners:rect(s,x-5,y-5,10,10)
  extrude(base,s,61,JOIN,'V2 four cover pillars')
  drilled(base,corners,52,12.2,2.5,'V2 lid pilots',b)
  # Two boards sit side by side along Y. No unknown PCB mounting-hole pattern is assumed.
  board_specs=[('MAIN',25,20,90,80,[(20,35),(120,35),(20,85),(120,85)]),('RELAY',35,117,70,50,[(30,127),(110,127),(30,157),(110,157)])]
  for name,x0,y0,w,h,cl in board_specs:
   s=sketch(base,3,'V2 '+name+' supports')
   for x in [x0-2,x0+w-4]:
    for y in [cl[0][1]-5,cl[2][1]-5]:rect(s,x,y,6,10)
   extrude(base,s,5,JOIN,'V2 '+name+' underside edge pads')
   posts(base,cl,3,6.6,8,'V2 '+name+' clamp pedestals')
   drilled(base,cl,3.1,6.6,2.5,'V2 '+name+' clamp pilots',b)
   for i,(x,y) in enumerate(cl):
    o,c=comp('V2 '+name+' clamp '+str(i+1),name.lower()+'_clamp_'+str(i+1))
    left=x<x0;start=x-4 if left else x-8.5
    cb=box(c,start,y-5,9.6,12.5,10,2.5,'Edge clamp',r=1)
    # Long slot allows about +/- 1.5 mm correction of the clamp position.
    ss=sketch(c,9.5,'Clamp adjustment slot');m.rounded(ss,x-3.2,y-1.7,6.4,3.4,1.6);extrude(c,ss,2.8,CUT,'Clamp adjustment slot',cb)
    paint(cb,'V2 graphite',(48,57,68))
  # Rear cable exits face the relay terminal strip. Tie eyes provide internal strain relief.
  for x in [38,78]:slot_side(base,176.5,x,14,24,10,'V2 rear wiring outlet',b)
  for x in [37,99]:
   eye=box(base,x,169,3,7,5,8,'V2 cable tie anchor',JOIN)
   box(base,x+1.5,168.9,5,4,5.2,3,'V2 cable tie tunnel',CUT,b)
  paint(b,'V2 graphite',(48,57,68));note('Existing base updated; main PCB and relay mounts complete')
  # Rework the same lid: fill old holes, lengthen it and raise it by 12 mm.
  lb=box(lid,0,0,52,140,180,15,'V2 refill old lid and extend',JOIN,r=6)
  lb=box(lid,-1,-1,39,142,182,25,'V2 remove obsolete underside spacers',CUT,lb);lb.name='LID_V2_PRINT'
  s=sketch(lid,60,'V2 lip segments')
  for q in [(3.3,16,2,148),(134.7,16,2,148),(16,3.3,108,2),(16,174.7,108,2)]:rect(s,*q)
  extrude(lid,s,4,JOIN,'V2 alignment lips')
  drilled(lid,corners,63.9,3.2,3.4,'V2 cover clearance',lb)
  drilled(lid,corners,65.8,1.3,6.4,'V2 cover head recess',lb)
  box(lid,30.5,114.5,63.9,79,27,3.3,'V2 LCD aperture',CUT,lb)
  lcdmount=[(23.5,100.5),(116.5,100.5),(23.5,155.5),(116.5,155.5)]
  posts(lid,lcdmount,54,10,7,'V2 LCD pillars')
  drilled(lid,lcdmount,53.9,8.1,2.5,'V2 LCD pilot holes',lb)
  # Replaceable control fascia. Real switches and LEDs remain on their common PCB.
  box(lid,48,27,63.9,44,40,3.3,'V2 control module opening',CUT,lb,r=3)
  panelholes=[(43,32),(97,32),(43,62),(97,62)]
  posts(lid,panelholes,58,6,7,'V2 control fascia screw bosses')
  drilled(lid,panelholes,58.1,9,2.5,'V2 fascia screw pilots',lb)
  text(lid,lb,'H2 PRO',70,167,80,5)
  text(lid,lb,'BRAIN / CONTROL UNIT',70,92,100,2.8)
  text(lid,lb,'POWER   LOCK   PURGE',70,75,110,3)
  text(lid,lb,'STATUS INDICATORS',70,16,80,2.5)
  paint(lb,'V2 light silver',(190,204,215));note('Existing lid updated for LCD and combined controls PCB')
  po,pc=comp('V2 removable control fascia','control_fascia')
  pb=box(pc,38,22,67,64,50,2.5,'Control fascia plate',r=4)
  drilled(pc,panelholes,66.9,2.8,3.4,'Fascia M3 clearance',pb)
  buttons=[(62.38,53),(70,53),(77.62,53)];leds=[(63.65,39),(70,39),(76.35,39)]
  drilled(pc,buttons,66.9,2.8,6.1,'Tactile actuator guide holes',pb)
  # LED window covers estimated pitch and positional uncertainty, with a retained diffuser strip.
  box(pc,59,35,66.9,22,8,2.8,'LED common viewing window',CUT,pb,r=2)
  for i,(x,y) in enumerate(buttons):text(pc,pb,str(i+1),x,62,7,2.5,z=69.5)
  paint(pb,'V2 graphite',(48,57,68))
  # Board carrier is screwed to fascia from below; board can shift within its shallow pocket.
  carrierholes=[(45,44),(95,44)]
  drilled(lid,carrierholes,63.9,3.3,8.2,'V2 carrier pillar clearance in lid',lb)
  posts(pc,carrierholes,49,18,7,'Control carrier standoffs')
  drilled(pc,carrierholes,48.9,8.1,2.5,'Carrier M3 pilots',pb)
  co,cc=comp('V2 control PCB removable carrier','control_carrier')
  cb=box(cc,40,28,46.5,60,35,2.5,'Control PCB carrier',r=2)
  drilled(cc,carrierholes,46.4,2.8,3.4,'Carrier M3 through',cb)
  # PCB underside at z53: four small pads avoid soldered joints in the centre.
  ss=sketch(cc,49,'Control PCB corner support pads')
  for x in [54,83]:
   for y in [30,57]:rect(ss,x,y,3,3)
  extrude(cc,ss,4,JOIN,'PCB stand-off pads')
  # End stops outside the estimated 30x30 board, open on the cable side.
  for q in [(51.5,30,2,30),(86.5,30,2,30),(54,27.5,32,2)]:box(cc,q[0],q[1],49,q[2],q[3],5,'Control PCB guide',JOIN)
  # Pair of tie slots accepts a thin reusable strap around a free PCB margin.
  for x in [52,85]:box(cc,x,48,46.4,2,6,2.8,'PCB retaining strap slot',CUT,cb)
  paint(cb,'V2 graphite',(48,57,68))
  for i,(x,y) in enumerate(buttons):
   o,c=comp('V2 captured tactile plunger '+str(i+1),'plunger_'+str(i+1))
   pl=cyl(c,x,y,58.6,4,8.2,'Tactile contact stem')
   cyl(c,x,y,65.2,7,1.6,'Captive flange',JOIN)
   cyl(c,x,y,66.8,5.5,3.6,'Finger tip',JOIN)
   paint(pl,'V2 button cyan',(0,165,185))
  # Electronics reference geometry; colours and component grouping reflect the supplied photos.
  mo,mc=comp('REF main board 90x80x20 - photo estimate')
  main=box(mc,25,20,8,90,80,1.6,'Main PCB');main.name='Main PCB'
  paint(main,'V2 pcb',(24,112,73))
  def refbox(c,x,y,z,w,h,t,name,col,rgb):
   a=box(c,x,y,z,w,h,t,name);a.name=name;paint(a,col,rgb);return a
  refbox(mc,68,38,9.6,27,50,12,'ESP32 reference','V2 black',(22,26,32))
  refbox(mc,33,75,9.6,35,18,10,'nRF24 reference','V2 black',(22,26,32))
  refbox(mc,36,34,9.6,21,16,6,'MPU6050 reference','V2 pcb',(24,112,73))
  refbox(mc,62,22,9.6,10,10,18.4,'Tallest component 20mm total','V2 pcb',(24,112,73))
  refbox(mc,18,23,11,7,16,12,'Rocker switch side clearance','V2 black',(22,26,32))
  refbox(mc,16,80,13,9,6,6,'SMA connector space','V2 brass',(188,146,53))
  rlo,rc=comp('REF four relay board 70x50x20')
  relay=refbox(rc,35,117,8,70,50,1.6,'Relay PCB','V2 blue pcb',(23,114,159))
  for i in range(4):refbox(rc,38+i*16,133,9.6,15,19,15.5,'Relay '+str(i+1),'V2 relay blue',(29,139,193))
  refbox(rc,38,155,9.6,62,10,12,'Screw terminal strip','V2 blue pcb',(23,114,159))
  refbox(rc,42,119,9.6,35,5,18.4,'Relay control connector envelope','V2 black',(22,26,32))
  lco,lc=comp('REF LCD2004 98x60 - estimated from photo')
  lpcb=refbox(lc,21,98,52.4,98,60,1.6,'LCD PCB','V2 pcb',(24,112,73))
  drilled(lc,lcdmount,52.3,1.8,3.2,'LCD reference mounting holes',lpcb)
  refbox(lc,21.5,108.5,54,97,39,9.5,'LCD bezel','V2 black',(22,26,32))
  refbox(lc,32,115.4,63.5,76,25.2,0.3,'LCD glass','V2 lcd blue',(22,53,152))
  refbox(lc,32,100,42,42,18,10.4,'LCD backpack reserve','V2 pcb',(24,112,73))
  uo,uc=comp('REF actual combined buttons LEDs PCB - estimated')
  refbox(uc,55,30,53,30,30,1.6,'Control PCB estimated 30x30','V2 pcb',(24,112,73))
  for x,y in buttons:
   refbox(uc,x-3,y-3,54.6,6,6,3.3,'Tactile switch 6x6','V2 silver',(150,155,160))
   cyl(uc,x,y,57.9,3.3,0.5,'Switch actuator')
  for (x,y),name,rgb in zip(leds,['BLUE','RED','GREEN'],[(20,92,210),(220,35,45),(35,200,83)]):
   a=cyl(uc,x,y,54.6,3,4.5,'LED '+name);paint(a,'V2 LED '+name,rgb)
  # Light tunnel leaves a common opening above all LEDs without requiring an exact pitch.
  note('Controls carrier, captured plungers and all electronics references complete')
  for _,c,_ in parts:
   c.isSketchFolderLightBulbOn=False;c.isConstructionFolderLightBulbOn=False
  for o in refs:o.component.isSketchFolderLightBulbOn=False;o.component.isConstructionFolderLightBulbOn=False
  d.computeAll();adsk.doEvents();d.computeAll()
  issues=[]
  for i in range(d.timeline.count):
   ent=d.timeline.item(i).entity
   if ent and hasattr(ent,'healthState') and ent.healthState!=adsk.fusion.FeatureHealthStates.HealthyFeatureHealthState:issues.append({'name':ent.name,'message':ent.errorOrWarningMessage})
  ip=d.createInterferenceInput(coll([o for _,_,o in parts]+refs));ip.areCoincidentFacesIncluded=False
  ir=d.analyzeInterference(ip)
  report={'version':2,'case_mm':[140,180,67],'overall_with_fascia_mm':[140,180,70.4],'main_board_mm':[90,80,20],'relay_board_mm':[70,50,20],'lcd_estimated_mm':[98,60],'controls_estimated_mm':[30,30],'button_pitch_estimated_mm':7.62,'led_pitch_estimated_mm':6.35,'feature_problems':issues,'interference_count':ir.count if ir else 0,'parts':[]}
  if ir:
   report['interferences']=[{'a':r.entityOne.name,'b':r.entityTwo.name,'volume_cm3':r.interferenceBody.volume} for r in ir]
  for name,c,o in parts:
   assert c.bRepBodies.count==1,name+' not a single solid'
   bb=c.bRepBodies.item(0);assert bb.isSolid
   report['parts'].append({'name':name,'volume_cm3':bb.volume})
   opt=ex.createSTLExportOptions(c,os.path.join(OUT,name+'.stl'));opt.meshRefinement=adsk.fusion.MeshRefinementSettings.MeshRefinementHigh;assert ex.execute(opt)
  json.dump(report,open(os.path.join(OUT,'verification_v2.json'),'w',encoding='utf-8'),indent=2)
  note(json.dumps(report));vp=app.activeViewport
  def view(eye,target,up):
   cam=vp.camera;cam.eye=p(*eye);cam.target=p(*target);cam.upVector=adsk.core.Vector3D.create(*up);cam.isFitView=True;cam.isSmoothTransition=False;vp.camera=cam;vp.fit();adsk.doEvents()
  view((280,-280,380),(70,90,30),(0,0,1));vp.saveAsImageFile(os.path.join(OUT,'assembled_v2.png'),1800,1500)
  view((70,90,500),(70,90,0),(0,1,0));vp.saveAsImageFile(os.path.join(OUT,'top_v2.png'),1400,1800)
  lo.isLightBulbOn=False;po.isLightBulbOn=False;co.isLightBulbOn=False;lco.isLightBulbOn=False;uo.isLightBulbOn=False
  for name,c,o in parts:
   if name.startswith('plunger'):o.isLightBulbOn=False
  view((190,-210,460),(70,90,15),(0,0,1));vp.saveAsImageFile(os.path.join(OUT,'internals_v2.png'),1800,1500)
  for _,_,o in parts:o.isLightBulbOn=True
  for o in refs:o.isLightBulbOn=True
  topocc=[lo,po,co,lco,uo]+[o for name,c,o in parts if name.startswith('plunger')]
  origins=[o.transform2 for o in topocc]
  for o in topocc:
   t=adsk.core.Matrix3D.create();t.translation=adsk.core.Vector3D.create(0,0,9);o.transform2=t
  view((300,-300,400),(70,90,65),(0,0,1));vp.saveAsImageFile(os.path.join(OUT,'exploded_v2.png'),1800,1700)
  for o,t in zip(topocc,origins):o.transform2=t
  view((280,-280,380),(70,90,30),(0,0,1))
  assert ex.execute(ex.createFusionArchiveExportOptions(os.path.join(OUT,'H2_Brain_LCD2004_v2.f3d')))
  assert ex.execute(ex.createSTEPExportOptions(os.path.join(OUT,'H2_Brain_LCD2004_v2.step')))
  rs=importlib.util.spec_from_file_location('repair_v2',os.path.join(OUT,'H2_Brain_Update','repair_v2.py'))
  rm=importlib.util.module_from_spec(rs);rs.loader.exec_module(rm);rm.run(context)
  note('V2 update and final clearance correction completed; see repair_log.txt')
 except:
  note(traceback.format_exc());app.userInterface.messageBox(traceback.format_exc())

