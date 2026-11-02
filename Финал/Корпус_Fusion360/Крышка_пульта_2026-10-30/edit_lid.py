import adsk.core,adsk.fusion,json,os,struct
OUT=r'S:\Projects\1Electrolizer\Финал\Корпус_Fusion360\Крышка_пульта_2026-10-30'
DESKTOP=r'C:\Users\vladm\Desktop'
def run(_context: str):
    app=adsk.core.Application.get()
    source=next(doc for doc in app.documents if doc.name=='корпус для пульта')
    source.activate()
    original=adsk.fusion.Design.cast(app.activeProduct)
    os.makedirs(OUT,exist_ok=True)
    before=os.path.join(OUT,'Исходник_до_правок.f3d')
    assert original.exportManager.execute(original.exportManager.createFusionArchiveExportOptions(before))
    doc=app.importManager.importToNewDocument(app.importManager.createFusionArchiveImportOptions(before))
    assert doc
    doc.name='Крышка пульта — клипсы 1.6 — без стоек и поддержек'
    d=adsk.fusion.Design.cast(app.activeProduct);r=d.rootComponent
    lid=r.bRepBodies.itemByName('Lid_PETG_1.2_DRAFT')
    housing=r.bRepBodies.itemByName('Housing_PETG_1.2_DRAFT')
    assert lid and housing and r.meshBodies.count==0
    mgr=adsk.fusion.TemporaryBRepManager.get();old=mgr.copy(lid)
    unchanged={b.name:b.volume for b in r.bRepBodies if b!=lid}
    OP=adsk.fusion.FeatureOperations
    def rectangle(x0,y0,x1,y1,z,depth,name,op):
        pi=r.constructionPlanes.createInput()
        pi.setByOffset(r.xYConstructionPlane,adsk.core.ValueInput.createByReal(z/10))
        pl=r.constructionPlanes.add(pi);pl.name=name+' plane';pl.isLightBulbOn=False
        sk=r.sketches.add(pl);sk.name=name
        def point(x,y):return sk.modelToSketchSpace(adsk.core.Point3D.create(x/10,y/10,z/10))
        sk.sketchCurves.sketchLines.addTwoPointRectangle(point(x0,y0),point(x1,y1))
        assert sk.profiles.count==1
        ei=r.features.extrudeFeatures.createInput(sk.profiles.item(0),op)
        ei.setDistanceExtent(False,adsk.core.ValueInput.createByReal(depth/10))
        ei.participantBodies=[lid]
        f=r.features.extrudeFeatures.add(ei);f.name=name;sk.isVisible=False
        assert f.healthState==adsk.fusion.FeatureHealthStates.HealthyFeatureHealthState
        return f
    # Stop precisely at the underside of the 1.2 mm front skin.
    rectangle(-14.1,-26.1,14.5,9.1,26,11.8,'REMOVE keypad standoffs and capture shelves',OP.CutFeatureOperation)
    rectangle(14.5,-22.4,49.5,11.4,26,11.8,'REMOVE OLED standoffs and capture shelves',OP.CutFeatureOperation)
    # Keep the existing hook/outside face. Add the extra 0.8 mm toward the interior.
    clips=[(-10.6,-36.1,-5.4,-34.5),(32.4,-36.1,37.6,-34.5),(-10.6,24.02,-5.4,25.62),(40.4,24.02,45.6,25.62)]
    for i,rect in enumerate(clips,1):
        rectangle(*rect,30.2,7.8,'CLIP %d beam thickness 1.6 mm inward reinforcement'%i,OP.JoinFeatureOperation)
    assert lid.isSolid and lid.lumps.count==1
    for b in r.bRepBodies:
        if b!=lid:assert abs(b.volume-unchanged[b.name])<1e-7,b.name
    def intersection(a,b):
        t=mgr.copy(a)
        assert mgr.booleanOperation(t,mgr.copy(b),adsk.fusion.BooleanTypes.IntersectionBooleanType)
        return t.volume*1000
    baseline_collision=intersection(old,housing)
    final_collision=intersection(lid,housing)
    assert final_collision<=baseline_collision+0.001,(baseline_collision,final_collision)
    # Isolate only the updated native lid for preview and STL export.
    for b in r.bRepBodies:b.isLightBulbOn=(b==lid)
    r.isBodiesFolderLightBulbOn=True
    lid.name='LID_PRINT_1p6_NO_SUPPORTS_NO_STANDOFFS'
    for sk in r.sketches:sk.isVisible=False
    for pl in r.constructionPlanes:pl.isLightBulbOn=False
    issues=[]
    for i in range(d.timeline.count):
        ent=d.timeline.item(i).entity
        if hasattr(ent,'healthState') and ent.healthState!=adsk.fusion.FeatureHealthStates.HealthyFeatureHealthState:
            issues.append({'name':ent.name,'message':ent.errorOrWarningMessage})
    assert not issues,issues
    native=os.path.join(OUT,'Крышка_пульта_клипсы_1.6_без_стоек.f3d')
    assert d.exportManager.execute(d.exportManager.createFusionArchiveExportOptions(native))
    stl=os.path.join(OUT,'lid_native_coordinates.stl')
    opt=d.exportManager.createSTLExportOptions(lid,stl)
    opt.unitType=adsk.fusion.DistanceUnits.MillimeterDistanceUnits
    opt.meshRefinement=adsk.fusion.MeshRefinementSettings.MeshRefinementHigh
    opt.isBinaryFormat=True;opt.sendToPrintUtility=False
    assert d.exportManager.execute(opt)
    # Rigid rotation of exported STL only: outer front face flat on the print bed.
    bb=lid.boundingBox;xmax=bb.maxPoint.x*10;ymin=bb.minPoint.y*10;zmax=bb.maxPoint.z*10
    data=bytearray(open(stl,'rb').read());count=struct.unpack_from('<I',data,80)[0]
    assert len(data)==84+50*count
    for i in range(count):
        offset=84+50*i;v=list(struct.unpack_from('<12fH',data,offset));v[0]*=-1;v[2]*=-1
        for k in (3,6,9):v[k:k+3]=[xmax-v[k],v[k+1]-ymin,zmax-v[k+2]]
        struct.pack_into('<12fH',data,offset,*v)
    final_stl=os.path.join(DESKTOP,'Крышка_пульта_клипсы_1.6_без_стоек_2026-10-30.stl')
    open(final_stl,'wb').write(data)
    open(os.path.join(OUT,os.path.basename(final_stl)),'wb').write(data)
    report={'source_cloud_document':'корпус для пульта v2; exact same native lid as Пульт с поддержками',
      'native_copy':native,'desktop_stl':final_stl,'clip_beam_mm':1.6,'clips':clips,
      'supports_in_export':0,'standoffs_removed':['keypad (4)','OLED (4)'],
      'skin_mm':1.2,'volume_before_mm3':old.volume*1000,'volume_after_mm3':lid.volume*1000,
      'lid_housing_interference_before_mm3':baseline_collision,'lid_housing_interference_after_mm3':final_collision,
      'feature_issues':issues,'triangles':count,'native_bbox_mm':[[v*10 for v in p.asArray()] for p in (bb.minPoint,bb.maxPoint)]}
    json.dump(report,open(os.path.join(OUT,'CAD_проверка.json'),'w',encoding='utf-8'),ensure_ascii=False,indent=2)
    cam=app.activeViewport.camera;cam.viewOrientation=adsk.core.ViewOrientations.IsoBottomRightViewOrientation;cam.isFitView=True
    app.activeViewport.camera=cam;app.activeViewport.fit()
    assert app.activeViewport.saveAsImageFile(os.path.join(OUT,'Крышка_после_правок.png'),1400,1000)
    print(json.dumps(report,ensure_ascii=False))
