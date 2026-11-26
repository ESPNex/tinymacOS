"""Actual ISO read-only Windows-like GPT preflight, not Windows certification.
Synthetic EFI/MSR/two NTFS signatures + 10GiB real gap. Never use real disks.
"""
from pathlib import Path
import os,struct,json,tempfile,hashlib,zlib,uuid
from retail_guest import Retail
base=Path(os.environ['TMC_QA_DIR']);iso=Path(os.environ['TMC_STAGE_ISO']);w=Path(tempfile.mkdtemp(prefix='tmc-hotfix-windows-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir();total=32*1024*2048;rec=total-1048576-2048;gap=rec-10*1024*2048
os.environ.update(TMC_STORAGE='nvme',TMC_POINTER='mouse',TMC_SMP='2',TMC_MEM='512',OCR_ROOT=os.environ['TOOLROOT'])
record={'passed':False,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'synthetic_Windows_layout_only':True,'physical_hardware_or_real_Windows_tested':False,'work':str(w),'cases':[]}
def fixture(path,degraded):
 path.parent.mkdir(exist_ok=True);parts=bytearray(16384);ranges=[(2048,206847),(206848,239615),(239616,gap-1),(rec,total-2049)];types=['C12A7328-F81F-11D2-BA4B-00A0C93EC93B','E3C9E316-0B5C-4DB8-817D-F92DF00215AE']+['EBD0A0A2-B9E5-4433-87C0-68B6B72699C7']*2
 for i,((a,b),typ) in enumerate(zip(ranges,types)):
  name=['EFI','Microsoft Reserved','Windows','Recovery'][i];struct.pack_into('<16s16sQQQ72s',parts,i*128,uuid.UUID(typ).bytes_le,uuid.UUID(int=i+1).bytes_le,a,b,0,name.encode('utf-16le'))
 def header(at,other,entries,entry_bytes):
  h=bytearray(512);struct.pack_into('<8sIIIIQQQQ16sQIII',h,0,b'EFI PART',65536,92,0,0,at,other,34,total-34,uuid.UUID(int=12345).bytes_le,entries,128,128,zlib.crc32(entry_bytes));struct.pack_into('<I',h,16,zlib.crc32(h[:92]));return h
 m=bytearray(512);m[510:]=b'\x55\xaa';m[450]=0xee;struct.pack_into('<II',m,454,1,total-1);a=header(1,total-1,2,parts);mirror=bytearray(parts)
 # Two individually CRC-valid but discordant copies. A bad backup CRC alone
 # is automatically repaired by OVMF before the OS; that is not an OS test.
 if degraded:mirror[2*128+48]^=1
 b=header(total-1,1,total-33,mirror)
 with path.open('wb') as f:
  f.truncate(total*512)
  for at,data in [(0,m),(1,a),(2,parts),(total-33,mirror),(total-1,b)]:f.seek(at*512);f.write(data)
  for index in (2,3):
   start,end=ranges[index];boot=bytearray(512);boot[3:11]=b'NTFS    ';struct.pack_into('<H',boot,11,512);boot[13]=8;struct.pack_into('<Q',boot,40,end-start+1);struct.pack_into('<Q',boot,48,4);boot[510:]=b'\x55\xaa';f.seek(start*512);f.write(boot)
 return ranges
# Hash all metadata and occupied-extent boundaries; guest does preflight only.
def fingerprints(path,ranges):
 with path.open('rb') as f:
  result={}
  for l,n in [(0,34),(total-33,33)]+[(l,8) for a,b in ranges for l in (a,b-7)]:f.seek(l*512);result[str(l)]=hashlib.sha256(f.read(n*512)).hexdigest()
  return result
try:
 for degraded in (False,True):
  tag='degraded' if degraded else 'healthy';disk=w/tag/'system.raw';ranges=fixture(disk,degraded);before=fingerprints(disk,ranges)
  with Retail(iso,w,ev,tag,disk=tag,live=False) as g:
   g.enroll(name='WinGap');g.key('f2');g.visible('Sessione locale');g.cmd('diskutil list');g.visible('10240 MiB');g.visible('Microsoft Reserved');g.shot(tag+'-disk-inventory')
   if degraded:g.visible('Protetto:')
   g.key('f11');g.visible('Installa tinymacOS Tramontana');g.key('ret');g.visible('Seleziona la destinazione');g.visible('10240 MiB non allocati');g.shot(tag+'-installer-gap')
   if degraded:g.visible('Non disponibile')
   else:g.key('down');g.key('ret');g.visible('Conferma l’installazione');g.shot('healthy-gap-selectable-confirmation-no-write')
  assert fingerprints(disk,ranges)==before
  record['cases'].append({'degraded':degraded,'actual_diskutil_and_installer_show_10GiB_gap':True,'MSR_preserved_not_free':True,'selection_allowed':not degraded,'metadata_and_occupied_boundary_hashes_unchanged':True})
 record['passed']=True
finally:(base/'hotfix-windows-guest.json').write_text(json.dumps(record,indent=2)+'\n')
print('PASS native read-only GPT Windows-like gap visibility, healthy selection and degraded protection; not real-Windows certification')
