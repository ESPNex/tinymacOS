#!/usr/bin/env python3
"""Same packets and ownership fixture against actual 10.1.4 vs current sources.
Previous decoder AND transport are compiled; failures must be value assertions.
This is a compatibility reproduction, not a physical mouse trace.
"""
from pathlib import Path
import subprocess,tempfile,json,hashlib
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS';commit='f4360c9c366efb74ea48e9d02bf06f31e64da832'
def prior(n):return subprocess.check_output(['git','--git-dir=/var/tmp/tmc-github-git/tinymacOS.git','show',commit+':tinymacOS/'+n],text=True)
with tempfile.TemporaryDirectory(prefix='tmc1015-regress-',dir='/var/tmp') as d:
 w=Path(d);cases=[]
 for kind,fixture,name in [('xhci','input_enumeration.c','drivers/xhci.c'),('usb2','usbinput_contract.c','drivers/usbinput.c'),('recovery','recovery_usb.c','recovery/usbhid.c')]:
  for test_case in ['boot-stream-despite-report-ack','zero-padded-boot','zero-padded-numbered-report']+(['preserve-firmware-owner'] if kind=='recovery' else []):
   outcomes=[]
   for old in [False,True]:
    source=prior(name) if old else (root/name).read_text();source=source.replace('"../tests/input_test_support.h"','"'+str(root/'tests/input_test_support.h')+'"').replace('"../tests/recovery_usb_support.h"','"'+str(root/'tests/recovery_usb_support.h')+'"')
    if kind=='xhci':source=source.replace('db[s]=ep;','db[s]=ep;test_doorbell(s,ep);')
    if kind=='usb2':
     source=source.replace('#include "../kernel/k.h"','#include "'+str(root/'tests/input_test_support.h')+'"');a=source.index('void usb_delay_ms(');b=source.index('int usb_input_enumerate(',a);source=source[:a]+'void usb_delay_ms(unsigned ms){(void)ms;}\n'+source[b:]
    src=w/(kind+str(old)+'.c');src.write_text(source)
    parser=w/('hidtablet'+str(old)+'.c');parser.write_text(prior('drivers/hidtablet.c') if old else (root/'drivers/hidtablet.c').read_text())
    f=(root/'tests'/fixture).read_text().split('int main(void)')[0].replace('"../drivers/input.c"','"'+str(root/'drivers/input.c')+'"').replace('"../recovery/usbhid.c"','"'+str(src)+'"')
    n=3 if test_case=='boot-stream-despite-report-ack' else 8
    packet='0,37,0' if n==3 else '0,37,0,0,0,0,0,0' if test_case=='zero-padded-boot' else '4,0,37,0,0,0,0,0'
    if kind=='xhci':
     init='acknowledge_without_switch=1;' if test_case!='boot-stream-despite-report-ack' else ''
     body=init+'assert(run(0,0,0,0));Event e;struct Hid*h=&usb[0].hid[0];h->armed=1;u8 p[]={'+packet+'};memcpy(h->report,p,sizeof p);TRB ev={0,(13u<<24)|'+str(8-n)+',TY(32)|(1u<<24)|(3u<<16)};transferred(&ev);assert(input_pop(&e)&&e.type==EV_MOUSE&&e.a==37&&e.b==0);'
    elif kind=='usb2':
     init='acknowledge_without_switch=1;' if test_case!='boot-stream-despite-report-ack' else ''
     body=init+'protocol=0;assert(enumerate());Event e;u8 p[]={'+packet+'};memcpy(u.pipe[0].report,p,sizeof p);usb_input_report(&u,0,sizeof p);assert(input_pop(&e)&&e.type==EV_MOUSE&&e.a==37&&e.b==0);'
    elif test_case=='preserve-firmware-owner':body='exclusive=1;int started=rec_pointer_start(&bs,100,100);assert(!disconnects&&!connected_back&&!started);'
    else:
     init='deny_report=1;' if test_case!='boot-stream-despite-report-ack' else ''
     body=init+'wide=1;state=0;assert(rec_pointer_start(&bs,100,100));TabletState s;u8 p[]={'+packet+'};received(p,sizeof p,0,0);assert(rec_pointer_next(&bs,&s)&&s.x==37&&s.y==0);'
    test=w/(kind+str(old)+'-test.c');test.write_text(f+'int main(void){'+body+'return 0;}\n');exe=w/(kind+str(old)+'.test')
    cmd=['gcc','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+str(root/'include'),'-I'+str(root/'tests')]
    if kind=='xhci':cmd+=['-DTMC_XHCI_TEST_SOURCE="'+str(src)+'"']
    if kind=='usb2':cmd+=['-DTMC_USBINPUT_TEST_SOURCE="'+str(src)+'"']
    subprocess.run(cmd+[str(test),str(root/'drivers/hidconfig.c'),str(parser),'-o',str(exe)],check=True)
    outcomes.append(subprocess.run([str(exe)],capture_output=True,text=True))
   assert outcomes[0].returncode==0,(kind,test_case,outcomes[0].stderr)
   assert outcomes[1].returncode!=0 and 'Assertion' in outcomes[1].stderr,(kind,test_case,outcomes[1].stderr)
   cases.append({'transport':kind,'case':test_case,'current_passed':True,'previous_commit':commit,'previous_transport_sha256':hashlib.sha256(prior(name).encode()).hexdigest(),'previous_decoder_sha256':hashlib.sha256(prior('drivers/hidtablet.c').encode()).hexdigest(),'previous_value_assertion_failed':True,'previous_assertion':outcomes[1].stderr.strip().splitlines()[-1]});print('PASS',kind,test_case,'current passes, real 10.1.4 transport/decoder fails value assertion',flush=True)
 (q/'regression-negative-controls.json').write_text(json.dumps({'passed':True,'cases':cases,'physical_hardware_tested':False,'not_a_physical_usb_capture':True},indent=2)+'\n')
