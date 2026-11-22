#!/usr/bin/env python3
"""Resume this release's interrupted suite; skip only successful same-ISO receipts.
Never imports historical results. A failed prior attempt is retained separately.
"""
import hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
q=Path(__file__).resolve().parent;home=q.parents[1];media=Path('/var/tmp/tmc-10.1.6');iso=media/'tinymacOS-10.1.6-x86_64.iso';digest=hashlib.file_digest(iso.open('rb'),'sha256').hexdigest()
env={**os.environ,'TOOLROOT':'/var/tmp/tmc/tools/root','OCR_ROOT':'/var/tmp/tmc/tools/root','TMC_MEM':'512','TMC_BUILD':str(media/'build'),'TMC_TEST_ISO':str(iso)}
def good(name):
 p=q/name
 if not p.exists():return False
 r=json.loads(p.read_text());return r.get('passed') is True and r.get('iso_sha256',digest)==digest
for fw in ['uefi','bios']:
 profiles=['ps2','ehci','companion','xhci','xhci-hub','xhci-hub-kbd','xhci-nested-hub']
 if fw=='uefi':profiles+=['uhci','ehci-kbd','companion-kbd']
 for profile in profiles:
  name=f'input-{fw}-{profile}.json'
  if good(name):continue
  result=q/name
  if result.exists():
   attempts=q/'attempts';attempts.mkdir(exist_ok=True);shutil.copyfile(result,attempts/('first-'+name));log=media/f'input-{fw}-{profile}.log'
   if log.exists():shutil.copyfile(log,attempts/(log.stem+'-first.log'))
  log=media/f'input-{fw}-{profile}-resume.log'
  with log.open('w') as f:subprocess.run([sys.executable,'tinymacOS/tests/input2_acceptance.py'],cwd=home,env={**env,'TMC_TEST_FIRMWARE':fw,'TMC_TEST_INPUT':profile,'TMC_INPUT_RESULT':str(result)},stdout=f,stderr=subprocess.STDOUT,check=True)
  assert good(name);print('PASS live',fw,profile,flush=True)
install=json.loads((q/'bios-installer-result.json').read_text());assert install['passed'] and install['iso_sha256']==digest
env['TMC_INPUT_DISK']=install['external_work']+'/target/system.raw'
for fw in ['uefi','bios']:
 for profile in ['ps2','ehci','companion','xhci-hub','xhci-nested-hub']:
  name=f'installed-{fw}-{profile}.json'
  if good(name):continue
  with (media/f'installed-{fw}-{profile}.log').open('w') as f:subprocess.run([sys.executable,'tinymacOS/tests/input2_acceptance.py'],cwd=home,env={**env,'TMC_TEST_FIRMWARE':fw,'TMC_TEST_INPUT':profile,'TMC_INPUT_RESULT':str(q/name)},stdout=f,stderr=subprocess.STDOUT,check=True)
  assert good(name);print('PASS HDD only',fw,profile,flush=True)
env.pop('TMC_INPUT_DISK')
if not good('bios-recovery-result.json'):
 with (media/'recovery.log').open('w') as f:subprocess.run([sys.executable,str(q/'bios_recovery_qa.py')],cwd=home,env=env,stdout=f,stderr=subprocess.STDOUT,check=True)
 print('PASS local sealed Recovery restore',flush=True)
for fw in ['uefi','bios']:
 for profile in ['ps2','xhci-hub-kbd']:
  name=f'recovery-keyboard-{fw}-{profile}.json'
  if good(name):continue
  with (media/f'recovery-kbd-{fw}-{profile}.log').open('w') as f:subprocess.run([sys.executable,str(q/'recovery_keyboard_qa.py')],cwd=home,env={**env,'TMC_TEST_FIRMWARE':fw,'TMC_TEST_INPUT':profile},stdout=f,stderr=subprocess.STDOUT,check=True)
  assert good(name);print('PASS Italian Recovery',fw,profile,flush=True)
