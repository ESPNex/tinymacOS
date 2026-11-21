#!/usr/bin/env python3
"""Execute Linux-reviewed TD/overlay invariants against actual new/previous HCD code."""
from pathlib import Path
import subprocess,tempfile,json,hashlib
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS';commit='77b73dc3e86eb9dff7ef4b8fa1f8571ce34703df'
def prior(n):return subprocess.check_output(['git','--git-dir=/var/tmp/tmc-github-git/tinymacOS.git','show',commit+':tinymacOS/'+n],text=True)
with tempfile.TemporaryDirectory(prefix='tmc1014-hcd-',dir='/var/tmp') as d:
 w=Path(d);cases=[]
 for kind in ['publish-complete-td','short-data-before-status','ehci-hardware-toggle','ehci-overlay-quiescence']:
  outcomes=[]
  for old in [False,True]:
   name='drivers/ehci.c' if kind.startswith('ehci') else 'drivers/xhci.c';s=prior(name) if old else (root/name).read_text()
   s=s.replace('"../tests/input_test_support.h"','"'+str(root/'tests/input_test_support.h')+'"').replace('#include "../kernel/k.h"','#include "'+str(root/'tests/input_test_support.h')+'"')
   if kind.startswith('ehci'):
    prefix='''#define ehci_init fixture_unused_ehci_init
#define ehci_poll fixture_unused_ehci_poll
#include "input_test_support.h"
#undef ehci_init
#undef ehci_poll
#include "usbhost.h"
u64 ticks;InputDiag input_diag;
static volatile u32 *mock_op;
static int stuck,observing,rearms,deliveries;
void usb_delay_ms(unsigned ms){(void)ms;if(mock_op&&!stuck&&!(mock_op[0]&(1u<<4)))mock_op[1]&=~(1u<<14);}
static void watch_overlay(void){if(observing){assert(!(mock_op[0]&(1u<<4)));assert(!(mock_op[1]&(1u<<14)));rearms++;}}
static u32 pci_reg(u32 a,int b){(void)a;(void)b;return 0;}
static void outl(unsigned a,u32 b){(void)a;(void)b;}
int usb_input_enumerate(UsbInput*u,unsigned n){(void)u;(void)n;return 0;}
void usb_input_report(UsbInput*u,unsigned p,unsigned n){(void)u;(void)p;assert(n<=8);deliveries++;}
void hid_keys(const u8 r[8]){(void)r;}
'''
    body='''int main(void){(void)boot;struct Edev d={0};d.u.speed=3;d.u.address=1;d.u.pipe[0].packet=8;
assert(!(epchar(&d.u,1,8)&(1u<<14)));assert(epchar(&d.u,0,64)&(1u<<14));
d.qh[0].token=1u<<31;epipe_arm(&d,0);assert(d.qh[0].token&(1u<<31));return 0;}
'''
    s=s.replace('static void epipe_arm(struct Edev *d,unsigned j)\n{','static void epipe_arm(struct Edev *d,unsigned j)\n{watch_overlay();')
    if kind=='ehci-overlay-quiescence':
     body='''int main(void){(void)boot;u32 op[32]={1u<<4,1u<<14};mock_op=op;struct Ehci*h=ehcis;h->op=op;h->active=1;h->used=1;nehci=1;struct Edev*d=h->dev;d->u.host=h;d->u.speed=3;d->u.address=1;d->u.npipe=1;d->u.pipe[0].packet=8;d->u.pipe[0].ep=1;d->u.pipe[0].interval=1;
observing=1;install_pipes(h,d);assert(rearms==1&&(op[0]&(1u<<4)));
op[1]|=1u<<14;d->qh[0].token=1u<<31;d->td[0].token=3u<<16;ehci_poll();assert(rearms==2&&deliveries==1&&(d->qh[0].token&(1u<<31))&&(op[0]&(1u<<4)));
op[1]|=1u<<14;stuck=1;struct Eqh saved=d->qh[0];d->td[0].token=0;ehci_poll();assert(rearms==2&&deliveries==1&&!memcmp(&saved,d->qh,sizeof saved));return 0;}
'''
    text=prefix+'\n'+s+'\n'+body
   else:
    s=s.replace('db[s]=ep;','db[s]=ep;test_doorbell(s,ep);')
    line='t->ctl=ctrl|r->cycle;' if old else 't->ctl=ctrl|(r->cycle^!publish);';assert line in s
    s=s.replace(line,line+'watch_enqueue(r);')
    # A passive observer checks cycle visibility at each producer store. It
    # doesn't change controller/kernel code or synthesize a successful result.
    f=(root/'tests/input_enumeration.c').read_text().split('int main(void)')[0]
    f=f.replace('static void test_doorbell(int,int);','static void test_doorbell(int,int);\nstatic void watch_enqueue(void*);\nstatic int watching,short_mode;')
    f=f.replace('"../drivers/input.c"','"'+str(root/'drivers/input.c')+'"')
    if old:s=s.replace('int id,port,speed,nhid,diag,ctrl_dead;','int id,port,speed,nhid,diag,ctrl_dead,ctrl_bytes; /* passive observation storage; old code never sets it */')
    src=w/(kind+str(old)+'.c');src.write_text(s)
    f=f.replace('#include TMC_XHCI_TEST_SOURCE','#include "'+str(src)+'"')
    f=f.replace('complete(code==6?setup:t,32,code,1,slot);','''if(short_mode && rq==6 && val==0x100){
 events[evpos]=(TRB){(u64)&u->ctrl.tr[1],(13u<<24)|3,TY(32)|(1u<<24)|(1u<<16)|(u32)evcycle};
 events[(evpos+1)%NE]=(TRB){(u64)t,1u<<24,TY(32)|(1u<<24)|(1u<<16)|(u32)evcycle};
}else complete(code==6?setup:t,32,code,1,slot);''')
    f+='''static void watch_enqueue(void*p){if(watching && p==&usb[0].ctrl)assert((usb[0].ctrl.tr[0].ctl&1)!=1);}
'''
    body='''int main(void){assert(run(0,0,1,0));watching=0;memset(events,0,sizeof(TRB)*NE);evpos=0;evcycle=1;ringinit(&usb[0].ctrl);
'''
    if kind=='publish-complete-td':body+='watching=1;assert(!control(usb,0x80,6,0x100,0,8));assert(usb[0].ctrl.tr[0].ctl&1);'
    else:body+='short_mode=1;assert(!control(usb,0x80,6,0x100,0,8));assert(usb[0].ctrl_bytes==5&&evpos==2);'
    text=f+body+'return 0;}\n'
   c=w/(kind+str(old)+'-test.c');c.write_text(text);exe=w/(kind+str(old)+'.test')
   subprocess.run(['gcc','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+str(root/'include'),'-I'+str(root/'tests'),str(c),str(root/'drivers/hidconfig.c'),str(root/'drivers/hidtablet.c'),'-o',str(exe)],check=True)
   outcomes.append(subprocess.run([str(exe)],capture_output=True,text=True))
  assert outcomes[0].returncode==0,(kind,outcomes[0].stderr)
  assert outcomes[1].returncode!=0 and 'Assertion' in outcomes[1].stderr,(kind,outcomes[1].stderr)
  cases.append({'case':kind,'current_passed':True,'previous_value_assertion_failed':True,'previous_commit':commit,'previous_assertion':outcomes[1].stderr.strip().splitlines()[-1]});print('PASS Linux-reviewed HCD invariant:',kind,'actual current passes, actual 10.1.3 fails assertion')
 (q/'linux-hcd-result.json').write_text(json.dumps({'passed':True,'cases':cases,'linux_driver_code_copied':False,'source_sha256':{n:hashlib.sha256((root/n).read_bytes()).hexdigest() for n in ['drivers/xhci.c','drivers/ehci.c']}},indent=2)+'\n')
