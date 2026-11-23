/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "linux_ps2.h"
#include <linux/libps2.h>
/* One physical i8042 command engine and two serio RX ports. This backend is
 * native, with original libps2 doing all command/ACK/resend/response handling.
 * Native controller I/O and current keyboard/mouse byte decoders stay native.
 */
static struct {
 struct serio port[2];struct ps2dev device[2];struct mutex commands;
 TmcPs2Write write;TmcPs2Progress progress;TmcPs2Receive receive;void *context;
 int initialized,in_progress;
} ps2;
static TmcLinuxPs2Stats tmc_linux_ps2_counters;
#define stats tmc_linux_ps2_counters
void *tmc_lx_memchr(const void *p,int value,size_t n)
{
 const u8 *data=p;for(size_t i=0;i<n;i++)if(data[i]==(u8)value)return (void *)(data+i);return 0;
}
static unsigned port_index(struct serio *port){return port==&ps2.port[1];}
static int write_byte(struct serio *port,u8 value)
{
 int result=ps2.write(ps2.context,port_index(port),value);
 if(result>0)return -EIO;if(!result)stats.tx_bytes++;return result;
}
static enum ps2_disposition pre_receive(struct ps2dev *device,u8 value,unsigned flags)
{
 if(flags&(SERIO_PARITY|SERIO_TIMEOUT)){
  stats.rx_errors++;ps2.receive(ps2.context,port_index(device->serio),value,flags);return PS2_ERROR;
 }
 return PS2_PROCESS;
}
static void data_received(struct ps2dev *device,u8 value)
{
 stats.data_bytes++;ps2.receive(ps2.context,port_index(device->serio),value,0);
}
int tmc_linux_ps2_setup(TmcPs2Write write,TmcPs2Progress progress,TmcPs2Receive receive,void *context)
{
 if(!write||!progress||!receive){stats.rejected++;return 0;}
 if(ps2.initialized)return ps2.write==write&&ps2.progress==progress&&ps2.receive==receive&&ps2.context==context;
 memset(&ps2,0,sizeof ps2);ps2.write=write;ps2.progress=progress;ps2.receive=receive;ps2.context=context;
 mutex_init(&ps2.commands);
 for(unsigned i=0;i<2;i++){
  struct serio *s=ps2.port+i;spin_lock_init(&s->lock);s->write=write_byte;s->ps2_cmd_mutex=&ps2.commands;
  tmc_linux_ps2_init(ps2.device+i,s,pre_receive,data_received);
 }
 ps2.initialized=1;stats.setups++;return 1;
}
int tmc_linux_ps2_rx_ready(unsigned port)
{
 return port<2&&ps2.initialized&&!__atomic_load_n(&ps2.port[port].lock.word,__ATOMIC_ACQUIRE);
}
int tmc_linux_ps2_receive(unsigned port,u8 byte,unsigned flags)
{
 if(port>=2||!ps2.initialized||flags&~(SERIO_TIMEOUT|SERIO_PARITY)){stats.rejected++;return 0;}
 /* Never consume from a paused port: the native pump must call rx_ready
  * before reading data. Try-lock also prevents same-CPU callback reentry.
  */
 u32 irq=tmc_lx_os_irq_save();struct serio *s=ps2.port+port;
 if(__atomic_exchange_n(&s->lock.word,1,__ATOMIC_ACQUIRE)){tmc_lx_os_irq_restore(irq);return 0;}
 stats.rx_bytes++;ps2_interrupt(s,byte,flags);
 __atomic_store_n(&s->lock.word,0,__ATOMIC_RELEASE);tmc_lx_os_irq_restore(irq);return 1;
}
void tmc_lx_ps2_wait_progress(void)
{
 if(ps2.initialized&&!ps2.in_progress){
  ps2.in_progress=1;stats.wait_polls++;ps2.progress(ps2.context);ps2.in_progress=0;
 }
 tmc_lx_os_spin_wait();
}
int tmc_linux_ps2_command(unsigned port,u8 *param,unsigned capacity,unsigned command)
{
 unsigned send=(command>>12)&15,receive=(command>>8)&15;
 if(!ps2.initialized||port>=2||ps2.in_progress||command>65535||receive>8||
    capacity>16||(send>capacity)||(receive>capacity)||((send||receive)&&!param)){
  stats.rejected++;return -EINVAL;
 }
 stats.commands++;int result=ps2_command(ps2.device+port,param,command);
 if(result)stats.command_errors++;return result;
}
void tmc_linux_ps2_stats(TmcLinuxPs2Stats *out)
{
 if(out)*out=stats; /* CPU0-poll owner snapshot; no keystrokes or addresses. */
}
