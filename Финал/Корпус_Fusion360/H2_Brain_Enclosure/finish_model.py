import adsk.core, adsk.fusion, os, json, traceback, importlib.util
OUT=r'S:\1Electrolizer\Финал\Корпус_Fusion360'

def run(context):
    app=adsk.core.Application.get(); log=[]
    def note(x):
        log.append(str(x))
        with open(os.path.join(OUT,'finish_log.txt'),'w',encoding='utf-8') as f: f.write('\n'.join(log))
    try:
        spec=importlib.util.spec_from_file_location('h2_builder',os.path.join(OUT,'H2_Brain_Enclosure','H2_Brain_Enclosure.py'))
        m=importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
        d=adsk.fusion.Design.cast(app.activeProduct); root=d.rootComponent
        occ=list(root.occurrences)
        assert len(occ)==7 and occ[0].component.name.startswith('01 BASE'),'Unexpected active design'
        base,lid=occ[0].component,occ[1].component; ref=occ[6].component
        # Re-evaluate any cached feature warnings before editing.
        d.computeAll(); adsk.doEvents()
        for f in list(lid.features.extrudeFeatures):
            if f.name=='Engrave MODE' and f.healthState!=adsk.fusion.FeatureHealthStates.HealthyFeatureHealthState:
                f.deleteMe()
                old=next(s for s in lid.sketches if s.name=='Engraving MODE')
                old.isLightBulbOn=False
                m.label(lid,lid.bRepBodies.item(0),'MODE',104,16,30,2.5)
                note('Recreated MODE engraving')
        # Library appearance source is required: a default body appearance can be null.
        source=None
        for lib in app.materialLibraries:
            for a in lib.appearances:
                if 'Plastic' in a.name and ('Matte' in a.name or 'Opaque' in a.name):
                    source=a; break
            if source: break
        if not source:
            for lib in app.materialLibraries:
                if lib.appearances.count: source=lib.appearances.item(0); break
        note('Appearance source: '+(source.name if source else 'NONE'))
        def paint(b,name,rgb):
            a=d.appearances.itemByName(name)
            if not a: a=d.appearances.addByCopy(source,name)
            props=[p for p in a.appearanceProperties if adsk.core.ColorProperty.cast(p)]
            chosen=next((p for p in props if p.id in ('opaque_albedo','generic_diffuse')),None)
            if chosen is None and props: chosen=props[0]
            if chosen: chosen.value=adsk.core.Color.create(*rgb,255)
            b.appearance=a
        paint(base.bRepBodies.item(0),'H2 final charcoal',(43,52,64))
        paint(lid.bRepBodies.item(0),'H2 final silver',(190,203,213))
        for o in occ[2:6]: paint(o.component.bRepBodies.item(0),'H2 final charcoal',(43,52,64))
        reference_names=['Main PCB - assumed substrate 1.6','Component envelope - total height 15 mm','LCD2004 PCB 98x60','LCD metal frame','LCD visible glass','I2C backpack','POWER barrel','POWER cap','LOCK barrel','LOCK cap','PURGE barrel','PURGE cap','LED POWER','LED LOCK','LED MODE']
        assert ref.bRepBodies.count==len(reference_names),'Unexpected reference geometry'
        for b,n in zip(ref.bRepBodies,reference_names): b.name=n
        ledindex=0
        for b in ref.bRepBodies:
            n=b.name
            if 'PCB' in n or 'backpack' in n: paint(b,'H2 final PCB',(22,110,76))
            elif 'glass' in n: paint(b,'H2 final blue',(15,63,170))
            elif 'LED' in n:
                rgb=[(35,210,85),(245,150,25),(30,125,245)][ledindex]; ledindex+=1
                paint(b,'H2 final LED '+str(ledindex),rgb)
            elif 'envelope' in n.lower(): paint(b,'H2 final PCB',(22,110,76)); b.opacity=0.2
            else: paint(b,'H2 final black',(20,25,32))
        # Save assembled, open and exploded inspection images.
        vp=app.activeViewport
        vp.visualStyle=adsk.core.VisualStyles.ShadedWithVisibleEdgesOnlyVisualStyle
        def view(eye,target,up):
            cam=vp.camera; cam.eye=m.p(*eye); cam.target=m.p(*target); cam.upVector=adsk.core.Vector3D.create(*up); cam.isFitView=True; cam.isSmoothTransition=False; vp.camera=cam; vp.fit(); adsk.doEvents()
        d.computeAll(); adsk.doEvents()
        probs=[]
        for i in range(d.timeline.count):
            ent=d.timeline.item(i).entity
            if ent and hasattr(ent,'healthState') and ent.healthState!=adsk.fusion.FeatureHealthStates.HealthyFeatureHealthState:
                probs.append({'name':ent.name,'message':ent.errorOrWarningMessage})
        note('Feature issues: '+json.dumps(probs))
        report=json.load(open(os.path.join(OUT,'verification.json'),encoding='utf-8'))
        report['feature_problems']=probs
        ip=d.createInterferenceInput(m.coll(occ)); ip.areCoincidentFacesIncluded=False
        results=d.analyzeInterference(ip)
        report['all_components_interferences']=results.count if results else 0
        note('All components interference: '+str(report['all_components_interferences']))
        with open(os.path.join(OUT,'verification.json'),'w',encoding='utf-8') as f: json.dump(report,f,indent=2)
        for o in occ:o.isLightBulbOn=True
        view((270,-260,350),(70,80,25),(0,0,1)); vp.saveAsImageFile(os.path.join(OUT,'assembled.png'),1800,1400)
        view((70,80,450),(70,80,0),(0,1,0)); vp.saveAsImageFile(os.path.join(OUT,'top.png'),1400,1600)
        occ[1].isLightBulbOn=False; occ[6].isLightBulbOn=False
        view((180,-170,410),(70,80,20),(0,0,1)); vp.saveAsImageFile(os.path.join(OUT,'inside.png'),1800,1400)
        occ[6].isLightBulbOn=True
        for b in ref.bRepBodies:
            b.isLightBulbOn=('Main PCB' in b.name or 'envelope' in b.name.lower())
        vp.saveAsImageFile(os.path.join(OUT,'pcb_in_base.png'),1800,1400)
        for b in ref.bRepBodies:b.isLightBulbOn=True
        occ[1].isLightBulbOn=True; occ[6].isLightBulbOn=False
        t=adsk.core.Matrix3D.create(); t.translation=adsk.core.Vector3D.create(0,0,7)
        original=occ[1].transform2; occ[1].transform2=t
        view((280,-280,350),(70,80,60),(0,0,1)); vp.saveAsImageFile(os.path.join(OUT,'exploded.png'),1800,1600)
        occ[1].transform2=original; occ[6].isLightBulbOn=True
        ex=d.exportManager
        names=['base','lid','clamp_1','clamp_2','clamp_3','clamp_4']
        for name,o in zip(names,occ[:6]):
            opt=ex.createSTLExportOptions(o.component,os.path.join(OUT,name+'.stl')); opt.meshRefinement=adsk.fusion.MeshRefinementSettings.MeshRefinementHigh
            assert ex.execute(opt)
        view((270,-260,350),(70,80,25),(0,0,1))
        assert ex.execute(ex.createFusionArchiveExportOptions(os.path.join(OUT,'H2_Brain_LCD2004.f3d')))
        assert ex.execute(ex.createSTEPExportOptions(os.path.join(OUT,'H2_Brain_LCD2004.step')))
        note('SUCCESS: final exports saved')
    except:
        note(traceback.format_exc()); app.userInterface.messageBox(traceback.format_exc())
