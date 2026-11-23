/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Execute both COMPLETE original Linux units and the real bounded adapter.
 * Scripted low-level I/O is host fault injection, not HCD/hardware evidence. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../compat/linux/hid_parser.c"
extern long tmc_hid_test_alive,tmc_hid_test_fail;
extern void (*tmc_hid_test_alloc_hook)(void);
static const u8 mouse[]={5,1,9,2,0xa1,1,0x85,4,9,1,0xa1,0,
 5,9,0x19,1,0x29,5,0x15,0,0x25,1,0x75,1,0x95,5,0x81,2,
 0x75,3,0x95,1,0x81,1,5,1,9,0x30,9,0x31,0x16,1,0x80,0x26,255,0x7f,0x75,16,0x95,2,0x81,6,
 9,0x38,0x15,0x81,0x25,0x7f,0x75,8,0x95,1,0x81,6,0xc0,0xc0};
static const u8 keyboard[]={5,1,9,6,0xa1,1,5,7,0x19,0xe0,0x29,0xe7,0x15,0,0x25,1,
 0x75,1,0x95,8,0x81,2,0x95,1,0x75,8,0x81,1,0x95,5,0x75,1,5,8,0x19,1,0x29,5,0x91,2,
 0x95,1,0x75,3,0x91,1,0x95,6,0x75,8,0x15,0,0x25,0x65,5,7,0x19,0,0x29,0x65,0x81,0,0xc0};
