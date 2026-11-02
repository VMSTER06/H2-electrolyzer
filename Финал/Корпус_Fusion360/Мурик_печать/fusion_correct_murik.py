"""Native Fusion features for the corrected insert and its missing plungers."""
import adsk.core,adsk.fusion,json,os,math
WORK=r'C:\Users\vladm\AppData\Local\Temp\murik_native_fix_20261018'
DOC='Murik corrected copy 2026-10-18'
NEW=adsk.fusion.FeatureOperations.NewBodyFeatureOperation
JOIN=adsk.fusion.FeatureOperations.JoinFeatureOperation
CUT=adsk.fusion.FeatureOperations.CutFeatureOperation

def pt(x,y,z=0):return adsk.core.Point3D.create(x/10,y/10,z/10)
def val(v):return adsk.core.ValueInput.createByString(str(v)+' mm')

def sketch(c,z,name):
    pi=c.constructionPlanes.createInput()
    assert pi.setByOffset(c.xYConstructionPlane,val(z))
    pl=c.constructionPlanes.add(pi);pl.isLightBulbOn=False
    s=c.sketches.add(pl);s.name=name
    return s

def extrude(c,s,h,operation,name):
    assert s.profiles.count>0,(name,'empty profile')
    profiles=adsk.core.ObjectCollection.create()
    for profile in s.profiles:profiles.add(profile)
    inp=c.features.extrudeFeatures.createInput(profiles,operation)
    inp.setDistanceExtent(False,val(h))
    if operation!=NEW:inp.participantBodies=[c.bRepBodies.item(0)]
    feature=c.features.extrudeFeatures.add(inp);feature.name=name
    s.isLightBulbOn=False
    return feature

def rectangle(c,z,x,y,w,h,name,operation,depth,expressions=None):
    s=sketch(c,z,name+' - sketch')
    lines=s.sketchCurves.sketchLines.addTwoPointRectangle(pt(x,y),pt(x+w,y+h))
    if expressions:
        points=[p for line in lines for p in [line.startSketchPoint,line.endSketchPoint]]
        p0=min(points,key=lambda p:(p.geometry.x-x/10)**2+(p.geometry.y-y/10)**2)
        p1=min(points,key=lambda p:(p.geometry.x-(x+w)/10)**2+(p.geometry.y-(y+h)/10)**2)
        H=adsk.fusion.DimensionOrientations.HorizontalDimensionOrientation
        V=adsk.fusion.DimensionOrientations.VerticalDimensionOrientation
        for a,b,orient,text,expr in [
            (s.originPoint,p0,H,pt(x/2,y-3),expressions[0]),
            (s.originPoint,p0,V,pt(x-4,y/2),expressions[1]),
            (p0,p1,H,pt(x+w/2,y-2),expressions[2]),
            (p0,p1,V,pt(x+w+2,y+h/2),expressions[3])]:
            dim=s.sketchDimensions.addDistanceDimension(a,b,orient,text)
            dim.parameter.expression=expr
    return extrude(c,s,depth,operation,name)

def circles(c,z,items,name,operation,depth):
    result=None
    for i,(x,y,diam,dx,dy,dd) in enumerate(items):
        suffix=(' '+str(i+1)) if len(items)>1 else ''
        s=sketch(c,z,name+suffix+' - sketch')
        circle=s.sketchCurves.sketchCircles.addByCenterRadius(pt(x,y),diam/20)
        dim=s.sketchDimensions.addDiameterDimension(circle,pt(x+5,y+5))
        dim.parameter.expression=dd
        for orientation,expr,offset in [(adsk.fusion.DimensionOrientations.HorizontalDimensionOrientation,dx,pt(x/2,y-5-i)),(adsk.fusion.DimensionOrientations.VerticalDimensionOrientation,dy,pt(x-6-i,y/2))]:
            dim=s.sketchDimensions.addDistanceDimension(s.originPoint,circle.centerSketchPoint,orientation,offset)
            dim.parameter.expression=expr
        result=extrude(c,s,depth,operation if i==0 else (JOIN if operation==NEW else operation),name+suffix)
    return result

