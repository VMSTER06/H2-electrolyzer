"""Preserve the original F3D and repack an identical-content working copy."""
from pathlib import Path
import zipfile,hashlib,shutil,json,re
ROOT=Path(__file__).resolve().parent
OUT=ROOT/'Fusion_исправление'
OUT.mkdir(exist_ok=True)
TEMP=Path(r'C:\Users\vladm\AppData\Local\Temp\murik_native_fix_20261018')
TEMP.mkdir(exist_ok=True)
src=Path(r'C:\Users\vladm\Desktop\мурик.f3d')
backup=OUT/'мурик_до_исправлений.f3d'
src_hash=hashlib.sha256(src.read_bytes()).hexdigest()
if backup.exists():
    assert hashlib.sha256(backup.read_bytes()).hexdigest()==src_hash
else:
    shutil.copy2(src,backup)
shutil.copy2(src,TEMP/'murik_original_copy.f3d')
repacked=TEMP/'murik_deflate_copy.f3d'
with zipfile.ZipFile(src) as zin,zipfile.ZipFile(repacked,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as zout:
    for entry in zin.infolist():
        info=zipfile.ZipInfo(entry.filename,entry.date_time)
        info.external_attr=entry.external_attr
        info.create_system=entry.create_system
        info.comment=entry.comment
        zout.writestr(info,zin.read(entry),compress_type=zipfile.ZIP_DEFLATED,compresslevel=6)
with zipfile.ZipFile(src) as a,zipfile.ZipFile(repacked) as b:
    assert a.namelist()==b.namelist()
    assert all(a.read(name)==b.read(name) for name in a.namelist())
    strings=[m.group().decode('utf-16le',errors='replace') for m in re.finditer(rb'(?:[\x20-\x7e\xc0-\xff][\x00\x04]){3,}',a.read('FusionAssetName[Active]/FusionDesignSegmentType1/BulkStream.dat'))]
    parts=[]
    breps=TEMP/'breps'
    breps.mkdir(exist_ok=True)
    for i,s in enumerate(strings):
        if re.fullmatch(r'BREP\.[0-9a-f-]+\.smbh',s):
            target=breps/(s[:-1])
            target.write_bytes(a.read('FusionAssetName[Active]/Breps.BlobParts/'+s))
            parts.append({'name':strings[i-1],'file':str(target),'blob':s})
    assert len(parts)==16,len(parts)
    (TEMP/'parts.json').write_text(json.dumps(parts,ensure_ascii=False,indent=2),encoding='utf-8')
    (OUT/'source_component_manifest.json').write_text(json.dumps(parts,ensure_ascii=False,indent=2),encoding='utf-8')
report=dict(original_sha256=src_hash,backup=str(backup),working_copy=str(repacked),
    cad_streams_unchanged=True,archive_compression='deflate',entries=len(a.namelist()))
(OUT/'copy_preparation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))
