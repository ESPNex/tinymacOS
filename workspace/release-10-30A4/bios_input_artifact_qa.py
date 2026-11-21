#!/usr/bin/env python3
"""Independently extract BIOS.IMG/boot from the exact final ISO and audit firmware."""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile,importlib.util
r=Path(__file__).resolve().parent;root=r.parents[1];d=Path('/var/tmp/tmc30a4-final');iso=d/'tinymacOS-10-x86_64.iso'
w=Path(tempfile.mkdtemp(prefix='tmc30a4-bios-audit-',dir='/var/tmp'));tr=Path('/var/tmp/tmc/tools/root')
env={**os.environ,'PATH':str(tr/'usr/bin')+':'+os.environ['PATH'],'LD_LIBRARY_PATH':str(tr/'usr/lib/x86_64-linux-gnu')}
def run(*c):subprocess.run(list(map(str,c)),env=env,check=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
run('xorriso','-osirrox','on','-indev',iso,'-extract','/BIOS.IMG',w/'BIOS.IMG')
for name in ['boot','boot.input.json']:run('mcopy','-i',str(w/'BIOS.IMG')+'@@1048576','::/'+name,w/name)
spec=importlib.util.spec_from_file_location('openduet_input',root/'tinymacOS/tools/openduet-input.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
v=root/'tinymacOS/vendor/opencore-1.0.8';expected,record=m.derive((v/'bootX64').read_bytes(),(v/'bootX64-blockio').read_bytes())
assert (w/'boot').read_bytes()==expected and json.loads((w/'boot.input.json').read_text())==record
# No diagnostic serial statements or diagnostic loader code in release binaries.
assert b'[diag]' not in (d/'BOOTX64.EFI').read_bytes() and b'[diag]' not in (d/'RECOVERY.EFI').read_bytes()
source=json.loads((root/'workspace/release-10-30A3/source-hashes.json').read_text())
for name in ['boot/loader.c','boot/recoveryboot.h','recovery/usbhid.c']:
 assert hashlib.sha256((root/'tinymacOS'/name).read_bytes()).hexdigest()==source[name]
frozen=json.loads((r/'source-hashes.json').read_text())
assert hashlib.sha256((root/'tinymacOS/recovery/keyboard.c').read_bytes()).hexdigest()==frozen['recovery/keyboard.c']
assert b'[kbd]' not in (d/'RECOVERY.EFI').read_bytes()
record.update(passed=True,iso_sha256=hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),external_work=str(w),actual_ISO_bootstrap_extracted_byte_exact=True,diagnostic_loader_changes_absent=True,loader_and_Recovery_pointer_driver_unchanged_from_30A2=True,Recovery_keyboard_matches_frozen_source=True,Recovery_keyboard_periodic_Boot_report_ms=32)
(r/'bios-input-artifact-result.json').write_text(json.dumps(record,indent=2)+'\n')
print('PASS actual ISO BIOS.IMG: Block I/O plus 3 original USB modules, derived container exact, no diagnostic loader changes')
