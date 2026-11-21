#!/usr/bin/env python3
"""Compile the actual previous 10.1.1 drivers against the new regression assertions.
A failing previous implementation is expected; sanitizer diagnostics alone are
not accepted as a reproduced logical regression.
"""
from pathlib import Path
import subprocess,tempfile,json,hashlib
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS';git=['git','--git-dir=/var/tmp/tmc-github-git/tinymacOS.git']
def old(name):return subprocess.check_output(git+['show','6c60166:tinymacOS/'+name],text=True)
with tempfile.TemporaryDirectory(prefix='tmc1012-negative-',dir='/var/tmp') as scratch:
 w=Path(scratch);cases=[]
 for name,fixture in [('drivers/xhci.c','input_enumeration.c'),('drivers/usbinput.c','usbinput_contract.c'),('recovery/usbhid.c','recovery_usb.c'),('drivers/usbroute.c','usbroute.c')]:
  s=old(name)
  s=s.replace('"../tests/input_test_support.h"','"'+str(root/'tests/input_test_support.h')+'"').replace('"../tests/recovery_usb_support.h"','"'+str(root/'tests/recovery_usb_support.h')+'"')
  if name.endswith('xhci.c'):s=s.replace('db[s]=ep;','db[s]=ep;test_doorbell(s,ep);')
  if name.endswith('usbinput.c'):
   s=s.replace('#include "../kernel/k.h"','#include "'+str(root/'tests/input_test_support.h')+'"')
   a=s.index('void usb_delay_ms(');b=s.index('int usb_input_enumerate(',a);s=s[:a]+'void usb_delay_ms(unsigned ms){(void)ms;}\n'+s[b:]
  source=w/('previous-'+Path(name).name);source.write_text(s)
  f=(root/'tests'/fixture).read_text().replace('"../drivers/input.c"','"'+str(root/'drivers/input.c')+'"')
  f=f.replace('"../recovery/usbhid.c"','"'+str(source)+'"').replace('"../drivers/usbroute.c"','"'+str(source)+'"')
  test=w/fixture;test.write_text(f);exe=w/(fixture+'.test')
  cmd=['gcc','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+str(root/'include'),'-I'+str(root/'tests')]
  if 'xhci' in name:cmd+=['-DTMC_XHCI_TEST_SOURCE="'+str(source)+'"']
  if 'usbinput' in name:cmd+=['-DTMC_USBINPUT_TEST_SOURCE="'+str(source)+'"']
  cmd += [str(test)]
  if 'usbroute' not in name:cmd+=[str(root/'drivers/hidconfig.c'),str(root/'drivers/hidtablet.c')]
  subprocess.run(cmd+['-o',str(exe)],check=True)
  r=subprocess.run([str(exe)],capture_output=True,text=True)
  assert r.returncode!=0 and 'Assertion' in r.stderr,(name,r.returncode,r.stderr)
  cases.append({'previous_source':name,'previous_commit':'6c60166891ac0fb8ade04bead70319bcafa34321','previous_sha256':hashlib.sha256(old(name).encode()).hexdigest(),'assertion_failed_as_expected':True,'exit_code':r.returncode,'assertion':r.stderr.strip().splitlines()[-1]})
  print('PASS negative control:',name,'previous 10.1.1 fails the new assertion')
 (q/'regression-negative-controls.json').write_text(json.dumps({'passed':True,'cases':cases,'physical_hardware_tested':False},indent=2)+'\n')
