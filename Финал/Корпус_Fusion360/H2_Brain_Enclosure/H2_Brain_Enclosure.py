"""Run inside Autodesk Fusion. All dimensions below are millimetres.
Creates a fresh document and exports locally; never modifies an existing design.
"""
import adsk.core, adsk.fusion, math, os, json, traceback

OUT = r'S:\1Electrolizer\Финал\Корпус_Fusion360'
W, L, H, WALL, FLOOR, LID = 140., 160., 52., 3., 3., 3.
BUTTON_D, LED_D = 12.2, 5.2
LCD_WINDOW_W, LCD_WINDOW_H = 79., 27.
NEW = adsk.fusion.FeatureOperations.NewBodyFeatureOperation
JOIN = adsk.fusion.FeatureOperations.JoinFeatureOperation
CUT = adsk.fusion.FeatureOperations.CutFeatureOperation

def p(x,y,z=0): return adsk.core.Point3D.create(x/10,y/10,z/10)
def val(v): return adsk.core.ValueInput.createByString(str(v)+' mm')
def coll(items):
    c=adsk.core.ObjectCollection.create()
    for i in items: c.add(i)
    return c

def sketch(c,z,name):
    if z==0: plane=c.xYConstructionPlane
    else:
        pi=c.constructionPlanes.createInput(); pi.setByOffset(c.xYConstructionPlane,val(z))
        plane=c.constructionPlanes.add(pi); plane.isLightBulbOn=False
    s=c.sketches.add(plane); s.name=name
    return s

def rect(s,x,y,w,h):
    s.sketchCurves.sketchLines.addTwoPointRectangle(p(x,y),p(x+w,y+h))

def rounded(s,x,y,w,h,r):
    lines=s.sketchCurves.sketchLines; arcs=s.sketchCurves.sketchArcs
    for a,b in [((x+r,y),(x+w-r,y)),((x+w,y+r),(x+w,y+h-r)),((x+w-r,y+h),(x+r,y+h)),((x,y+h-r),(x,y+r))]:
        lines.addByTwoPoints(p(*a),p(*b))
    for center,start in [((x+w-r,y+r),(x+w-r,y)),((x+w-r,y+h-r),(x+w,y+h-r)),((x+r,y+h-r),(x+r,y+h)),((x+r,y+r),(x,y+r))]:
        arcs.addByCenterStartSweep(p(*center),p(*start),math.pi/2)

def circles(s,centers,d,expr=None):
    for x,y in centers:
        c=s.sketchCurves.sketchCircles.addByCenterRadius(p(x,y),d/20)
        if expr:
            dim=s.sketchDimensions.addDiameterDimension(c,p(x+d,y+d))
            dim.parameter.expression=expr

def extrude(c,s,depth,op,name,body=None):
    profiles=coll([s.profiles.item(i) for i in range(s.profiles.count)])
    if profiles.count==0: raise RuntimeError('No profile: '+name)
    inp=c.features.extrudeFeatures.createInput(profiles,op)
    inp.setDistanceExtent(False,val(depth))
    if body and op==CUT: inp.participantBodies=[body]
    f=c.features.extrudeFeatures.add(inp); f.name=name; s.isLightBulbOn=False
    return f.bodies.item(0)

def box(c,x,y,z,w,l,h,name,op=NEW,body=None,r=0):
    s=sketch(c,z,name+' sketch')
    if r: rounded(s,x,y,w,l,r)
    else: rect(s,x,y,w,l)
    return extrude(c,s,h,op,name,body)

def drilled(c,centers,z,depth,d,name,body,expr=None):
    s=sketch(c,z,name+' sketch'); circles(s,centers,d,expr)
    return extrude(c,s,depth,CUT,name,body)

def posts(c,centers,z,h,d,name):
    s=sketch(c,z,name+' sketch'); circles(s,centers,d)
    return extrude(c,s,h,JOIN,name)

def paint(design,body,name,rgb):
    try:
        a=design.appearances.itemByName(name)
        if not a:
            source=body.appearance
            a=design.appearances.addByCopy(source,name)
            cp=a.appearanceProperties.itemById('generic_diffuse')
            if cp: cp.value=adsk.core.Color.create(*rgb,255)
        body.appearance=a
    except: pass

