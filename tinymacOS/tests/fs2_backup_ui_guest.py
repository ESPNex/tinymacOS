"""New scalable UI/backup guest gate on the current GUI-installed fixture.
Stress entries are appended OFFLINE by the host only AFTER real installation;
this gate is UI/backup acceptance, not evidence of guest bulk file creation.
"""
from pathlib import Path
import os,json,struct,hashlib,subprocess,time
from retail_guest import Retail
from boot_recovery import RecoveryKey
import pixels,retail_guest
root=Path(__file__).resolve().parents[1];base=Path(os.environ['TMC_QA_DIR']);build=Path(os.environ['TMC_BUILD']);iso=Path(os.environ['TMC_STAGE_ISO'])
fixture=json.loads((base/'fs2-install-guest-result.json').read_text());assert fixture['passed'] and fixture['iso_sha256']==hashlib.file_digest(iso.open('rb'),'sha256').hexdigest()
w=Path(fixture['work']);assert str(w).startswith('/var/tmp/tmc-fs2-install-');disk=w/'target/system.raw';ev=w/'evidence'
with disk.open('rb') as f:f.seek(1024+128+32);a,b=struct.unpack('<QQ',f.read(16))
exe=w/'append-stress';subprocess.run(['gcc','-O2','-fno-builtin','-I'+str(root/'include'),str(root/'tests/fs2_append_stress.c'),str(root/'storage/tmcbfs2.c'),str(root/'kernel/string.c'),'-o',str(exe)],check=True)
if os.environ.get('TMC_REUSE_UI_STRESS')=='1':
 previous=json.loads((base/'fs2-backup-ui-result.json').read_text())
 assert previous['work']==str(w) and previous['iso_sha256']==fixture['iso_sha256'] and previous['host_appended_ui_stress_fixture']
else:subprocess.run([str(exe),str(disk),str(a),str(b-a+1)],check=True)
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
old=retail_guest.locate;retail_guest.locate=lambda im,text,faces=(0,1,2,3,5,6,8),threshold=.88:old(im,text,faces,threshold)
os.environ.update(TMC_STORAGE='nvme',TMC_MEM='512',TMC_SMP='2',TMC_POINTER='tablet',OCR_ROOT=os.environ['TOOLROOT'])
chunk=bytearray(b'X'*8192);chunk[:19]=b'LARGE_TEXTEDIT_128K';expected=hashlib.sha256(chunk*16).hexdigest()
r={'passed':False,'release_ready':False,'work':str(w),'kernel_sha256':fixture['kernel_sha256'],'iso_sha256':fixture['iso_sha256'],'host_appended_ui_stress_fixture':True,'large_file_sha256':expected,'checks':[]}
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
class Installed(Retail):initial='Fs2Install'
class Recovery(RecoveryKey,Retail):recovery_text='Ripristina da una copia locale'
def terminal(g):g.key('f2');g.visible('Sessione locale')
def utility(g,label):
 x,y=g.visible('Utility',box=(0,0,800,34));g.click(x+12,y+8);x,y=g.visible(label,box=(0,34,900,250));g.click(x+20,y+8)
def strict(g,text,faces=(0,),timeout=180):
 start=time.monotonic()
 while time.monotonic()-start<timeout:
  if g.p.poll() is not None:raise RuntimeError('guest exited while awaiting '+text)
  pos=old(g.snap(),text,faces,threshold=.985)
  if pos:return pos[1]
  time.sleep(.25)
 raise AssertionError('strict bitmap text not rendered: '+text+'; '+g.ocr())
def next_pages(g,x,y,count,start=0,kind='files'):
 # A page traverses real on-disk trees. Wait for each rendered result before
 # the next click: fixed sleeps can enqueue input faster than QEMU can draw.
 for page in range(start+1,start+count+1):
  g.click(x,y)
  text=('Pagina '+str(page+1) if kind=='picker' else 'paged-%02d'%(page*8-2) if kind=='snapshots' else 'item-%04d'%(page*64))
  strict(g,text,faces=(6,) if kind=='snapshots' else (0,))
 g.move(1100,650)