static const u8 nkro[]={5,1,9,6,0xa1,1,0x85,2,5,7,0x19,4,0x29,19,0x15,0,0x25,1,0x75,1,0x95,16,0x81,2,0xc0};
struct Sink {unsigned starts,stops,opens,closes,adds,removes,events,syncs,raw,output;int start_error,open_error,input_error,raw_error,output_error;int x,y,wheel,hiwheel,keys[KEY_CNT];unsigned apps[17],tokens,bytes;u8 last[64];TmcHidGeneric *reenter;unsigned nested;};
static int start(void *p){struct Sink *s=p;s->starts++;return s->start_error;}
static void stop(void *p){((struct Sink *)p)->stops++;}
static int open_input(void *p){struct Sink *s=p;s->opens++;return s->open_error;}
static void close_input(void *p){((struct Sink *)p)->closes++;}
static int register_input(void *p,unsigned token,unsigned app,int add)
{
 struct Sink *s=p;assert(token&&token<17);
 if(add){s->adds++;s->apps[token]=app;s->tokens=MAX(s->tokens,token);return s->input_error;}
 s->removes++;return 0;
}
static void event(void *p,unsigned token,unsigned type,unsigned code,int value)
{
 struct Sink *s=p;assert(token&&token<=s->tokens);s->events++;
 if(s->reenter){assert(tmc_hid_generic_report(s->reenter,(u8[]){0},1)==-EBUSY);s->nested++;}
 if(type==EV_KEY){assert(code<KEY_CNT);s->keys[code]=value;}
 if(type==EV_REL){if(code==REL_X)s->x+=value;if(code==REL_Y)s->y+=value;if(code==REL_WHEEL)s->wheel+=value;if(code==REL_WHEEL_HI_RES)s->hiwheel+=value;}
 if(type==EV_SYN)s->syncs++;
}
static int raw(void *p,unsigned id,u8 *bytes,unsigned n,unsigned type,unsigned request)
{
 struct Sink *s=p;s->raw++;
 if(request!=HID_REQ_SET_REPORT||type!=HID_OUTPUT_REPORT||id!=0)return -EOPNOTSUPP;
 if(s->raw_error)return s->raw_error;assert(n<=sizeof s->last);memcpy(s->last,bytes,n);s->bytes=n;return n;
}
static int output(void *p,const u8 *bytes,unsigned n)
{
 struct Sink *s=p;s->output++;(void)bytes;(void)n;return s->output_error?s->output_error:-ENOSYS;
}
static const TmcHidGenericOps ops={start,stop,open_input,close_input,register_input,event,raw,output};
static void clean(void)
{
 TmcHidParserStats c;tmc_linux_hid_parser_stats(&c);assert(!c.live_bytes&&c.allocations==c.frees&&!tmc_hid_test_alive);
}
static void detach(TmcHidGeneric *g,struct Sink *s)
{
 assert(!tmc_hid_generic_detach(g));assert(s->starts==s->stops&&s->opens==s->closes&&s->adds==s->removes);clean();
}
int main(void)
{
 clean();struct Sink s={0};TmcHidGeneric *g=tmc_hid_generic_attach(mouse,sizeof mouse,&ops,&s);assert(g);
 assert(s.starts==1&&s.opens==1&&s.adds==1&&s.apps[1]==HID_GD_MOUSE);
 u8 m[]={4,0x15,0x34,0x12,0xfe,0xff,255};assert(!tmc_hid_generic_report(g,m,sizeof m));
 assert(s.x==0x1234&&s.y==-2&&s.keys[BTN_LEFT]&&s.keys[BTN_MIDDLE]&&s.keys[BTN_EXTRA]&&s.wheel==-1&&s.hiwheel==-120);
 for(unsigned n=0;n<sizeof m;n++)assert(tmc_hid_generic_report(g,m,n)<0);
 m[0]=5;assert(tmc_hid_generic_report(g,m,sizeof m)<0);m[0]=4;
 s.reenter=g;assert(!tmc_hid_generic_report(g,m,sizeof m));assert(s.nested);s.reenter=NULL;
 detach(g,&s);assert(!s.keys[BTN_LEFT]&&!s.keys[BTN_MIDDLE]&&!s.keys[BTN_EXTRA]);
 puts("PASS whole Linux hid-generic probe/match + hid-input mapping/events: signed 16-bit XY, five buttons, high/low-res wheel, strict framing, nested rejection and disconnect releases");
 s=(struct Sink){0};g=tmc_hid_generic_attach(keyboard,sizeof keyboard,&ops,&s);assert(g);
 u8 k[8]={2,0,4};assert(!tmc_hid_generic_report(g,k,8));assert(s.keys[KEY_A]&&s.keys[KEY_LEFTSHIFT]);
 unsigned old=s.events;assert(!tmc_hid_generic_report(g,k,8));assert(s.events==old+1); /* sync only */
 k[2]=1;assert(!tmc_hid_generic_report(g,k,8)&&s.keys[KEY_A]); /* ErrorRollOver retains held array */
 memset(k,0,8);assert(!tmc_hid_generic_report(g,k,8)&&!s.keys[KEY_A]&&!s.keys[KEY_LEFTSHIFT]);
 assert(!tmc_hid_generic_set_led(g,LED_CAPSL,1)&&!s.raw);assert(!tmc_hid_generic_service(g));assert(s.output==1&&s.raw==1&&s.bytes==1&&s.last[0]==2);
 s.raw_error=-32;assert(!tmc_hid_generic_set_led(g,LED_CAPSL,0));assert(!tmc_hid_generic_service(g));
 TmcHidGenericStats gc;tmc_hid_generic_stats(&gc);assert(gc.output_errors>=1);s.raw_error=0;
 detach(g,&s);
 s=(struct Sink){0};g=tmc_hid_generic_attach(nkro,sizeof nkro,&ops,&s);assert(g);
 assert(!tmc_hid_generic_report(g,(u8[]){2,255,255},3));unsigned held=0;for(unsigned i=0;i<KEY_CNT;i++)held+=!!s.keys[i];assert(held==16);detach(g,&s);
 puts("PASS whole Linux hid-input keyboard arrays/modifiers, unchanged-report filtering, rollover preservation, 16-key report bitmap and original deferred LED worker/output/raw fallback/errors");
 /* Original generic matching must defer to an actually registered competing
  * driver's static ID table, not a fabricated always-match bus result. */
 struct hid_device d={.bus=3,.group=HID_GROUP_GENERIC};
 assert(hid_generic_match(&d,0));d.quirks=HID_QUIRK_HAVE_SPECIAL_DRIVER;assert(!hid_generic_match(&d,0)&&hid_generic_match(&d,1));d.quirks=0;
 const struct hid_device_id ids[]={{HID_DEVICE(3,HID_GROUP_ANY,HID_ANY_ID,HID_ANY_ID)},{0}};
 struct hid_driver competitor={.name="host-registered-competitor",.id_table=ids};hid_bus_type.drivers[hid_bus_type.count++]=&competitor;
 assert(!hid_generic_match(&d,0));hid_bus_type.count--;assert(hid_generic_match(&d,0));
 /* Composite per-application input ownership, two actual registrations but
  * one low-level open/close reference. */
 u8 composite[256];memcpy(composite,mouse,sizeof mouse);memcpy(composite+sizeof mouse,nkro,sizeof nkro);
 s=(struct Sink){0};g=tmc_hid_generic_attach(composite,sizeof mouse+sizeof nkro,&ops,&s);assert(g&&s.adds==2&&s.opens==1&&s.starts==1);detach(g,&s);
 puts("PASS original generic competing-driver preference and special-driver override, composite per-application registration and low-level open/close references");
 TmcHidParserStats before,after;tmc_linux_hid_parser_stats(&before);
 s=(struct Sink){0};g=tmc_hid_generic_attach(keyboard,sizeof keyboard,&ops,&s);assert(g);tmc_linux_hid_parser_stats(&after);unsigned allocations=after.allocations-before.allocations;detach(g,&s);
 for(unsigned n=0;n<allocations;n++){
  s=(struct Sink){0};tmc_hid_test_fail=n;g=tmc_hid_generic_attach(keyboard,sizeof keyboard,&ops,&s);tmc_hid_test_fail=-1;assert(!g);clean();assert(s.starts==s.stops&&s.opens==s.closes&&s.adds==s.removes);
 }
 s=(struct Sink){.start_error=-5};assert(!tmc_hid_generic_attach(mouse,sizeof mouse,&ops,&s));assert(s.starts==1&&!s.stops&&!s.adds);clean();
 s=(struct Sink){.open_error=-5};assert(!tmc_hid_generic_attach(mouse,sizeof mouse,&ops,&s));assert(s.starts==s.stops&&s.adds==s.removes&&s.opens==1&&!s.closes);clean();
 s=(struct Sink){.input_error=-5};assert(!tmc_hid_generic_attach(mouse,sizeof mouse,&ops,&s));assert(s.starts==s.stops&&s.adds==1&&!s.removes&&!s.opens);clean();
 puts("PASS all generic/input allocation-failure rollbacks, real transport-start/open and input-registration failures; no successful placeholder lifecycle or leaked objects");
 for(unsigned n=0;n<1024;n++){
  s=(struct Sink){0};g=tmc_hid_generic_attach(keyboard,sizeof keyboard,&ops,&s);assert(g);k[2]=4;assert(!tmc_hid_generic_report(g,k,8));detach(g,&s);
 }
 puts("PASS 1024 complete upstream generic/input attach/report/disconnect lifecycles with release, work cancellation and zero outstanding allocations");
 u32 random=0x13579bdf;
 for(unsigned n=0;n<4096;n++){
  memcpy(composite,keyboard,sizeof keyboard);random=random*1664525u+1013904223u;unsigned at=random%sizeof keyboard;
  random=random*1664525u+1013904223u;composite[at]^=1u<<(random%8);
  s=(struct Sink){0};g=tmc_hid_generic_attach(composite,sizeof keyboard,&ops,&s);
  if(g){u8 packet[64]={0};(void)tmc_hid_generic_report(g,packet,sizeof packet);detach(g,&s);}else clean();
 }
 puts("PASS 4096 deterministic whole generic/input descriptor mutations, real report dispatch when accepted, complete teardown and sanitizers");
 return 0;
}
