#pragma once
#include "abi.h"
/* Italian Windows keyboard: Windows = Command, Alt = Option, Ctrl = Control.
 * Preserve the HID modifier snapshot in Event.d, including AltGr separately. */
#define KEY_CONTROL 0x11u
#define KEY_SHIFT 0x22u
#define KEY_OPTION 0x04u
#define KEY_ALTGR 0x40u
#define KEY_COMMAND 0x88u
static inline int key_command(const Event *e){return !!((unsigned)e->d&KEY_COMMAND);}
static inline int key_control(const Event *e){return !((unsigned)e->d&KEY_ALTGR)&&!!((unsigned)e->d&KEY_CONTROL);}
static inline int key_option(const Event *e){return !!((unsigned)e->d&KEY_OPTION);}
static inline int key_lower(int c){return c>='A'&&c<='Z'?c+32:c;}
static inline int key_mission(const Event *e){return key_control(e)&&e->a==K_UP&&!key_command(e);}
/* UEFI Text Input Ex shift-state bits, deliberately independent of USB HID. */
static inline int key_boot_recovery(unsigned unicode,unsigned state){
    return (unicode=='r'||unicode=='R')&&(state&0x80000000u)&&(state&0xc0u);
}
