/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Original Linux packet and detector functions, actual libps2 and native
 * adapter, with only physical byte transport and OS services substituted. */
#define main baseline_usbmouse
#include "linux_usbmouse.c"
#undef main
#undef stats
#include "../compat/linux/ps2_input.c"
static struct {
 u8 queue[128],rates[3];unsigned head,tail,parameter,id,capability,fail,writes;
} wire;
static void ack(u8 byte){assert(wire.head<sizeof wire.queue);wire.queue[wire.head++]=byte;}
static int tx(void *ctx,unsigned port,u8 byte)
{
 assert(ctx==&wire&&port==1);wire.writes++;
 if(wire.fail)return -EIO;
 ack(0xfa);
 if(wire.parameter){
  wire.parameter=0;wire.rates[0]=wire.rates[1];wire.rates[1]=wire.rates[2];wire.rates[2]=byte;
  if(wire.capability>=3&&!memcmp(wire.rates,(u8[]){200,100,80},3))wire.id=3;
  if(wire.capability>=4&&!memcmp(wire.rates,(u8[]){200,200,80},3))wire.id=4;
 }else if(byte==0xf3)wire.parameter=1;
 else if(byte==0xf2)ack((u8)wire.id);
 return 0;
}
static void rx(void *ctx,unsigned port,u8 byte,unsigned flags)
{
 assert(ctx==&wire&&port==1);tmc_linux_psmouse_byte(byte,flags);
}
static void progress(void *ctx)
{
 assert(ctx==&wire);
 while(wire.tail<wire.head){u8 byte=wire.queue[wire.tail++];assert(tmc_linux_ps2_receive(1,byte,0));}
 wire.head=wire.tail=0;
}
static void packet(unsigned id,u8 first,u8 x,u8 y,u8 wheel)
{
 assert(tmc_linux_ps2_receive(1,first,0));assert(tmc_linux_ps2_receive(1,x,0));assert(tmc_linux_ps2_receive(1,y,0));
 if(id)assert(tmc_linux_ps2_receive(1,wheel,0));
}
int main(void)
{
 assert(!baseline_usbmouse());
 assert(tmc_linux_ps2_setup(tx,progress,rx,&wire));
 assert(!tmc_linux_psmouse_start(1));
 for(unsigned cap=0;cap<=4;cap+=cap==0?3:1){
  wire.capability=cap;wire.id=0;memset(wire.rates,0,3);unsigned before=wire.writes;
  assert(tmc_linux_psmouse_probe());assert(wire.writes>before);
  TmcLinuxPsMouseStats s;tmc_linux_psmouse_stats(&s);assert(s.packet_bytes==(cap?4:3));assert(s.protocol==(cap==4?6:cap==3?5:1));
  assert(!tmc_linux_psmouse_start(cap)); /* do not corrupt a live device */
  for(unsigned bits=0;bits<4;bits++)for(unsigned b=0;b<256;b++){
   u8 status=8|((bits&1)?16:0)|((bits&2)?32:0);
   packet(cap,status,(u8)b,(u8)(255-b),0);
   int x=b?(int)b-((bits&1)?256:0):0;
   int y=(255-b)?(int)(255-b)-((bits&2)?256:0):0;
   assert(last_x==x&&last_y==-y&&last_buttons==0);
  }
  /* Upstream zero-byte/sign quirk: no fabricated -256 motion. */
  packet(cap,0x38,0,0,0);assert(!last_x&&!last_y);
  for(unsigned buttons=0;buttons<8;buttons++){
   packet(cap,(u8)(8|buttons),1,2,0);assert(last_buttons==(int)buttons);
  }
  if(cap==3)for(unsigned w=0;w<256;w++){
   packet(cap,8,0,0,(u8)w);assert(last_wheel==-(int)(s8)w); /* full signed 8-bit IMPS, not nibble */
  }
  if(cap==4){
   for(unsigned b=0;b<4;b++)for(unsigned w=0;w<16;w++){
    packet(cap,8,0,0,(u8)(b*16+w));assert(last_buttons==((b&1?8:0)|(b&2?16:0)));assert(last_wheel==-(w<8?(int)w:(int)w-16));
   }
   packet(cap,8,0,0,0x81);assert(last_wheel==-1);
   packet(cap,8,0,0,0xbf);assert(last_wheel==1);
   last_hwheel=0;packet(cap,8,0,0,0x41);assert(last_hwheel==-1&&!last_wheel);
   packet(cap,8,0,0,0x7f);assert(last_hwheel==1&&!last_wheel);
  }
  int e=events;assert(tmc_linux_ps2_receive(1,0,0));assert(events==e);
  packet(cap,0xc8,255,255,0);assert(events==e); /* overflow discarded */
  /* ACK-looking motion bytes must stay packet data. */
  packet(cap,8,0xfa,0xfe,0);assert(last_x==250&&last_y==-254);
  packet(cap,9,0,0,0);assert(last_buttons==1);
  assert(tmc_linux_ps2_receive(1,0,TMC_PS2_PARITY));assert(last_buttons==0);
  packet(cap,8,7,9,0);assert(last_x==7&&last_y==-9);
  tmc_linux_psmouse_stop();tmc_linux_psmouse_stop();
 }
 /* A PS/2 idle/release/error/detach must not release a USB-held drag. */
 assert(tmc_linux_psmouse_start(4));int usb_key=0;
 assert(tmc_linux_mouse_attach(&usb_key,1,2,4,1));
 for(unsigned p=0;p<32;p++)for(unsigned u=0;u<32;u++){
  packet(4,(u8)(8|(p&7)),2,3,(u8)(((p&8)?16:0)|((p&16)?32:0)));
  assert(tmc_linux_mouse_report(&usb_key,(u8[]){(u8)u,5,250,1},4));
  assert(last_buttons==(int)(p|u)&&last_x==5&&last_y==-6&&last_wheel==1);
  packet(4,(u8)(8|(p&7)),0,0,(u8)(((p&8)?16:0)|((p&16)?32:0)));
  assert(last_buttons==(int)(p|u)&&!last_x&&!last_y&&!last_wheel);
 }
 packet(4,9,0,0,0);assert(tmc_linux_mouse_report(&usb_key,(u8[]){2,0,0,0},4));assert(last_buttons==3);
 assert(tmc_linux_ps2_receive(1,0,TMC_PS2_PARITY));assert(last_buttons==2);
 packet(4,9,0,0,0);tmc_linux_psmouse_stop();assert(last_buttons==2&&!last_x&&!last_y&&!last_wheel);
 assert(tmc_linux_psmouse_start(4));packet(4,9,0,0,0);assert(last_buttons==3);
 tmc_linux_mouse_detach(&usb_key);assert(last_buttons==1);
 tmc_linux_psmouse_stop();assert(!last_buttons&&!alive);
 /* Failed negotiation must not leave an active slot or fake success. */
 wire.fail=1;assert(!tmc_linux_psmouse_probe());wire.fail=0;
 assert(tmc_linux_psmouse_start(0));tmc_linux_psmouse_stop();
 TmcLinuxPsMouseStats result;tmc_linux_psmouse_stats(&result);
 assert(result.starts==result.stops&&result.packets>5000&&result.rx_errors==4&&result.overflow_drops==3);
 puts("PASS original Linux psmouse packet/detect functions: real libps2 0/3/4 negotiation, 3072 sign/axis cases, full IMPS wheel, Explorer five buttons/vertical/horizontal wheel, sync/overflow/parity recovery, ACK-like packet data, 1024 PS2+USB button-union states, independent release/error/detach, failed-negotiation rollback; native i8042/framing, not full psmouse driver");
 return 0;
}
