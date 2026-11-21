"""10 black-box acceptance: QMP -> emulated USB -> native rendered pixels.
Run: python tests/levante_pointer.py NEW_EXTERNAL_DIR --firmware uefi|bios
Installed locked login has a separate levante_installed_pointer.py test.
These QEMU tests are not physical-hardware verification.
"""
import argparse, hashlib, json, os, shutil, time
from pathlib import Path
import numpy as np
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tinymacOS/tests'))
from guest import ROOT
from retail_guest import Retail
from boot_recovery import RecoveryKey

ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('--skip-recovery',action='store_true');ap.add_argument('--firmware',choices=['uefi','bios'],default='uefi');ap.add_argument('out',type=Path);ap.add_argument('--profile',choices=['both','tablet','mouse'],default='both');args=ap.parse_args()
out=args.out.resolve();assert not out.is_relative_to('/home/user')
out.mkdir(parents=True,exist_ok=False);ev=out/'evidence';ev.mkdir()
media=Path(os.environ['TMC_BUILD']);iso=Path(os.environ.get('TMC_TEST_ISO',str(media/'tinymacOS-10-x86_64.iso')))
os.environ['TMC_BIOS_ISO']=str(iso)
def digest(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
records={'iso_sha256':digest(iso),'devices':{},'checks':[],'firmware':args.firmware,'physical_hardware_tested':False}
sprite=np.frombuffer((ROOT/'assets/symbols.rgba').read_bytes(),dtype=np.uint8).reshape(-1,64,64,4)[15]
def ok(s):
    print('OK:',s,flush=True);records['checks'].append(s);(ev/'pointer.json').write_text(json.dumps(records,indent=2,ensure_ascii=False))
class Probe(Retail):
    initial='Crea un account'
    def __init__(self,*a,**kw):
        if args.firmware=='bios':kw['launcher']=Path(os.environ.get('TMC_OPTICAL_BIOS_LAUNCHER',str(ROOT/'tools/run-bios-iso.sh')))
        super().__init__(*a,**kw)
    def button(self,key,down):
        self.req('input-send-event',events=[{'type':'btn','data':{'button':key,'down':down}}]);time.sleep(.105)
    def mouse(self,down):self.button('left',down)
    def devices(self,profile):
        report=self.req('query-mice');usb=self.hmp('info usb');want=profile=='tablet'
        active=[p for p in report if p['current']]
        assert len(active)==1 and active[0]['absolute']==want and 'HID' in active[0]['name'],report
        assert all(p['current'] or (args.firmware=='bios' and p['name']=='QEMU PS/2 Mouse') for p in report),report
        assert ('QEMU USB Tablet' if want else 'QEMU USB Mouse') in usb and ('QEMU USB Mouse' if want else 'QEMU USB Tablet') not in usb and ('QEMU USB Keyboard' in usb or (args.firmware=='bios' and os.environ.get('TMC_OPTICAL_BIOS_LAUNCHER')))
        records['devices'][self.tag]={'query-mice':report,'info usb':usb.strip().splitlines(),'framebuffer':self.snap().size}
class Rec(RecoveryKey,Probe):
    initial='Scegli come proseguire.'
    def wait(self,*a,**kw):
        if args.firmware=='uefi':return RecoveryKey.wait(self,*a,**kw)
        from pixels import locate
        end=time.monotonic()+100
        while time.monotonic()<end:
            self.key('r',.08,hold=.05)
            im=self.snap()
            # Full-frame template search across nine font faces can exceed the
            # loader's key window. Cheap frame fingerprint first, text after.
            if im.size==(1280,800) and im.getpixel((0,50))==(233,234,240) and im.getpixel((10,200))==(233,234,240):
                if locate(im,self.initial):return
        raise AssertionError('BIOS optical Recovery key not received')
class Locked(Probe):initial='Elena'

def cursor(g,x,y,recovery=False):
    # Predict the actual arrow from the released atlas and the underlying pixels.
    # This includes partial sprites at all four screen corners. No guest state is read.
    n=23 if recovery else 26; g.move(70,75); bg=np.asarray(g.snap(),dtype=np.int32);h,w=bg.shape[:2]
    ix=np.arange(n)*64//n;rgba=sprite[ix[:,None],ix].astype(np.int32)
    xx=x-3;yy=y-2;u=max(xx,0);v=max(yy,0);r=min(w,xx+n);b=min(h,yy+n)
    region=bg[v:b,u:r];p=rgba[v-yy:b-yy,u-xx:r-xx];a=p[:,:,3:4];mask=a[:,:,0]>=200;assert mask.any()
    if recovery:expect=(region*(255-a)+p[:,:,:3]*a+127)//255
    else:expect=np.where(a==255,p[:,:,:3],(region*(256-a)+p[:,:,:3]*a)//256)
    g.move(x,y);deadline=time.monotonic()+3
    while True:
        actual=np.asarray(g.snap(),dtype=np.int32)[v:b,u:r];m=np.max(np.abs(actual-expect),axis=2)
        # At the extreme bottom-right only the dark tip is on screen. On this
        # wallpaper its true contrast is four levels, so require its exact pixel
        # instead of imposing an arbitrary minimum brightness change.
        if mask.sum()==1:
            if np.array_equal(actual[mask],expect[mask]) and np.any(actual[mask]!=region[mask]):return
        elif np.max(m[mask])<=2 and np.max(np.abs(actual-region)[mask])>=3:return
        if time.monotonic()>=deadline:raise AssertionError((g.tag,(x,y),'cursor mismatch',int(m[mask].max()),mask.sum()))
        time.sleep(.05)

def enroll_by_click(g):
    for y,t in [(448,'Ada'),(489,'local-test-285'),(530,'local-test-285')]:g.click(641,y);g.type(t)
    g.click(640,575);g.visible('Preferiti',40);time.sleep(.7)

def help_click(g,word):
    x,y=g.visible(word);g.click(x+6,y+5)

for profile in (['tablet','mouse'] if args.profile=='both' else [args.profile]):
    os.environ['TMC_POINTER']=profile
    with Rec(iso,out,ev,profile+'-recovery',disk=profile+'-rec-vars',live=True) as g:
        g.devices(profile);assert any(str(iso) in str(b) for b in g.req('query-block')) and 'USB MSD' not in g.hmp('info usb');g.mx,g.my=1280-48,800-55
        if profile=='tablet':
            for x,y in [(1279,799),(0,0),(350,489)]:cursor(g,x,y,True)
        help_click(g,'Terminale');help_click(g,'Continua');g.visible('Shell di recupero locale',8);g.cmd('sw_vers','tinymacOS Recovery 10');g.shot(profile+'-rec-terminale');ok(f'{profile} / Recupero autonomo: selezione di Terminale e Continua con due clic, solo ISO, nessun disco o immagine esterna, versione 10 letta sulla shell firmware di Recupero')

records['passed']=True
(ev/'pointer.json').write_text(json.dumps(records,indent=2,ensure_ascii=False))

(Path(__file__).parent/('recovery-pointer-'+args.firmware+'.json')).write_text(json.dumps(records,indent=2,ensure_ascii=False)+'\n')
