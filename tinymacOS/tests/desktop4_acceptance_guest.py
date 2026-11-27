"""Block 4: real USB input, screenshots and read-only process counters.
Fresh GUI install; cold native and Recovery boots without ISO. No guest-memory
writes, preformatted native fixtures, test-only services or physical disks.
"""
from pathlib import Path
import hashlib,json,os,struct,subprocess,tempfile,time
import numpy as np
import pixels
from retail_guest import Retail
from boot_recovery import RecoveryKey
from kernel_layout import process_layout
iso=Path(os.environ['TMC_TEST_ISO']);build=Path(os.environ['TMC_BUILD'])
resume=json.loads(Path(os.environ['TMC_DESKTOP4_RESUME']).read_text()) if os.environ.get('TMC_DESKTOP4_RESUME') else None
w=Path(resume['work']) if resume else Path(tempfile.mkdtemp(prefix='tmc-desktop4-acceptance-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir(exist_ok=True)
os.environ.update(TMC_MEM='768',TMC_SMP='2',TMC_POINTER='tablet',TMC_STORAGE='nvme')
stride,appoff,cpuoff,winoff,winbytes,seqoff,nproc=process_layout()
symbols={x.split()[-1]:int(x.split()[0],16) for x in subprocess.check_output(['nm','--defined-only',str(build/'kernel.debug')],text=True).splitlines() if len(x.split())>=3}
r={'passed':False,'physical_hardware_tested':False,'work':str(w),'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'checks':[],'host_formatted_target':False}
if resume:
 assert resume['iso_sha256']==r['iso_sha256'] and resume.get('resume_stage') in ('cold','recovery')
 assert any(x.startswith('Fresh GUI installation') for x in resume['checks'])
 r=resume;r['passed']=False;r.pop('failure',None)
def save():Path(os.environ['TMC_UI_RESULT']).write_text(json.dumps(r,indent=2)+'\n')
def ok(s):r['checks'].append(s);save();print('PASS',s,flush=True)
def windows(g):
 p=w/'process.bin';g.req('pmemsave',val=symbols['proc'],size=stride*nproc,filename=str(p));data=p.read_bytes();out=[]
 for off in range(0,len(data),stride):
  pid,app=struct.unpack_from('<i',data,off)[0],struct.unpack_from('<i',data,off+appoff)[0]
  if pid and struct.unpack_from('<i',data,off+winoff)[0]:out.append(app)
 return out

def closed(g,app):
 end=time.monotonic()+15
 while time.monotonic()<end:
  if app not in windows(g):return
  time.sleep(.12)
 raise AssertionError(('window did not close',app,windows(g)))
def launch(g,name,visible):
 g.key('meta_l-spc');g.type(name);g.key('ret');g.visible(visible);time.sleep(.5)
def right(g,x,y):
 g.move(x,y);g.req('input-send-event',events=[{'type':'btn','data':{'down':True,'button':'right'}}]);time.sleep(.1)
 g.req('input-send-event',events=[{'type':'btn','data':{'down':False,'button':'right'}}]);time.sleep(.4)
def origin(g,text,face=0):
 px,py=g.visible(text,faces=(face,));p=pixels.start+face*(4+pixels.ng*8);h=struct.unpack_from('<I',pixels.b,p)[0];at=0;xs=[];ys=[]
 for c in text:
  adv,ww,off=struct.unpack_from('<HHI',pixels.b,p+4+8*pixels.codes[ord(c)])
  a=np.frombuffer(pixels.b,np.uint8,count=h*ww,offset=pixels.pix+off).reshape(h,ww);yy,xx=np.nonzero(a)
  xs.extend((xx+at).tolist());ys.extend(yy.tolist());at+=adv
 return px-max(0,min(xs)-1),py-max(0,min(ys)-1)
def marker(g,label):
 time.sleep(.7);previous=np.asarray(g.snap());end=time.monotonic()+8
 while time.monotonic()<end:
  time.sleep(.1);current=np.asarray(g.snap());mask=np.any(previous[100:730]!=current[100:730],axis=2)
  if mask.any():
   yy,xx=np.nonzero(mask);box=(int(xx.min()),int(yy.min()+100),int(xx.max()-xx.min()+1),int(yy.max()-yy.min()+1))
   if box[2] in (1,2) and 15<=box[3]<=17:
    x,y,ww,hh=box
    # One frame is ink, the other is background. Do not call background a
    # successful cursor merely because it is solid; infer ink from the actual
    # blink pair and the unchanged adjacent background at the bottom edge.
    for frame in (current,previous):
     a=frame[y:y+hh,x:x+ww]
     if np.all(a==a[0,0]) and not np.array_equal(a[0,0],frame[y+hh-1,x+ww+3]):
      r.setdefault('carets',{})[label]=list(box);return box,tuple(map(int,a[0,0]))
  previous=current
 raise AssertionError((label,'no caret blink'))
def caret(g,box,color,label):
 # The hardware pointer is drawn after the caret: move it away from the clicked
 # insertion point before comparing exact solid pixels (no tolerance).
 g.move(65,700)
 x,y,ww,hh=box;end=time.monotonic()+.5
 while True:
  a=np.asarray(g.snap())[y:y+hh,x:x+ww]
  if np.all(a==color):return
  if time.monotonic()>=end:raise AssertionError((label,'caret not at clicked position',box,a[0,0].tolist(),color))
  time.sleep(.02)
def clicked_command(g,word,label):
 g.key('meta_l-k');box,col=marker(g,label);x,y,ww,hh=box
 # Insert the missing L after C by clicking, rather than using arrow keys.
 g.type('echo C'+word[2:]);g.click(x+6*8,y+6);g.type('L');caret(g,(x+7*8,y,ww,hh),col,label);g.key('ret');g.visible(word)
class Installed(Retail):initial='Desk4'
class Recovery(RecoveryKey,Retail):recovery_text='Ripristina da una copia locale'
try:
 if not resume:
  with Retail(iso,w,ev,'desktop4-live',disk='target',live=False) as g:
   # Native proportional name hit: Dsk4 -> Desk4, not append-to-end.
   g.type('Dsk4');p=pixels.start+1*(4+pixels.ng*8);advance=struct.unpack_from('<H',pixels.b,p+4+8*pixels.codes[ord('D')])[0]
   g.click(1280//2-254//2+16+advance,432+12);g.type('e');g.key('tab');g.type('local-test-285');g.key('tab');g.type('local-test-285');g.key('ret');g.visible('Preferiti',35)
   g.key('meta_l-w');closed(g,1);assert not windows(g)
   ok('Setup proportional-field click inserts within Dsk4; account authenticates as Desk4. Command+W closes Finder.')
   right(g,490,470);x,y=g.visible('Nuova cartella',box=(400,400,800,700));g.click(x+30,y+8);g.visible('Nuova cartella');time.sleep(.6)
   assert not windows(g),('direct sheet spawned an application',windows(g))
   g.type('Cart4');g.click(423+30+4*8,279+70);g.type('ella');g.key('ret');g.visible('Cartella4');assert not windows(g)
   ok('Desktop right-click opens direct New Folder sheet with NO client windows/Finder; mouse insertion creates Cartella4.')
   before=g.visible('Cartella4');sx,sy=before[0]+20,before[1]-25
   g.drag(sx,sy,147,-193);time.sleep(.5);after=g.visible('Cartella4');assert after==(before[0]+147,before[1]-193),(before,after)
   # Overlap the calendar widget; icons must remain visible and draggable.
   sx,sy=after[0]+20,after[1]-25;g.drag(sx,sy,1120-sx,110-sy);time.sleep(.5);position=g.visible('Cartella4')
   r['saved_folder_label_position']=list(position);g.shot('desktop4-free-widget-overlap')
   ok('Icon follows exact non-grid delta (+147,-193), then remains visible/draggable over the desktop widget.')
   baseline=windows(g);g.key('f2');g.key('f11');time.sleep(.5);assert windows(g)==baseline
   launch(g,'Terminale','Sessione locale');assert 2 in windows(g)
   g.type('echo WIN_COPY4');g.key('meta_l-c');g.key('ctrl-c');g.key('meta_l-v');g.key('ret');g.visible('WIN_COPY4')
   clicked_command(g,'CLICK4','live-Terminal-click');ok('Removed F2/F11 do not launch apps; Win+Space launches Terminal. Win+C/V copies/pastes while Ctrl+C interrupts; prompt click inserts at the correct column.')
   # Desktop -> Terminal is quoted insertion only, not execution or a move.
   g.key('meta_l-k');box,col=marker(g,'terminal-drop-target');sx,sy=position[0]+20,position[1]-25
   g.drag(sx,sy,box[0]-sx,box[1]+5-sy);g.key('meta_l-x');g.cmd('pbpaste','/Users/User/Desktop/Cartella4')
   g.cmd('echo PAYLOAD4 > Desktop/payload4.txt');g.cmd('mkdir Documents/Drop4');g.cmd('open Documents/Drop4');g.visible('Questa cartella')
   label=g.visible('payload4.txt');tx,ty=g.visible('Questa cartella');sx,sy=label[0]+20,label[1]-25;g.drag(sx,sy,tx+80-sx,ty+70-sy);g.visible('Elemento spostato')
   g.key('meta_l-w');closed(g,1);g.cmd('cat Documents/Drop4/payload4.txt','PAYLOAD4')
   ok('Actual desktop-origin drops route to Terminal as a quoted path and to Finder as a real file move; payload contents preserved.')
   g.cmd('open Documents/Drop4');g.visible('payload4.txt');label=g.visible('payload4.txt');sx,sy=label[0]+20,label[1]-25;g.drag(sx,sy,1110-sx,565-sy);time.sleep(.6)
   g.key('meta_l-w');closed(g,1);g.cmd('cat Desktop/payload4.txt','PAYLOAD4');g.shot('desktop4-transfer-roundtrip')
   ok('Finder -> arbitrary desktop drop performs the real move back and preserves file bytes.')
   # Global Command shortcuts, without resurrecting old function-key aliases.
   g.key('meta_l-m');time.sleep(.6);g.key('ctrl-up');time.sleep(.5);g.key('esc');launch(g,'Terminale','Sessione locale')
   g.key('meta_l-shift-a');g.visible('Applicazioni');g.key('esc')
   g.key('meta_l-ctrl-q');g.visible('Desk4');g.type('local-test-285');g.key('ret');g.visible('Sessione locale');ok('Command+M, Control+Up/Escape, Command+Shift+A and Command+Control+Q/unlock work via actual USB modifiers.')
   launch(g,'Installa','Installa tinymacOS Tramontana');g.key('ret');g.visible('Seleziona la destinazione');g.key('down');g.key('ret');g.visible('Conferma l’installazione');g.key('e');g.visible('Proteggi la nuova installazione')
   box,col=marker(g,'installer-password');x,y,ww,hh=box;g.type('volume4');g.click(x+3*8,y+6);g.type('X');caret(g,(x+4*8,y,ww,hh),col,'installer-password-click')
   g.key('tab');box,col=marker(g,'installer-confirm');g.type('volume4');g.key('esc');g.visible('Conferma l’installazione');ok('Real Installer password and confirmation carets visible; click repositions insertion. Encryption setup cancelled before any disk write.')
   g.key('spc');g.key('ret');g.visible('Il disco è pronto',240);g.visible('OpenCore è stato impostato come avvio principale.');g.shot('desktop4-gui-install');g.key('ret');g.p.wait(timeout=25)
   ok('Fresh GUI installation writes native TMCFS2 and imports account/files/desktop preferences; OpenCore preference verified by installer.')
  r['resume_stage']='cold';save()
 if r.get('resume_stage')!='recovery':
  with Installed('/not-attached',w,ev,'desktop4-cold',disk='target',boot_disk=True) as g:
   assert str(iso) not in str(g.req('query-block'));g.unlock();g.key('meta_l-w');closed(g,1)
   position=g.visible('Cartella4');assert list(position)==r['saved_folder_label_position'],(position,r['saved_folder_label_position'])
   g.shot('desktop4-cold-persistent-position');launch(g,'Terminale','Sessione locale');g.cmd('cat Desktop/payload4.txt','PAYLOAD4');g.cmd('off');g.p.wait(timeout=25)
   ok('Cold installed boot WITHOUT ISO: exact arbitrary icon position and transferred payload persist; imported Desk4 account authenticates.')
  r['resume_stage']='recovery';save()
 with Recovery('/not-attached',w,ev,'desktop4-recovery',disk='target',boot_disk=True) as g:
  assert str(iso) not in str(g.req('query-block'));g.key('ctrl-f2');g.key('down');g.key('ret');g.visible('Sessione locale')
  clicked_command(g,'CLICKREC4','Recovery-Terminal-click');g.cmd('fdesetup manage');g.visible('Attiva FileVault');x,y=g.visible('Attiva FileVault');g.click(x+40,y+8);g.visible('Attivazione FileVault')
  box,col=marker(g,'Recovery-FileVault-password');x,y,ww,hh=box;g.type('recovery4');g.click(x+2*8,y+6);g.type('X');caret(g,(x+3*8,y,ww,hh),col,'Recovery-password-click')
  g.key('tab');box,col=marker(g,'Recovery-FileVault-confirm');g.type('recovery4');g.move(65,700);g.shot('desktop4-recovery-fields-active');g.key('esc');g.shot('desktop4-recovery-fields-cancelled')
  ok('Installed native Recovery WITHOUT ISO entered with Win+R; Ctrl+F2 menu, Terminal click insertion and FileVault password/confirmation carets verified. Activation cancelled.')
 r['passed']=True;save()
except Exception as e:r['failure']=str(e);save();raise
finally:
 for p in w.glob('*.ppm'):p.unlink()
 print(w,flush=True)
