"""2.4.5 black-box acceptance: QMP -> emulated USB -> native rendered pixels.
Run: python tests/levante_pointer.py NEW_EXTERNAL_DIR --firmware uefi|bios
Installed locked login has a separate levante_installed_pointer.py test.
These QEMU tests are not physical-hardware verification.
"""
import argparse, hashlib, json, os, shutil, time
from pathlib import Path
import numpy as np
from guest import ROOT
from retail_guest import Retail
from boot_recovery import RecoveryKey

ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('--skip-recovery',action='store_true');ap.add_argument('--firmware',choices=['uefi','bios'],default='uefi');ap.add_argument('out',type=Path);ap.add_argument('--profile',choices=['both','tablet','mouse'],default='both');args=ap.parse_args()
out=args.out.resolve();assert not out.is_relative_to('/home/user')
out.mkdir(parents=True,exist_ok=False);ev=out/'evidence';ev.mkdir()
media=Path(os.environ['TMC_BUILD']);iso=Path(os.environ.get('TMC_TEST_ISO',str(media/'tinymacOS-2.4.5-x86_64.iso')))
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
        if args.firmware=='bios':kw['launcher']=ROOT/'tools/run-bios-iso.sh'
        super().__init__(*a,**kw)
    def button(self,key,down):
        self.req('input-send-event',events=[{'type':'btn','data':{'button':key,'down':down}}]);time.sleep(.105)
    def mouse(self,down):self.button('left',down)
    def devices(self,profile):
        report=self.req('query-mice');usb=self.hmp('info usb');want=profile=='tablet'
        active=[p for p in report if p['current']]
        assert len(active)==1 and active[0]['absolute']==want and 'HID' in active[0]['name'],report
        assert all(p['current'] or (args.firmware=='bios' and p['name']=='QEMU PS/2 Mouse') for p in report),report
        assert ('QEMU USB Tablet' if want else 'QEMU USB Mouse') in usb and ('QEMU USB Mouse' if want else 'QEMU USB Tablet') not in usb and 'QEMU USB Keyboard' in usb
        records['devices'][self.tag]={'query-mice':report,'info usb':usb.strip().splitlines(),'framebuffer':self.snap().size}
class Rec(RecoveryKey,Probe):
    initial='Scegli come proseguire.'
    def wait(self,*a,**kw):
        if args.firmware=='uefi':return RecoveryKey.wait(self,*a,**kw)
        # The BIOS media deliberately enables verbose: there is no Apple-logo
        # frame to wait for. Send the real Recovery key across the boot window.
        from pixels import locate
        end=time.monotonic()+95
        while time.monotonic()<end:
            self.key('r',.06,hold=.04)
            if locate(self.snap(),self.initial):return
        raise AssertionError('BIOS verbose Recovery key did not reach menu')
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
    # Exercise the launcher default for tablet, and a separately booted mouse-only VM.
    if profile=='tablet':os.environ['TMC_POINTER']='tablet'
    else:os.environ['TMC_POINTER']='mouse'
    os.environ.pop('DISK',None);os.environ.pop('RECOVERY_IMG',None)
    with Probe(iso,out,ev,profile+'-live',disk=profile+'-vars',live=True) as g:
        g.devices(profile);w,h=g.snap().size;assert (w,h)==(1280,800)
        points=[(0,0),(w-1,h-1),(w-1,0),(0,h-1),(639,398),(65,731),(1217,82),(639,398)] if profile=='tablet' else [(762,482),(142,613)]
        for x,y in points:cursor(g,x,y)
        ok(f'{profile} / ISO: unico puntatore USB attivo. Cursore nativo confrontato con lo sprite di distribuzione su {len(points)} posizioni del setup: {points}')
        g.key('f11');g.visible('Input nativo');g.visible('Puntatori configurati: 1',faces=(5,));g.move(100,110);time.sleep(1.2);g.shot(profile+'-native-diagnostics');g.key('f11')
        ok(f'{profile} / login: pannello F11 nativo accessibile prima dell’account, un puntatore configurato, movimento con pannello aperto')
        enroll_by_click(g);ok(f'{profile} / account: focus sui tre campi e pulsante di creazione azionati con clic; accesso alla scrivania senza Tab né Invio')
        cursor(g,1003,361);cursor(g,308,115)
        x,y=g.visible('Preferiti',faces=(0,));g.move(x+200,y-40);g.mouse(True);g.move(x+287,y+1);g.mouse(False);g.move(1050,605)
        assert g.visible('Preferiti',faces=(0,))==(x+87,y+41),(x,y,g.visible('Preferiti',faces=(0,)))
        ok(f'{profile} / scrivania: cursore verificato in due punti; trascinamento Finder di (+87,+41) pixel esatti e rilascio: la finestra resta ferma mentre il puntatore si sposta')
        g.move(1005,502);g.button('right',True);g.button('right',False);g.visible('Apri Scrivania in Finder');g.shot(profile+'-menu');g.key('esc',.4)
        if profile=='tablet':
            g.move(1205,250)
            for down in (True,False,True,False):
                g.req('input-send-event',events=[{'type':'btn','data':{'button':'left','down':down}}]);time.sleep(.055)
            g.visible('Introduzione');g.shot('tablet-installer');g.key('ctrl-w',.5)
            g.key('f2');g.visible('Sessione locale');x,y=g.visible('su console',faces=(5,));g.move(x+18,y+47);g.button('wheel-up',True);g.button('wheel-up',False);g.visible('Cronologia',6,faces=(0,));g.button('wheel-down',True);g.button('wheel-down',False);g.visible('Sessione locale',6,faces=(0,));ok('tablet / app: menu contestuale da clic destro; doppio clic sull’icona apre l’installer; rotella su/giù attiva e chiude la cronologia di Terminale')
        else:ok('mouse / scrivania: il clic destro apre il menu contestuale con il solo usb-mouse')
    if args.skip_recovery:
        records['excluded']=['Optical Recovery key selection; separately tested installed local Recovery']
        continue
    with Rec(iso,out,ev,profile+'-recovery',disk=profile+'-rec-vars',live=True) as g:
        g.devices(profile);assert any(str(iso) in str(b) for b in g.req('query-block')) and 'USB MSD' not in g.hmp('info usb');g.mx,g.my=1280-48,800-55
        if profile=='tablet':
            for x,y in [(1279,799),(0,0),(350,489)]:cursor(g,x,y,True)
        help_click(g,'Terminale');help_click(g,'Continua');g.visible('Shell di recupero locale',8);g.cmd('sw_vers','tinymacOS Recovery 2.4.5');g.shot(profile+'-rec-terminale');ok(f'{profile} / Recupero autonomo: selezione di Terminale e Continua con due clic, solo ISO, nessun disco o immagine esterna, versione 2.4.5 letta sulla shell firmware di Recupero')

records['passed']=True
(ev/'pointer.json').write_text(json.dumps(records,indent=2,ensure_ascii=False))
