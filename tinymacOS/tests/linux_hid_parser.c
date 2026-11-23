/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "linux_hid_parser.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
extern long tmc_hid_test_alive,tmc_hid_test_fail;
extern void (*tmc_hid_test_alloc_hook)(void);
static const u8 mouse[]={
 5,1,9,2,0xa1,1,0x85,4,9,1,0xa1,0,
 5,9,0x19,1,0x29,5,0x15,0,0x25,1,0x75,1,0x95,5,0x81,2,
 0x75,3,0x95,1,0x81,1,
 5,1,9,0x30,9,0x31,0x16,0x01,0x80,0x26,0xff,0x7f,0x75,16,0x95,2,0x81,6,
 9,0x38,0x15,0x81,0x25,0x7f,0x75,8,0x95,1,0x81,6,0xc0,0xc0
};
static const u8 keyboard[]={
 5,1,9,6,0xa1,1,5,7,0x19,0xe0,0x29,0xe7,0x15,0,0x25,1,
 0x75,1,0x95,8,0x81,2,0x95,1,0x75,8,0x81,1,
 0x95,5,0x75,1,5,8,0x19,1,0x29,5,0x91,2,
 0x95,1,0x75,3,0x91,1,0x95,6,0x75,8,0x15,0,0x25,0x65,
 5,7,0x19,0,0x29,0x65,0x81,0,0xc0
};
static const u8 tablet[]={
 5,1,9,2,0xa1,1,9,1,0xa1,0,
 5,9,0x19,1,0x29,3,0x15,0,0x25,1,0x75,1,0x95,3,0x81,2,
 0x75,5,0x95,1,0x81,1,
 5,1,9,0x30,9,0x31,0x15,0,0x26,0xff,0x7f,0x75,16,0x95,2,0x81,2,0xc0,0xc0
};
static unsigned parallel_success;
static void nested(void){assert(!tmc_linux_hid_parse(mouse,sizeof mouse));}
static void *parallel_parse(void *arg)
{
 (void)arg;
 for(unsigned i=0;i<256;i++){
  TmcLinuxHidParser *p=tmc_linux_hid_parse(mouse,sizeof mouse);
  if(p){TmcHidReportInfo r;assert(tmc_linux_hid_report(p,0,4,&r)&&r.bytes==7);tmc_linux_hid_close(p);__atomic_add_fetch(&parallel_success,1,__ATOMIC_RELAXED);}
 }
 return NULL;
}
static void clean(void)
{
 TmcHidParserStats s;tmc_linux_hid_parser_stats(&s);
 assert(!tmc_hid_test_alive&&!s.live_bytes&&s.allocations==s.frees);
}
static void reject(const u8 *d,unsigned n)
{
 TmcLinuxHidParser *p=tmc_linux_hid_parse(d,n);assert(!p);clean();
}
int main(void)
{
 clean();TmcLinuxHidParser *p=tmc_linux_hid_parse(mouse,sizeof mouse);assert(p);
 TmcHidReportInfo r;TmcHidFieldInfo f;u32 usage;
 assert(tmc_linux_hid_report(p,0,4,&r)&&r.bits==48&&r.bytes==7&&r.fields==3&&r.numbered&&r.application==0x10002);
 assert(tmc_linux_hid_field(p,0,4,1,&f)&&f.offset==8&&f.width==16&&f.count==2&&f.minimum==-32767&&f.maximum==32767);
 assert(tmc_linux_hid_usage(p,0,4,1,0,&usage)&&usage==0x10030);
 assert(tmc_linux_hid_usage(p,0,4,1,1,&usage)&&usage==0x10031);
 assert(!tmc_linux_hid_usage(p,0,4,1,2,&usage)&&!tmc_linux_hid_field(p,0,4,3,&f));
 assert(!tmc_linux_hid_report(p,3,4,&r)&&!tmc_linux_hid_report(p,0,256,&r));
 u8 packet[]={4,5,0x34,0x12,0xfe,0xff,255};
 for(unsigned n=0;n<sizeof packet;n++)assert(!tmc_linux_hid_frame(p,0,packet,n,&r));
 assert(tmc_linux_hid_frame(p,0,packet,sizeof packet,&r));packet[0]=3;
 assert(!tmc_linux_hid_frame(p,0,packet,sizeof packet,&r));packet[0]=4;
 tmc_linux_hid_close(p);clean();
 TabletLayout t;TabletState state;
 assert(tmc_linux_hid_pointer_layout(mouse,sizeof mouse,64,&t)&&t.id==4&&t.bytes==7&&t.relative&&t.x.bit==16&&t.y.bit==32);
 assert(pointer_read(&t,packet,sizeof packet,1280,800,&state)&&state.x==0x1234&&state.y==-2&&state.buttons==5&&state.wheel==-1);
 assert(!tmc_linux_hid_pointer_layout(mouse,sizeof mouse,6,&t));
 assert(tmc_linux_hid_pointer_layout(tablet,sizeof tablet,64,&t)&&!t.relative&&t.x.bit==8&&t.y.bit==24);
 u8 ab[]={1,0xff,0x7f,0,0};
 assert(pointer_read(&t,ab,sizeof ab,1280,800,&state)&&state.x==1279&&state.y==0&&state.buttons==1);
 puts("PASS Linux HID field model -> native relative/absolute XY, buttons, wheel, packet limits and strict report-ID framing");
 p=tmc_linux_hid_parse(keyboard,sizeof keyboard);assert(p);
 assert(tmc_linux_hid_report(p,0,0,&r)&&r.bytes==8&&r.fields==2&&!r.numbered);
 assert(tmc_linux_hid_field(p,0,0,1,&f)&&f.count==6&&f.usages==102&&f.offset==16);
 assert(tmc_linux_hid_usage(p,0,0,0,7,&usage)&&usage==0x700e7);
 assert(tmc_linux_hid_report(p,1,0,&r)&&r.bytes==1&&r.fields==1);
 assert(tmc_linux_hid_usage(p,1,0,0,4,&usage)&&usage==0x80005);
 tmc_linux_hid_close(p);assert(!tmc_linux_hid_pointer_layout(keyboard,sizeof keyboard,64,&t));clean();
 puts("PASS Linux generic descriptor model: keyboard modifier bitmap, key array, LED output usages (not hid-input keyboard event dispatch)");
 /* Independent report IDs and real Linux global Push/Pop, extended usages. */
 u8 multi[1024];const u8 vendor[]={0x06,0,0xff,0x09,1,0xa1,1,0x85,7,0x75,8,0x95,12,0x81,2,0xc0};
 memcpy(multi,vendor,sizeof vendor);memcpy(multi+sizeof vendor,mouse,sizeof mouse);
 assert(tmc_linux_hid_pointer_layout(multi,sizeof vendor+sizeof mouse,64,&t)&&t.id==4&&t.x.bit==16&&t.y.bit==32);
 memcpy(multi,mouse,12);const u8 push[]={0xa4,0x06,0,0xff,0x75,32,0x95,31,0xb4};
 memcpy(multi+12,push,sizeof push);memcpy(multi+12+sizeof push,mouse+12,sizeof mouse-12);
 assert(tmc_linux_hid_pointer_layout(multi,sizeof mouse+sizeof push,64,&t)&&t.x.bit==16);
 /* More than 16 collections forces upstream collection and stack growth. */
 unsigned z=0;for(unsigned i=0;i<40;i++){multi[z++]=0xa1;multi[z++]=0;}
 for(unsigned i=0;i<40;i++)multi[z++]=0xc0;
 p=tmc_linux_hid_parse(multi,z);assert(p);tmc_linux_hid_close(p);clean();
 puts("PASS Linux per-ID offsets, global Push/Pop, dynamic collection/stack growth and close/reopen lifetime");
 const u8 pop[]={0xb4},end[]={0xc0},id0[]={0x85,0},id256[]={0x86,0,1};
 const u8 longitem[]={0xfe,1,0,0};const u8 delimiter[]={0xa9,1};
 reject(pop,sizeof pop);reject(end,sizeof end);reject(id0,sizeof id0);reject(id256,sizeof id256);
 reject(longitem,sizeof longitem);reject(delimiter,sizeof delimiter);
 reject(NULL,1);reject(mouse,0);reject(mouse,1025);
 for(unsigned n=0;n<sizeof mouse;n++){
  /* Linux accepts a syntactically complete empty prefix; transport pointer
   * selection still requires the complete XY/button report. */
  p=tmc_linux_hid_parse(mouse,n);if(p)tmc_linux_hid_close(p);
  assert(!tmc_linux_hid_pointer_layout(mouse,n,64,&t));clean();
 }
 /* Every OS allocator boundary, including field allocation ignored by original
  * hid_add_field: adapter must fail instead of returning a partial model. */
 long allocations=0;
 TmcHidParserStats before,after;tmc_linux_hid_parser_stats(&before);
 p=tmc_linux_hid_parse(mouse,sizeof mouse);assert(p);tmc_linux_hid_parser_stats(&after);
 allocations=after.allocations-before.allocations;tmc_linux_hid_close(p);clean();
 for(long fail=0;fail<allocations;fail++){
  tmc_hid_test_fail=fail;assert(!tmc_linux_hid_parse(mouse,sizeof mouse));tmc_hid_test_fail=-1;clean();
 }
 /* Full-parser aggregate heap cap with independently owned open objects. */
 TmcLinuxHidParser *held[512];unsigned count=0;
 while(count<512&&(held[count]=tmc_linux_hid_parse(mouse,sizeof mouse)))count++;
 assert(count>0&&count<512);while(count)tmc_linux_hid_close(held[--count]);clean();
 puts("PASS malformed/truncated descriptors, every allocation-failure rollback, no partial-success field model, aggregate heap limit");
 /* Genuine feature multiplier path, then hostile signed range arithmetic. */
 const u8 feature[]={5,1,9,2,0xa1,1,0x09,0x48,0x15,0,0x25,1,0x35,1,0x45,4,0x75,8,0x95,1,0xb1,2,0xc0};
 p=tmc_linux_hid_parse(feature,sizeof feature);assert(p&&tmc_linux_hid_report(p,2,0,&r)&&r.bytes==1);tmc_linux_hid_close(p);clean();
 const u8 overflow[]={5,1,9,2,0xa1,1,9,0x48,0x17,0,0,0,0x80,0x27,0xff,0xff,0xff,0x7f,0x35,1,0x45,4,0x75,8,0x95,1,0xb1,2,0xc0};
 reject(overflow,sizeof overflow);
 tmc_hid_test_alloc_hook=nested;p=tmc_linux_hid_parse(mouse,sizeof mouse);assert(p);tmc_linux_hid_close(p);clean();
 pthread_t threads[4];for(unsigned i=0;i<4;i++)assert(!pthread_create(&threads[i],NULL,parallel_parse,NULL));
 for(unsigned i=0;i<4;i++)assert(!pthread_join(threads[i],NULL));assert(parallel_success>0);clean();
 puts("PASS real feature report parsing, checked multiplier arithmetic, nested-call rejection and 4-thread owned-object lifetimes");
 u32 random=0x12345678;
 for(unsigned iteration=0;iteration<4096;iteration++){
  memcpy(multi,mouse,sizeof mouse);random=random*1664525u+1013904223u;unsigned at=random%sizeof mouse;
  random=random*1664525u+1013904223u;multi[at]^=(u8)(1u<<(random%8));
  p=tmc_linux_hid_parse(multi,sizeof mouse);if(p)tmc_linux_hid_close(p);clean();
 }
 for(unsigned iteration=0;iteration<1024;iteration++){
  p=tmc_linux_hid_parse(keyboard,sizeof keyboard);assert(p);tmc_linux_hid_close(p);clean();
 }
 tmc_linux_hid_parser_stats(&after);
 printf("PASS 4096 deterministic descriptor mutations, 1024 keyboard reopen cycles; peak heap %llu bytes; all allocations freed\n",(unsigned long long)after.peak_bytes);
 return 0;
}
