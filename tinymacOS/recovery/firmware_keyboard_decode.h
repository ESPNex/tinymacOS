#pragma once
#include "keyboard_it.h"
/* Physical set-1 decoding only. No controller access, commands, IRQs, USB
 * requests, firmware Unicode assumptions, or native PS/2 driver inclusion.
 * Transport belongs exclusively to the derived firmware Ps2KeyboardDxe.
 */
typedef struct {
    unsigned mods,ext,pause_left,numlock;
    u8 held[256];
} RecFirmwareSet1;
static inline void rec_fw_set1_reset(RecFirmwareSet1 *s)
{
    *s=(RecFirmwareSet1){.numlock=1};
}
static inline unsigned rec_fw_set1(RecFirmwareSet1 *s,u8 b)
{
    static const u8 map[128]={
     [1]=41,[2]=30,[3]=31,[4]=32,[5]=33,[6]=34,[7]=35,[8]=36,[9]=37,[10]=38,[11]=39,
     [12]=45,[13]=46,[14]=42,[15]=43,[16]=20,[17]=26,[18]=8,[19]=21,[20]=23,[21]=28,[22]=24,[23]=12,[24]=18,[25]=19,
     [26]=47,[27]=48,[28]=40,[30]=4,[31]=22,[32]=7,[33]=9,[34]=10,[35]=11,[36]=13,[37]=14,[38]=15,[39]=51,[40]=52,[41]=53,
     [43]=49,[44]=29,[45]=27,[46]=6,[47]=25,[48]=5,[49]=17,[50]=16,[51]=54,[52]=55,[53]=56,[55]=85,[57]=44,[58]=57,
     [59]=58,[60]=59,[61]=60,[62]=61,[63]=62,[64]=63,[65]=64,[66]=65,[67]=66,[68]=67,[86]=100,[87]=68,[88]=69,
     [71]=74,[72]=82,[73]=75,[74]=86,[75]=80,[76]=93,[77]=79,[78]=87,[79]=77,[80]=81,[81]=78,[82]=98,[83]=76
    };
    if(s->pause_left){s->pause_left--;return 0;}
    if(b==0xe1){s->pause_left=5;s->ext=0;return 0;}
    if(b==0xe0){s->ext=1;return 0;}
    if(b==0xfa||b==0xfe||b==0||b==255)return 0;
    unsigned e=s->ext;s->ext=0;
    unsigned code=b&127,slot=code+(e?128:0),release=b&128,was=s->held[slot];
    s->held[slot]=!release;
    unsigned bit=code==29?(e?16:1):code==56?(e?64:4):!e&&code==42?2:!e&&code==54?32:0;
    if(bit){if(release)s->mods&=~bit;else s->mods|=bit;return 0;}
    if(release)return 0;
    if(e&&(code==42||code==54||code==55))return 0;
    if(!e&&(code==69||code==70)){
        if(code==69&&!was)s->numlock=!s->numlock;
        return 0;
    }
    unsigned usage=map[code];if(!usage||(usage==57&&was))return 0;
    if(e&&code==53)usage=84;
    if(e&&code==28)usage=88;
    if(!e&&(s->numlock^!!(s->mods&0x22))){
        static const u8 pad[13]={95,96,97,0,92,93,94,0,89,90,91,98,99};
        if(code>=71&&code<=83&&pad[code-71])usage=pad[code-71];
    }
    return usage;
}
