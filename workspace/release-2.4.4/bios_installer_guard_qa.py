#!/usr/bin/env python3
"""A dirty byte inside the intended OpenDuet gap must prevent EVERY write."""
import hashlib,json,os,sys,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root/'tinymacOS/tests'))
from retail_guest import Retail
iso=Path('/var/tmp/tmc-oc2-stage/tinymacOS-2.4.4-OpenCore-Installer-x86_64.iso');w=Path(tempfile.mkdtemp(prefix='tmc-bios-dirty-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir();(w/'target').mkdir();disk=w/'target/system.raw'
with disk.open('xb') as f:f.truncate(96<<20);f.seek(64*512);f.write(b'X')
def digest():return hashlib.file_digest(disk.open('rb'),'sha256').hexdigest()
before=digest();os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_BIOS_ISO=str(iso),TMC_POINTER='mouse',TMC_SMP='2')
record={'passed':False,'dirty_lba':64,'original_sha256':before,'external_evidence':str(ev)}
try:
 with Retail(iso,w,ev,'guard',disk='target',live=False,launcher=root/'tinymacOS/tools/run-bios-installed.sh') as g:
  g.enroll(name='Guard');g.key('f11');g.visible('Introduzione');g.key('ret');g.key('down');g.key('ret');g.visible('Conferma l’installazione');g.key('ret');g.key('spc');g.key('ret');g.visible('Installazione interrotta',45);g.visible('Il disco contiene dati');g.shot('dirty-gap-refused')
 after=digest();assert before==after;record.update(passed=True,after_sha256=after,entire_disk_unchanged=True,gui_refusal=True)
finally:(Path(__file__).parent/'bios-installer-guard-result.json').write_text(json.dumps(record,indent=2)+'\n')
print('PASS dirty reserved gap: real installer refusal, whole 96 MiB target unchanged byte-for-byte')
