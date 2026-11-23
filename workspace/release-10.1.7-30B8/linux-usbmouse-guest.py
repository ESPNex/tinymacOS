#!/usr/bin/env python3
"""Real post-EBS Linux usbmouse IRQ execution + rendered XY, native HCD transport.
Read only the driver's non-secret counters via QEMU physical-memory observation;
no diagnostic UI, application or replacement decoder in the guest.
"""
from pathlib import Path
import hashlib,json,os,struct,subprocess,sys,tempfile,time
q=Path(__file__).resolve().parent;home=q.parents[1];root=home/'tinymacOS';sys.path.insert(0,str(root/'tests'))
from retail_guest import Retail
from input_pixels import cursor
fw=sys.argv[1];profile=sys.argv[2];base=Path(os.environ.get('TMC_LINUX_STAGE','/var/tmp/tmc-10.1.7/linux-hid-v11'));build=base/'build';iso=base/'tinymacOS-10.1.7-x86_64.iso';w=Path(tempfile.mkdtemp(prefix='tmc-linux-usb-'+fw+'-'+profile+'-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',OCR_ROOT='/var/tmp/tmc/tools/root',TMC_MEM='512',TMC_BUILD=str(build),TMC_TEST_INPUT=profile,TMC_TEST_FIRMWARE=fw,TMC_POINTER='mouse')
r={'passed':False,'firmware':fw,'profile':profile,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'work':str(w),'actual_upstream_boot_mouse_driver':True,'linux_hcd_port':False,'linux_full_usb_core_port':False,'native_hcd_transport_adapter':True,'physical_hardware_tested':False}
fields=['probes','attached','completions','frames','errors','rejected','disconnected','allocations','frees']
try:
 symbols=subprocess.check_output(['nm',str(build/'kernel.debug')],text=True).splitlines();addr=int(next(s.split()[0] for s in symbols if s.endswith(' b tmc_linux_usb_stats')),16)
 for symbol in ['usb_mouse_irq','usb_mouse_probe','tmc_lx_usbmouse_driver']:assert any(s.endswith(' '+symbol) for s in symbols),symbol
 tool='/var/tmp/tmc/tools/root';env={**os.environ,'PATH':tool+'/usr/bin:'+os.environ['PATH'],'LD_LIBRARY_PATH':tool+'/usr/lib/x86_64-linux-gnu'}
 subprocess.run(['xorriso','-osirrox','on','-indev',str(iso),'-extract','/System/Library/KernelCollections/KernelCache',str(w/'actual.cache')],env=env,check=True,capture_output=True)
 assert (w/'actual.cache').read_bytes()==(build/'KernelCache').read_bytes();r['actual_iso_kernel_equals_build']=True
 with Retail(iso,w,ev,'linux-usbmouse',live=True,launcher=root/'tools/run-input-qa.sh') as g:
  mice=g.req('query-mice');target=next(m for m in mice if m['name']=='QEMU HID Mouse');g.hmp('mouse_set '+str(target['index']));assert [m['name'] for m in g.req('query-mice') if m['current']]==['QEMU HID Mouse'];r['usb']=g.hmp('info usb');assert 'USB Keyboard' not in r['usb']
  def counters():
   p=w/'counters.bin';g.req('pmemsave',val=addr,size=8*len(fields),filename=str(p));return dict(zip(fields,struct.unpack('<9Q',p.read_bytes())))
  def ehci_counters():
   address=int(next(s.split()[0] for s in symbols if s.endswith(' B tmc_linux_ehci_qtd_stats')),16)
   p=w/'ehci-counters.bin';g.req('pmemsave',val=address,size=24,filename=str(p))
   return dict(zip(['calls','bytes','rejected'],struct.unpack('<3Q',p.read_bytes())))
  r['ehci_qtd_before']=ehci_counters()
  def urb_counters():
   address=int(next(s.split()[0] for s in symbols if s.endswith(' b tmc_linux_urb_counters')),16)
   p=w/'urb-counters.bin';g.req('pmemsave',val=address,size=48,filename=str(p))
   return dict(zip(['submits','givebacks','unlinks','rejected','active','highwater'],struct.unpack('<6Q',p.read_bytes())))
  for name in ['usb_submit_urb','usb_init_urb','usb_get_urb','usb_kill_urb','usb_poison_anchored_urbs']:
   assert any(s.endswith(' '+name) for s in symbols),name
  for name in ['tmc_config_get_configuration','tmc_config_get_bos_descriptor','tmc_config_destroy_configuration']:
   assert any(s.endswith(' '+name) for s in symbols),name
  config_address=int(next(s.split()[0] for s in symbols if s.endswith(' b tmc_linux_config_counters')),16)
  p=w/'config-stats.bin';g.req('pmemsave',val=config_address,size=80,filename=str(p))
  r['config_core']=cfg=dict(zip(['opens','failures','descriptor_reads','configs','interfaces','endpoints','bos','notices','allocations','frees'],struct.unpack('<10Q',p.read_bytes())))
  assert cfg['opens']>=1 and cfg['descriptor_reads']>=2 and cfg['configs']>=1 and cfg['interfaces']>=1 and cfg['endpoints']>=1 and cfg['allocations']==cfg['frees'] and not cfg['failures'],cfg
  r['actual_upstream_core_config_file_executed']=True
  for name in ['hid_open_report','hid_register_report','hid_setup_resolution_multiplier','tmc_linux_hid_pointer_layout']:
   assert any(s.endswith(' '+name) for s in symbols),name
  hid_address=int(next(s.split()[0] for s in symbols if s.endswith(' b tmc_linux_hid_parser_counters')),16)
  p=w/'hid-parser-stats.bin';g.req('pmemsave',val=hid_address,size=72,filename=str(p))
  r['hid_parser']=hid=dict(zip(['opens','failures','closes','allocations','frees','live_bytes','peak_bytes','notices','busy'],struct.unpack('<9Q',p.read_bytes())))
  assert hid['opens']>=1 and hid['opens']==hid['closes'] and hid['allocations']==hid['frees'] and not hid['live_bytes'] and not hid['failures'] and not hid['busy'],hid
  r['actual_upstream_hid_parser_functions_executed']=True
  r['whole_linux_hid_core_or_hid_generic_driver']=False
  if 'nested-hub' in profile:assert cfg['opens']>=3,cfg
  elif 'hub' in profile:assert cfg['opens']>=2,cfg
  r['urb_core_before']=urb_counters()
  r['before']=before=counters();assert before['attached']>=1,before
  for x,y in [(762,482),(142,482),(142,613),(762,613),(142,613),(762,482)]:cursor(g,x,y)
  g.shot('actual-linux-boot-mouse');r['after']=after=counters()
  assert after['completions']>before['completions'] and after['frames']>before['frames'],(before,after)
  assert after['completions']==after['frames'] and not after['errors'] and not after['rejected'],after
  r['ehci_qtd_after']=ehci_counters()
  if profile=='ehci':
   assert r['ehci_qtd_after']['calls']>r['ehci_qtd_before']['calls'] and not r['ehci_qtd_after']['rejected']
   r['upstream_linux_ehci_qtd_fill_executed']=True
  r['urb_core_after']=core=urb_counters();previous=r['urb_core_before']
  assert core['submits']>previous['submits'] and core['givebacks']>previous['givebacks'],(previous,core)
  assert core['submits']==core['givebacks']+core['active'] and core['active']>=1 and not core['rejected'],core
  r['actual_upstream_core_urb_file_executed']=True
  r['exact_rendered_xy_checks']=6;r['passed']=True
 print('PASS actual Linux usbmouse IRQ + rendered XY',fw,profile,r['after'],flush=True)
finally:(q/f'linux-usbmouse-{fw}-{profile}.json').write_text(json.dumps(r,indent=2)+'\n')
