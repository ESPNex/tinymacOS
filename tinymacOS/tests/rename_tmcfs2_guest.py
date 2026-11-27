"""Real live GUI: Utility Disco creates TMCFS2; Rinomina on every RW backend.
Only disposable /var/tmp virtual media; no debug/action-event injection.
"""
from pathlib import Path
import os,json,struct,hashlib,subprocess,tempfile,time,shutil
import sys
resume=Path(sys.argv[2]) if len(sys.argv)==3 and sys.argv[1]=="--verify-existing" else None
from retail_guest import Retail
import pixels,retail_guest
root=Path(__file__).resolve().parents[1];qa=Path(os.environ['TMC_QA_DIR']);qa.mkdir(parents=True,exist_ok=True);iso=Path(os.environ['TMC_STAGE_ISO']);build=Path(os.environ['TMC_BUILD'])
w=resume if resume else Path(tempfile.mkdtemp(prefix='tmc-rename-tmcfs2-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir(exist_ok=True)
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
old=retail_guest.locate;retail_guest.locate=lambda im,text,faces=(0,1,2,3,5,6,8),threshold=.96:old(im,text,faces,threshold)
os.environ.update(TMC_STORAGE='nvme',TMC_MEM='512',TMC_SMP='2',TMC_POINTER='tablet',OCR_ROOT=os.environ['TOOLROOT'])
r={'passed':False,'work':str(w),'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'kernel_sha256':hashlib.file_digest((build/'KernelCache').open('rb'),'sha256').hexdigest(),'checks':[],'physical_hardware_tested':False,'windows_tested':False}
def run(*args,**kw):return subprocess.run(list(map(str,args)),check=True,**kw)
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
def terminal(g):g.key('f2');g.visible('Sessione locale')
def strict(g,text,timeout=45,faces=(0,)):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  pos=old(g.snap(),text,faces,threshold=.985)
  if pos:return pos[1]
  time.sleep(.15)
 raise AssertionError('Text not present: '+text+'; '+g.ocr())
def right(g,x,y):
 g.move(x,y);g.req('input-send-event',events=[{'type':'btn','data':{'down':True,'button':'right'}}]);time.sleep(.12);g.req('input-send-event',events=[{'type':'btn','data':{'down':False,'button':'right'}}])
def rename(g,name,new,click=False,collision=False):
 x,y=strict(g,name);right(g,x+8,y+7);x,y=strict(g,'Rinomina',faces=(1,));g.click(x+12,y+8);g.visible('Scegli un nome non ancora usato.');
 if collision:
  g.type('occupato.txt');g.key('ret');g.visible('Nome non valido, già usato');g.key('ctrl-a')
 g.type(new)
 if click:
  # New shared 430x184 dialog, real clickable confirmation (not an action event).
  tx,ty=g.visible('Scegli un nome non ancora usato.')
  g.click(tx+306,ty+61)
 else:g.key('ret')
 g.visible(new,timeout=90)
try:
 if resume:
  prior=json.loads((qa/'rename-tmcfs2-guest-result.json').read_text())
  assert prior['work']==str(w) and prior['iso_sha256']==r['iso_sha256'] and prior['kernel_sha256']==r['kernel_sha256'] and len(prior['checks'])==6
  r['checks']=prior['checks'];r['resumed_readonly_postshutdown_verification']=True
  proof=w/'proof.bin';disk=w/'data.raw';at=2048;mbr=bytearray(512);mbr[510:512]=b'\x55\xaa'
  for i,name in enumerate(['fat32','ntfs','exfat']):
   count=(w/(name+'.img')).stat().st_size//512;off=446+i*16;mbr[off+4]=12 if i==0 else 7;struct.pack_into('<II',mbr,off+8,at,count);at=(at+count+2047)//2048*2048
  r['created_partition_start']=at
 else:
  proof=w/'proof.bin';proof.write_bytes(bytes((i*17+23)&255 for i in range(65537)));mini=w/'mini.txt';mini.write_text('PRESERVED')
  images=[]
  for name in ['fat32','ntfs']:
   p=w/(name+'.img');p.touch();os.truncate(p,64<<20);images.append(p)
  run('mkfs.fat','-F32',images[0]);run('mcopy','-i',images[0],proof,'::proof.bin');run('mcopy','-i',images[0],mini,'::mini.txt')
  run('mkntfs','-F','-Q',images[1],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);run('ntfscp',images[1],proof,'/proof.bin');run('ntfscp',images[1],mini,'/mini.txt')
  generator=w/'generator';run('gcc','-O2','-fno-builtin','-I'+str(root/'include'),root/'tests/foreignfs_host.c',root/'storage/foreignfs.c',root/'storage/exfat_copy.c',root/'kernel/string.c','-o',generator)
  p=w/'exfat.img';run(generator,'export',p);images.append(p)
  disk=w/'data.raw';mbr=bytearray(512);mbr[510:512]=b'\x55\xaa';at=2048
  with disk.open('wb') as f:
   for i,p in enumerate(images):
    count=p.stat().st_size//512;off=446+i*16;mbr[off+4]=12 if i==0 else 7;struct.pack_into('<II',mbr,off+8,at,count);f.seek(at*512)
    with p.open('rb') as src:shutil.copyfileobj(src,f)
    at=(at+count+2047)//2048*2048
   f.truncate(384<<20);f.seek(0);f.write(mbr)
  r['created_partition_start']=at
  launcher=w/'media.sh';launcher.write_text('#!/bin/bash\nexec '+str(root/'tools/run-qemu.sh')+' -drive if=none,id=rwdata,format=raw,file='+str(disk)+' -device nvme,drive=rwdata,serial=TMCRENAMEDATA "$@"\n');launcher.chmod(0o755)
  with Retail(iso,w,ev,'rename-create-native',live=True,launcher=launcher) as g:
   g.enroll('Gestione');terminal(g);g.cmd('diskutil list','exFAT');g.cmd('open -a "Utility Disco"');ix,iy=g.visible('Interni',faces=(2,));X,Y=ix-15,iy-18
   # Three partitions, the sub-MiB leading gap, then the large free extent.
   for _ in range(5):g.key('down')
   g.visible('Crea partizione…');x,y=g.visible('Crea partizione…');g.click(x+35,y+9);g.visible('Nuova partizione');g.key('ret');g.visible('Crea TMCFS2:');g.key('ret');g.visible('Partizione nativa creata e verificata.',timeout=180)
   for _ in range(4):g.key('down')
   g.visible('TMCFS2');g.visible('Mostra nel Finder');x,y=g.visible('Mostra nel Finder');g.click(x+35,y+8);g.visible('disk0s4',faces=(3,));g.shot('tmcfs2-created-browsable')
   terminal(g);g.cmd('mkdir /Volumes/disk0s4/Nuova');g.cmd('echo SAME_SYSTEM_CORE > /Volumes/disk0s4/Nuova/prova.txt');g.cmd('cat /Volumes/disk0s4/Nuova/prova.txt','SAME_SYSTEM_CORE');g.cmd('cp /Volumes/disk0s1/proof.bin /Volumes/disk0s4/large.bin');g.cmd('diskutil list','TMCFS2')
   ok('Actual Utility Disco creates new TMCFS2 in verified unallocated space, recognizes TMCFS2 and Mostra nel Finder opens it; real directory/file writes and 65537-byte copy work')
   paths=['Documents']+['/Volumes/disk0s'+str(i) for i in range(1,5)]
   for i,base in enumerate(paths):
    for cmd in ['echo CONTENT_'+str(i)+' > '+base+'/prima.txt','mkdir '+base+'/prima-cartella','echo CHILD_'+str(i)+' > '+base+'/prima-cartella/interno.txt']:g.cmd(cmd)
    if i==3:g.cmd('echo KEEP_ORIGINAL > '+base+'/occupato.txt')
    g.cmd('open '+base);g.visible('prima.txt');rename(g,'prima.txt','dopo.txt',click=i==0,collision=i==3);rename(g,'prima-cartella','dopo-cartella')
    if i==4:g.shot('tmcfs2-rename-dialog-result')
    g.key('ctrl-w');terminal(g);g.cmd('cat '+base+'/dopo.txt','CONTENT_'+str(i));g.cmd('cat '+base+'/dopo-cartella/interno.txt','CHILD_'+str(i))
    if i==3:g.cmd('cat '+base+'/occupato.txt','KEEP_ORIGINAL')
    ok('Actual Rinomina context menu + editable dialog rename file and directory on '+['native home','FAT32','NTFS','exFAT','new TMCFS2'][i]+', readback of file and nested child succeeds')
   g.cmd('off');g.p.wait(timeout=20)
 with disk.open('rb') as f:after=f.read(512)
 assert after[:494]==mbr[:494] and after[510:]==mbr[510:];start,count=struct.unpack_from('<II',after,502);assert start==at and count>65536 and after[498]==0xda
 with disk.open('rb') as f:f.seek(start*512);assert f.read(8)==b'TMCBFS2\0'
 extractor=w/'extract';run('gcc','-O2','-fno-builtin','-I'+str(root/'include'),root/'tests/volume_export_fixture.c',root/'storage/tmcbfs2.c',root/'crypto/sha256.c',root/'kernel/string.c','-o',extractor)
 extracted=w/'large-after.bin';run(extractor,'extract',disk,start,count,'large.bin',extracted);assert extracted.read_bytes()==proof.read_bytes()
 child=w/'child-after.txt';run(extractor,'extract',disk,start,count,'dopo-cartella/interno.txt',child);assert child.read_bytes()==b'CHILD_4\n'
 for i,name in enumerate(['fat32','ntfs','exfat']):
  start,count=struct.unpack_from('<II',after,454+i*16);p=w/(name+'-after.img')
  with disk.open('rb') as src,p.open('wb') as dst:src.seek(start*512);dst.write(src.read(count*512))
  run(*(['fsck.fat','-n',p] if i==0 else ['ntfsresize','-i','-f',p] if i==1 else ['fsck.exfat','-n',p]))
 ok('After actual guest poweroff, same shipping TMCFS2 core mounts created partition and independently extracts exact 65537-byte payload + renamed child; original 3 partition entries unchanged; independent foreign fsck passes')
 r['passed']=True
except Exception as e:r['failure']=str(e);raise
finally:(qa/'rename-tmcfs2-guest-result.json').write_text(json.dumps(r,indent=2)+'\n')
