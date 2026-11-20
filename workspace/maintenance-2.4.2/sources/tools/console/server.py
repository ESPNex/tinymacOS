#!/usr/bin/env python3
"""Optional HOST display only. One disposable QEMU per same-origin WebSocket. No QMP, disk upload, command endpoint, or exposed VNC port."""
import asyncio, os, tempfile
from pathlib import Path
from urllib.parse import urlsplit
from aiohttp import web,WSMsgType

HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
NOVNC=Path(os.environ.get('NOVNC','/usr/share/novnc')).resolve()
assert (NOVNC/'core/rfb.js').is_file(),'Install noVNC 1.6 or set NOVNC to its directory'
OUT=Path(os.environ.get('TMC_BUILD',Path('/var/tmp')/f'tinymacos-{os.getuid()}'/'2.4.2'))
CD=Path(os.environ.get('ISO',OUT/'tinymacOS-2.4.2-x86_64.iso')).resolve()
# Keep HTML served to previous, already-running releases unchanged.
V=next((v for v in ('2.0','2.0.1','2.1.0') if CD.name==f'tinymacOS-{v}-x86_64.iso'),'2.4.2')
LIMIT=2; slots=asyncio.Semaphore(LIMIT)
async def cdrom(_):
    if not CD.is_file():raise web.HTTPNotFound(text='Compila prima il CD con ./build.sh')
    return web.FileResponse(CD,headers={'Content-Type':'application/octet-stream','Content-Disposition':f'attachment; filename="tinymacOS-{V}-x86_64.iso"','Cache-Control':'no-store'})

async def recovery(_):
    img=OUT/'BaseSystem.img'
    if not img.is_file():raise web.HTTPNotFound(text='Immagine di recupero non trovata')
    return web.FileResponse(img,headers={'Content-Type':'application/octet-stream','Content-Disposition':'attachment; filename="BaseSystem.img"','Cache-Control':'no-store'})
async def index(_):
    html=(HERE/('integrata.html' if V in ('2.1.0','2.2.0','2.3.0','2.4.2') else 'riviera.html')).read_text()
    if V=='2.0.1':
        html=(html.replace('tinymacOS 2.0','tinymacOS 2.0.1').replace('Release 2.0','Release 2.0.1').replace('20A1','20A2').replace('tinymacOS-2.0-x86','tinymacOS-2.0.1-x86')
              .replace('La VM è temporanea e senza rete.','Il puntatore USB assoluto segue il mouse. La VM è temporanea e senza rete.'))
    return web.Response(text=html,content_type='text/html',headers={'Cache-Control':'no-store'})
async def module(_):return web.FileResponse(HERE/('integrata.js' if V in ('2.1.0','2.2.0','2.3.0','2.4.2') else 'riviera.js'),headers={'Content-Type':'text/javascript'})
async def cover(_):return web.FileResponse(ROOT/'assets/riviera-master.jpg')
async def display(req):
    mode=req.query.get('boot','live')
    if mode not in (('live',) if V in ('2.1.0','2.2.0','2.3.0','2.4.2') else ('live','recovery')):raise web.HTTPBadRequest(text='Avvio non riconosciuto')
    # Same-origin display. A VM belongs to one socket, never shared by visitors.
    origin=req.headers.get('Origin','');host=urlsplit(origin).netloc.lower()
    forwarded=req.headers.get('X-Forwarded-Host','').split(',')[0].strip().lower()
    preview=os.environ.get('E2B_SANDBOX_ID','');preview_host=f"{os.environ.get('PORT','8080')}-{preview}.e2b.app" if preview else None
    # HTTPS proxy may forward only its internal Host. Prefer Sec-Fetch-Site for
    # browser checks; omitted headers are accepted only for an exact origin/host.
    if not host or req.headers.get('Sec-Fetch-Site','same-origin') not in ('same-origin','none') or not (host in (req.host.lower(),forwarded,preview_host)):raise web.HTTPForbidden()
    ws=web.WebSocketResponse(protocols=('binary',),max_msg_size=2<<20,heartbeat=30,compress=False);await ws.prepare(req)
    if slots.locked():await ws.close(code=1013,message=b'Console occupata');return ws
    async with slots:
        with tempfile.TemporaryDirectory(prefix='riviera-display-') as tmp:
            path=Path(tmp)/'screen';env={**os.environ,'STATE':tmp,'LIVE':'0','ISO':str(CD),'BOOT_DISK':'0'};q=None;w=None;tasks=[]
            for key in ('DISK','LEGACY_DATA','RECOVERY_IMG'):env.pop(key,None)
            if mode=='recovery':env.update(RECOVERY_IMG=str(OUT/'BaseSystem.img'),BOOT_DISK='1',LIVE='1')
            try:
                q=await asyncio.create_subprocess_exec(str(ROOT/'tools/run-qemu.sh'),'-display','none','-vnc','unix:'+str(path),'-serial','none','-monitor','none',cwd=str(ROOT),env=env,stdin=asyncio.subprocess.DEVNULL,stdout=asyncio.subprocess.DEVNULL,stderr=asyncio.subprocess.DEVNULL)
                async with asyncio.timeout(15):
                    while not path.exists():
                        if q.returncode is not None:raise RuntimeError('VM startup failed')
                        await asyncio.sleep(.025)
                    r,w=await asyncio.open_unix_connection(str(path))
                async def video():
                    while data:=await r.read(65536):await ws.send_bytes(data)
                async def input_and_protocol():
                    async for m in ws:
                        if m.type!=WSMsgType.BINARY:break
                        w.write(m.data);await w.drain()
                tasks=[asyncio.create_task(x) for x in (video(),input_and_protocol(),q.wait())];await asyncio.wait(tasks,timeout=1200,return_when=asyncio.FIRST_COMPLETED)
            except (TimeoutError,ConnectionError,RuntimeError,FileNotFoundError):pass
            finally:
                for t in tasks:t.cancel()
                if tasks:await asyncio.gather(*tasks,return_exceptions=True)
                if w:w.close()
                if q and q.returncode is None:
                    try:q.terminate();await asyncio.wait_for(q.wait(),4)
                    except ProcessLookupError:pass
                    except TimeoutError:q.kill();await q.wait()
                if not ws.closed:await ws.close()
    return ws
app=web.Application(client_max_size=2<<20);app.router.add_get('/',index);app.router.add_get('/console.js',module);app.router.add_get('/costa.jpg',cover);app.router.add_get('/session',display);app.router.add_get('/cd-di-avvio',cdrom)
if V not in ('2.1.0','2.2.0','2.3.0','2.4.2'):app.router.add_get('/base-system',recovery)
for name in ('core','vendor'):app.router.add_static('/novnc/'+name,NOVNC/name,show_index=False,follow_symlinks=False)
web.run_app(app,host=os.environ.get('BIND','127.0.0.1'),port=int(os.environ.get('PORT','8080')),access_log=None)
