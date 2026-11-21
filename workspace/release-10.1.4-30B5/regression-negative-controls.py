#!/usr/bin/env python3
"""Run identical X-only/Y-only wire packets through actual 10.1.3 and current transports.
A negative control must compile and fail a value assertion, not a build/sanitizer error.
"""
from pathlib import Path
import subprocess,tempfile,json,hashlib
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS';commit='77b73dc3e86eb9dff7ef4b8fa1f8571ce34703df'
def prior(n):return subprocess.check_output(['git','--git-dir=/var/tmp/tmc-github-git/tinymacOS.git','show',commit+':tinymacOS/'+n],text=True)
with tempfile.TemporaryDirectory(prefix='tmc1014-framing-',dir='/var/tmp') as d:
 w=Path(d);cases=[]
 for name,fixture,kind in [('drivers/xhci.c','input_enumeration.c','xhci'),('drivers/usbinput.c','usbinput_contract.c','usb2'),('recovery/usbhid.c','recovery_usb.c','recovery')]:
  pair=[]
  for old in [False,True]:
   source=prior(name) if old else (root/name).read_text()
   source=source.replace('"../tests/input_test_support.h"','"'+str(root/'tests/input_test_support.h')+'"').replace('"../tests/recovery_usb_support.h"','"'+str(root/'tests/recovery_usb_support.h')+'"')
   if kind=='xhci':source=source.replace('db[s]=ep;','db[s]=ep;test_doorbell(s,ep);')
   if kind=='usb2':
    source=source.replace('#include "../kernel/k.h"','#include "'+str(root/'tests/input_test_support.h')+'"');a=source.index('void usb_delay_ms(');b=source.index('int usb_input_enumerate(',a);source=source[:a]+'void usb_delay_ms(unsigned ms){(void)ms;}\n'+source[b:]
   src=w/(kind+str(old)+'.c');src.write_text(source)
   f=(root/'tests'/fixture).read_text().split('int main(void)')[0].replace('"../drivers/input.c"','"'+str(root/'drivers/input.c')+'"').replace('"../recovery/usbhid.c"','"'+str(src)+'"')
   if kind=='xhci':body='acknowledge_without_switch=1;assert(run(0,0,0,0));Event e;struct Hid*h=&usb[0].hid[0];h->armed=1;memcpy(h->report,(u8[]){4,0,64,0,0,0,0},7);TRB ev={0,(13u<<24)|1,TY(32)|(1u<<24)|(3u<<16)};transferred(&ev);assert(input_pop(&e)&&e.type==EV_MOUSE&&e.a==64&&e.b==0);memcpy(h->report,(u8[]){4,0,0,0,64,0,0},7);transferred(&ev);assert(input_pop(&e)&&e.a==0&&e.b==64);'
   elif kind=='usb2':body='acknowledge_without_switch=1;protocol=0;assert(enumerate());Event e;memcpy(u.pipe[0].report,(u8[]){4,0,64,0,0,0,0},7);usb_input_report(&u,0,7);assert(input_pop(&e)&&e.type==EV_MOUSE&&e.a==64&&e.b==0);memcpy(u.pipe[0].report,(u8[]){4,0,0,0,64,0,0},7);usb_input_report(&u,0,7);assert(input_pop(&e)&&e.a==0&&e.b==64);'
   else:body='wide=1;state=0;deny_report=1;assert(rec_pointer_start(&bs,100,100));TabletState s;u8 p[]={4,0,64,0,0,0,0};received(p,7,0,0);assert(rec_pointer_next(&bs,&s)&&s.x==64&&s.y==0);p[2]=0;p[4]=64;received(p,7,0,0);assert(rec_pointer_next(&bs,&s)&&s.x==0&&s.y==64);'
   test=w/(kind+str(old)+'-test.c');test.write_text(f+'int main(void){'+body+'return 0;}\n');exe=w/(kind+str(old)+'.test')
   cmd=['gcc','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+str(root/'include'),'-I'+str(root/'tests')]
   if kind=='xhci':cmd+=['-DTMC_XHCI_TEST_SOURCE="'+str(src)+'"']
   if kind=='usb2':cmd+=['-DTMC_USBINPUT_TEST_SOURCE="'+str(src)+'"']
   subprocess.run(cmd+[str(test),str(root/'drivers/hidconfig.c'),str(root/'drivers/hidtablet.c'),'-o',str(exe)],check=True)
   r=subprocess.run([str(exe)],capture_output=True,text=True);pair.append(r)
  assert pair[0].returncode==0,pair[0].stderr
  assert pair[1].returncode!=0 and 'Assertion' in pair[1].stderr,(kind,pair[1].stderr)
  cases.append({'transport':kind,'current_passed':True,'previous_source':name,'previous_commit':commit,'previous_sha256':hashlib.sha256(prior(name).encode()).hexdigest(),'previous_value_assertion_failed':True,'previous_assertion':pair[1].stderr.strip().splitlines()[-1]});print('PASS',kind,'same numbered/16-bit X-only and Y-only packets: current correct, actual 10.1.3 fails axis assertion')
 (q/'regression-negative-controls.json').write_text(json.dumps({'passed':True,'cases':cases,'same_wire_bytes':[4,0,64,0,0,0,0],'previous_wrong_boot_decode':{'x':0,'y':64},'current_correct_decode':{'x':64,'y':0},'physical_hardware_tested':False},indent=2)+'\n')
