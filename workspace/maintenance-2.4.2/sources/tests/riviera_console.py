"""Optional host console smoke test: check HTTP and read the actual QEMU RFB framebuffer.
Reads only the requested local display; does not save media, type passwords or inspect guest memory."""
import asyncio,hashlib,json,struct,sys,time
from pathlib import Path
import aiohttp
from PIL import Image
from pixels import locate

base=sys.argv[1].rstrip('/'); expected=Path(sys.argv[2]).resolve();ev=Path(sys.argv[3]);ev.mkdir(parents=True,exist_ok=True);checks=[]
async def watch(cl,mode):
    u=base+'/session'
    initial='Scegli come proseguire.' if mode=='recovery' else 'Crea un account'
    async with cl.ws_connect(u,origin=base,protocols=('binary',),max_msg_size=16<<20) as ws:
        buffer=bytearray()
        async def read(n):
            while len(buffer)<n:
                m=await ws.receive(); assert m.type==aiohttp.WSMsgType.BINARY,m.type;buffer.extend(m.data)
            b=bytes(buffer[:n]);del buffer[:n];return b
        version=await read(12);assert version==b'RFB 003.008\n';await ws.send_bytes(version)
        n=(await read(1))[0];assert 1 in await read(n);await ws.send_bytes(b'\x01');assert await read(4)==bytes(4);await ws.send_bytes(b'\0')
        init=await read(24);w,h=struct.unpack_from('>HH',init);await read(struct.unpack_from('>I',init,20)[0]);img=Image.new('RGB',(w,h))
        await ws.send_bytes(bytes(4)+struct.pack('>BBBBHHHBBBxxx',32,24,0,1,255,255,255,16,8,0));await ws.send_bytes(struct.pack('>BBHii',2,0,2,-223,0))
        async def click(x,y):
            # RFB PointerEvent with framebuffer coordinates and button transitions.
            for mask in (0,1,0):
                await ws.send_bytes(struct.pack('>BBHH',5,mask,x,y));await asyncio.sleep(.12)
        phase=0;pressed=False
        while True:
            w,h=img.size;await ws.send_bytes(struct.pack('>BBHHHH',3,0,0,0,w,h));typ=(await read(1))[0];assert typ==0,typ
            _,n=struct.unpack('>BH',await read(3))
            for _ in range(n):
                x,y,w,h,enc=struct.unpack('>HHHHi',await read(12));assert w*h<=4_000_000
                if enc==-223:img=Image.new('RGB',(w,h));continue
                assert enc==0;img.paste(Image.frombytes('RGB',(w,h),await read(4*w*h),'raw','BGRX'),(x,y))
            if img.size==(1280,800):
                if mode=='recovery' and not pressed and img.getpixel((640,378))==(255,255,255) and img.getpixel((40,40))==(0,0,0) and img.getpixel((730,492))==(52,52,52):
                    pressed=True
                    for _ in range(7):
                        for down in (1,0):await ws.send_bytes(struct.pack('>BBHI',4,down,0,0x72));await asyncio.sleep(.055)
                if phase==0 and locate(img,initial):
                    img.save(ev/('console-'+mode+'.png'))
                    if mode=='live':await click(640,575)
                    else:
                        x,y=locate(img,'Terminale')[1];await click(x+6,y+5)
                        x,y=locate(img,'Continua')[1];await click(x+6,y+5)
                    phase=1
                elif phase==1 and locate(img,'Inserisci il tuo nome.' if mode=='live' else 'Shell di recupero locale'):
                    img.save(ev/('console-'+mode+'-clic.png'));break
            await asyncio.sleep(.26)
    checks.append('WebSocket/noVNC, '+mode+': '+initial+' e risposta visibile al clic assoluto RFB 3.8 sul controllo del guest');print(checks[-1],flush=True)
async def main():
    async with aiohttp.ClientSession() as cl:
        async with cl.get(base+'/') as res:
            body=await res.text();assert res.status==200 and 'Invia R' in body and 'Riviera' in body and 'base-system' not in body
        async with cl.get(base+'/base-system') as res:assert res.status==404
        checks.append('La console offre un unico download ISO: nessun link o endpoint BaseSystem separato')
        for url,fn in [('cd-di-avvio','tinymacOS-2.4.2-x86_64.iso')]:
            h=hashlib.sha256();n=0
            async with cl.get(base+'/'+url) as res:
                assert res.status==200
                async for part in res.content.iter_chunked(262144):n+=len(part);h.update(part)
            with (expected/fn).open('rb') as f: assert hashlib.file_digest(f,'sha256').digest()==h.digest()
            checks.append(f'Download console {fn}: {n} byte, SHA-256 {h.hexdigest()}');print(checks[-1],flush=True)
        for mode in ('live','recovery'):
            async with asyncio.timeout(70):await watch(cl,mode)
    (ev/'console.json').write_text(json.dumps(checks,indent=2,ensure_ascii=False))
asyncio.run(main())
