/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Actual whole Linux generic/input -> shared Boot/native Italian event sink.
 * EP0 is scripted here; real native enumeration/completion has separate tests. */
#define TMC_INPUT_TEST 1
#include "../drivers/input.c"
#include "linux_usb.h"
#include "linux_hid_generic.h"
u64 ticks;
extern long tmc_hid_test_alive,tmc_hid_test_fail;
static const u8 mouse[]={5,1,9,2,0xa1,1,0x85,4,9,1,0xa1,0,
 5,9,0x19,1,0x29,5,0x15,0,0x25,1,0x75,1,0x95,5,0x81,2,
 0x75,3,0x95,1,0x81,1,5,1,9,0x30,9,0x31,0x16,1,0x80,0x26,255,0x7f,0x75,16,0x95,2,0x81,6,
 9,0x38,0x15,0x81,0x25,0x7f,0x75,8,0x95,1,0x81,6,0xc0,0xc0};
static const u8 keyboard[]={5,1,9,6,0xa1,1,5,7,0x19,0xe0,0x29,0xe7,0x15,0,0x25,1,
 0x75,1,0x95,8,0x81,2,0x95,1,0x75,8,0x81,1,0x95,5,0x75,1,5,8,0x19,1,0x29,5,0x91,2,
 0x95,1,0x75,3,0x91,1,0x95,6,0x75,8,0x15,0,0x25,0x65,5,7,0x19,0,0x29,0x65,0x81,0,0xc0};
static const u8 tablet[]={5,1,9,2,0xa1,1,9,1,0xa1,0,5,9,0x19,1,0x29,3,0x15,0,0x25,1,
 0x75,1,0x95,3,0x81,2,0x75,5,0x95,1,0x81,1,5,1,9,0x30,9,0x31,0x15,0,0x26,0xff,0x7f,
 0x75,16,0x95,2,0x81,2,0xc0,0xc0};
