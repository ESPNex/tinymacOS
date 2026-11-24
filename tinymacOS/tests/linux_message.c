/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual original message -> URB -> HCD -> copying EP0 backend.
 * Scripted native transport, not a physical Linux HCD or sleepable scheduler. */
#define main boot_mouse_contract
#include "linux_usbmouse.c"
#undef main
static int owner,other,mode,calls;
static unsigned last_index;
static int transport(void *context,unsigned rt,unsigned request,unsigned value,unsigned index,u8 *data,unsigned n)
{
 assert(context==&owner);calls++;last_index=index;
 assert(tmc_linux_core_control_busy());
 assert(tmc_linux_core_control_detach(&owner)==-EBUSY);
 assert(tmc_linux_core_control(&other,2,8,transport,&owner,0xa1,1,0,0,data,n,100)==-EBUSY);
 tmc_linux_usb_service(); /* reentry while native owns EP0 must not deadlock */
 if(mode==1)return -EPIPE;
 if(mode==2)return n+1;
 if(rt&128){
  for(unsigned i=0;i<n;i++)data[i]=(u8)(i*17+3);
  if(rt==0x80&&request==6){assert(value==0x100&&n>=2);data[0]=18;data[1]=mode==3?2:1;}
 }else for(unsigned i=0;i<n;i++)assert(data[i]==(u8)(i*17+3));
 if(mode==4){struct timespec delay={0,8000000};nanosleep(&delay,0);}
 return mode==5?n-1:n;
}
static int request(unsigned rt,unsigned req,unsigned value,unsigned index,u8 *data,unsigned n,unsigned timeout)
{return tmc_linux_core_control(&owner,2,16,transport,&owner,rt,req,value,index,data,n,timeout);}
static void completion_callback(struct urb *u){int *status=u->context;*status=u->status;}
static unsigned callback_entered,callback_release;
static int simple_transport(void *context,unsigned rt,unsigned req,unsigned value,unsigned index,u8 *data,unsigned n)
{(void)context;(void)rt;(void)req;(void)value;(void)index;if(n)memset(data,0,n);return n;}
static int blocked_transport(void *context,unsigned rt,unsigned req,unsigned value,unsigned index,u8 *data,unsigned n)
{
 __atomic_store_n(&callback_entered,1,__ATOMIC_RELEASE);
 while(!__atomic_load_n(&callback_release,__ATOMIC_ACQUIRE))tmc_lx_os_spin_wait();
 return simple_transport(context,rt,req,value,index,data,n);
}
static void *message_thread(void *context)
{
 u8 data[16];assert(tmc_linux_core_control(context,2,8,blocked_transport,context,0xa1,1,0,0,data,16,1000)==16);return 0;
}
int main(void)
{
 assert(!boot_mouse_contract());errors=0;u8 data[1024],original[1024];
 memset(data,0xaa,sizeof data);assert(request(0xa1,1,0,0x409,data,1024,1000)==1024&&last_index==0x409);
 for(unsigned i=0;i<1024;i++)assert(data[i]==(u8)(i*17+3));memcpy(original,data,1024);
 assert(request(0x21,9,0,7,data,1024,1000)==1024&&!memcmp(data,original,1024));
 assert(!request(0,5,9,0,0,0,1000));struct LxBinding *b=binding(&owner);assert(b&&b->kind==3&&b->usb.state==6);
 assert(!request(0,9,1,0,0,0,1000)&&b->usb.state==7);
 assert(request(0x80,6,0x100,0,data,18,1000)==18&&data[1]==1);
 mode=3;int before=calls;assert(request(0x80,6,0x100,0,data,18,1000)==-ENODATA&&calls==before+3);
 mode=1;assert(request(0xa1,1,0,0,data,16,1000)==-EPIPE);
 mode=2;memset(data,0xaa,16);assert(request(0xa1,1,0,0,data,16,1000)==-75);for(unsigned i=0;i<16;i++)assert(data[i]==0xaa);
 mode=4;memset(data,0xaa,16);u64 start=tmc_lx_os_clock_ms();assert(request(0xa1,1,0,0,data,16,2)==-ETIMEDOUT);assert(tmc_lx_os_clock_ms()-start>=8);
 for(unsigned i=0;i<16;i++)assert(data[i]==0xaa);
 mode=5;assert(request(0xa1,1,0,0,data,16,1000)==15);
 /* Execute original recv/send convenience functions too. Their USB objects
  * are internal; the public native entry point is the serialized facade. */
 b->deadline=tmc_lx_os_clock_ms()+1000;memset(data,0xaa,16);
 assert(usb_control_msg_recv(&b->usb,0,1,0xa1,0,0,data,16,1000,GFP_KERNEL)==-EREMOTEIO);
 for(unsigned i=0;i<16;i++)assert(data[i]==0xaa);
 mode=0;b->deadline=tmc_lx_os_clock_ms()+1000;
 assert(!usb_control_msg_recv(&b->usb,0,1,0xa1,0,0,data,16,1000,GFP_KERNEL));
 assert(!usb_control_msg_send(&b->usb,0,9,0x21,0,0,data,16,1000,GFP_KERNEL));
 assert(usb_control_msg_recv(&b->usb,0,1,0xa1,0,0,data,0,1000,GFP_KERNEL)==-EINVAL);
 for(int fail=0;fail<2;fail++){
  fail_after=fail;assert(request(0xa1,1,0,0,data,16,1000)==-ENOMEM);fail_after=-1;assert(!alive&&!b->control_urb&&!b->active_control);
 }
 before=calls;assert(request(0xa1,1,0,0,data,1025,1000)==-EINVAL);assert(request(0xa1,1,0,0,data,16,0)==-EINVAL);assert(calls==before);
 /* Cancel a real queued message URB before handing it to native transport. */
 struct urb *u=usb_alloc_urb(0,GFP_KERNEL);assert(u);int status=0;u8 setup[8]={0xa1,1,0,0,0,0,16,0};
 usb_fill_control_urb(u,&b->usb,usb_rcvctrlpipe(&b->usb,0),setup,data,16,completion_callback,&status);
 b->deadline=tmc_lx_os_clock_ms()+1000;assert(!tmc_message_submit(u,GFP_KERNEL)&&u->submitted);before=calls;
 tmc_message_kill(u);assert(status==-ENOENT&&!u->submitted&&!b->active_control&&!b->control_urb&&calls==before);usb_free_urb(u);
 assert(!alive&&atomic_read(&b->usb.references)==1&&!b->active_control&&list_empty(&b->ep0.urb_list));
 assert(!tmc_linux_core_control_detach(&owner)&&!binding(&owner));
 for(unsigned i=0;i<1024;i++){assert(request(0xa1,1,0,0,data,16,1000)==16);assert(!tmc_linux_core_control_detach(&owner)&&!alive);}
 /* Exhaust and reuse all bounded device records without partial success. */
 int keys[LX_BINDINGS+1];
 for(unsigned i=0;i<LX_BINDINGS;i++)assert(tmc_linux_core_control(keys+i,2,8,simple_transport,0,0xa1,1,0,0,data,16,1000)==16);
 assert(tmc_linux_core_control(keys+LX_BINDINGS,2,8,simple_transport,0,0xa1,1,0,0,data,16,1000)==-ENOSPC);
 for(unsigned i=0;i<LX_BINDINGS;i++)assert(!tmc_linux_core_control_detach(keys+i));assert(!alive);
 pthread_t thread;assert(!pthread_create(&thread,0,message_thread,&owner));
 while(!__atomic_load_n(&callback_entered,__ATOMIC_ACQUIRE))tmc_lx_os_spin_wait();
 assert(tmc_linux_core_control_busy()&&tmc_linux_core_control_detach(&owner)==-EBUSY);
 assert(tmc_linux_core_control(&other,2,8,simple_transport,0,0xa1,1,0,0,data,16,1000)==-EBUSY);
 tmc_linux_usb_service();__atomic_store_n(&callback_release,1,__ATOMIC_RELEASE);
 assert(!pthread_join(thread,0)&&!tmc_linux_core_control_detach(&owner)&&!alive&&!tmc_linux_core_control_busy());
 puts("PASS original Linux message.c control/send/recv/descriptor -> urb.c -> hcd.c -> native EP0: 1024-byte IN/OUT, language index, real address/config transitions, descriptor retries, exact short-read policy, STALL/overrun/late-timeout without stale copy, allocation rollback, queued cancel, reentrant busy/detach protection , cross-thread serialization, bounded slot exhaustion and 1024 leak-free sessions; no full HCD/core/hub claim");
}
