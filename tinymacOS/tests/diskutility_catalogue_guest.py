"""Real Utility Disco/Finder UI and syscalls on the current GUI-installed ISO.
Host-appended stress snapshots are declared, not passed off as guest creation.
No physical filesystem or Windows mount acceptance is claimed.
"""
from pathlib import Path
import os,json,struct,hashlib,subprocess,time
from retail_guest import Retail
import pixels,retail_guest
root=Path(__file__).resolve().parents[1];qa=Path(os.environ['TMC_QA_DIR']);build=Path(os.environ['TMC_BUILD']);iso=Path(os.environ['TMC_STAGE_ISO'])
fixture=json.loads((qa/'fs2-install-guest-result.json').read_text());assert fixture['passed'] and fixture['iso_sha256']==hashlib.file_digest(iso.open('rb'),'sha256').hexdigest()
w=Path(fixture['work']);assert str(w).startswith('/var/tmp/tmc-fs2-install-');disk=w/'target/system.raw';ev=w/'evidence'
with disk.open('rb') as f:f.seek(1024+128+32);a,b=struct.unpack('<QQ',f.read(16))
exe=w/'catalogue-fixture';subprocess.run(['gcc','-O2','-fno-builtin','-I'+str(root/'include'),'-I'+str(root/'tests'),str(root/'tests/diskutility_snapshot_fixture.c'),str(root/'storage/tmcbfs2.c'),str(root/'crypto/sha256.c'),str(root/'kernel/string.c'),'-o',str(exe)],check=True)
stress=w/'catalogue-fixture-result.json'
if os.environ.get('TMC_REUSE_CATALOGUE_FIXTURE')=='1':
 previous=json.loads(stress.read_text());assert previous['iso_sha256']==fixture['iso_sha256'] and previous['count']==43
 subprocess.run([str(exe),str(disk),str(a),str(b-a+1),'--verify'],check=True)
else:
 subprocess.run([str(exe),str(disk),str(a),str(b-a+1)],check=True)
 stress.write_text(json.dumps({'iso_sha256':fixture['iso_sha256'],'count':43,'host_appended':True})+'\n')
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
old=retail_guest.locate;retail_guest.locate=lambda im,text,faces=(0,1,2,3,5,6,8),threshold=.88:old(im,text,faces,threshold)
os.environ.update(TMC_STORAGE='nvme',TMC_MEM='512',TMC_SMP='2',TMC_POINTER='tablet',OCR_ROOT=os.environ['TOOLROOT'])
r={'passed':False,'macroblock_2_complete':False,'release_ready':False,'work':str(w),'iso_sha256':fixture['iso_sha256'],'kernel_sha256':fixture['kernel_sha256'],'host_appended_snapshot_stress':True,'snapshot_count_before_guest':43,'physical_hardware_tested':False,'foreign_filesystem_browsing_tested':False,'exfat_export_tested':False,'checks':[]}
def ok(text):r['checks'].append(text);print('PASS',text,flush=True)
class Installed(Retail):initial='Fs2Install'
def terminal(g):g.key('f2');g.visible('Sessione locale')
def wheel(g,direction,count=12):
 for _ in range(count):
  g.req('input-send-event',events=[{'type':'btn','data':{'down':True,'button':'wheel-'+direction}},{'type':'btn','data':{'down':False,'button':'wheel-'+direction}}]);time.sleep(.09)
def strict(g,text,timeout=30,box=None,faces=(0,)):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  im=g.snap()
  if box:im=im.crop(box)
  pos=old(im,text,faces,threshold=.985)
  if pos:return (pos[1][0]+(box[0] if box else 0),pos[1][1]+(box[1] if box else 0))
  time.sleep(.2)
 raise AssertionError('strict UI text absent: '+text+'; '+g.ocr())
