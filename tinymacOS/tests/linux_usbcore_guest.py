#!/usr/bin/env python3
"""Original canonical USB core/HCD/hub + opaque original xHCI/EHCI + Boot.
Read-only guest observation; actual rendered login and cursor. Detects original usbhid,
not physical hardware, not release/install acceptance. No PS/2 input.
"""
from pathlib import Path
import hashlib,json,os,struct,subprocess,sys,tempfile,time
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tests'))
from retail_guest import Retail
from input_pixels import cursor
fw=sys.argv[1];profile=sys.argv[2]
assert fw in ('uefi','bios')
assert profile in ('xhci-two-mice-kbd','ehci-kbd','xhci-hub-kbd','xhci-tablet-kbd')
stage=Path(os.environ.get('TMC_CANONICAL_STAGE','/var/tmp/tmc-10.1.7/linux-usbcore-live'))
build=stage/'build'
iso=Path(os.environ.get('TMC_CANONICAL_ISO',str(build/'tinymacOS-10.1.7-x86_64.iso')))
work=Path(tempfile.mkdtemp(prefix='tmc-canonical-'+fw+'-',dir='/var/tmp'));ev=work/'evidence';ev.mkdir()
toolroot=os.environ.get('TOOLROOT','/var/tmp/tmc/tools/root')
os.environ.update(TOOLROOT=toolroot,OCR_ROOT=os.environ.get('OCR_ROOT',toolroot),TMC_MEM='512',TMC_BUILD=str(build),TMC_TEST_INPUT=profile,TMC_TEST_FIRMWARE=fw,TMC_POINTER='tablet' if profile=='xhci-tablet-kbd' else 'mouse')
symbols=subprocess.check_output(['nm',str(build/'kernel.debug')],text=True).splitlines()
def address(name):return int(next(s.split()[0] for s in symbols if s.endswith(' '+name)),16)
def sha(p):return hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
full_hid=any(s.endswith(' tmc_usbcore_usbhid_probe') for s in symbols)
# usbhid probe is local; use exported parser and input registration symbols too.
full_hid=all(any(s.endswith(' '+n) for s in symbols) for n in ('tmc_usbcore_hid_parse_report','tmc_usbcore_hidinput_connect','tmc_usbcore_hid_add_device'))
r={'passed':False,'work':str(work),'firmware':fw,'profile':profile,'physical_hardware_tested':False,'usbhid_tested':full_hid,'release_gate':False,'iso_sha256':sha(iso),'kernelcache_sha256':sha(build/'KernelCache'),'checks':[]}
try:
 for name in ('tmc_usbcore_usb_hcd_submit_urb','tmc_usbcore_usb_new_device','tmc_usbcore_usb_alloc_dev','tmc_usbcore_usb_register_driver','tmc_usb_stack_init'):address(name)
 # Extract the actual boot media's cache, not an unrelated kernel image.
 tool=os.environ['TOOLROOT'];env={**os.environ,'PATH':tool+'/usr/bin:'+os.environ['PATH'],'LD_LIBRARY_PATH':tool+'/usr/lib/x86_64-linux-gnu'}
 subprocess.run(['xorriso','-osirrox','on','-indev',str(iso),'-extract','/System/Library/KernelCollections/KernelCache',str(work/'actual.cache')],env=env,check=True,capture_output=True)
 assert (work/'actual.cache').read_bytes()==(build/'KernelCache').read_bytes()
 r['actual_iso_kernel_equals_build']=True
 with Retail(iso,work,ev,'canonical',live=True,launcher=ROOT/'tools/run-input-qa.sh') as g:
  def memory(n,size):
   p=work/'memory.bin';g.req('pmemsave',val=address(n),size=size,filename=str(p));return p.read_bytes()
  def counts():return list(struct.unpack('<6Q',memory('tmc_usb_canonical_counters',48)))
  def native():
   assert not any(s.endswith(' tmc_linux_usb_stats') for s in symbols),'retired native facade must not be linked'
   return [0]*9
  def wait_bound(keyboards,pointers):
   deadline=time.monotonic()+35
   while time.monotonic()<deadline:
    if counts()[4:]==[keyboards,pointers]:return
    time.sleep(.12)
   raise AssertionError(('canonical input registration',counts(),keyboards,pointers))
  def controller():
   n='ehci_stats' if profile=='ehci-kbd' else 'xhci_stats'
   fields=['controllers','submissions','givebacks','unlinks','map_failures','irq_polls','schedule_polls','timer_fires','root_polls','errors']
   return dict(zip(fields,struct.unpack('<10Q',memory(n,80))))
  pointers=2 if profile=='xhci-two-mice-kbd' else 1
  wait_bound(1,pointers)
  r['before']=before=counts();r['controller_before']=controller()
  assert before[0]==1 and before[3]==0,before
  # The historical native class facade must NOT own any devices in this mode.
  assert native()==[0]*9
  r['checks'].append('Canonical core/HCD/hub and original '+('HID-core/usbhid/hid-input/generic' if full_hid else 'Boot')+' binding; native facade inactive')
  if full_hid:
   log=memory('tmc_usb_canonical_log',16384)
   assert b'registered new interface driver usbhid' in log
   assert b'registered new interface driver usbkbd' not in log and b'registered new interface driver usbmouse' not in log
   r['worker_polls_before']=struct.unpack('<Q',memory('tmc_usb_canonical_counters',56)[48:])[0]
   assert r['worker_polls_before']>0
  g.enroll('Canonico');g.key('f2');g.visible('Sessione locale')
  cursor(g,650,182)
  r['checks'].append('USB-only Italian keyboard completes rendered enrollment and opens Terminal; actual cursor sprite verified at pixel coordinates')
  r['before_unplug']=counts()
  # Disconnect a real virtual device while URBs are armed; reattach on the
  # same port. This exercises original hub disconnect, cancel and re-probe.
  victim='pointer2' if pointers==2 else 'pointer'
  port='2' if pointers==2 else '1.3' if profile=='xhci-hub-kbd' else '1'
  bus='ehci.0' if profile=='ehci-kbd' else 'xhci.0'
  g.req('device_del',id=victim);wait_bound(1,pointers-1)
  r['after_unplug']=counts()
  g.req('device_add',driver='usb-tablet' if profile=='xhci-tablet-kbd' else 'usb-mouse',id=victim,bus=bus,port=port,usb_version=1 if profile=='xhci-hub-kbd' else 2)
  wait_bound(1,pointers)
  g.key('f2');g.visible('Sessione locale')
  cursor(g,700,190)
  r['checks'].append('Armed interrupt URB hot unplug drains original class/HCD teardown; same-port reattach re-enumerates and rendered input remains functional')
  r['after']=after=counts();r['controller_after']=ca=controller()
  assert after[1]>before[1] and after[2]>before[2] and after[3]==0,after
  assert ca['givebacks']>r['controller_before']['givebacks']>0,ca
  assert ca['controllers']>=1 and not ca['errors'] and not ca['map_failures'],ca
  assert native()==[0]*9
  r['usb']=g.hmp('info usb');g.shot('canonical-live-input')
  (work/'canonical-log.txt').write_bytes(memory('tmc_usb_canonical_log',16384).rstrip(b'\0'))
  if full_hid:
   r['worker_polls_after']=struct.unpack('<Q',memory('tmc_usb_canonical_counters',56)[48:])[0]
   assert r['worker_polls_after']>r['worker_polls_before']
  r['passed']=True
 print('PASS canonical USB guest',fw,profile,r['after'],flush=True)
finally:
 (work/'result.json').write_text(json.dumps(r,indent=2)+'\n')
 # Raw PPMs are test intermediates; retain compressed screenshots/logs/receipt.
 for p in work.glob('*.ppm'):p.unlink()
 for p in (work/'actual.cache',work/'memory.bin'):p.unlink(missing_ok=True)
 print('RESULT',work,flush=True)
 if os.environ.get('TMC_CANONICAL_RECEIPT'):
  Path(os.environ['TMC_CANONICAL_RECEIPT']).write_text(json.dumps(r,indent=2)+'\n')
