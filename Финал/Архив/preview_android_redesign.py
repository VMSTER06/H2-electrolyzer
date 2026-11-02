import base64
import json
import sys
import time
import urllib.request
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent / 'python_deps'))
from websockets.sync.client import connect

root = Path(__file__).parent.parent
out = Path(__file__).parent / 'android_redesign_2026-10-15'
out.mkdir(exist_ok=True)
targets = json.load(urllib.request.urlopen('http://127.0.0.1:9224/json'))
target = next(t for t in targets if t['type'] == 'page')
mock = r'''
window.uiErrors=[];window.addEventListener('error',e=>uiErrors.push(e.message));
window.addEventListener('unhandledrejection',e=>uiErrors.push(String(e.reason)));
window.mockCalls=[];window.mockOffline=false;window.mockReject=false;
window.mockState={p:2.74,tgt:3,h:.15,pwr:1,lck:0,prg:0,psh:0,rep:0,mgs:0,pzo:0,ele:1,val:0,rf:1,ovw:0,ovl:0,tlk:0,tcal:0,wd:0,rt:5143,ta:3,up:342,t:26.1,tok:1,oh:0,f1:0,f2:1,f2m:3,f2on:45,f2off:40,rfc:76,fsok:1,oledok:1,lcdok:1,mpuok:1,nrfok:1};
window.Brain={
status(id){setTimeout(()=>h2Response(id,mockOffline?-1:200,mockOffline?'Нет подключения':JSON.stringify(mockState)),10)},
soundConfig(id){setTimeout(()=>h2Response(id,200,JSON.stringify({out:2,vol:7,alm:1,clk:1,list:[{id:0,name:'Своя мелодия'}]})),10)},
appearance(...a){mockCalls.push(['appearance',...a])},haptic(){mockCalls.push(['haptic'])},openWifiSettings(){mockCalls.push(['wifi'])}
};
for(const method of ['command','pressure','hysteresis','fan','soundSet','soundPlay','soundStop'])Brain[method]=(id,...args)=>{mockCalls.push([method,...args]);setTimeout(()=>h2Response(id,mockReject?500:200,mockReject?'Ошибка':'OK'),10)};
'''