def run(_context: str):
    app=adsk.core.Application.get()
    doc=next(doc for doc in app.documents if doc.name==DOC)
    doc.activate()
    d=adsk.fusion.Design.cast(app.activeProduct)
    assert d.rootComponent.occurrences.count==16,'Only modify fresh recovered copy'
    c=next(c for c in d.allComponents if c.name=='V2 removable control fascia')
    assert c.bRepBodies.count==1
    params=[('murik_guide_d',6.3,'Guide diameter: 0.4 mm radial gap for 5.5 mm tip'),
        ('murik_button_pitch',7.62,'Existing V3 switch centre spacing'),
        ('murik_button_center_x',70,'Middle switch X'),('murik_button_row_y',53,'Switch row Y'),
        ('murik_led_w',22,'Common LED window width'),('murik_led_h',8,'Common LED window height'),
        ('murik_led_r',2,'LED window corner radius'),('murik_led_center_x',70,'LED window centre X'),
        ('murik_led_center_y',39,'LED row centre Y'),('murik_plunger_d',5.5,'Plunger finger diameter')]
    for name,value,comment in params:d.userParameters.add(name,val(value),'mm',comment)
    rectangle(c,67,61.24,51.49,17.02,3.52,'FIX 01 fill obsolete narrow button slot',JOIN,2.5)
    rectangle(c,67,60.49,37.99,19.52,6.52,'FIX 02 fill shifted LED window',JOIN,2.5)
    items=[(x,53,6.3,expr,'murik_button_row_y','murik_guide_d') for x,expr in [(62.38,'murik_button_center_x-murik_button_pitch'),(70,'murik_button_center_x'),(77.62,'murik_button_center_x+murik_button_pitch')]]
    circles(c,66.9,items,'FIX 03 three editable 6.3 mm button guides',CUT,2.7)
    rectangle(c,66.9,61,35,18,8,'FIX 04 LED window vertical span',CUT,2.7,
        ['murik_led_center_x-murik_led_w/2+murik_led_r','murik_led_center_y-murik_led_h/2','murik_led_w-2*murik_led_r','murik_led_h'])
    rectangle(c,66.9,59,37,22,4,'FIX 05 LED window horizontal span',CUT,2.7,
        ['murik_led_center_x-murik_led_w/2','murik_led_center_y-murik_led_h/2+murik_led_r','murik_led_w','murik_led_h-2*murik_led_r'])
    items=[]
    for x,sx in [(61,'-'),(79,'+')]:
        for y,sy in [(37,'-'),(41,'+')]:
            items.append((x,y,4,'murik_led_center_x'+sx+'(murik_led_w/2-murik_led_r)','murik_led_center_y'+sy+'(murik_led_h/2-murik_led_r)','2*murik_led_r'))
    circles(c,66.9,items,'FIX 06 rounded LED window corners',CUT,2.7)
    c.bRepBodies.item(0).name='CONTROL_FASCIA_CORRECTED_PRINT'
    assert abs(c.bRepBodies.item(0).volume-8.49635332323544)<.0001,c.bRepBodies.item(0).volume
    # Add the three existing V3 mechanism plungers, absent from the source archive.
    for i,(x,xexpr) in enumerate([(62.38,'murik_button_center_x-murik_button_pitch'),(70,'murik_button_center_x'),(77.62,'murik_button_center_x+murik_button_pitch')],1):
        o=d.rootComponent.occurrences.addNewComponent(adsk.core.Matrix3D.create())
        cc=o.component;cc.name='FIX V3 plunger '+str(i)
        cc.description='Native editable V3 plunger for the corrected insert; this component was absent from the source archive.'
        for z,diam,h,op,label,expr in [(59.25,4,7.55,NEW,'Contact stem','4 mm'),(65.2,7,1.6,JOIN,'Captive flange','7 mm'),(66.8,5.5,3.6,JOIN,'Finger tip','murik_plunger_d')]:
            circles(cc,z,[(x,53,diam,xexpr,'murik_button_row_y',expr)],label,op,h)
        cc.bRepBodies.item(0).name='PLUNGER_'+str(i)+'_PRINT'
        assert abs(cc.bRepBodies.item(0).volume-.22187498115977)<.0001
    source=None
    for library in app.materialLibraries:
        source=next((a for a in library.appearances if a.name=='Plastic - Matte (Black)'),None)
        if source:break
    appearances={}
    def color(body,name,rgb):
        if source:
            if name not in appearances:
                a=d.appearances.addByCopy(source,'Murik '+name)
                cp=next((p for p in a.appearanceProperties if adsk.core.ColorProperty.cast(p) and p.id in ('opaque_albedo','generic_diffuse')),None)
                if cp:cp.value=adsk.core.Color.create(*rgb,255)
                appearances[name]=a
            body.appearance=appearances[name]
    for comp in d.allComponents:
        comp.isSketchFolderLightBulbOn=False;comp.isConstructionFolderLightBulbOn=False
        for i,b in enumerate(comp.bRepBodies):
            if comp.name.startswith('02 LID'):color(b,'silver',(190,204,215))
            elif comp.name.startswith('FIX V3 plunger'):color(b,'cyan',(0,165,185))
            elif comp.name.startswith('REF LCD'):
                key,rgb=[('pcb',(24,112,73)),('black',(22,26,32)),('lcd',(22,53,152)),('pcb',(24,112,73))][i]
                color(b,key,rgb)
            elif comp.name.startswith('REF actual'):
                key,rgb=('pcb',(24,112,73)) if i==0 else ('silver',(150,155,160))
                if i>=7:key,rgb=[('led_blue',(20,92,210)),('led_red',(220,35,45)),('led_green',(35,200,83))][i-7]
                color(b,key,rgb)
            elif comp.name.startswith('REF main'):
                key,rgb=('pcb',(24,112,73)) if i in (0,3,4) else ('black',(22,26,32))
                if i==6:key,rgb='brass',(188,146,53)
                color(b,key,rgb)
            elif comp.name.startswith('REF relay'):color(b,'relayblue',(29,139,193))
            else:color(b,'graphite',(48,57,68))
    assert d.computeAll()
    adsk.doEvents()
    problems=[]
    for i in range(d.timeline.count):
        entity=d.timeline.item(i).entity
        if entity and hasattr(entity,'healthState') and entity.healthState!=adsk.fusion.FeatureHealthStates.HealthyFeatureHealthState:
            problems.append({'name':entity.name,'message':entity.errorOrWarningMessage})
    assert not problems,problems
    assert d.rootComponent.occurrences.count==19
    d.attributes.add('MurikFix','corrections','3 x 6.3 mm guides, pitch 7.62; centred 22 x 8 mm LED window R2; native V3 plungers added')
    report={'document':DOC,'components':d.rootComponent.occurrences.count,'timeline_count':d.timeline.count,'fascia_volume_cm3':c.bRepBodies.item(0).volume,'feature_problems':problems,'added_plungers':3,'parameters':{p.name:p.expression for p in d.userParameters}}
    json.dump(report,open(os.path.join(WORK,'native_correction_report.json'),'w',encoding='utf-8'),ensure_ascii=False,indent=2)
    print(json.dumps(report,ensure_ascii=False))
