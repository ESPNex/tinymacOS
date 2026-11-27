"""Actual installed OS: foreign RW through Terminal/Finder and REAL partitions.
Disposable virtual media only; no debug APIs or application-action injection.
Exactly three final screenshots are saved after the relevant assertions.
"""
from pathlib import Path
import os,json,struct,hashlib,subprocess,tempfile,time,shutil
from retail_guest import Retail
import pixels,retail_guest
root=Path(__file__).resolve().parents[1];qa=Path(os.environ['TMC_QA_DIR']);build=Path(os.environ['TMC_BUILD']);iso=Path(os.environ['TMC_STAGE_ISO'])
fixture=json.loads((qa/'fs2-install-guest-result.json').read_text());assert fixture['passed'] and fixture['iso_sha256']==hashlib.file_digest(iso.open('rb'),'sha256').hexdigest()
installed=Path(fixture['work']);disk=installed/'target/system.raw'
w=Path(tempfile.mkdtemp(prefix='tmc-volume-export-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
old=retail_guest.locate;retail_guest.locate=lambda im,text,faces=(0,1,2,3,5,6,8),threshold=.96:old(im,text,faces,threshold)
os.environ.update(TMC_STORAGE='nvme',TMC_MEM='512',TMC_SMP='2',TMC_POINTER='tablet',OCR_ROOT=os.environ['TOOLROOT'])
r={'passed':False,'work':str(w),'installed_work':str(installed),'iso_sha256':fixture['iso_sha256'],'kernel_sha256':fixture['kernel_sha256'],'host_created_external_fixtures':True,'windows_tested':False,'physical_hardware_tested':False,'checks':[],'outputs':[]}
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
def run(*args,**kw):return subprocess.run(list(map(str,args)),check=True,**kw)
def digest(p):return hashlib.file_digest(p.open('rb'),'sha256').hexdigest()
def rangehash(p,a,n):
 h=hashlib.sha256()
 with p.open('rb') as f:
  f.seek(a)
  while n:
   b=f.read(min(n,1<<20));assert b;n-=len(b);h.update(b)
 return h.hexdigest()
class Installed(Retail):initial='Fs2Install'
def terminal(g):g.key('f2');g.visible('Sessione locale')
def strict(g,text,timeout=30,box=None,faces=(0,)):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  im=g.snap();im=im.crop(box) if box else im;pos=old(im,text,faces,threshold=.985)
  if pos:return(pos[1][0]+(box[0] if box else 0),pos[1][1]+(box[1] if box else 0))
  time.sleep(.2)
 raise AssertionError('strict UI text absent: '+text+'; '+g.ocr())
def right(g,x,y):
 g.move(x,y);g.req('input-send-event',events=[{'type':'btn','data':{'down':True,'button':'right'}}]);time.sleep(.12);g.req('input-send-event',events=[{'type':'btn','data':{'down':False,'button':'right'}}])
def menu(g,label):x,y=g.visible(label);g.click(x+12,y+7);time.sleep(.4)
def drag(g,a,b):
 g.move(*a);g.req('input-send-event',events=[{'type':'btn','data':{'down':True,'button':'left'}}]);time.sleep(.25)
 for j in range(1,9):g.move(round(a[0]+(b[0]-a[0])*j/8),round(a[1]+(b[1]-a[1])*j/8))
 time.sleep(.35);g.req('input-send-event',events=[{'type':'btn','data':{'down':False,'button':'left'}}]);time.sleep(.7)
try:
 exe=w/'fixture';run('gcc','-O2','-fno-builtin','-I'+str(root/'include'),root/'tests/volume_export_fixture.c',root/'storage/tmcbfs2.c',root/'crypto/sha256.c',root/'kernel/string.c','-o',exe)
 proof=w/'proof.bin';proof.write_bytes(bytes((i*17+23)&255 for i in range(65537)));mini=w/'mini.txt';mini.write_text('CIAO')
 images=[]
 for name,size in [('native',32<<20),('fat32',64<<20),('ntfs',64<<20)]:
  p=w/(name+'.img');p.touch();os.truncate(p,size);images.append(p)
 run(exe,'seed',images[0],0,(32<<20)//512)
 run('mkfs.fat','-F32',images[1]);run('mcopy','-i',images[1],proof,'::proof.bin');run('mcopy','-i',images[1],mini,'::mini.txt')
 run('mkntfs','-F','-Q',images[2],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);run('ntfscp',images[2],proof,'/proof.bin');run('ntfscp',images[2],mini,'/mini.txt')
 generator=w/'generator';run('gcc','-O2','-fno-builtin','-I'+str(root/'include'),root/'tests/foreignfs_host.c',root/'storage/foreignfs.c',root/'storage/exfat_copy.c',root/'kernel/string.c','-o',generator)
 p=w/'exfat.img';run(generator,'export',p);images.append(p)
 external=w/'external.raw';mbr=bytearray(512);mbr[510:512]=b'\x55\xaa';at=2048;source_extent=None
 with external.open('wb') as f:
  for i,p in enumerate(images):
   size=p.stat().st_size//512;entry=446+i*16;mbr[entry+4]=12 if i==1 else 7;struct.pack_into('<II',mbr,entry+8,at,size)
   if i==0:source_extent=(at*512,size*512)
   f.seek(at*512)
   with p.open('rb') as src:shutil.copyfileobj(src,f)
   at=(at+size+2047)//2048*2048
  f.truncate(at*512);f.seek(0);f.write(mbr)
 native_before=rangehash(external,*source_extent)
 target=w/'destination.raw';target.touch();os.truncate(target,256<<20);target_mbr=bytearray(512);target_mbr[510:512]=b'\x55\xaa';target_mbr[450]=12;struct.pack_into('<II',target_mbr,454,2048,(64<<20)//512)
 with target.open('r+b') as f,images[1].open('rb') as src:f.write(target_mbr);f.seek(1048576);shutil.copyfileobj(src,f)
 existing_before=rangehash(target,1048576,64<<20)
 launcher=w/'external.sh';launcher.write_text('#!/bin/bash\nexec '+str(root/'tools/run-qemu.sh')+' -drive if=none,id=foreign,format=raw,file='+str(external)+' -device nvme,drive=foreign,serial=TMCFOREIGN -drive if=none,id=copytarget,format=raw,file='+str(target)+' -device nvme,drive=copytarget,serial=TMCEXFATCOPY "$@"\n');launcher.chmod(0o755)
 with Installed('/not-attached',installed,ev,'block2-rw-real-partitions',disk='target',boot_disk=True,launcher=launcher) as g:
  g.unlock();terminal(g);g.cmd('ls /Volumes','disk1s4')
  for part in [2,3,4]:
   base='/Volumes/disk1s'+str(part)
   for cmd in ['mkdir '+base+'/RW','touch '+base+'/RW/empty.txt','echo WRITE_FS_'+str(part)+' > '+base+'/RW/write.txt','mv '+base+'/RW/write.txt '+base+'/RW/renamed.txt','cp '+base+'/proof.bin Documents/proof'+str(part)+'.bin','cp Documents/proof'+str(part)+'.bin '+base+'/RW/copy.bin'] :g.cmd(cmd)
   g.cmd('cat '+base+'/RW/renamed.txt','WRITE_FS_'+str(part),timeout=60)
   g.cmd('rm '+base+'/RW/empty.txt');g.cmd('rm '+base+'/RW/copy.bin')
  ok('Installed real Terminal creates/writes/reads/renames/deletes FAT32, NTFS, exFAT; copies 65537-byte streams both ways through native FS2')
  g.cmd('mv /Volumes/disk1s2/RW/renamed.txt /Volumes/disk1s3/RW/from-fat32.txt');g.cmd('cat /Volumes/disk1s3/RW/from-fat32.txt','WRITE_FS_2',timeout=60)
  g.cmd('mv /Volumes/disk1s3/RW/from-fat32.txt /Volumes/disk1s4/RW/from-ntfs.txt');g.cmd('cat /Volumes/disk1s4/RW/from-ntfs.txt','WRITE_FS_2')
  g.cmd('mv /Volumes/disk1s4/RW/from-ntfs.txt Documents/from-exfat.txt');g.cmd('cat Documents/from-exfat.txt','WRITE_FS_2')
  ok('Actual cross-filesystem S_MOVE chain FAT32 -> NTFS -> exFAT -> native performs verified copy before source removal')
  for cmd in ['mkdir /Volumes/disk1s2/SourceRW','mkdir /Volumes/disk1s3/DestinationRW','mkdir /Volumes/disk1s4/DragRW','echo FINDER_CUT_FOREIGN > /Volumes/disk1s2/SourceRW/cut.txt','echo FINDER_DRAG_FOREIGN > /Volumes/disk1s3/DestinationRW/drag.txt']:g.cmd(cmd)
  g.cmd('open /Volumes/disk1s2/SourceRW');g.visible('SourceRW',faces=(3,));x,y=strict(g,'cut.txt');right(g,x+8,y+7);menu(g,'Taglia');g.visible('Elemento tagliato:')
  terminal(g);g.cmd('open /Volumes/disk1s3/DestinationRW');hx,hy=g.visible('DestinationRW',faces=(3,));right(g,hx+300,hy+240);menu(g,'Incolla');g.visible('Elemento spostato.',timeout=90)
  terminal(g);g.cmd('cat /Volumes/disk1s3/DestinationRW/cut.txt','FINDER_CUT_FOREIGN')
  g.cmd('open /Volumes/disk1s3/DestinationRW');hx,hy=g.visible('DestinationRW',faces=(3,));cx,cy=hx-192,hy-72;g.drag(cx+380,cy+21,35-cx,70-cy);time.sleep(.6);x,y=strict(g,'drag.txt');a=(x+8,y+7)
  terminal(g);g.cmd('open /Volumes/disk1s4/DragRW');hx,hy=g.visible('DragRW',faces=(3,));cx,cy=hx-192,hy-72;g.drag(cx+380,cy+21,460-cx,340-cy);time.sleep(.6);g.drag(1234,770,0,-100);time.sleep(.5);hx,hy=g.visible('DragRW',faces=(3,));b=(hx+200,hy+180);g.click(180,91);drag(g,a,b);g.visible('Elemento spostato.',timeout=90)
  terminal(g);g.cmd('cat /Volumes/disk1s4/DragRW/drag.txt','FINDER_DRAG_FOREIGN');ok('Real Finder context Cut/Paste crosses FAT32->NTFS; held-button drag crosses NTFS->exFAT, actual bytes read back in Terminal')
  g.cmd('open /Volumes/disk1s4/DragRW');g.visible('drag.txt');g.shot('finder')
  terminal(g);g.cmd('diskutil exportExFAT /Volumes/disk1s1 disk2','Anteprima pronta:',timeout=90)
  g.cmd('diskutil exportExFAT /Volumes/disk1s1 disk2 --confirm','Nuova partizione exFAT creata',timeout=240)
  g.cmd('cat /Volumes/disk2s2/Documenti/esterno.txt','EXTERNAL_NATIVE_GUEST')
  ok('Installed authenticated export previews and creates NEW REAL exFAT disk2s2 inside verified unallocated gap; copied native source is mounted and read')
  g.cmd('diskutil list disk2','exFAT',timeout=60);g.shot('terminale')
  g.cmd('open -a "Utility Disco"');ix,iy=g.visible('Interni',faces=(2,));X,Y=ix-15,iy-18
  # Existing GUI fixture snapshot: factory first, user installer-guest second.
  g.click(X+90,Y+54+6*27+8);g.visible('Sola lettura');strict(g,'installer-guest',box=(X+205,Y+53,X+800,Y+280),faces=(0,))
  g.click(X+510,Y+336);g.visible('Esporta una copia exFAT');g.key('end');g.key('backspace');g.type('2');g.key('ret');g.visible('Anteprima pronta:',timeout=90);g.key('ret');g.visible('Copia exFAT creata',timeout=240);g.shot('utility-disco')
  g.key('esc');g.key('ctrl-w');terminal(g);g.cmd('cat Documents/persistent.txt','FS2_INSTALL_PERSISTENT');g.cmd('diskutil apfs snapshot verify installer-guest','Snapshot valido:',timeout=90);g.cmd('off');g.p.wait(timeout=20)
  ok('Real Utility Disco previews/confirms export of immutable snapshot to another new real partition; original native live data and snapshot remain valid')
 assert rangehash(external,*source_extent)==native_before
 assert rangehash(target,1048576,64<<20)==existing_before
 with target.open('rb') as f:final_mbr=f.read(512)
 assert final_mbr[:462]==target_mbr[:462] and final_mbr[510:]==target_mbr[510:]
 for slot in [1,2]:
  at=446+slot*16;start,length=struct.unpack_from('<II',final_mbr,at+8);assert final_mbr[at+4]==7 and start>=2048+(64<<20)//512
  p=w/('native-copy.img' if slot==1 else 'snapshot-copy.img')
  with target.open('rb') as src,p.open('wb') as dest:
   src.seek(start*512);remaining=length*512
   while remaining:b=src.read(min(remaining,1<<20));assert b;dest.write(b);remaining-=len(b)
  run('fsck.exfat','-n',p);r['outputs'].append({'file':p.name,'sha256':digest(p),'start':start,'sectors':length})
 for part,name in [(2,'fat32'),(3,'ntfs'),(4,'exfat')]:
  start,length=struct.unpack_from('<II',mbr,446+(part-1)*16+8);p=w/(name+'-after.img')
  with external.open('rb') as src,p.open('wb') as dest:src.seek(start*512);dest.write(src.read(length*512))
  run(*(['fsck.fat','-n',p] if name=='fat32' else ['ntfsresize','-i','-f',p] if name=='ntfs' else ['fsck.exfat','-n',p]))
 ok('Independent fsck accepts all three guest-written foreign filesystems and both NEW exFAT partitions; existing destination partition and native source full byte ranges SHA256 unchanged')
 r['screenshots']=[str(ev/(name+'.png')) for name in ['finder','terminale','utility-disco']];r['passed']=True
except Exception as e:r['failure']=str(e);raise
finally:(qa/'volume-export-guest-result.json').write_text(json.dumps(r,indent=2)+'\n')
