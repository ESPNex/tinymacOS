"""Final-ISO GUI installation with missing PMBR 55AA and intact Windows-like GPT.
Synthetic disposable disk only; never a physical Windows certification.
"""
from pathlib import Path
import ast,os,struct,json,tempfile,hashlib,zlib,uuid
from retail_guest import Retail
base=Path(os.environ['TMC_QA_DIR']);iso=Path(os.environ['TMC_STAGE_ISO'])
w=Path(tempfile.mkdtemp(prefix='tmc-pmbr-signature-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
total=32*1024*2048;rec=total-1048576-2048;gap=rec-10*1024*2048
module=ast.parse(Path(__file__).with_name('hotfix_windows_guest.py').read_text())
exec(compile(ast.Module(body=[n for n in module.body if isinstance(n,ast.FunctionDef) and n.name in ('fixture','fingerprints')],type_ignores=[]),'windows_fixture','exec'))
os.environ.update(TMC_STORAGE='nvme',TMC_POINTER='mouse',TMC_SMP='2',TMC_MEM='512',OCR_ROOT=os.environ['TOOLROOT'])
r={'passed':False,'work':str(w),'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'synthetic_layout_only':True,'physical_Windows_tested':False,'firmware_repaired_before_OS':False,'checks':[]}
disk=w/'target/system.raw';ranges=fixture(disk,False)
with disk.open('r+b') as f:
 old=bytearray(f.read(512));old[510:512]=b'\0\0';f.seek(0);f.write(old);f.seek(1024);original_entries=f.read(512)
before=fingerprints(disk,ranges)
def occupied_boundaries():
 result=[]
 with disk.open('rb') as f:
  for a,b in ranges:
   for start in (a,b-7):f.seek(start*512);result.append(hashlib.sha256(f.read(4096)).hexdigest())
 return result
occupied=occupied_boundaries()
class Installed(Retail):initial='SigFix'
try:
 with Retail(iso,w,ev,'pmbr-install',disk='target',live=False) as g:
  g.enroll(name='SigFix');g.key('f2');g.visible('Sessione locale')
  with disk.open('rb') as f:f.seek(510);r['firmware_repaired_before_OS']=f.read(2)!=b'\0\0'
  assert not r['firmware_repaired_before_OS'],'Firmware repaired PMBR before OS; this cannot prove native repair'
  g.cmd('echo SIGNATURE_REPAIR_IMPORT > Documents/proof.txt')
  g.key('f11');g.visible('Installa tinymacOS Tramontana');g.key('ret');g.visible('Seleziona la destinazione');g.visible('10240 MiB non allocati');g.visible('GPT verificata; firma MBR assente');g.key('down');g.key('ret');g.visible('Conferma l’installazione');g.visible('Confermo destinazione e ripristino firma MBR');g.shot('pmbr-explicit-repair-confirmation')
  assert fingerprints(disk,ranges)==before
  g.key('esc');g.visible('Seleziona la destinazione');assert fingerprints(disk,ranges)==before
  r['checks'].append('GPT recognized despite absent 55AA; explicit repair warning shown; preflight and cancellation leave all metadata/boundaries unchanged')
  g.key('ret');g.visible('Conferma l’installazione');g.key('spc');g.key('ret');g.visible('Il disco è pronto',300);g.shot('pmbr-install-complete');g.key('ret');g.p.wait(timeout=20)
 with disk.open('rb') as f:
  m=f.read(512);assert m[510:512]==b'\x55\xaa';assert m[440:510]==bytes(old[440:510]);f.seek(1024);entries=f.read(16384);assert entries[:512]==original_entries
  start=struct.unpack_from('<Q',entries,4*128+32)[0];assert start>=gap and start<rec
  f.seek((start+10)*512);assert f.read(512)==bytes(old)
 assert occupied_boundaries()==occupied
 r['checks'].append('Confirmed real installer repairs signature only at activation, preserves PMBR entries/signature region, four existing GPT entries and occupied boundaries; exact original bad MBR saved inside new extent')
 with Installed('/not-attached',w,ev,'pmbr-cold-installed',disk='target',boot_disk=True) as g:
  g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd('sw_vers -buildVersion','30B19');g.cmd('cat Documents/proof.txt','SIGNATURE_REPAIR_IMPORT');g.shot('pmbr-cold-boot-import');g.cmd('off');g.p.wait(timeout=20)
 r['checks'].append('Cold native NVMe boot with ISO detached imports original live account/document and reports 30B19')
 r['passed']=True
except Exception as e:r['failure']=str(e);raise
finally:(base/'pmbr-signature-guest-result.json').write_text(json.dumps(r,indent=2)+'\n')
print('PASS missing PMBR signature: explicit-confirmed GUI installation in real gap, preservation, saved old MBR, cold native boot; synthetic not real Windows')