def label(c,body,text,x,y,width,height=3):
    s=sketch(c,55,'Engraving '+text)
    ti=s.sketchTexts.createInput3("'"+text+"'",val(height))
    ti.fontName='Arial'
    ti.setAsMultiLine(p(x-width/2,y-2.5),p(x+width/2,y+2.5),adsk.core.HorizontalAlignments.CenterHorizontalAlignment,adsk.core.VerticalAlignments.MiddleVerticalAlignment,0)
    t=s.sketchTexts.add(ti)
    inp=c.features.extrudeFeatures.createInput(t,CUT)
    inp.setDistanceExtent(False,val(-0.4)); inp.participantBodies=[body]
    f=c.features.extrudeFeatures.add(inp); f.name='Engrave '+text; s.isLightBulbOn=False

def run(context):
    app=adsk.core.Application.get(); log=[]
    os.makedirs(OUT,exist_ok=True)
    def note(s):
        log.append(s)
        with open(os.path.join(OUT,'build_log.txt'),'w',encoding='utf-8') as f: f.write('\n'.join(log))
    try:
        doc=app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType)
        doc.name='H2 PRO - Brain enclosure LCD2004'
        design=adsk.fusion.Design.cast(app.activeProduct)
        design.designType=adsk.fusion.DesignTypes.ParametricDesignType
        design.unitsManager.distanceDisplayUnits=adsk.fusion.DistanceUnits.MillimeterDistanceUnits
        root=design.rootComponent
        for name,v,comment in [('button_d',BUTTON_D,'ASSUMED: measure actual panel button'),('led_d',LED_D,'ASSUMED: 5 mm LED clearance'),('screw_clearance',3.4,'M3 clearance'),('pilot_d',2.5,'Plastic tapping pilot; verify printed fit')]:
            design.userParameters.add(name,val(v),'mm',comment)
        def component(name):
            o=root.occurrences.addNewComponent(adsk.core.Matrix3D.create()); o.component.name=name
            return o,o.component
        bo,base=component('01 BASE - PCB at bottom')
        body=box(base,0,0,0,W,L,H,'Outer shell',r=6)
        body=box(base,3,3,3,134,154,50,'Hollow cavity',CUT,body,r=3)
        body.name='BASE_PRINT'
        corners=[(8,8),(132,8),(8,152),(132,152)]
        s=sketch(base,3,'Four lid screw pillars')
        for x,y in corners: rect(s,x-5,y-5,10,10)
        extrude(base,s,49,JOIN,'Lid pillars')
        drilled(base,corners,40,12.1,2.5,'M3 lid pilot holes',body,'pilot_d')
        # Supports touch PCB only along its lower edge. The 3 mm edge band must be component-free.
        s=sketch(base,3,'PCB support pads')
        for x in [23,111]:
            for y in [40,110]: rect(s,x,y,6,10)
        extrude(base,s,5,JOIN,'PCB supports z8')
        s=sketch(base,3,'PCB lateral guides')
        for x in [22.2,115.4]:
            for y in [40,110]: rect(s,x,y,2.4,10)
        for y in [27.2,130.4]: rect(s,55,y,30,2.4)
        extrude(base,s,6.6,JOIN,'Board edge guides clearance 0.4')
        clamps=[(20,45),(120,45),(20,115),(120,115)]
        posts(base,clamps,3,6.6,8,'Clamp screw pedestals')
        drilled(base,clamps,3.1,6.6,2.5,'M3 clamp pilots',body,'pilot_d')
        paint(design,body,'H2 charcoal',(47,55,65)); note('Base complete')

        lo,lid=component('02 LID - LCD buttons LEDs')
        lb=box(lid,0,0,H,W,L,LID,'Lid plate',r=6); lb.name='LID_PRINT'
        s=sketch(lid,48,'Locating lip segments - 0.3 clearance')
        for q in [(3.3,16,2,128),(134.7,16,2,128),(16,3.3,108,2),(16,154.7,108,2)]: rect(s,*q)
        extrude(lid,s,4,JOIN,'Locating lips')
        drilled(lid,corners,51.9,3.2,3.4,'M3 lid through holes',lb,'screw_clearance')
        drilled(lid,corners,53.8,1.3,6.4,'Screw head pockets',lb)
        box(lid,70-LCD_WINDOW_W/2,113-LCD_WINDOW_H/2,51.9,LCD_WINDOW_W,LCD_WINDOW_H,3.2,'LCD viewing aperture',CUT,lb)
        lcdmount=[(23.5,85.5),(116.5,85.5),(23.5,140.5),(116.5,140.5)]
        posts(lid,lcdmount,42,10,7,'LCD underside spacers')
        drilled(lid,lcdmount,41.9,8.1,2.5,'LCD M3 pilot holes',lb,'pilot_d')
        drilled(lid,[(36,65),(70,65),(104,65)],51.9,3.2,BUTTON_D,'Three panel buttons',lb,'button_d')
        drilled(lid,[(36,26),(70,26),(104,26)],51.9,3.2,LED_D,'Three indicator LEDs',lb,'led_d')
        for t,x,y,w,h in [('H2 PRO',70,147,65,4),('POWER',36,51,30,3),('LOCK',70,51,30,3),('PURGE',104,51,30,3),('POWER',36,16,30,2.5),('LOCK',70,16,30,2.5),('MODE',104,16,30,2.5)]:
            label(lid,lb,t,x,y,w,h)
        paint(design,lb,'H2 light gray',(185,195,202)); note('Lid complete')

        printparts=[('base',base),('lid',lid)]; clampocc=[]
        for i,(x,y) in enumerate(clamps):
            co,cc=component('03 PCB clamp '+str(i+1))
            x0=16 if x==20 else 111.5
            cb=box(cc,x0,y-5,9.6,12.5,10,2.5,'Removable edge clamp',r=1)
            cb.name='CLAMP_PRINT_'+str(i+1)
            drilled(cc,[(x,y)],9.5,2.7,3.4,'Clamp screw clearance',cb,'screw_clearance')
            paint(design,cb,'H2 charcoal',(47,55,65))
            clampocc.append(co); printparts.append(('clamp_'+str(i+1),cc))
        # Reference components are simplified dimensional placeholders, not printable electronics.
        ro,ref=component('90 REFERENCES - DO NOT PRINT')
        pcb=box(ref,25,30,8,90,100,1.6,'Main PCB 90x100 reference')
        pcb.name='Main PCB - assumed substrate 1.6'; paint(design,pcb,'PCB green',(21,107,75))
        env=box(ref,28.5,33.5,9.6,83,93,13.4,'Main board component keepout to z23')
        env.name='Component envelope - user total height 15 mm'; paint(design,env,'Envelope',(85,140,115)); env.opacity=0.22
        lcdpcb=box(ref,21,83,40.4,98,60,1.6,'LCD2004 PCB assumed 98x60')
        drilled(ref,lcdmount,40.3,1.8,3.2,'LCD PCB mounting reference',lcdpcb)
        paint(design,lcdpcb,'PCB green',(21,107,75))
        frame=box(ref,21.5,93.5,42,97,39,9.5,'LCD metal frame reference')
        paint(design,frame,'LCD black',(20,24,29))
        screen=box(ref,32,100.4,51.5,76,25.2,0.3,'LCD visible glass reference')
        paint(design,screen,'LCD blue',(15,62,170))
        bp=box(ref,32,85,30,42,18,10.4,'I2C backpack reserved volume')
        paint(design,bp,'PCB green',(21,107,75))
        for x in [36,70,104]:
            s=sketch(ref,37,'Button body reference'); circles(s,[(x,65)],12)
            b=extrude(ref,s,18,NEW,'Panel button barrel reference'); paint(design,b,'LCD black',(20,24,29))
            s=sketch(ref,55,'Button cap reference'); circles(s,[(x,65)],16)
            b=extrude(ref,s,2,NEW,'Button cap reference'); paint(design,b,'H2 charcoal',(47,55,65))
        for x,rgb in [(36,(38,210,90)),(70,(240,150,20)),(104,(35,120,240))]:
            s=sketch(ref,51,'LED reference'); circles(s,[(x,26)],5)
            b=extrude(ref,s,5,NEW,'LED 5mm reference'); paint(design,b,'LED '+str(x),rgb)
        note('Reference electronics complete')
        for c in [base,lid,ref]+[o.component for o in clampocc]:
            c.isSketchFolderLightBulbOn=False
            c.isConstructionFolderLightBulbOn=False
        design.computeAll()
        # Geometric checks against printable solids and the main board substrate.
        check={'case_mm':[W,L,H+LID],'pcb_mm':[90,100,15],'pcb_bottom_z_mm':8,'pcb_top_z_mm':23,'backpack_bottom_z_mm':30,'nominal_vertical_gap_mm':7,'parts':[],'feature_problems':[]}
        for name,c in printparts:
            assert c.bRepBodies.count==1, name+' must be one connected solid'
            b=c.bRepBodies.item(0)
            assert b.isSolid and b.volume>0,name+' invalid solid'
            check['parts'].append({'name':name,'solid':b.isSolid,'volume_cm3':b.volume})
        for i in range(design.timeline.count):
            ent=design.timeline.item(i).entity
            if ent and hasattr(ent,'healthState') and ent.healthState!=adsk.fusion.FeatureHealthStates.HealthyFeatureHealthState:
                check['feature_problems'].append({'name':ent.name,'message':ent.errorOrWarningMessage})
        inp=design.createInterferenceInput(coll([bo,lo]+clampocc))
        inp.areCoincidentFacesIncluded=False
        inter=design.analyzeInterference(inp)
        check['printable_part_interferences']=inter.count if inter else 0
        with open(os.path.join(OUT,'verification.json'),'w',encoding='utf-8') as f: json.dump(check,f,indent=2)
        note('Verification: '+json.dumps(check))
        vp=app.activeViewport
        def view(eye,target,up):
            cam=vp.camera; cam.eye=p(*eye); cam.target=p(*target); cam.upVector=adsk.core.Vector3D.create(*up); cam.isFitView=True; cam.isSmoothTransition=False; vp.camera=cam; vp.fit(); adsk.doEvents()
        view((270,-260,350),(70,80,25),(0,0,1))
        vp.saveAsImageFile(os.path.join(OUT,'assembled.png'),1800,1400)
        view((70,80,450),(70,80,0),(0,1,0))
        vp.saveAsImageFile(os.path.join(OUT,'top.png'),1400,1600)
        lo.isLightBulbOn=False; ro.isLightBulbOn=False
        view((270,-260,350),(70,80,25),(0,0,1))
        vp.saveAsImageFile(os.path.join(OUT,'inside.png'),1800,1400)
        lo.isLightBulbOn=True
        export=design.exportManager
        for name,c in printparts:
            opts=export.createSTLExportOptions(c,os.path.join(OUT,name+'.stl'))
            opts.meshRefinement=adsk.fusion.MeshRefinementSettings.MeshRefinementHigh
            assert export.execute(opts),'STL export '+name
        ro.isLightBulbOn=True
        view((270,-260,350),(70,80,25),(0,0,1))
        assert export.execute(export.createFusionArchiveExportOptions(os.path.join(OUT,'H2_Brain_LCD2004.f3d')))
        assert export.execute(export.createSTEPExportOptions(os.path.join(OUT,'H2_Brain_LCD2004.step')))
        note('SUCCESS - native F3D, STEP, six STL parts and previews saved')
        import importlib.util
        spec=importlib.util.spec_from_file_location('h2_finish',os.path.join(OUT,'H2_Brain_Enclosure','finish_model.py'))
        fin=importlib.util.module_from_spec(spec); spec.loader.exec_module(fin); fin.run(context)
    except:
        note('ERROR\n'+traceback.format_exc())
        app.userInterface.messageBox('H2 enclosure script failed. See build_log.txt\n'+traceback.format_exc())
