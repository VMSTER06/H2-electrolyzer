import base64
import json
import sys
import time
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent / 'python_deps'))
from websockets.sync.client import connect

targets = json.load(urllib.request.urlopen('http://127.0.0.1:9223/json'))
target = next(t for t in targets if t['type'] == 'page' and
              (t['url'] == 'about:blank' or 'H2_Android_App' in t['url']))
url = (Path(__file__).parent.parent / 'H2_Android_App/app/src/main/assets/index.html').as_uri()

mock = r'''
window.Brain = {
  status(id) { setTimeout(() => window.h2Response(id, 200, JSON.stringify({
    p:2.74,tgt:3.0,h:0.15,pwr:1,lck:0,prg:0,psh:0,rep:0,mgs:0,
    pzo:0,ele:1,val:0,rf:1,ovw:0,ovl:0,tlk:0,tcal:0,wd:0,
    rt:5143,ta:3,up:342,t:26.1,tok:1,oh:0,f1:0,f2:1,
    f1m:2,f1on:40,f1off:35,f2m:3,f2on:45,f2off:40,
    rce:8,rcv:63,rcp:74,rcf1:1,rcf2:3,relw:0,pco:0.5,pcs:2.5,
    rfc:76,rfh:58,sout:2,svol:10,sp:0,fsok:1,oledok:1,lcdok:1,mpuok:1,nrfok:1
  })), 30) },
  soundConfig(id) { setTimeout(() => window.h2Response(id,200,JSON.stringify({out:2,vol:10,alm:1,clk:1,list:[]})),30) }
};
'''

with connect(target['webSocketDebuggerUrl'], max_size=10_000_000) as ws:
    seq = 0
    def call(method, params=None):
        global seq
        seq += 1
        ws.send(json.dumps({'id':seq,'method':method,'params':params or {}}))
        while True:
            msg = json.loads(ws.recv())
            if msg.get('id') == seq:
                if 'error' in msg: raise RuntimeError(msg['error'])
                return msg.get('result',{})
    call('Page.enable')
    call('Emulation.setDeviceMetricsOverride', {'width':390,'height':844,'deviceScaleFactor':1,'mobile':True})
    call('Page.addScriptToEvaluateOnNewDocument', {'source':mock})
    call('Page.navigate', {'url':url})
    time.sleep(2)
    width = call('Runtime.evaluate', {'expression':'window.innerWidth','returnByValue':True})['result']['value']
    for page in ('home','control','sound','diag'):
        call('Runtime.evaluate', {'expression':f"tab('{page}')"})
        time.sleep(.2)
        data = call('Page.captureScreenshot', {'format':'png','captureBeyondViewport':False})['data']
        out = Path(__file__).parent / f'h2_android_mobile_{page}.png'
        out.write_bytes(base64.b64decode(data))
        print(f'viewport={width} page={page} screenshot={out}')
