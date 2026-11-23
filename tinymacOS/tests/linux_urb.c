/* SPDX-License-Identifier: GPL-2.0-or-later
 * Execute complete upstream core/urb.c with the real bounded native backend.
 * Reuse host memory/locking fixtures, not a substitute URB implementation.
 */
#define main boot_mouse_contract
#include "linux_usbmouse.c"
#undef main
struct callback_state {int calls,status,submit_result,requeue,drop;};
static void completed(struct urb *u)
{
 struct callback_state *s=u->context;s->calls++;s->status=u->status;
 if(s->requeue)s->submit_result=usb_submit_urb(u,GFP_ATOMIC);
 if(s->drop)usb_free_urb(u);
}
static void prepare(struct urb *u,struct callback_state *s)
{
 u->context=s;u->complete=completed;
}
static int control_done(void *arg,const u8 setup[8],u8 *data,unsigned n)
{
 unsigned *calls=arg;(*calls)++;assert(setup[0]==0x21&&setup[1]==9&&data&&n==1);return 0;
}
static void forbidden_release(struct kref *r){(void)r;assert(!"saturated reference must not release");}
static void *resume_anchor(void *arg)
{
 struct usb_anchor *a=arg;struct timespec delay={0,3000000};nanosleep(&delay,0);
 usb_anchor_resume_wakeups(a);return 0;
}
int main(void)
{
 assert(!boot_mouse_contract());errors=0;
 struct kref saturated;kref_init(&saturated);atomic_set(&saturated.refcount,INT_MAX);
 kref_get(&saturated);assert(!kref_put(&saturated,forbidden_release)&&atomic_read(&saturated.refcount)==INT_MAX);
 atomic_set(&saturated.refcount,0);kref_get(&saturated);assert(atomic_read(&saturated.refcount)==INT_MAX);
 atomic_set(&saturated.refcount,0);assert(!kref_put(&saturated,forbidden_release)&&atomic_read(&saturated.refcount)==INT_MAX);
 assert(!usb_alloc_urb(-1,GFP_KERNEL));assert(!usb_alloc_urb(INT_MAX,GFP_KERNEL));
 assert(!usb_get_urb(0));usb_free_urb(0);usb_init_urb(0);
 struct urb stack;usb_init_urb(&stack);assert(atomic_read(&stack.kref.refcount)==1&&list_empty(&stack.urb_list));
 assert(usb_submit_urb(0,GFP_KERNEL)==-EINVAL);assert(usb_unlink_urb(0)==-EINVAL);
 int keys[2];assert(tmc_linux_mouse_attach(keys,1,2,8,10));assert(tmc_linux_mouse_attach(keys+1,1,2,8,10));
 struct LxBinding *b=binding(keys),*c=binding(keys+1);struct urb *u=b->urb,*v=c->urb;
 void (*old_complete)(struct urb *)=u->complete;void *ctx=u->context,*ctx2=v->context;
 usb_kill_urb(u);usb_kill_urb(v);
 struct callback_state a={0},d={0};prepare(u,&a);prepare(v,&d);
 assert(atomic_read(&u->kref.refcount)==1&&!atomic_read(&u->use_count));
 assert(usb_urb_ep_type_check(u)==0);assert(usb_pipe_type_check(&b->usb,u->pipe^(2u<<30))==-EINVAL);
 assert(usb_pipe_type_check(&b->usb,(1u<<30)|(15u<<15)|128)==-EINVAL);
 unsigned original_pipe=u->pipe;u->pipe=(1u<<30)|(15u<<15)|128;assert(usb_submit_urb(u,GFP_ATOMIC)==-ENOENT);u->pipe=original_pipe;
 int speed=b->usb.speed,state=b->usb.state;b->usb.state=0;assert(usb_submit_urb(u,GFP_ATOMIC)==-ENODEV);b->usb.state=state;
 b->usb.state=USB_STATE_UNAUTHENTICATED;assert(usb_submit_urb(u,GFP_ATOMIC)==-ENODEV);b->usb.state=state;
 b->endpoint.desc.wMaxPacketSize=0;assert(usb_submit_urb(u,GFP_ATOMIC)==-EMSGSIZE);b->endpoint.desc.wMaxPacketSize=8;
 u->interval=0;assert(usb_submit_urb(u,GFP_ATOMIC)==-EINVAL);
 u->interval=256;assert(usb_submit_urb(u,GFP_ATOMIC)==-EINVAL);
 u->interval=10;assert(!usb_submit_urb(u,GFP_ATOMIC)&&u->interval==8);tmc_lx_native_giveback(u,0,8);assert(a.calls==1);
 b->usb.speed=USB_SPEED_HIGH;u->interval=20000;assert(!usb_submit_urb(u,GFP_ATOMIC)&&u->interval==8192);tmc_lx_native_giveback(u,0,8);
 b->usb.speed=USB_SPEED_SUPER;u->interval=32769;assert(usb_submit_urb(u,GFP_ATOMIC)==-EINVAL);
 u->interval=1000;assert(!usb_submit_urb(u,GFP_ATOMIC)&&u->interval==512);tmc_lx_native_giveback(u,0,8);
 b->usb.speed=speed;u->interval=8;
 struct scatterlist sg={7,0};u->num_sgs=2;u->sg=&sg;assert(usb_submit_urb(u,GFP_ATOMIC)==-EINVAL);
 sg.length=8;u->num_sgs=1;assert(usb_submit_urb(u,GFP_ATOMIC)==-EOPNOTSUPP);u->num_sgs=0;u->sg=0;
 u->transfer_buffer_length=-1;assert(usb_submit_urb(u,GFP_ATOMIC)==-EINVAL);u->transfer_buffer_length=8;
 assert(usb_submit_urb(u,73)==-EINVAL);
 usb_poison_urb(u);assert(atomic_read(&u->reject)==1&&usb_submit_urb(u,GFP_ATOMIC)==-EPERM);usb_unpoison_urb(u);
 usb_block_urb(u);assert(usb_submit_urb(u,GFP_ATOMIC)==-EPERM);usb_unpoison_urb(u);
 assert(!usb_submit_urb(u,GFP_ATOMIC));assert(atomic_read(&u->kref.refcount)==2&&atomic_read(&u->use_count)==1);
 a.requeue=1;usb_kill_urb(u);assert(a.status==-ENOENT&&a.submit_result==-EPERM&&!u->submitted);a.requeue=0;
 assert(atomic_read(&u->kref.refcount)==1&&!atomic_read(&u->reject)&&!atomic_read(&u->use_count));
 assert(!usb_submit_urb(u,GFP_ATOMIC));assert(usb_unlink_urb(u)==-EINPROGRESS&&a.status==-ECONNRESET);assert(usb_unlink_urb(u)==-EIDRM);
 u->transfer_flags|=URB_SHORT_NOT_OK;assert(!usb_submit_urb(u,GFP_ATOMIC));tmc_lx_native_giveback(u,0,3);assert(a.status==-EREMOTEIO);u->transfer_flags&=~URB_SHORT_NOT_OK;
 a.requeue=1;assert(!usb_submit_urb(u,GFP_ATOMIC));tmc_lx_native_giveback(u,0,8);assert(a.submit_result==0&&u->submitted&&atomic_read(&u->use_count)==1);a.requeue=0;usb_kill_urb(u);
 struct usb_anchor anchor;init_usb_anchor(&anchor);assert(usb_anchor_empty(&anchor)&&usb_wait_anchor_empty_timeout(&anchor,0));
 usb_anchor_urb(u,&anchor);assert(atomic_read(&u->kref.refcount)==2&&!usb_anchor_empty(&anchor));
 assert(usb_get_from_anchor(&anchor)==u&&usb_anchor_empty(&anchor));assert(atomic_read(&u->kref.refcount)==2);usb_free_urb(u);
 usb_anchor_urb(u,&anchor);usb_anchor_urb(v,&anchor);usb_scuttle_anchored_urbs(&anchor);assert(usb_anchor_empty(&anchor)&&!u->anchor&&!v->anchor);
 usb_anchor_urb(u,&anchor);usb_anchor_urb(v,&anchor);assert(!usb_submit_urb(u,GFP_ATOMIC)&&!usb_submit_urb(v,GFP_ATOMIC));
 a.requeue=d.requeue=1;usb_kill_anchored_urbs(&anchor);a.requeue=d.requeue=0;
 assert(usb_anchor_empty(&anchor)&&!u->submitted&&!v->submitted&&a.submit_result==-EPERM&&d.submit_result==-EPERM);
 usb_anchor_urb(u,&anchor);usb_anchor_urb(v,&anchor);assert(!usb_submit_urb(u,GFP_ATOMIC)&&!usb_submit_urb(v,GFP_ATOMIC));usb_poison_anchored_urbs(&anchor);
 assert(usb_anchor_empty(&anchor)&&anchor.poisoned&&atomic_read(&u->reject)==1&&atomic_read(&v->reject)==1);
 usb_unpoison_urb(u);usb_unpoison_urb(v);usb_unpoison_anchored_urbs(&anchor);
 usb_poison_anchored_urbs(&anchor);usb_anchor_urb(u,&anchor);assert(atomic_read(&u->reject)==1);usb_unpoison_anchored_urbs(&anchor);assert(!atomic_read(&u->reject));usb_unanchor_urb(u);
 usb_anchor_urb(u,&anchor);usb_anchor_urb(v,&anchor);assert(!usb_submit_urb(u,GFP_ATOMIC)&&!usb_submit_urb(v,GFP_ATOMIC));usb_unlink_anchored_urbs(&anchor);assert(usb_anchor_empty(&anchor)&&a.status==-ECONNRESET&&d.status==-ECONNRESET);
 /* Wakeup suspension means empty-list alone is insufficient. Both real
  * timeout expiry and a concurrently resumed wakeup are tested. */
 usb_anchor_suspend_wakeups(&anchor);u64 start=tmc_lx_os_clock_ms();assert(!usb_wait_anchor_empty_timeout(&anchor,4));assert(tmc_lx_os_clock_ms()-start>=4);
 pthread_t thread;assert(!pthread_create(&thread,0,resume_anchor,&anchor));assert(usb_wait_anchor_empty_timeout(&anchor,1000));assert(!pthread_join(thread,0));
 assert(!atomic_read(&u->use_count)&&atomic_read(&u->kref.refcount)==1);
 /* Endpoint queue depth is explicitly bounded: reject a second active
  * logical URB instead of silently losing its completion ownership. */
 assert(!usb_submit_urb(u,GFP_ATOMIC));
 struct urb *second=usb_alloc_urb(0,GFP_KERNEL);assert(second);
 usb_fill_int_urb(second,&b->usb,original_pipe,u->transfer_buffer,8,completed,&a,8);
 assert(usb_submit_urb(second,GFP_ATOMIC)==-EBUSY);b->urb=u;usb_free_urb(second);usb_kill_urb(u);
 struct urb *ctrl=usb_alloc_urb(0,GFP_KERNEL);assert(ctrl);u8 request[8]={0x21,9,0,2,7,0,1,0},payload=2;
 unsigned control_calls=0;b->control=control_done;b->control_context=&control_calls;
 usb_fill_control_urb(ctrl,&b->usb,usb_sndctrlpipe(&b->usb,0),request,&payload,1,completed,&a);
 ctrl->setup_packet=0;assert(usb_submit_urb(ctrl,GFP_KERNEL)==-ENOEXEC);ctrl->setup_packet=request;
 request[6]=2;assert(usb_submit_urb(ctrl,GFP_KERNEL)==-EBADR);request[6]=1;
 request[0]=0xa1;assert(usb_submit_urb(ctrl,GFP_KERNEL)==-EOPNOTSUPP);request[0]=0x21;
 assert(!usb_submit_urb(ctrl,GFP_KERNEL));tmc_linux_usb_service();assert(control_calls==1&&!ctrl->submitted&&a.status==0);
 b->control_urb=0;b->control=0;b->control_context=0;usb_free_urb(ctrl);
 /* Core, not driver, keeps an active URB alive after the driver's last put.
  * Giveback may run a callback that drops the driver's final reference. */
 struct urb *temporary=usb_alloc_urb(0,GFP_KERNEL);assert(temporary);
 void *buffer=tmc_lx_zalloc(8,GFP_KERNEL);assert(buffer);
 usb_fill_int_urb(temporary,&b->usb,original_pipe,buffer,8,completed,&a,8);temporary->transfer_flags=URB_FREE_BUFFER;
 long before=alive;assert(!usb_submit_urb(temporary,GFP_ATOMIC));usb_free_urb(temporary);assert(alive==before);tmc_lx_native_giveback(temporary,0,8);assert(alive==before-2);
 temporary=usb_alloc_urb(0,GFP_KERNEL);buffer=tmc_lx_zalloc(8,GFP_KERNEL);assert(temporary&&buffer);
 usb_fill_int_urb(temporary,&b->usb,original_pipe,buffer,8,completed,&a,8);temporary->transfer_flags=URB_FREE_BUFFER;a.drop=1;
 assert(!usb_submit_urb(temporary,GFP_ATOMIC));before=alive;tmc_lx_native_giveback(temporary,0,8);assert(alive==before-2);a.drop=0;b->urb=u;
 u->complete=old_complete;u->context=ctx;v->complete=old_complete;v->context=ctx2;
 tmc_linux_mouse_detach(keys);tmc_linux_mouse_detach(keys+1);assert(!alive);
 TmcLinuxUrbStats out;tmc_linux_urb_stats(&out);assert(out.submits==out.givebacks&&!out.active&&out.unlinks>4000&&out.rejected>=6);
 puts("PASS complete Linux v6.12 core/urb.c: allocation overflow/refcounts, endpoint/state/packet/SG/interval checks, short packets, requeue, kill/unlink/poison/block, all anchor operations, real timeout and concurrent wakeup, active-driver-put and callback-free lifetime; native HCD backend still bounded");
 return 0;
}
