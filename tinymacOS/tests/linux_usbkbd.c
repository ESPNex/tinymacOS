/* Actual upstream usbkbd + compatibility + actual native Italian input.
 * No replacement keyboard decoder and no privileged instructions here. */
#include <time.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#define TMC_INPUT_TEST 1
#include "../drivers/input.c"
#include "../compat/linux/usb_input.c"
u64 ticks;
static long alive,fail_after=-1;
static unsigned lock_word,control_calls,control_mode;
static unsigned last_led,last_iface;
static void *active_key;
static pthread_mutex_t gate=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condition=PTHREAD_COND_INITIALIZER;
static int entered,released;
static void *allocate(size_t n,int dma)
{
 if(!fail_after)return 0;if(fail_after>0)fail_after--;
 void *p=dma?aligned_alloc(4096,4096):malloc(n);if(p)alive++;return p;
}
void *tmc_lx_os_alloc(size_t n){return allocate(n,0);}
void tmc_lx_os_free(void *p){if(p){alive--;free(p);}}
void *tmc_lx_os_dma_alloc(size_t n,u64 *dma){assert(n<=4096);void *p=allocate(n,1);*dma=(u64)p;return p;}
void tmc_lx_os_dma_free(void *p,u64 dma){assert((u64)p==dma);tmc_lx_os_free(p);}
u32 tmc_lx_os_lock(void){while(__sync_lock_test_and_set(&lock_word,1)){}return 0;}
void tmc_lx_os_unlock(u32 flags){(void)flags;__sync_lock_release(&lock_word);}
void tmc_lx_os_pointer(int x,int y,int b,int w){hid_relative(x,y,b,w);}
void tmc_lx_os_keyboard(const u8 bits[32],unsigned mods){hid_linux_keys(bits,mods);}
void tmc_lx_os_error(const char *s){assert(s&&*s);}
u32 tmc_lx_os_irq_save(void){return 0;}
void tmc_lx_os_irq_restore(u32 f){assert(f==0||f==0x200);}
void tmc_lx_os_spin_wait(void){__asm__ volatile("pause");}
static int control(void *context,const u8 setup[8],u8 *data,unsigned n)
{
 assert(context==active_key&&n==1&&setup[0]==0x21&&setup[1]==9);
 assert(setup[2]==0&&setup[3]==2&&setup[5]==0&&setup[6]==1&&setup[7]==0);
 control_calls++;last_led=data[0];last_iface=setup[4];
 /* Reentry during native EP0 wait must not deadlock the compatibility lock. */
 assert(tmc_linux_keyboard_report(context,(u8[8]){0},8));
 if(control_mode==1){
  pthread_mutex_lock(&gate);entered=1;pthread_cond_broadcast(&condition);
  while(!released)pthread_cond_wait(&condition,&gate);
  pthread_mutex_unlock(&gate);
 }else if(control_mode==2){
  struct LxBinding *b=binding(context);u32 flags=tmc_lx_os_lock();usb_kill_urb(b->control_urb);tmc_lx_os_unlock(flags);
  tmc_linux_keyboard_leds(4);
 }else if(control_mode==3)return -EPIPE;
 return 0;
}
static int attach(void *key){return tmc_linux_keyboard_attach(key,0x1234,0x5678,8,10,7,control,key);}
static void report(void *key,unsigned modifiers,unsigned usage)
{
 u8 raw[8]={(u8)modifiers,0,(u8)usage};assert(tmc_linux_keyboard_report(key,raw,8));
}
static void drain(void){Event e;while(input_pop(&e)){};}
static void expect(int code)
{
 Event e;assert(input_pop(&e));assert(e.type==EV_KEY&&e.a==code);assert(!input_pop(&e));
}
static void key(void *owner,unsigned usage,unsigned modifiers,int code)
{
 report(owner,0,0);report(owner,modifiers,usage);expect(code);report(owner,0,0);
}
static void *service(void *unused){(void)unused;tmc_linux_usb_service();return 0;}
int main(void)
{
 assert(boot.w==1280);int owner=0,other=0;active_key=&owner;
 assert(!tmc_linux_keyboard_attach(0,1,2,8,10,7,control,0));
 assert(!tmc_linux_keyboard_attach(&owner,1,2,7,10,7,control,0));
 assert(!tmc_linux_keyboard_attach(&owner,1,2,8,10,256,control,0));
 for(int fail=0;fail<7;fail++){
  fail_after=fail;assert(!attach(&owner));assert(!binding(&owner)&&!alive);
 }
 fail_after=-1;assert(attach(&owner)&&attach(&owner));
 struct LxBinding *b=binding(&owner);assert(b&&b->kind==1&&b->urb&&b->control_urb&&b->urb!=b->control_urb);
 assert(!strcmp(b->input->name,"tinymacOS Linux usbkbd"));assert(!tmc_linux_mouse_attach(&owner,1,2,8,10));
 key(&owner,4,2,'A');key(&owner,31,2,'"');key(&owner,32,2,0xa3);
 key(&owner,47,0,0xe8);key(&owner,47,2,0xe9);key(&owner,46,0,0xec);
 key(&owner,51,64,'@');key(&owner,52,64,'#');key(&owner,8,64,0x20ac);
 key(&owner,100,0,'<');key(&owner,100,2,'>');key(&owner,56,2,'_');
 key(&owner,47,66,'{');key(&owner,48,66,'}');
 report(&owner,0,4);expect('a');report(&owner,0,4);assert(!input_pop(&(Event){0}));
 ticks+=50;input_repeat();expect('a');report(&owner,0,0);ticks+=50;input_repeat();assert(!input_pop(&(Event){0}));
 report(&owner,0,57);report(&owner,0,57);assert(caps);report(&owner,0,0);
 key(&owner,4,0,'A');key(&owner,4,2,'a');report(&owner,0,57);report(&owner,0,0);assert(!caps);
 /* Reject rollover/truncation without manufacturing releases or a stuck URB. */
 report(&owner,0,4);expect('a');u8 rollover[8]={0,0,1,1,1,1,1,1};
 assert(tmc_linux_keyboard_report(&owner,rollover,8));assert(test_bit(30,b->input->key));
 assert(!tmc_linux_keyboard_report(&owner,rollover,7));assert(!tmc_linux_keyboard_report(&owner,(u8[9]){0},9));
 report(&owner,0,0);assert(!test_bit(30,b->input->key));
 assert(attach(&other));report(&owner,2,0);report(&other,0,4);expect('A');
 report(&owner,0,0);ticks+=50;input_repeat();expect('a');report(&other,0,0);
 report(&owner,0,4);expect('a');report(&other,0,4);assert(!input_pop(&(Event){0}));
 report(&owner,0,0);ticks+=50;input_repeat();expect('a');report(&other,0,0);
 u8 six_a[8]={0,0,4,5,6,7,8,9},six_b[8]={0,0,10,11,12,13,14,15};
 assert(tmc_linux_keyboard_report(&owner,six_a,8));assert(tmc_linux_keyboard_report(&other,six_b,8));
 unsigned count=0;Event event;while(input_pop(&event))count++;assert(count==12);
 tmc_linux_keyboard_detach(&other);report(&owner,0,0);assert(!repeat);
 /* Logical URB cancellation/resubmission runs the actual upstream IRQ. */
 usb_kill_urb(b->urb);assert(!b->urb->submitted);assert(!b->input->open(b->input));key(&owner,4,0,'a');
 tmc_linux_keyboard_leds(2);assert(!control_calls&&b->control_urb->submitted);
 tmc_linux_keyboard_leds(4);tmc_linux_usb_service();assert(control_calls==1&&last_led==2&&last_iface==7);
 assert(b->control_urb->submitted);tmc_linux_usb_service();assert(control_calls==2&&last_led==4&&!b->control_urb->submitted);
 /* Cancel + resubmit same URB during an EP0 wait: submit-serial ABA guard. */
 tmc_linux_keyboard_leds(2);control_mode=2;tmc_linux_usb_service();control_mode=0;
 assert(b->control_urb->submitted);tmc_linux_usb_service();assert(last_led==4&&!b->control_urb->submitted);
 /* Disconnect/free/rebind while service is outside the lock. */
 tmc_linux_keyboard_leds(2);control_mode=1;pthread_t thread;assert(!pthread_create(&thread,0,service,0));
 pthread_mutex_lock(&gate);while(!entered)pthread_cond_wait(&condition,&gate);pthread_mutex_unlock(&gate);
 tmc_linux_keyboard_detach(&owner);assert(!alive);assert(attach(&owner));
 pthread_mutex_lock(&gate);released=1;pthread_cond_broadcast(&condition);pthread_mutex_unlock(&gate);
 assert(!pthread_join(thread,0));control_mode=0;b=binding(&owner);assert(b&&b->opened&&!b->control_urb->submitted);
 TmcLinuxKeyboardStats ks;tmc_linux_keyboard_stats(&ks);assert(ks.stale_controls>=2);
 tmc_linux_keyboard_leds(2);control_mode=3;tmc_linux_usb_service();control_mode=0;
 tmc_linux_keyboard_stats(&ks);assert(ks.control_errors==1&&!b->control_urb->submitted);
 tmc_linux_keyboard_detach(&owner);assert(!alive&&!repeat);drain();
 TmcLinuxUsbStats ms;tmc_linux_mouse_stats(&ms);assert(ms.allocations==ms.frees);
 tmc_linux_keyboard_stats(&ks);assert(ks.attached==ks.disconnected&&ks.completions>30&&ks.controls>=6);
 puts("PASS complete upstream Linux usbkbd: 7 allocation rollbacks, actual Italian events/AltGr/ISO/caps/repeat, multi-keyboard union >6 keys, rollover guards, IRQ cancel/reopen, deferred native LED control, reentry, submit-serial and disconnect/rebind ABA, STALL and leak-free teardown");
 return 0;
}

u64 tmc_lx_os_clock_ms(void){struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (u64)t.tv_sec*1000+t.tv_nsec/1000000;}
