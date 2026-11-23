/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Actual unmodified upstream usbmouse.c is linked separately. Only OS memory,
 * lock and event imports are provided here; no replacement mouse decoder. */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "../compat/linux/usb_input.c"
static long alive,fail_after=-1;static unsigned lock_word;
static int events,last_x,last_y,last_buttons,last_wheel,errors;
static void *allocate(size_t n,int aligned)
{
 if(fail_after==0)return 0;if(fail_after>0)fail_after--;
 void *p=aligned?aligned_alloc(4096,4096):malloc(n);if(p)alive++;return p;
}
void *tmc_lx_os_alloc(size_t n){return allocate(n,0);}
void tmc_lx_os_free(void *p){if(p){alive--;free(p);}}
void *tmc_lx_os_dma_alloc(size_t n,u64 *dma){assert(n<=4096);void *p=allocate(n,1);*dma=(u64)p;return p;}
void tmc_lx_os_dma_free(void *p,u64 dma){assert((u64)p==dma);tmc_lx_os_free(p);}
u32 tmc_lx_os_lock(void){while(__sync_lock_test_and_set(&lock_word,1)){}return 0;}
void tmc_lx_os_unlock(u32 f){(void)f;__sync_lock_release(&lock_word);}
void tmc_lx_os_pointer(int x,int y,int b,int w){events++;last_x=x;last_y=y;last_buttons=b;last_wheel=w;}
void tmc_lx_os_error(const char *s){assert(s&&*s);errors++;}
void tmc_lx_os_keyboard(const u8 bitmap[32],unsigned mods){(void)bitmap;(void)mods;assert(!"unexpected keyboard in mouse-only fixture");}
u32 tmc_lx_os_irq_save(void){return 0;}
void tmc_lx_os_irq_restore(u32 f){assert(!f);}
void tmc_lx_os_spin_wait(void){__asm__ volatile("pause");}
static void check(void *key,const u8 *raw,unsigned n,int x,int y,int buttons,int wheel)
{
 int prior=events;assert(tmc_linux_mouse_report(key,raw,n));assert(events==prior+1);
 assert(last_x==x&&last_y==y&&last_buttons==buttons&&last_wheel==wheel);
}
static void *worker(void *p)
{
 int key=0;for(unsigned i=0;i<2000;i++){
  assert(tmc_linux_mouse_attach(&key,0x1234,(unsigned)(u64)p,8,10));
  assert(tmc_linux_mouse_report(&key,(u8[]){0,4,252,1},4));tmc_linux_mouse_detach(&key);
 }return 0;
}
int main(void)
{
 int key=0;assert(!tmc_linux_mouse_attach(0,0,0,8,10));assert(!tmc_linux_mouse_attach(&key,0,0,2,10));
 for(int fail=0;fail<4;fail++){
  fail_after=fail;assert(!tmc_linux_mouse_attach(&key,1,2,8,10));assert(!binding(&key)&&!alive);
 }
 fail_after=-1;assert(tmc_linux_mouse_attach(&key,0x1234,0x5678,8,10));
 assert(tmc_linux_mouse_attach(&key,0x1234,0x5678,8,10));
 struct LxBinding *b=binding(&key);assert(b&&b->input&&b->opened&&b->urb->submitted);
 assert(b->input->id.vendor==0x1234&&b->input->id.product==0x5678);
 assert(strcmp(b->input->name,"tinymacOS Linux usbmouse")==0);
 assert(strcmp(b->input->phys,"native-hcd/interrupt-in/input0")==0);
 check(&key,(u8[]){31,127,128,255},4,127,-128,31,-1);
 check(&key,(u8[]){0,128,127},3,-128,127,0,0);
 check(&key,(u8[]){1,0,0,127},4,0,0,1,127);
 int previous_events=events;assert(!tmc_linux_mouse_report(&key,(u8[]){0,1},2));assert(events==previous_events);
 assert(!tmc_linux_mouse_report(&key,0,3));assert(!tmc_linux_mouse_report(&key,(u8[9]){0},9));
 assert(usb_submit_urb(b->urb,GFP_ATOMIC)==-EBUSY);
 b->urb->submitted=0;b->urb->status=-EPIPE;b->urb->complete(b->urb);assert(b->urb->submitted&&events==previous_events);
 int statuses[]={-ENOENT,-ECONNRESET,-ESHUTDOWN};
 for(unsigned i=0;i<COUNT(statuses);i++){
  b->urb->submitted=0;b->urb->status=statuses[i];b->urb->complete(b->urb);assert(!b->urb->submitted);
  assert(!b->input->open(b->input));assert(b->urb->submitted);
 }
 usb_kill_urb(b->urb);assert(!b->urb->submitted);assert(!b->input->open(b->input));
 check(&key,(u8[]){0,9,251,0},4,9,-5,0,0);
 tmc_linux_mouse_detach(&key);assert(!alive&&!binding(&key));tmc_linux_mouse_detach(&key);
 assert(!tmc_linux_mouse_report(&key,(u8[4]){0},4));
 assert(tmc_linux_mouse_attach(&key,1,2,4,10));check(&key,(u8[]){1,9,8,7},4,9,8,1,7);
 previous_events=events;tmc_linux_mouse_detach(&key);
 assert(events==previous_events+1&&!last_buttons&&!last_x&&!last_y&&!last_wheel&&!alive);
 int keys[LX_BINDINGS+1];for(unsigned i=0;i<LX_BINDINGS;i++)assert(tmc_linux_mouse_attach(keys+i,1,i,4,10));
 assert(!tmc_linux_mouse_attach(keys+LX_BINDINGS,1,2,4,10));
 for(unsigned i=0;i<LX_BINDINGS;i++)tmc_linux_mouse_detach(keys+i);assert(!alive);
 pthread_t a,c;assert(!pthread_create(&a,0,worker,(void *)1));assert(!pthread_create(&c,0,worker,(void *)2));assert(!pthread_join(a,0));assert(!pthread_join(c,0));assert(!alive);
 TabletLayout layout={0};assert(tmc_linux_mouse_boot_layout(&layout));layout.bytes=8;layout.id=4;assert(!tmc_linux_mouse_boot_layout(&layout));
 layout=(TabletLayout){.bytes=4,.relative=1,.x={8,8,-127,127},.y={16,8,-127,127},.wheel={24,8,-127,127}};
 assert(tmc_linux_mouse_boot_layout(&layout));layout.x.width=16;assert(!tmc_linux_mouse_boot_layout(&layout));
 char text[12];assert(tmc_lx_snprintf(text,sizeof text,"%04x:%04x",0x1234u,0x5678u)==9&&!strcmp(text,"1234:5678"));
 assert(tmc_lx_snprintf(text,5,"%s-%d","abc",-123)==8&&!strcmp(text,"abc-"));assert(tmc_lx_snprintf(0,0,"%d",-123)==4);
 assert(tmc_lx_snprintf(text,sizeof text,"%q")==-EINVAL);
 assert(tmc_lx_strscpy(text,"abcdefghijklm",sizeof text)==-7&&text[11]==0);
 TmcLinuxUsbStats out;tmc_linux_mouse_stats(&out);assert(out.allocations==out.frees&&out.attached==out.disconnected&&!errors);
 puts("PASS actual Linux v6.12 usbmouse: probe/open/IRQ signed XY/buttons/wheel, three-byte zero padding, bad/truncated reports, URB resubmit/error/cancel/reopen, all allocation-failure rollbacks, bounded slots, disconnect, 4000 concurrent lifecycles, POSIX string/format bounds and non-Boot layout exclusion");
}
