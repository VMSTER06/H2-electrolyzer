"""Run in Fusion: recover exact source solids as native parametric base features."""
import adsk.core,adsk.fusion,json,os
WORK=r'C:\Users\vladm\AppData\Local\Temp\murik_native_fix_20261018'
DOC='Murik corrected copy 2026-10-18'

def run(_context: str):
    app=adsk.core.Application.get()
    doc=app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType)
    doc.name=DOC
    d=adsk.fusion.Design.cast(app.activeProduct)
    d.designType=adsk.fusion.DesignTypes.ParametricDesignType
    d.unitsManager.distanceDisplayUnits=adsk.fusion.DistanceUnits.MillimeterDistanceUnits
    root=d.rootComponent
    root.description='Recovered exact BRep geometry from murik.f3d (2705.1.11) for Fusion 2704.1.36; old feature history unavailable. Corrections are native editable features.'
    d.attributes.add('MurikFix','source_sha256','004681582065bf75e82226859ce4e096f64a0aede6988ce9a6723eb0a7b954a7')
    d.attributes.add('MurikFix','source_file',r'C:\Users\vladm\Desktop\мурик.f3d')
    d.attributes.add('MurikFix','history','Native source bodies recovered; original feature tree not recoverable in installed version')
    parts=json.load(open(os.path.join(WORK,'parts.json'),encoding='utf-8'))
    tbm=adsk.fusion.TemporaryBRepManager.get()
    scale=adsk.core.Matrix3D.create()
    for k in range(3):scale.setCell(k,k,.2)
    report={'document':DOC,'components':[]}
    names={
        '01 BASE':['BASE_cutaway_A_source','BASE_cutaway_B_source','BASE_PRINT'],
        '02 LID':['LID_PRINT'],
        'V2 removable control fascia':['CONTROL_FASCIA_PRINT'],
        'V2 control PCB removable carrier':['CONTROL_CARRIER_PRINT'],
        'REF main board':['Main PCB','ESP32 reference','nRF24 reference','MPU6050 reference','Tallest component reserve','Rocker switch reserve','SMA connector reserve'],
        'REF actual':['Control PCB 30x30 estimate','Button 1 body','Button 1 actuator','Button 2 body','Button 2 actuator','Button 3 body','Button 3 actuator','LED blue','LED red','LED green'],
        'REF relay':['Relay PCB envelope','Relay 1','Relay 2','Relay 3','Relay 4','Terminal strip','Connector reserve'],
        'REF LCD':['LCD PCB 98x60','LCD bezel reserve','LCD viewing area','LCD I2C backpack reserve']}
    for part in parts:
        o=root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
        c=o.component;c.name=part['name']
        c.description='Exact source component, restored from '+part['blob']
        bf=c.features.baseFeatures.add();bf.name='Source CAD solid - '+c.name
        assert bf.startEdit()
        bodies=tbm.createFromFile(part['file'])
        assert bodies and bodies.count>0,c.name
        chosen=next((value for key,value in names.items() if c.name.startswith(key)),None)
        for i,temp in enumerate(bodies):
            assert temp.isSolid,(c.name,i)
            assert tbm.transform(temp,scale)
            b=c.bRepBodies.add(temp,bf)
            assert b is not None,c.name
            b.name=chosen[i] if chosen else 'PRINT_BODY'
        assert bf.finishEdit()
        if c.name.startswith('01 BASE'):
            for i,b in enumerate(c.bRepBodies):b.isLightBulbOn=(i==2)
            b=c.bRepBodies.item(2)
            bb=b.boundingBox
            size=[(bb.maxPoint.x-bb.minPoint.x)*10,(bb.maxPoint.y-bb.minPoint.y)*10,(bb.maxPoint.z-bb.minPoint.z)*10]
            assert max(abs(a-v) for a,v in zip(size,[140,180,64]))<.001,size
            assert abs(b.volume-211.765994)<.002,b.volume
        c.isSketchFolderLightBulbOn=False
        c.isConstructionFolderLightBulbOn=False
        report['components'].append({'name':c.name,'body_count':c.bRepBodies.count,'volumes_cm3':[b.volume for b in c.bRepBodies]})
    assert root.occurrences.count==16
    app.activeViewport.fit();adsk.doEvents()
    json.dump(report,open(os.path.join(WORK,'recovered_components.json'),'w',encoding='utf-8'),ensure_ascii=False,indent=2)
    print(json.dumps({'document':doc.name,'component_count':root.occurrences.count,'body_count':sum(c.bRepBodies.count for c in d.allComponents),'timeline_features':d.timeline.count},ensure_ascii=False))