with connect(target['webSocketDebuggerUrl'], max_size=20_000_000) as ws:
    seq=0
    def call(method,params=None):
        global seq
        seq+=1
        ws.send(json.dumps({'id':seq,'method':method,'params':params or {}}))
        while True:
            msg=json.loads(ws.recv())
            if msg.get('id')==seq:
                if 'error' in msg: raise RuntimeError(msg['error'])
                return msg.get('result',{})
    def js(source):
        value=call('Runtime.evaluate',{'expression':source,'returnByValue':True,'awaitPromise':True})
        if 'exceptionDetails' in value: raise AssertionError(value['exceptionDetails'])
        return value['result'].get('value')
    def check(source):
        result=js(source)
        assert result, (source,result)
    def screenshot(name):
        time.sleep(.25)
        data=call('Page.captureScreenshot',{'format':'png','captureBeyondViewport':False})['data']
        (out / (name+'.png')).write_bytes(base64.b64decode(data))
    call('Page.enable')
    call('Runtime.enable')
    call('Emulation.setDeviceMetricsOverride',{'width':390,'height':844,'deviceScaleFactor':2,'mobile':True})
    call('Page.addScriptToEvaluateOnNewDocument',{'source':mock})
    call('Page.navigate',{'url':(root/'H2_Android_App/app/src/main/assets/index.html').as_uri()})
    time.sleep(1)
    js("setPreference('theme','light');clearInterval(pollTimer)")
    check("online && byId('pressure').textContent==='2,74'")
    for theme in ['light','dark']:
        js(f"setPreference('theme','{theme}')")
        for page in ['home','control','sound','diag','settings']:
            js(f"tab('{page}');window.scrollTo(0,0)")
            screenshot(theme+'_'+page)
            check('document.documentElement.scrollWidth<=window.innerWidth')
    js("byId('targetSlider').value='4.2';byId('targetSlider').dispatchEvent(new Event('input'));render(mockState)")
    check("byId('targetSlider').value==='4.2' && pressureDirty")
    js('lastAction=0;setPressure()')
    time.sleep(.1)
    check("mockCalls.some(c=>c[0]==='pressure'&&c[1]===4.2)&&!pressureDirty")
    js("selectFan(0);byId('fanOn').value=50;byId('fanOff').value=42;mockReject=true;lastAction=0;setFan()")
    time.sleep(.1)
    check('fanDirty')
    js('mockReject=false;lastAction=0;setFan()')
    time.sleep(.1)
    check("!fanDirty&&mockCalls.some(c=>c[0]==='fan'&&c[1]===0&&c[2]===50&&c[3]===42)")
    js("lastAction=0;byId('hystSlider').value='.25';hystDirty=true;setHysteresis()")
    time.sleep(.1)
    check("!hystDirty&&mockCalls.some(c=>c[0]==='hysteresis'&&c[1]===.25)")
    js("setPreference('digits',1);setPreference('awake',true);setPreference('haptics',false);setPreference('refresh',5000);openWifi()")
    check("byId('pressure').textContent==='2,7'&&mockCalls.some(c=>c[0]==='wifi')&&mockCalls.some(c=>c[0]==='appearance'&&c[2]===true)")
    js("lastAction=0;selectSound(1);byId('volumeSlider').value=6;setSound()")
    time.sleep(.1)
    check("mockCalls.some(c=>c[0]==='soundSet'&&c[1]===1&&c[2]===6)")
    js("lastAction=0;playSound(-2)")
    time.sleep(.1)
    check("mockCalls.some(c=>c[0]==='soundPlay'&&c[1]===-2)")
    js("lastAction=0;stopSound()")
    time.sleep(.1)
    check("mockCalls.some(c=>c[0]==='soundStop')")
    js("lastAction=0;sendCmd(2)")
    time.sleep(.1)
    check("mockCalls.some(c=>c[0]==='command'&&c[1]===2)")
    js("tab('control');tab('settings');handleBack()")
    check("currentTab==='control'")
    for width in [320,360,430,768]:
        call('Emulation.setDeviceMetricsOverride',{'width':width,'height':844,'deviceScaleFactor':1,'mobile':True})
        for page in ['home','control','sound','diag','settings']:
            js(f"tab('{page}')")
            check('document.documentElement.scrollWidth<=window.innerWidth')
        js("tab('home');window.scrollTo(0,0)")
        screenshot('width_'+str(width))
    call('Emulation.setDeviceMetricsOverride',{'width':390,'height':844,'deviceScaleFactor':2,'mobile':True})
    js('mockOffline=true;poll()')
    time.sleep(.2)
    check("!online&&byId('pressure').textContent==='—'&&byId('powerButton').disabled&&byId('outElectro').querySelector('.output-state').textContent==='—'")
    screenshot('dark_offline')
    check('uiErrors.length===0')
    call('Page.reload')
    time.sleep(.5)
    check("preferences.theme==='dark'&&preferences.digits===1&&preferences.awake&&preferences.refresh===5000&&!preferences.haptics")
    js("setPreference('theme','auto')")
    call('Emulation.setEmulatedMedia',{'features':[{'name':'prefers-color-scheme','value':'light'}]})
    time.sleep(.1)
    check("document.documentElement.dataset.theme==='light'")
    call('Emulation.setEmulatedMedia',{'features':[{'name':'prefers-color-scheme','value':'dark'}]})
    time.sleep(.1)
    check("document.documentElement.dataset.theme==='dark'")
    js("setPreference('theme','light');setPreference('digits',2);setPreference('awake',false);setPreference('haptics',true);setPreference('refresh',2200);tab('home')")
    check('uiErrors.length===0')
    print('PASS: responsive layouts at 320/360/390/430/768; both themes; persistence; automatic system theme; settings bridge; commands; sound; edited values; failed save; offline data clearing; no JS errors.')
    print('Screenshots: '+str(out))
