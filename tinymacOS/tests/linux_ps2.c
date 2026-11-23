/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Real original libps2.c and actual native adapter; scripted physical byte
 * transport only. Does not substitute ACK parsing, command logic or waits. */
#define main baseline_usb_mouse_contract
#include "linux_usbmouse.c"
#undef main
#undef stats
#include "../compat/linux/ps2_input.c"
struct Byte {unsigned port,flags;u8 value;};
struct Model {
 struct Byte queue[128];unsigned head,tail,writes,delivered,errors,parameters;
 u8 tx[128],last_data;unsigned last_port,last_flags;
 int nak,no_ack_id,write_error,timeout,rx_error,partial_id,interleave,nested;
} model;
static void enqueue(unsigned port,u8 value,unsigned flags)
{
 assert(model.head<COUNT(model.queue));model.queue[model.head++]=(struct Byte){port,flags,value};
}
static int write_test(void *context,unsigned port,u8 value)
{
 assert(context==&model&&port<2&&tmc_linux_ps2_rx_ready(port));
 assert(model.writes<sizeof model.tx);model.tx[model.writes++]=value;
 if(model.write_error)return -EIO;
 if(model.timeout)return 0;
 if(model.rx_error){enqueue(port,0x7f,model.rx_error);return 0;}
 if(model.nak){model.nak--;enqueue(port,0xfe,0);return 0;}
 if(model.interleave){enqueue(port^1,0x08,0);enqueue(port^1,0x7f,0);enqueue(port^1,0,0);}
 if(model.parameters){model.parameters--;enqueue(port,0xfa,0);return 0;}
 if(value==0xed){enqueue(port,0x1e,0);model.parameters=1;}
 else if(value==0xf3||value==0xf0||value==0xe8)model.parameters=1;
 if(!(value==0xf2&&model.no_ack_id))enqueue(port,0xfa,0);
 if(value==0xf2){
  if(port)enqueue(port,3,0);
  else {enqueue(port,0xab,0);if(!model.partial_id)enqueue(port,0x83,0);}
 }
 if(value==0xff)enqueue(port,0xaa,0); /* Linux permits missing 2nd reset byte. */
 return 0;
}
static void receive_test(void *context,unsigned port,u8 value,unsigned flags)
{
 assert(context==&model);model.last_data=value;model.last_port=port;model.last_flags=flags;
 if(flags)model.errors++;else model.delivered++;
 if(model.nested)assert(tmc_linux_ps2_command(0,0,0,0xf4)==-EINVAL);
}
static void progress_test(void *context)
{
 assert(context==&model);
 while(model.tail<model.head){
  struct Byte *b=&model.queue[model.tail];if(!tmc_linux_ps2_rx_ready(b->port))break;
  assert(tmc_linux_ps2_receive(b->port,b->value,b->flags));model.tail++;
 }
}
static void fresh(void){memset(&model,0,sizeof model);}
int main(void)
{
 assert(!baseline_usb_mouse_contract());
 assert(!tmc_linux_ps2_setup(0,progress_test,receive_test,&model));
 assert(tmc_linux_ps2_command(0,0,0,0xf4)==-EINVAL);
 assert(tmc_linux_ps2_setup(write_test,progress_test,receive_test,&model));
 assert(tmc_linux_ps2_setup(write_test,progress_test,receive_test,&model));
 assert(!tmc_linux_ps2_setup(write_test,progress_test,receive_test,0));
 assert(ps2.port[0].ps2_cmd_mutex==ps2.port[1].ps2_cmd_mutex);
 assert(!tmc_linux_ps2_command(0,0,0,0xf5)&&model.writes==1&&model.tx[0]==0xf5&&!model.delivered);
 fresh();u8 set=2;assert(!tmc_linux_ps2_command(0,&set,1,0x10f0)&&model.writes==2&&model.tx[1]==2);
 fresh();u8 id[2]={0};assert(!tmc_linux_ps2_command(0,id,2,0x02f2)&&id[0]==0xab&&id[1]==0x83);
 fresh();assert(!tmc_linux_ps2_command(1,id,2,0x02f2)&&id[0]==3);
 fresh();model.no_ack_id=1;assert(!tmc_linux_ps2_command(1,id,2,0x02f2)&&id[0]==3&&model.writes==1);
 fresh();model.nak=1;assert(!tmc_linux_ps2_command(0,0,0,0xf4)&&model.writes==2);
 fresh();model.nak=2;assert(tmc_linux_ps2_command(0,0,0,0xf4)==-EPROTO&&model.writes==2);
 fresh();model.nak=1;assert(ps2_sendbyte(ps2.device,0xf4,10)==-EAGAIN&&model.writes==1);
 fresh();model.write_error=1;assert(tmc_linux_ps2_command(1,0,0,0xf4)==-EIO&&model.writes==1);
 fresh();model.timeout=1;u64 start=tmc_lx_os_clock_ms();assert(ps2_sendbyte(ps2.device,0xf4,8)==-EIO);assert(tmc_lx_os_clock_ms()-start>=8);
 fresh();model.rx_error=SERIO_PARITY;assert(tmc_linux_ps2_command(1,0,0,0xf4)==-EIO&&model.errors==1&&model.last_flags==SERIO_PARITY);
 fresh();model.rx_error=SERIO_TIMEOUT;assert(tmc_linux_ps2_command(0,0,0,0xf4)==-EIO&&model.errors==1);
 fresh();model.partial_id=1;assert(tmc_linux_ps2_command(0,id,2,0x02f2)==-EPROTO&&id[0]==0xab);
 fresh();u8 reset[2]={0};assert(!tmc_linux_ps2_command(1,reset,2,0x02ff)&&reset[0]==0xaa);
 fresh();model.interleave=1;model.nested=1;u8 leds=4;
 assert(!tmc_linux_ps2_command(0,&leds,1,0x10ed));assert(model.delivered==7&&model.writes==2);model.nested=0;
 fresh();assert(!ps2_sliced_command(ps2.device+1,0xd2));
 const u8 sliced[]={0xe6,0xe8,3,0xe8,1,0xe8,0,0xe8,2};assert(model.writes==sizeof sliced&&!memcmp(model.tx,sliced,sizeof sliced));
 fresh();for(unsigned i=0;i<8;i++)enqueue(1,(u8)i,0);ps2_drain(ps2.device+1,16,10);assert(!model.delivered&&!ps2.device[1].cmdcnt);
 fresh();assert(ps2_command(ps2.device,0,0x0900)==-EINVAL);assert(ps2_command(ps2.device,0,0x10ed)==-EINVAL);
 assert(!model.writes);assert(tmc_linux_ps2_command(2,0,0,0xf4)==-EINVAL);
 assert(tmc_linux_ps2_command(0,&leds,0,0x10ed)==-EINVAL);assert(tmc_linux_ps2_command(0,id,1,0x02f2)==-EINVAL);
 assert(tmc_linux_ps2_command(0,id,17,0xf2)==-EINVAL);assert(tmc_linux_ps2_command(0,0,0,0x10000)==-EINVAL);
 assert(!tmc_linux_ps2_receive(2,0,0));assert(!tmc_linux_ps2_receive(0,0,4));
 serio_pause_rx(ps2.port);assert(!tmc_linux_ps2_rx_ready(0)&&!tmc_linux_ps2_receive(0,0x1e,0));serio_continue_rx(ps2.port);
 assert(tmc_linux_ps2_receive(0,0x1e,0)&&model.delivered==1&&model.last_data==0x1e);
 assert(ps2_is_keyboard_id(0xab)&&ps2_is_keyboard_id(0xac)&&ps2_is_keyboard_id(0x2b)&&ps2_is_keyboard_id(0x5d)&&ps2_is_keyboard_id(0x60)&&ps2_is_keyboard_id(0x47));
 assert(!ps2_is_keyboard_id(3)&&!ps2_is_keyboard_id(0));
 TmcLinuxPs2Stats result;tmc_linux_ps2_stats(&result);assert(result.setups==1&&result.commands>=12&&result.command_errors>=4&&result.tx_bytes>20&&result.rx_bytes>result.data_bytes&&result.rx_errors==2&&result.wait_polls&&result.rejected>=10);
 assert(!alive&&!ps2.commands.word&&!ps2.port[0].lock.word&&!ps2.port[1].lock.word&&!ps2.device[0].flags&&!ps2.device[1].flags);
 puts("PASS complete original Linux libps2: keyboard+mouse ACK/NAK/retry, translated keyboard ID, non-ACK mouse ID, reset partial response, sliced commands, shared command mutex, independent RX locks, capacity/flags checks, parity/timeout abort, real timed wait, interleaved input and nested-command rejection; native byte decoders still separate");
 return 0;
}
