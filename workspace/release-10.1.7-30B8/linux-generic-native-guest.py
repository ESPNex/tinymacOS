#!/usr/bin/env python3
"""Actual native endpoint -> whole Linux generic/input, observed after EBS.
Non-secret counters + exact rendered absolute coordinates, no test driver.
"""
from pathlib import Path
import hashlib,json,os,struct,subprocess,sys,tempfile
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS';sys.path.insert(0,str(root/'tests'))
from retail_guest import Retail
from linux_message_observer import observe_message
from linux_dma_observer import observe_dma
from linux_uhci_observer import observe_uhci
from linux_ehci_observer import observe_ehci
from linux_xhci_observer import observe_xhci
from input_pixels import cursor
fw,profile=sys.argv[1:];assert fw in ['uefi','bios'];assert profile in ['xhci-tablet','ehci-tablet','uhci-tablet','companion-tablet','xhci-tablet-hub']
base=Path(os.environ.get('TMC_LINUX_STAGE','/var/tmp/tmc-10.1.7/linux-generic-native-v13'));build=base/'build';iso=base/'tinymacOS-10.1.7-x86_64.iso'
w=Path(tempfile.mkdtemp(prefix='tmc-generic-'+fw+'-'+profile+'-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',OCR_ROOT='/var/tmp/tmc/tools/root',TMC_MEM='512',TMC_BUILD=str(build),TMC_TEST_INPUT=profile,TMC_TEST_FIRMWARE=fw,TMC_POINTER='tablet')
record={'passed':False,'firmware':fw,'profile':profile,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'work':str(w),'linux_hcd_port':profile.startswith(('ehci','uhci','companion','xhci')),'full_linux_usb_core_hub':False,'physical_hardware_tested':False}
try:
 symbols=subprocess.check_output(['nm',str(build/'kernel.debug')],text=True).splitlines()
 def address(name):return int(next(s.split()[0] for s in symbols if s.endswith(' '+name)),16)
 for name in ['hid_generic_probe','hidinput_hid_event','hidinput_report_event','tmc_linux_generic_report']:address(name)
 tool='/var/tmp/tmc/tools/root';env={**os.environ,'PATH':tool+'/usr/bin:'+os.environ['PATH'],'LD_LIBRARY_PATH':tool+'/usr/lib/x86_64-linux-gnu'}
 subprocess.run(['xorriso','-osirrox','on','-indev',str(iso),'-extract','/System/Library/KernelCollections/KernelCache',str(w/'actual.cache')],env=env,check=True,capture_output=True)
 assert (w/'actual.cache').read_bytes()==(build/'KernelCache').read_bytes();record['actual_iso_kernel_equals_build']=True
 with Retail(iso,w,ev,'generic',live=True,launcher=root/'tools/run-input-qa.sh') as g:
  record['message_core']=observe_message(g,symbols,w);record['actual_upstream_message_control_functions_executed']=True
  if profile.startswith(('ehci','uhci','companion','xhci')):record['dma_pool']=observe_dma(g,symbols,w);record['original_dma_pool_in_native_controller_executed']=True
  if profile.startswith(('uhci','companion')):record['whole_original_uhci_runtime']=observe_uhci(g,symbols,w)
  if profile.startswith('xhci'):
   record['whole_original_xhci_runtime']=observe_xhci(g,symbols,w)
   record['linux_controller_hcd_port']=True;record['native_hcd_transport_adapter']=False;record['native_class_enumeration_facade']=True
  if profile.startswith('ehci'):
   record['whole_original_ehci_runtime']=observe_ehci(g,symbols,w)
   record['linux_controller_hcd_port']=True;record['native_hcd_transport_adapter']=False;record['native_class_enumeration_facade']=True
  mice=g.req('query-mice');target=next(m for m in mice if m['name']=='QEMU HID Tablet');g.hmp('mouse_set '+str(target['index']))
  assert [m['name'] for m in g.req('query-mice') if m['current']]==['QEMU HID Tablet'];record['usb']=g.hmp('info usb')
  def counters(name,fields):
   p=w/(name+'.bin');g.req('pmemsave',val=address(name),size=8*len(fields),filename=str(p));return dict(zip(fields,struct.unpack('<'+'Q'*len(fields),p.read_bytes())))
  def native():return counters('tmc_native_generic_counters',['attached','failed','received','dispatched','rejected','queued','overflows','errors','detached','controls','control_errors'])
  record['before']=before=native();assert before['attached']>=1 and not before['failed'],before
  for x,y in [(762,482),(142,482),(142,613),(762,613),(142,613),(762,482)]:cursor(g,x,y)
  record['after']=after=native();assert after['dispatched']>before['dispatched'] and after['received']>=after['dispatched'],(before,after)
  assert not any(after[k] for k in ['failed','rejected','overflows','errors','control_errors']),after
  record['whole_source_generic_input']=generic=counters('tmc_hid_generic_counters',['probes','attached','detached','failures','reports','rejected','events','inputs','outputs','output_errors','busy'])
  assert generic['attached']>=1 and generic['events']>0 and generic['reports']>=after['dispatched'] and not generic['failures'] and not generic['rejected'],generic
  if profile.startswith('xhci'):
   record['whole_original_xhci_runtime_after']=observe_xhci(g,symbols,w)
   assert record['whole_original_xhci_runtime_after']['givebacks']>record['whole_original_xhci_runtime']['givebacks'],record['whole_original_xhci_runtime_after']
   record['linux_xhci_controller_executed']=True
  if profile.startswith('ehci'):
   record['whole_original_ehci_runtime_after']=observe_ehci(g,symbols,w)
   assert record['whole_original_ehci_runtime_after']['givebacks']>record['whole_original_ehci_runtime']['givebacks'],record['whole_original_ehci_runtime_after']
   record['linux_ehci_controller_executed']=True
  if profile.startswith(('uhci','companion')):
   record['whole_original_uhci_runtime_after']=observe_uhci(g,symbols,w)
   assert record['whole_original_uhci_runtime_after']['givebacks']>record['whole_original_uhci_runtime']['givebacks'],record['whole_original_uhci_runtime_after']
   record['linux_uhci_controller_executed']=True
  record['exact_rendered_xy_checks']=6;record['native_generic_endpoint_event_dispatch_executed']=True;g.shot('whole-source-generic-input');record['passed']=True
 print('PASS actual whole-source Linux generic/input endpoint + exact rendered absolute XY',fw,profile,after,flush=True)
finally:(q/f'linux-generic-native-{fw}-{profile}.json').write_text(json.dumps(record,indent=2)+'\n')