struct Transport {const u8 *desc;unsigned length,reads,writes,led,mode;void *key;};
static int control(void *arg,unsigned rt,unsigned request,unsigned value,unsigned iface,u8 *data,unsigned n)
{
 struct Transport *t=arg;assert(iface==7);
 if(rt==0x81&&request==6){assert(value==0x2200&&n<=t->length);t->reads++;memcpy(data,t->desc,n);if(t->mode==1)data[0]^=1;return t->mode==2?n-1:n;}
 assert(rt==0x21&&request==9&&value==0x200&&n==1);t->writes++;t->led=data[0];
 if(t->mode>=3){
  /* EP0 wait dispatches another device's report while generic owns its lock.
   * It must be copied and later delivered, not dropped or recursively run. */
  unsigned count=t->mode==3?1:9;
  for(unsigned i=0;i<count;i++){
   int result=tmc_linux_generic_report(t->key,(u8[]){4,1,7,0,0,0,0},7);
   assert(result==(i==8?-28:0));
  }
 }
 return n;
}
static int boot_control(void *arg,const u8 setup[8],u8 *data,unsigned n)
{(void)arg;(void)setup;(void)data;return n==1?0:-22;}
static int attach(void *key,struct Transport *t)
{
 return tmc_linux_generic_attach(key,t->desc,t->length,64,7,control,t,boot.w,boot.h,hid_absolute);
}
static void drain(void){Event e;while(input_pop(&e)){};}
static void key_report(void *key,unsigned mods,unsigned usage)
{
 assert(!tmc_linux_generic_report(key,(u8[]){mods,0,usage,0,0,0,0,0},8));
}
static void expect_key(int code)
{
 Event e;assert(input_pop(&e)&&e.type==EV_KEY&&e.a==code);assert(!input_pop(&e));
}
static void key(void *owner,unsigned usage,unsigned mods,int code)
{
 key_report(owner,0,0);key_report(owner,mods,usage);expect_key(code);key_report(owner,0,0);
}
int main(void)
{
 int a,b,c;struct Transport k={.desc=keyboard,.length=sizeof keyboard},m={.desc=mouse,.length=sizeof mouse},t={.desc=tablet,.length=sizeof tablet};
 k.mode=1;assert(!attach(&a,&k));k.mode=2;assert(!attach(&a,&k));k.mode=0;assert(!tmc_hid_test_alive);
 for(int fail=0;fail<12;fail++){
  tmc_hid_test_fail=fail;assert(!attach(&a,&k));tmc_hid_test_fail=-1;assert(!tmc_hid_test_alive);
 }
 assert(attach(&a,&k)==1&&k.reads>=3);assert(!attach(&a,&k));
 key(&a,31,2,'"');key(&a,32,2,0xa3);key(&a,47,0,0xe8);key(&a,47,2,0xe9);
 key(&a,51,64,'@');key(&a,8,64,0x20ac);key(&a,100,0,'<');key(&a,100,2,'>');
 assert(tmc_linux_keyboard_attach(&b,1,2,8,10,7,boot_control,0));
 key_report(&a,2,0);assert(tmc_linux_keyboard_report(&b,(u8[]){0,0,4,0,0,0,0,0},8));expect_key('A');
 assert(!tmc_linux_generic_detach(&a));ticks+=50;input_repeat();expect_key('a');tmc_linux_keyboard_detach(&b);drain();
 puts("PASS native generic Italian keyboard, AltGr/ISO, descriptor revalidation/short failure, allocation rollback and mixed Boot+generic keyboard ownership");
 assert(!tmc_linux_generic_attach(&a,mouse,sizeof mouse,6,7,control,&m,boot.w,boot.h,hid_absolute));drain();assert(!tmc_hid_test_alive);
 assert(attach(&a,&m)==2);assert(tmc_linux_mouse_attach(&b,1,2,8,10));
 assert(tmc_linux_mouse_report(&b,(u8[]){2,0,0},3));drain();
 assert(!tmc_linux_generic_report(&a,(u8[]){4,1,0x34,0x12,0xfe,0xff,255},7));
 Event e;assert(input_pop(&e)&&e.type==EV_MOUSE&&e.a==0x1234&&e.b==-2&&e.c==3);
 assert(input_pop(&e)&&e.type==EV_SCROLL&&e.a==-1);drain();
 tmc_linux_generic_error(&a,0);assert(input_pop(&e)&&e.type==EV_MOUSE&&e.c==2);drain();
 assert(!tmc_linux_generic_report(&a,(u8[]){4,1,0,0,1,0,0},7));
 assert(input_pop(&e)&&e.type==EV_MOUSE&&e.b==1&&e.c==3);drain();
 assert(!tmc_linux_generic_detach(&a));assert(input_pop(&e)&&e.c==2);drain();tmc_linux_mouse_detach(&b);drain();
 assert(attach(&c,&t)==2);assert(!tmc_linux_generic_report(&c,(u8[]){1,255,127,0,64},5));
 assert(input_pop(&e)&&e.type==EV_POINTER_ABS&&e.a==1279&&e.b>=399&&e.b<=400&&e.c==1);drain();
 tmc_linux_generic_error(&c,1);drain();assert(tmc_linux_generic_report(&c,(u8[5]){0},5)==-19);assert(!tmc_linux_generic_detach(&c));drain();
 puts("PASS native generic 16-bit relative/wheel and absolute scale; mixed Boot+generic button ownership across STALL, recovery, fatal retirement and detach");
 assert(attach(&a,&k)==1&&attach(&b,&m)==2);k.key=&b;k.mode=3;
 tmc_linux_generic_leds(2);tmc_linux_generic_service();assert(k.writes&&k.led==2);
 assert(input_pop(&e)&&e.type==EV_MOUSE&&e.a==7&&e.c==1);drain();
 k.mode=4;tmc_linux_generic_leds(0);tmc_linux_generic_service();
 /* Queue overflow discards stale frames and releases the device's held keys/buttons. */
 assert(input_pop(&e)&&e.type==EV_MOUSE&&!e.c);drain();
 assert(!tmc_linux_generic_report(&b,(u8[]){4,1,1,0,0,0,0},7));assert(input_pop(&e)&&e.c==1);drain();
 assert(!tmc_linux_generic_detach(&a)&&!tmc_linux_generic_detach(&b));drain();assert(!tmc_hid_test_alive);
 for(unsigned i=0;i<128;i++){assert(attach(&a,&m)==2);assert(!tmc_linux_generic_detach(&a));drain();assert(!tmc_hid_test_alive);}
 puts("PASS deferred real LED SET_REPORT, cross-device EP0 reentry, bounded queue overflow/quiesce/recovery, 128 leak-free native generic lifecycles");
}
