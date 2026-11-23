#!/usr/bin/env python3
"""Execute the real upstream usbkbd IRQ and LED control in the actual ISO.
USB-only virtual keyboard (i8042 off); no diagnostic guest UI or test decoder.
Native controller transport is explicitly NOT a Linux HCD/core port.
"""
from pathlib import Path
import hashlib,json,os,struct,subprocess,sys,tempfile,time
q=Path(__file__).resolve().parent;home=q.parents[1];root=home/'tinymacOS';sys.path.insert(0,str(root/'tests'))
from retail_guest import Retail
fw,profile=sys.argv[1:];assert fw in ['uefi','bios'];assert profile in ['ehci-kbd','companion-kbd','xhci-hub-kbd']
base=Path(os.environ.get('TMC_LINUX_STAGE','/var/tmp/tmc-10.1.7/linux-ps2-v8'));build=base/'build';iso=base/'tinymacOS-10.1.7-x86_64.iso'
w=Path(tempfile.mkdtemp(prefix=f'tmc-linux-kbd-{fw}-{profile}-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',OCR_ROOT='/var/tmp/tmc/tools/root',TMC_MEM='512',TMC_BUILD=str(build),TMC_TEST_INPUT=profile,TMC_TEST_FIRMWARE=fw,TMC_POINTER='mouse')
r={'passed':False,'firmware':fw,'profile':profile,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'work':str(w),'actual_upstream_boot_keyboard_driver':True,'linux_hcd_port':False,'linux_full_usb_core_port':False,'native_hcd_transport_adapter':True,'physical_hardware_tested':False}
fields=['probes','attached','completions','frames','rejected','disconnected','controls','control_errors','stale_controls']
try:
 symbols=subprocess.check_output(['nm',str(build/'kernel.debug')],text=True).splitlines()
 addr=int(next(s.split()[0] for s in symbols if s.endswith(' b tmc_linux_kbd_stats')),16)
 for name in ['usb_kbd_irq','usb_kbd_probe','usb_kbd_event','usb_kbd_led','usb_kbd_disconnect','tmc_lx_usbkbd_driver']:
  assert any(s.endswith(' '+name) for s in symbols),name
 tool='/var/tmp/tmc/tools/root';env={**os.environ,'PATH':tool+'/usr/bin:'+os.environ['PATH'],'LD_LIBRARY_PATH':tool+'/usr/lib/x86_64-linux-gnu'}
 subprocess.run(['xorriso','-osirrox','on','-indev',str(iso),'-extract','/System/Library/KernelCollections/KernelCache',str(w/'actual.cache')],env=env,check=True,capture_output=True)
 assert (w/'actual.cache').read_bytes()==(build/'KernelCache').read_bytes();r['actual_iso_kernel_equals_build']=True
 with Retail(iso,w,ev,'linux-usbkbd',live=True,launcher=root/'tools/run-input-qa.sh') as g:
  r['usb']=g.hmp('info usb');assert 'USB Keyboard' in r['usb']
  def counters():
   p=w/'kbd-stats.bin';g.req('pmemsave',val=addr,size=8*len(fields),filename=str(p));return dict(zip(fields,struct.unpack('<9Q',p.read_bytes())))
  def urb_counters():
   address=int(next(s.split()[0] for s in symbols if s.endswith(' b tmc_linux_urb_counters')),16)
   p=w/'urb-counters.bin';g.req('pmemsave',val=address,size=48,filename=str(p))
   return dict(zip(['submits','givebacks','unlinks','rejected','active','highwater'],struct.unpack('<6Q',p.read_bytes())))
  for name in ['usb_submit_urb','usb_init_urb','usb_get_urb','usb_kill_urb','usb_poison_anchored_urbs']:
   assert any(s.endswith(' '+name) for s in symbols),name
  r['urb_core_before']=urb_counters()
  r['before']=before=counters();assert before['attached']==1,before
  g.enroll('PortLinux');g.key('f2');g.visible('Sessione locale')
  g.cmd('echo ITA-àèéìòù-£-€-@-#','ITA-àèéìòù-£-€-@-#')
  g.key('caps_lock');g.type('echo caps');g.key('caps_lock');g.key('ret');g.visible('ECHO')
  time.sleep(.5);r['after']=after=counters()
  assert after['completions']>before['completions']+50 and after['frames']==after['completions'],(before,after)
  assert after['controls']>=before['controls']+2 and not after['control_errors'] and not after['stale_controls'],(before,after)
  r['urb_core_after']=core=urb_counters();previous=r['urb_core_before']
  assert core['submits']>previous['submits'] and core['givebacks']>previous['givebacks'],(previous,core)
  assert core['submits']==core['givebacks']+core['active'] and core['active']>=1 and not core['rejected'],core
  r['actual_upstream_core_urb_file_executed']=True
  g.shot('linux-usbkbd-italian-leds');r['passed']=True
  r['checks']=['USB-only keyboard with virtual i8042 disabled','actual ISO KernelCache equals linked build','actual upstream probe/IRQ/event/LED/disconnect symbols','enrollment + F2 + rendered Italian Shift/AltGr/accent/euro command','Caps Lock on/off triggers two successful native EP0 LED controls']
 print('PASS actual Linux usbkbd IRQ + Italian input + LED controls',fw,profile,r['after'],flush=True)
finally:(q/f'linux-usbkbd-{fw}-{profile}.json').write_text(json.dumps(r,indent=2,ensure_ascii=False)+'\n')