try:
 with Installed('/not-attached',w,ev,'block2-catalogue',disk='target',boot_disk=True) as g:
  g.unlock();terminal(g)
  g.cmd('echo AFTER_CATALOGUE > Documents/persistent.txt')
  g.cmd('cat "/.snapshots/Copia locale/Users/User/Documents/persistent.txt"','FS2_INSTALL_PERSISTENT')
  g.cmd('open -a "Utility Disco"');ix,iy=g.visible('Interni',faces=(2,));X,Y=ix-15,iy-18
  # Data volume row: actual used and capacity on one line, not suffix piles.
  g.click(X+90,Y+54+2*27+8);g.visible('Utilizzo');g.visible('43');g.shot('block2-actual-used-and-total')
  # Reach the otherwise offscreen pagination controls with the real wheel.
  g.move(X+90,Y+280);wheel(g,'down');nx,ny=g.visible('Altri snapshot',faces=(0,));g.click(nx+12,ny+8)
  strict(g,'catalog-13',box=(X+205,Y+53,X+800,Y+280))
  px,py=g.visible('Snapshot precedenti',faces=(0,));g.click(px+12,py+8)
  strict(g,'catalog-12',box=(X+205,Y+53,X+800,Y+280))
  g.move(X+90,Y+280);wheel(g,'up')
  ok('Real mouse wheel reveals Next; clicking Next and Previous reaches the correct actual snapshot pages without keyboard assistance')
  # Local checkpoint is the third catalogue entry (after Installazione and installer-guest).
  g.click(X+90,Y+54+7*27+8);g.visible('Sola lettura');g.visible('Mostra nel Finder',faces=(0,))
  g.click(X+270,Y+336);g.visible('Preferiti');strict(g,'Copia locale',faces=(3,));g.shot('block2-local-checkpoint-finder')
  g.key('ctrl-w');g.visible('Mostra nel Finder',faces=(0,))
  # First snapshot -> sixteen Down events crosses the real Next row once.
  g.click(X+90,Y+54+5*27+8)
  for _ in range(16):g.key('down',dt=.08)
  strict(g,'catalog-13',box=(X+205,Y+53,X+800,Y+280))
  for _ in range(16):g.key('down',dt=.08)
  strict(g,'catalog-29',box=(X+205,Y+53,X+800,Y+280));g.shot('block2-third-snapshot-page')
  g.click(X+385,Y+298);g.visible('Integrità dello snapshot verificata.',faces=(0,))
  g.click(X+270,Y+336);g.visible('Preferiti');strict(g,'catalog-29',faces=(3,));g.shot('block2-third-page-snapshot-finder')
  g.key('ctrl-w');g.visible('Mostra nel Finder',faces=(0,));g.key('up');strict(g,'catalog-28',box=(X+205,Y+53,X+800,Y+280))
  g.key('ctrl-w');terminal(g)
  g.cmd('cat Documents/persistent.txt','AFTER_CATALOGUE')
  g.cmd('cat /.snapshots/catalog-29/Users/User/Documents/persistent.txt','FS2_INSTALL_PERSISTENT')
  g.cmd('diskutil apfs snapshot verify installer-guest','Snapshot valido:')
  ok('Actual installed Utility Disco renders 43 snapshots via bounded pages; keyboard crosses Next/Previous and verifies snapshot 33, beyond the old eight-slot ceiling')
  ok('Mostra nel Finder opens the actual read-only local checkpoint and third-page snapshot; frozen marker bytes remain original while the live file is different')
  # Finder location is not a painted label: navigate through its real sidebar.
  g.cmd('open /');px,py=g.visible('Macintosh HD',faces=(3,));FX,FY=px-191,py-29
  assert 'macintosh' in g.ocr(box=(FX+171,FY+15,FX+700,FY+59),psm=6).lower()
  g.click(FX+70,FY+350);strict(g,'.snapshots',box=(FX+171,FY+15,FX+700,FY+59),faces=(3,))
  assert 'snapshots' in g.ocr(box=(FX+171,FY+15,FX+700,FY+59),psm=6).lower();g.shot('block2-finder-snapshot-location')
  g.key('ctrl-w');terminal(g);g.cmd('off');g.p.wait(timeout=20)
  ok('Finder Snapshot sidebar location navigates to the real /.snapshots catalogue; original live file and pre-existing snapshot remain usable')
 r['passed']=True
except Exception as e:r['failure']=str(e);raise
finally:(qa/'diskutility-catalogue-guest-result.json').write_text(json.dumps(r,indent=2)+'\n')
