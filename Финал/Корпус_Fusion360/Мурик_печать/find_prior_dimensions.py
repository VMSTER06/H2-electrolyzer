"""Extract the user's enclosure-related source messages, excluding tool outputs."""
from pathlib import Path
import json,re,hashlib
from compression import zstd
ROOT=Path(__file__).resolve().parent
sources=list(Path(r'C:\Users\vladm\.codex\sessions\2026\09\27').glob('*01a0e1b5-2336-75f1-ac25-19954ee948b9*'))
pattern=re.compile(r'размер|габарит|замер|сантим|миллимет|плат|кноп|светодиод|корпус|вентилятор|тумбл|реле|диспле|экран|\d\s*[xх×*]\s*\d',re.I)
found=[];seen=set()
for path in sources:
    stream=zstd.open(path,'rt',encoding='utf-8') if path.suffix=='.zst' else path.open(encoding='utf-8')
    with stream:
        for line_no,line in enumerate(stream,1):
            try:record=json.loads(line)
            except Exception:continue
            message=None
            if record.get('type')=='user':
                message=record.get('message')
            elif record.get('type')=='response_item':
                p=record.get('payload',{})
                if p.get('type')=='message' and p.get('role')=='user':message=p
            elif record.get('type')=='event_msg':
                p=record.get('payload',{})
                if p.get('type')=='user_message':message={'content':p.get('message','')}
            if not isinstance(message,dict):continue
            content=message.get('content',[])
            texts=[];images=[]
            if isinstance(content,str):texts=[content]
            elif isinstance(content,list):
                for block in content:
                    if not isinstance(block,dict):continue
                    if block.get('type') in ('text','input_text'):texts.append(block.get('text',''))
                    elif block.get('type') in ('image','input_image'):
                        source=block.get('source',{})
                        images.append({'type':block['type'],'media_type':source.get('media_type'),
                            'has_base64':bool(source.get('data')), 'url':str(block.get('image_url',''))[:80] if not str(block.get('image_url','')).startswith('data:') else '[embedded image]'})
            text='\n'.join(texts)
            if not pattern.search(text) and not images:continue
            if '<environment_context>' in text or '<INSTRUCTIONS>' in text:continue
            digest=hashlib.sha256((text+str(images)).encode()).hexdigest()
            if digest in seen:continue
            seen.add(digest)
            item=dict(source=str(path),line=line_no,timestamp=record.get('timestamp'),text=text,images=images)
            found.append(item)
            print(json.dumps(item,ensure_ascii=False))
(ROOT/'prior_user_dimensions_messages.json').write_text(json.dumps(found,ensure_ascii=False,indent=2),encoding='utf-8')
