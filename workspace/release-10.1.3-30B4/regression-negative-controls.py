#!/usr/bin/env python3
"""Execute current cases against actual 10.1.2 source; assertion failures only.
Neither a compile failure nor a sanitizer crash is counted as reproduction.
"""
from pathlib import Path
import subprocess,tempfile,json,hashlib
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS';commit='fd74c7cc08a06f009806058682490a6fd89225bc'
def old(n):return subprocess.check_output(['git','--git-dir=/var/tmp/tmc-github-git/tinymacOS.git','show',commit+':tinymacOS/'+n],text=True)
with tempfile.TemporaryDirectory(prefix='tmc1013-negative-',dir='/var/tmp') as d:
 w=Path(d);cases=[]
 for name,fixture,kind in [('drivers/xhci.c','input_enumeration.c','boot-first-xhci'),('drivers/usbinput.c','usbinput_contract.c','boot-first-usb2'),('recovery/usbhid.c','recovery_usb.c','relative-deltas'),('recovery/usbhid.c','recovery_usb.c','async-error-restart'),('recovery/usbhid.c','recovery_usb.c','mouse-child-ownership')]:
  s=old(name).replace('"../tests/input_test_support.h"','"'+str(root/'tests/input_test_support.h')+'"').replace('"../tests/recovery_usb_support.h"','"'+str(root/'tests/recovery_usb_support.h')+'"')
  if name.endswith('xhci.c'):s=s.replace('db[s]=ep;','db[s]=ep;test_doorbell(s,ep);')
  if name.endswith('usbinput.c'):
   s=s.replace('#include "../kernel/k.h"','#include "'+str(root/'tests/input_test_support.h')+'"');a=s.index('void usb_delay_ms(');b=s.index('int usb_input_enumerate(',a);s=s[:a]+'void usb_delay_ms(unsigned ms){(void)ms;}\n'+s[b:]
  src=w/(kind+'.c');src.write_text(s)
  f=(root/'tests'/fixture).read_text().replace('"../drivers/input.c"','"'+str(root/'drivers/input.c')+'"').replace('"../recovery/usbhid.c"','"'+str(src)+'"')
  if name.startswith('recovery'):
   f=f[:f.index('int main(void)')].replace('interval==10','(interval==8||interval==10)')
   body={'relative-deltas':'u8 r[]={0,1,0};received(r,3,0,0);assert(rec_pointer_next(&bs,&s)&&s.x==1&&s.y==0);',
         'async-error-restart':'int before=armed;received(0,0,0,2);for(int i=0;i<8;i++)rec_pointer_next(&bs,&s);assert(armed==before+1);',
         'mouse-child-ownership':'exclusive=1;assert(rec_pointer_start(&bs,100,100)&&disconnects==1);'}[kind]
   f+='int main(void){TabletState s;(void)s;state=0;deny_report=1;assert(rec_pointer_start(&bs,100,100));'+body+'return 0;}\n'
  test=w/(kind+'-test.c');test.write_text(f);exe=w/(kind+'.test')
  cmd=['gcc','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+str(root/'include'),'-I'+str(root/'tests')]
  if 'xhci' in name:cmd+=['-DTMC_XHCI_TEST_SOURCE="'+str(src)+'"']
  if 'usbinput' in name:cmd+=['-DTMC_USBINPUT_TEST_SOURCE="'+str(src)+'"']
  subprocess.run(cmd+[str(test),str(root/'drivers/hidconfig.c'),str(root/'drivers/hidtablet.c'),'-o',str(exe)],check=True)
  r=subprocess.run([str(exe)],capture_output=True,text=True)
  assert r.returncode!=0 and 'Assertion' in r.stderr,(kind,r.returncode,r.stderr)
  cases.append({'case':kind,'previous_source':name,'previous_commit':commit,'previous_sha256':hashlib.sha256(old(name).encode()).hexdigest(),'assertion_failed_as_expected':True,'exit_code':r.returncode,'assertion':r.stderr.strip().splitlines()[-1]});print('PASS negative control: actual 10.1.2 fails',kind)
 (q/'regression-negative-controls.json').write_text(json.dumps({'passed':True,'cases':cases,'physical_hardware_tested':False},indent=2)+'\n')
