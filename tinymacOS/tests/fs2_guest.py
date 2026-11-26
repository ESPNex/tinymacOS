"""Opt-in TMCBFS2 guest gate. Use --prepare for a fresh disposable fixture.
Build with TMC_FS2_BACKEND=1 first. Host formatting is explicitly NOT proof of
installer support. Never attach physical disks or use a user-owned fixture.
"""
from pathlib import Path
import hashlib,json,os,struct,time,sys,tempfile,subprocess
from retail_guest import Retail
import pixels,retail_guest
root=Path(__file__).resolve().parents[1];base=Path(os.environ['TMC_QA_DIR']);build=Path(os.environ['TMC_BUILD'])
if '--prepare' in sys.argv:
 old=json.loads((base/'nvme-guest-result.json').read_text());source=Path(old['work'])/'target/system.raw';assert str(source).startswith('/var/tmp/tmc-nvme-guest-') and source.is_file() and source.stat().st_size<1<<30
 work=Path(tempfile.mkdtemp(prefix='tmc-fs2-guest-',dir='/var/tmp'));(work/'target').mkdir();(work/'evidence').mkdir();target=work/'target/system.raw';subprocess.run(['cp','--sparse=always',str(source),str(target)],check=True)
 with target.open('rb') as f:f.seek(1024+128);entry=f.read(128)
 start,end=struct.unpack_from('<QQ',entry,32)
 exe=work/'seed';subprocess.run(['gcc','-O2','-fno-builtin','-I'+str(root/'include'),str(root/'tests/fs2_seed_fixture.c'),str(root/'storage/tmcbfs2.c'),str(root/'kernel/string.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe),str(target),str(start),str(end-start+1)],check=True)
 (base/'fs2-fixture.json').write_text(json.dumps({'work':str(work),'data_start':start,'data_sectors':end-start+1,'source':str(source),'preparation':'host-formatted fixture, not GUI installer acceptance'},indent=2)+'\n')
fixture=json.loads((base/'fs2-fixture.json').read_text());w=Path(fixture['work']);assert str(w).startswith('/var/tmp/tmc-fs2-guest-');ev=w/'evidence'
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
old=retail_guest.locate;retail_guest.locate=lambda im,text,faces=(0,1,2,3,5,6,8),threshold=.88:old(im,text,faces,threshold)
os.environ.update(TMC_STORAGE='nvme',TMC_MEM='512',TMC_SMP='2',TMC_POINTER='tablet',OCR_ROOT=os.environ['TOOLROOT'])
payload=(build/'install.esp').read_bytes();disk=w/'target/system.raw'
with disk.open('r+b') as f:f.seek(2048*512);f.write(payload);f.flush();os.fsync(f.fileno());f.seek(2048*512);assert f.read(len(payload))==payload
b=bytearray(b'X'*8192);proof=bytearray()
for i in range(16):
 if not i:b[:23]=b'TMCBFS2_FILE_EXCEEDS_8K\n'[:23]
 if i==15:b[-32:-10]=b'TMCBFS2_END_AFTER_128K\n'[:22]
 proof.extend(b)
expected=hashlib.sha256(proof).hexdigest()
result={'passed':False,'fixture':fixture,'scope':'actual opt-in kernel mounts real TMCBFS2, native NVMe; host prepared data extent, NOT installer acceptance','installer_formats_v2':False,'expected_128KiB_sha256':expected,'esp_sha256':hashlib.sha256(payload).hexdigest(),'kernel_sha256':hashlib.file_digest((build/'KernelCache').open('rb'),'sha256').hexdigest()}
class Installed(Retail):initial='Fs2Disk'
try:
 with Retail('/not-attached',w,ev,'fs2-initial',disk='target',boot_disk=True) as g:
  g.enroll(name='Fs2Disk');g.key('f2');g.visible('Sessione locale');g.cmd('bootverbose log','[fs2] mounted full data extent');g.cmd('shasum -a 256 Documents/large-proof.txt',expected,timeout=180);g.shot('native-fs2-128k-stream-hash')
  g.cmd('echo GUEST_FS2_PERSISTENCE > Documents/guest.txt');g.cmd('diskutil apfs snapshot create guest-checkpoint','Snapshot creato.');g.cmd('echo CHANGED > Documents/guest.txt');g.cmd('diskutil apfs snapshot restore guest-checkpoint','Volume ripristinato.');g.cmd('cat Documents/guest.txt','GUEST_FS2_PERSISTENCE');g.shot('native-fs2-cow-restore');g.cmd('off');g.p.wait(timeout=20)
 with Installed('/not-attached',w,ev,'fs2-reboot',disk='target',boot_disk=True) as g:
  g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd('cat Documents/guest.txt','GUEST_FS2_PERSISTENCE');g.cmd('shasum -a 256 Documents/large-proof.txt',expected,timeout=180);g.shot('native-fs2-reboot-account-persistence');g.cmd('off');g.p.wait(timeout=20)
 result['passed']=True
except Exception as error:result['failure']=str(error);raise
finally:(base/'fs2-guest-result.json').write_text(json.dumps(result,indent=2)+'\n')
print('PASS-NATIVE-FS2-GUEST-128K-STREAM-COW-AND-REBOOT',flush=True)
