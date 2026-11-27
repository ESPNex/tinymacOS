"""M1 acceptance: USB keys/mouse and rendered pixels; no guest-memory writes.
All credentials are disposable QA data. Long document inputs are appended
offline AFTER GUI installation; actual edit/save/readback is native guest IO.
"""
from pathlib import Path
import hashlib,json,os,tempfile,time,struct,subprocess
import pixels
import numpy as np
from retail_guest import Retail
from guest import KEYS
from boot_recovery import RecoveryKey
iso=Path(os.environ['TMC_STAGE_ISO']);qa=Path(os.environ['TMC_QA_DIR']);qa.mkdir(parents=True,exist_ok=True)
resume=json.loads(Path(os.environ['TMC_M1_RESUME_RESULT']).read_text()) if os.environ.get('TMC_M1_RESUME_RESULT') else None
w=Path(resume['work']) if resume else Path(tempfile.mkdtemp(prefix='tmc-m1-acceptance-',dir='/var/tmp'));assert str(w).startswith('/var/tmp/tmc-m1-acceptance-');ev=w/'evidence';ev.mkdir(exist_ok=True)
os.environ.update(TMC_MEM='512',TMC_SMP='2',TMC_POINTER='tablet',TMC_STORAGE='nvme')
r={'passed':False,'release_ready':False,'physical_hardware_tested':False,'work':str(w),'checks':[],
   'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest()}
if resume:
    assert resume['iso_sha256']==r['iso_sha256'] and resume['host_prepared_long_document_inputs_only']
    r.update(checks=resume['checks'],markers=resume['markers'],host_prepared_long_document_inputs_only=True,resumed_from=str(os.environ['TMC_M1_RESUME_RESULT']))
class QA(Retail):
    def type(self,text):
        for c in text:self.key(KEYS.get(c,('shift-'+c.lower() if c.isupper() else c)),dt=.055,hold=.025)
class Recovery(RecoveryKey,QA):recovery_text='Ripristina da una copia locale'
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
def patch(g,box):
    x,y,ww,hh=box;return np.asarray(g.snap())[y:y+hh,x:x+ww].copy()
def marker(g,label):
    time.sleep(.8);previous=np.asarray(g.snap());end=time.monotonic()+7
    while time.monotonic()<end:
        time.sleep(.09);current=np.asarray(g.snap());mask=np.any(previous[100:700]!=current[100:700],axis=2)
        if mask.any():
            yy,xx=np.nonzero(mask);box=(int(xx.min()),int(yy.min()+100),int(xx.max()-xx.min()+1),int(yy.max()-yy.min()+1))
            if box[2] in (1,2) and 15<=box[3]<=17:
                # All calls start with an empty field or a fixed no-echo prompt.
                g.key('left',dt=.2);a=patch(g,box);assert np.all(a==a[0,0]),(label,'not a solid insertion marker',box)
                adjacent=patch(g,(box[0]+box[2]+2,box[1],1,1))[0,0];assert not np.array_equal(a[0,0],adjacent),(label,'caret did not reset on arrow',box)
                r.setdefault('markers',{})[label]=list(box)
                return box,tuple(map(int,a[0,0]))
        previous=current
    raise AssertionError((label,'no bounded caret blink observed'))
def visible_marker(g,box,color,label):
    deadline=time.monotonic()+.30
    while True:
        a=patch(g,box)
        if a.shape[:2]==(box[3],box[2]) and np.all(a==color):return
        if time.monotonic()>=deadline:
            g.shot('caret-failure');raise AssertionError((label,'caret not visible within 300ms after input',box,color,a[0,0].tolist()))
        time.sleep(.015)
def body_advance(c):
    face=1;p=pixels.start+face*(4+pixels.ng*8);return struct.unpack_from('<H',pixels.b,p+4+8*pixels.codes[ord(c)])[0]
def typing(g,text,box,color,label,hidden=False,advance=8):
    x,y,ww,hh=box
    for c in text:
        g.key(KEYS.get(c,('shift-'+c.lower() if c.isupper() else c)),dt=.065,hold=.025)
        if not hidden:x+=advance(c) if callable(advance) else advance
        visible_marker(g,(x,y,ww,hh),color,label)
    return (x,y,ww,hh)
def edit_restore(g,box,color,label):
    x,y,ww,hh=box
    for key,delta in [('left',-8),('right',8),('backspace',-8)]:
        g.key(key,dt=.075);x+=delta;visible_marker(g,(x,y,ww,hh),color,label+' '+key)
    return (x,y,ww,hh)
def text_origin(g,text,face=0):
    px,py=g.visible(text,faces=(face,));p=pixels.start+face*(4+pixels.ng*8)
    h,=struct.unpack_from('<I',pixels.b,p);at=0;xs=[];ys=[]
    for c in text:
        adv,w,off=struct.unpack_from('<HHI',pixels.b,p+4+8*pixels.codes[ord(c)])
        a=np.frombuffer(pixels.b,np.uint8,count=h*w,offset=pixels.pix+off).reshape(h,w)
        yy,xx=np.nonzero(a)
        xs.extend((xx+at).tolist());ys.extend(yy.tolist());at+=adv
    return px-max(0,min(xs)-1),py-max(0,min(ys)-1)
def terminal(g):g.key('f2');g.move(65,700);g.visible('Sessione locale')
def menu(g,title,item):
    x,y=g.visible(title,box=(0,0,1100,34));g.click(x+12,y+8);x,y=g.visible(item,box=(0,34,1250,320));g.click(x+20,y+8)
try:
    if not resume:
        with QA(iso,w,ev,'m1-complete-live',disk='target',live=False) as g:
            box,col=marker(g,'setup-name-empty');typing(g,'M1Test',box,col,'setup-name',advance=body_advance)
            g.key('tab');box,col=marker(g,'setup-password-empty');typing(g,'local-test-285',box,col,'setup-password-masked',advance=9)
            g.key('tab');box,col=marker(g,'setup-confirm-empty');typing(g,'local-test-285',box,col,'setup-confirm-masked',advance=9)
            g.key('ret');g.visible('Preferiti',40);ok('Setup empty/name/password/confirmation: real Tab focus, continuous per-key visible caret, masked password fields')
            terminal(g);box,col=marker(g,'Terminal-normal-empty');end=typing(g,'echo QATEST123',box,col,'Terminal-normal-continuous');edit_restore(g,end,col,'Terminal-normal');g.key('ctrl-u');ok('Terminal normal prompt: empty/continuous typing/arrows/backspace caret')
            g.cmd('apfs_unlockfv');g.visible('Senza eco');box,col=marker(g,'Terminal-no-echo')
            before=patch(g,(box[0]-165,box[1],195,box[3]))
            typing(g,'PrivateQAè123',box,col,'no-echo',hidden=True)
            g.key('backspace');visible_marker(g,box,col,'no-echo backspace');g.key('left');visible_marker(g,box,col,'no-echo left')
            assert np.array_equal(before,patch(g,(box[0]-165,box[1],195,box[3])))
            g.key('esc');g.visible('Sessione locale');g.cmd('diskutil apfs unlockVolume');g.visible('Senza eco');g.type('CanaryOnly');g.key('ret');g.visible('Sblocco non riuscito');g.cmd('history');
            g.shot('terminal-no-echo-returned');ok('Terminal real no-echo aliases: invariant prompt pixels/cursor for typing/deletion/movement, Escape and backend failure return safely')
            g.cmd('open -a TextEdit');g.visible('Testo semplice');box,col=marker(g,'TextEdit-empty')
            end=typing(g,'àè0123456789abcdefghij',box,col,'TextEdit-continuous');edit_restore(g,end,col,'TextEdit');ok('TextEdit empty and continuous UTF8 typing/arrows/backspace caret')
            g.key('ctrl-w');g.visible('Chiudere senza salvare?');g.key('ret');terminal(g)
            for app,title,label in [('Note','Tutte le note','Note'),('Memo','Tutti i memo','Memo')]:
                g.cmd('open -a '+app);g.visible(title);box,col=marker(g,label+'-empty')
                end=typing(g,'àèabcdefghij01234',box,col,label+'-continuous');end=edit_restore(g,end,col,label)
                # A system menu suppresses the editing caret without a client redraw.
                g.click(23,16);time.sleep(.3);stable=patch(g,end);assert not np.all(stable==col)
                for _ in range(8):time.sleep(.15);assert np.array_equal(stable,patch(g,end))
                g.key('esc');ok(label+': continuous typing, UTF8 movement/deletion and caret suppression behind menu')
                terminal(g)
            # Installer password fields: inspect/cancel before any destination write.
            g.key('f11');g.visible('Installa tinymacOS Tramontana');g.key('ret');g.visible('Seleziona la destinazione');g.key('down');g.key('ret');g.visible('Conferma l’installazione');g.key('e');g.visible('Proteggi la nuova installazione')
            box,col=marker(g,'installer-password-empty');typing(g,'volume-test',box,col,'installer-password-masked');g.key('tab');box,col=marker(g,'installer-confirm-empty');typing(g,'volume-test',box,col,'installer-confirm-masked')
            g.key('esc');g.visible('Conferma l’installazione');g.key('esc');g.key('esc');g.key('esc');terminal(g)
            ok('Installer: empty/masked password and Tab confirmation per-key visible caret; cancel discards enrollment before installation')
            # Install normally before offline INPUT fixture preparation.
            g.key('f11');g.visible('Installa tinymacOS Tramontana');g.key('ret');g.visible('Seleziona la destinazione');g.key('down');g.key('ret');g.visible('Conferma l’installazione');g.key('spc');g.key('ret');g.visible('Il disco è pronto',300);g.key('ret');g.p.wait(timeout=20)
        disk=w/'target/system.raw'
        with disk.open('rb') as f:
            f.seek(1024+128+32);a,b=struct.unpack('<QQ',f.read(16))
        root=Path(__file__).resolve().parents[1];exe=w/'append-utf8'
        subprocess.run(['gcc','-O2','-fno-builtin','-I'+str(root/'include'),str(root/'tests/m1_utf8_fixture.c'),str(root/'storage/tmcbfs2.c'),str(root/'crypto/sha256.c'),str(root/'kernel/string.c'),'-o',str(exe)],check=True)
        subprocess.run([str(exe),str(disk),str(a),str(b-a+1)],check=True)
        r['host_prepared_long_document_inputs_only']=True
    class Installed(QA):initial='M1Test'
    with Installed('/not-attached',w,ev,'m1-installed-roundtrip',disk='target',boot_disk=True) as g:
        g.unlock();terminal(g)
        for newline,name in [(False,'raw'),(True,'lines')]:
            path='Documents/m1-'+name+'.txt';expected=b''.join(('\n' if newline and i%53==0 else 'à€Z').encode() for i in range(24000))
            digest=hashlib.sha256(expected).hexdigest();g.cmd('shasum -a 256 '+path,digest)
            g.cmd('open '+path);g.visible('Documento aperto')
            tx,ty=text_origin(g,'Testo semplice');X=tx-(758-117);Y=ty-16
            g.click(X+45,Y+78);g.key('home');g.key('delete');g.type('\n' if newline else 'à');g.key('end');g.type('Z');g.key('backspace');g.key('home');g.type('è');g.key('backspace')
            g.move(X+280,Y+130);g.req('input-send-event',events=[{'type':'btn','data':{'down':True,'button':'wheel-down'}},{'type':'btn','data':{'down':False,'button':'wheel-down'}}]);time.sleep(.3)
            g.drag(X+752,Y+476,-160,-80);time.sleep(.5);tx2,ty2=text_origin(g,'Testo semplice');assert tx2==tx-160,(tx,tx2,'native resize did not occur');g.key('home');g.type('à');g.key('backspace');g.key('ctrl-s');g.visible('Salvato su disco')
            g.key('ctrl-w');terminal(g);g.cmd('shasum -a 256 '+path,digest)
            g.cmd('open '+path);g.visible('Documento aperto');g.key('ctrl-s');g.visible('Salvato su disco');g.key('ctrl-w');terminal(g);g.cmd('shasum -a 256 '+path,digest)
            ok('Native guest TextEdit '+name+': '+str(len(expected))+' UTF8 bytes, SHA256 exact after click/scroll/resize/Home-End/insert/backspace/save/close/reopen/save')
        g.cmd('off');g.p.wait(timeout=20)
    with Recovery('/not-attached',w,ev,'m1-complete-recovery',disk='target',boot_disk=True) as g:
        menu(g,'Utility','Terminale');g.visible('Sessione locale');g.cmd('fdesetup manage');g.visible('FileVault');g.visible('Attiva FileVault');x,y=g.visible('Attiva FileVault');g.click(x+40,y+8);g.visible('Attivazione FileVault')
        box,col=marker(g,'recovery-password-empty');typing(g,'recoveryQA123',box,col,'Recovery-password-masked');g.key('tab');box,col=marker(g,'recovery-confirm-empty');typing(g,'recoveryQA123',box,col,'Recovery-confirm-masked')
        g.key('esc');g.shot('recovery-masked-fields-cancelled');ok('Installed native Recovery, ISO detached: FileVault empty/typed masked fields and Tab caret verified, activation cancelled')
    r['passed']=True
except Exception as e:r['failure']=str(e);raise
finally:
    (qa/'m1-acceptance-result.json').write_text(json.dumps(r,indent=2)+'\n')
    for p in w.glob('*.ppm'):p.unlink()
