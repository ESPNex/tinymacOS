/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "linux_ps2.h"
#include <linux/libps2.h>
/* Complete original packet/report and IntelliMouse/Explorer detect functions,
 * not the whole psmouse-base.c driver or Linux serio bus. Layout below is the
 * used field subset, not a claim of binary Linux struct psmouse ABI support.
 */
enum psmouse_type {
 PSMOUSE_NONE,PSMOUSE_PS2,PSMOUSE_PS2PP,PSMOUSE_THINKPS,PSMOUSE_GENPS,
 PSMOUSE_IMPS,PSMOUSE_IMEX,PSMOUSE_SYNAPTICS,PSMOUSE_ALPS,PSMOUSE_LIFEBOOK,
 PSMOUSE_TRACKPOINT,PSMOUSE_TOUCHKIT_PS2,PSMOUSE_CORTRON
};
typedef enum {PSMOUSE_BAD_DATA,PSMOUSE_GOOD_DATA,PSMOUSE_FULL_PACKET} psmouse_ret_t;
struct psmouse_protocol {enum psmouse_type type;};
struct psmouse {
 struct input_dev *dev;struct ps2dev ps2dev;const struct psmouse_protocol *protocol;
 u8 packet[8],pktcnt,pktsize,extra_buttons;const char *vendor,*name;
};
#define PSMOUSE_CMD_SETRATE 0x10f3
#define PSMOUSE_CMD_GETID 0x02f2
#define __set_bit set_bit
static inline int sign_extend32(u32 value,unsigned bit)
{
 u32 sign=1u<<bit;return (int)(value&(sign-1))-(int)(value&sign);
}
static inline int abs(int value){return value<0?-value:value;}
static const bool psmouse_a4tech_2wheels=false; /* no native configuration for this workaround */
static struct psmouse mouse;
static struct input_dev pointer;
static struct psmouse_protocol protocol;
static TmcLinuxPsMouseStats tmc_linux_psmouse_counters;
#define mstats tmc_linux_psmouse_counters
static int tmc_psmouse_command(struct ps2dev *device,u8 *param,unsigned command)
{
 (void)device; /* private single-port wrapper: all detector commands target AUX */
 unsigned send=(command>>12)&15,receive=(command>>8)&15;
 return tmc_linux_ps2_command(1,param,send>receive?send:receive,command);
}
#define ps2_command tmc_psmouse_command
#include "../../vendor/linux-v6.12/psmouse-packets.inc"
#include "../../vendor/linux-v6.12/psmouse-detect.inc"
#undef ps2_command
int tmc_linux_psmouse_start(unsigned device_id)
{
 if(device_id!=0&&device_id!=3&&device_id!=4)return 0;
 if(pointer.registered)return 0;
 memset(&mouse,0,sizeof mouse);memset(&pointer,0,sizeof pointer);
 protocol.type=device_id==4?PSMOUSE_IMEX:device_id==3?PSMOUSE_IMPS:PSMOUSE_PS2;
 mouse.dev=&pointer;mouse.protocol=&protocol;mouse.pktsize=device_id?4:3;
 pointer.name="Linux psmouse packet engine";pointer.id.bustype=0x11;
 if(tmc_lx_input_register_pointer(&pointer))return 0;
 mstats.starts++;mstats.protocol=protocol.type;mstats.packet_bytes=mouse.pktsize;return 1;
}
int tmc_linux_psmouse_probe(void)
{
 if(!tmc_linux_psmouse_start(0))return 0;
 /* The original detect functions issue actual libps2 commands through the
  * native adapter. Do not claim success on a failed command sequence. */
 TmcLinuxPs2Stats before,after;tmc_linux_ps2_stats(&before);
 if(!im_explorer_detect(&mouse,true))protocol.type=PSMOUSE_IMEX;
 else if(!intellimouse_detect(&mouse,true))protocol.type=PSMOUSE_IMPS;
 tmc_linux_ps2_stats(&after);
 if(after.command_errors!=before.command_errors){tmc_linux_psmouse_stop();return 0;}
 mstats.protocol=protocol.type;mstats.packet_bytes=mouse.pktsize;return 1;
}
void tmc_linux_psmouse_stop(void)
{
 if(pointer.registered){tmc_lx_input_unregister_pointer(&pointer);mstats.stops++;}
 mouse.pktcnt=0;
}
void tmc_linux_psmouse_byte(u8 byte,unsigned flags)
{
 if(!pointer.registered)return;
 mstats.bytes++;
 if(flags){
  mstats.rx_errors++;mouse.pktcnt=0;
  if(pointer.buttons){pointer.buttons=0;pointer.x=pointer.y=pointer.wheel=pointer.hwheel=0;input_sync(&pointer);}
  return;
 }
 /* i8042 framing remains native. Do not filter ACK-looking bytes inside a
  * packet: those can also be valid motion/button data. Discard a complete
  * overflow packet without interpreting it as an enormous displacement. */
 if(!mouse.pktcnt&&!(byte&8)){mstats.sync_drops++;return;}
 mouse.packet[mouse.pktcnt++]=byte;
 if(mouse.pktcnt<mouse.pktsize)return;
 if(mouse.packet[0]&0xc0){mstats.overflow_drops++;mouse.pktcnt=0;return;}
 if(psmouse_process_byte(&mouse)==PSMOUSE_FULL_PACKET)mstats.packets++;
 mouse.pktcnt=0;
}
void tmc_linux_psmouse_stats(TmcLinuxPsMouseStats *out){if(out)*out=mstats;}