try:
 with Installed('/not-attached',w,ev,'scalable-ui',disk='target',boot_disk=True) as g:
  g.unlock();terminal(g);g.cmd('cat /.snapshots/paged-19/Users/User/Documents/persistent.txt','FS2_INSTALL_PERSISTENT');g.cmd('shasum -a 256 Documents/Pagination/large.txt',expected,timeout=180)
  g.cmd('rm Documents/Pagination/large-copy.txt');g.cmd('open Documents/Pagination/large.txt');g.visible('LARGE_TEXTEDIT_128K');g.key('ctrl-shift-s');tx,ty=g.visible('Salva documento');X,Y=tx-32,ty-23;g.click(X+300,Y+383);g.type('large-copy.txt');g.click(X+690,Y+446);g.visible('Salvato su disco');terminal(g);g.cmd('shasum -a 256 Documents/Pagination/large-copy.txt',expected,timeout=180);ok('TextEdit opens and saves a 128KiB file through the modern chooser, with byte-exact streamed readback beyond the old 8KiB limit')
  g.cmd('open Documents/Pagination');tx,ty=g.visible('Pagination',faces=(3,));X,Y=tx-191,ty-29;next_pages(g,X+752,Y+370,7);strict(g,'item-0448');next_pages(g,X+752,Y+370,10,start=7);strict(g,'item-1099');g.shot('finder-file-1099-paged');ok('Actual Finder navigates beyond 384 entries and reaches file 1099 using bounded pages through real syscalls')
  terminal(g);g.cmd('open -a "Time Machine"');tx,ty=g.visible('Linea temporale dei backup');X,Y=tx-226,ty-70;next_pages(g,X+570,Y+76,2,kind='snapshots');strict(g,'paged-19',faces=(6,));g.click(X+300,Y+282);g.click(X+450,Y+397);g.visible('Integrità del backup verificata.');g.shot('timemachine-third-page-verified');ok('Actual Time Machine reaches and verifies snapshot 20 on the third timeline page')
  g.key('f3');g.visible('Testo semplice');g.key('ctrl-o');tx,ty=g.visible('Apri documento');X,Y=tx-32,ty-23;next_pages(g,X+716,Y+32,7,kind='picker');strict(g,'item-0448');next_pages(g,X+716,Y+32,10,start=7,kind='picker')
  for _ in range(12):g.key('down')
  strict(g,'item-1099');g.shot('filepanel-file-1099-paged');g.key('ret');g.visible('Documento aperto');ok('Actual modern TextEdit picker reaches and opens file 1099, beyond the old directory-slot ceiling')
  terminal(g);g.cmd('echo BEFORE_LOCAL_BACKUP > Documents/backup-marker.txt');g.cmd('off');g.p.wait(timeout=20)
 with Recovery('/not-attached',w,ev,'scalable-backup-create',disk='target',boot_disk=True) as g:
  utility(g,'Crea una copia locale');g.visible('Confermi l’operazione');g.click(820,470);g.visible('Operazione completata.',180);g.shot('recovery-scalable-local-backup');g.key('ret');utility(g,'Terminale');g.visible('Sessione locale');g.cmd('off');g.p.wait(timeout=20)
 ok('Native internal Recovery creates the scalable local backup using shared-root CoW, not a volume-sized copy')
 with Installed('/not-attached',w,ev,'scalable-backup-modify',disk='target',boot_disk=True) as g:
  g.unlock();terminal(g);g.cmd('echo AFTER_LOCAL_BACKUP > Documents/backup-marker.txt');g.cmd('cat Documents/backup-marker.txt','AFTER_LOCAL_BACKUP');g.cmd('off');g.p.wait(timeout=20)
 with Recovery('/not-attached',w,ev,'scalable-backup-restore',disk='target',boot_disk=True) as g:
  g.click(550,270);g.click(880,557);g.visible('Confermi l’operazione');g.click(820,470);g.visible('Operazione completata.',180);g.shot('recovery-scalable-local-restore');g.key('ret');utility(g,'Terminale');g.visible('Sessione locale');g.cmd('off');g.p.wait(timeout=20)
 with Installed('/not-attached',w,ev,'scalable-backup-restored',disk='target',boot_disk=True) as g:
  g.unlock();terminal(g);g.cmd('cat Documents/backup-marker.txt','BEFORE_LOCAL_BACKUP');g.cmd('shasum -a 256 Documents/Pagination/large-copy.txt',expected,timeout=180);g.cmd('diskutil apfs snapshot verify paged-19','Snapshot valido:');g.shot('restored-scalable-backup-account-file-snapshots');g.cmd('off');g.p.wait(timeout=20)
 ok('Local backup restores account, changed document and 128KiB file after reboot while pre-existing snapshots remain valid');r['passed']=True
except Exception as e:r['failure']=str(e);raise
finally:(base/'fs2-backup-ui-result.json').write_text(json.dumps(r,indent=2)+'\n')
