"""Final dual-mode optical package smoke: UEFI and BIOS/OpenDuet, real SATA.
The full filesystem matrix uses the separately shipped exact native UEFI ISO;
independent extracted-file hashes connect both media to identical OS payloads.
"""
from pathlib import Path
import hashlib,json,os,struct,tempfile
from retail_guest import Retail
import pixels,retail_guest
base=Path(os.environ['TMC_QA_DIR']);iso=Path(os.environ['TMC_DUAL_ISO']);root=Path(__file__).resolve().parents[1]
w=Path(tempfile.mkdtemp(prefix='tmc-release-optical-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
old=retail_guest.locate;retail_guest.locate=lambda im,text,faces=(0,1,2,3,5,6,8),threshold=.88:old(im,text,faces,threshold)
os.environ.update(TMC_STORAGE='sata',TMC_MEM='1024',TMC_SMP='2',TMC_POINTER='tablet',TMC_BIOS_ISO=str(iso),OCR_ROOT=os.environ['TOOLROOT'])
r={'passed':False,'work':str(w),'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'physical_hardware_tested':False,'checks':[]}
try:
 for firmware in ['uefi','bios']:
  with Retail(iso,w,ev,'release-optical-'+firmware,disk=firmware,live=False,launcher=root/'tools/run-bios-installed.sh' if firmware=='bios' else None) as g:
   g.enroll(name='OpticalQA');g.key('f2');g.visible('Sessione locale');g.cmd('diskutil list','disk0');g.cmd('echo OPTICAL_MEDIA_VERIFIED > Documents/media.txt');g.cmd('cat Documents/media.txt','OPTICAL_MEDIA_VERIFIED');g.shot('release-optical-'+firmware);g.cmd('off');g.p.wait(timeout=20)
  r['checks'].append(firmware+': exact dual-mode ISO reaches native desktop, account, Terminal and real AHCI inventory; live document write/read and shutdown');print('PASS optical',firmware,flush=True)
 r['passed']=True
except Exception as e:r['failure']=str(e);raise
finally:(base/'release-optical-guest-result.json').write_text(json.dumps(r,indent=2)+'\n')
