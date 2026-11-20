#ifdef TMC_PS2_RECOVERY
#include "../recovery/ps2_support.h"
#elif defined(TMC_PS2_TEST)
#include "../tests/ps2_test_support.h"
#else
#include "../kernel/k.h"
#endif
/* Native i8042, polled on CPU0's PIT. No BIOS/UEFI runtime input calls.
 * IRQ1/12 stay disabled at the controller: one consumer owns the data port.
 * Keyboard: device Set 2 with controller translation to Set 1.
 * Mouse: standard 3-byte plus negotiated IntelliMouse 3/4 wheel packets. */
static int keyboard_on,mouse_on,ext,pause_left,mouse_bytes=3,mouse_at;
static u8 keys[256],mouse_packet[4];
static int kmods,numlock=1,scrolllock;
static int led_want=2,led_applied=-1,led_phase,led_sent,led_retries;
static u64 led_deadline;
void ps2_setleds(int caps)
{
    led_want=(caps?4:0)|(numlock?2:0)|(scrolllock?1:0);
}

static const u8 set1[128]={
 [1]=41,[2]=30,[3]=31,[4]=32,[5]=33,[6]=34,[7]=35,[8]=36,[9]=37,[10]=38,[11]=39,
 [12]=45,[13]=46,[14]=42,[15]=43,[16]=20,[17]=26,[18]=8,[19]=21,[20]=23,[21]=28,[22]=24,[23]=12,[24]=18,[25]=19,
 [26]=47,[27]=48,[28]=40,[30]=4,[31]=22,[32]=7,[33]=9,[34]=10,[35]=11,[36]=13,[37]=14,[38]=15,[39]=51,[40]=52,[41]=53,
 [43]=49,[44]=29,[45]=27,[46]=6,[47]=25,[48]=5,[49]=17,[50]=16,[51]=54,[52]=55,[53]=56,[55]=85,[57]=44,[58]=57,
 [59]=58,[60]=59,[61]=60,[62]=61,[63]=62,[64]=63,[65]=64,[66]=65,[67]=66,[68]=67,[86]=100,[87]=68,
 [71]=74,[72]=82,[73]=75,[74]=86,[75]=80,[76]=93,[77]=79,[78]=87,[79]=77,[80]=81,[81]=78,[82]=98,[83]=76
};
static void keyboard_byte(u8 b)
{
    if(pause_left){pause_left--;return;}
    if(b==0xe1){pause_left=5;ext=0;return;}
    if(b==0xe0){ext=1;return;}
    if(b==0xfa) {
        if(led_phase==1)led_phase=2;
        else if(led_phase==3){led_applied=led_sent;led_phase=0;}
        return;
    }
    if(b==0xfe) {
        if(++led_retries>2){led_applied=led_sent;led_phase=0;}
        else if(led_phase==1)led_phase=4;else if(led_phase==3)led_phase=2;
        return;
    }
    if(b==0x00||b==0xff)return;
    int e=ext;ext=0;unsigned code=b&127,slot=code+(e?128:0);int release=b&128,was=keys[slot];
    keys[slot]=!release;
    int bit=code==29?(e?16:1):code==56?(e?64:4):!e&&code==42?2:!e&&code==54?32:0;
    if(bit){if(release)kmods&=~bit;else kmods|=bit;return;}
    if(release)return;
    /* E0 2A/36 and E0 37 are PrintScreen's synthetic Shift fragments. */
    if(e&&(code==42||code==54||code==55))return;
    if(!e&&(code==69||code==70)) {
        if(!was){if(code==69)numlock=!numlock;else scrolllock=!scrolllock;
            led_want=(led_want&4)|(numlock?2:0)|(scrolllock?1:0);}
        return;
    }
    int usage=set1[code];if(!usage || (usage==57&&was))return;
    if(e&&code==53)usage=84;
    if(!e && (numlock ^ !!(kmods&0x22))) {
        static const u8 pad[13]={95,96,97,0,92,93,94,0,89,90,91,98,99};
        if(code>=71&&code<=83&&pad[code-71])usage=pad[code-71];
    }
    input_key_usage(usage,kmods);
}
static void mouse_byte(u8 b)
{
    if(!mouse_at && !(b&8))return; /* first-byte sync, not ACK filtering mid-packet */
    mouse_packet[mouse_at++]=b;
    if(mouse_at<mouse_bytes)return;
    mouse_at=0;u8 s=mouse_packet[0];
    if(s&0xc0)return; /* overflow: do not turn an overflow into cursor teleportation */
    int x=(int)mouse_packet[1]-((s&16)?256:0);
    int y=(int)mouse_packet[2]-((s&32)?256:0);
    int wheel=0;if(mouse_bytes==4){wheel=mouse_packet[3]&15;if(wheel&8)wheel-=16;wheel=-wheel;}
    hid_relative(x,-y,(s&1)|((s&2)?2:0)|((s&4)?4:0),wheel);
}
static int ready_write(void)
{
    for(int i=0;i<60000;i++){if(!(inb(0x64)&2))return 1;waitio();}return 0;
}
static int cmd(u8 v){if(!ready_write())return 0;outb(0x64,v);return 1;}
static int data(u8 v){if(!ready_write())return 0;outb(0x60,v);return 1;}
static int receive(int aux)
{
    for(int i=0;i<60000;i++) {
        u8 s=inb(0x64);if(s==255)return -1;
        if(s&1){u8 b=inb(0x60);if(s&0xc0)continue;if(!!(s&32)==aux)return b;
            if((s&32)&&mouse_on)mouse_byte(b);else if(!(s&32)&&keyboard_on)keyboard_byte(b);
        }else waitio();
    }return -1;
}
static int device(int aux,u8 v)
{
    for(int retry=0;retry<3;retry++) {
        if((aux&&!cmd(0xd4))||!data(v))return 0;
        int b=receive(aux);if(b==0xfa)return 1;if(b!=0xfe)return 0;
    }return 0;
}
static int rate(int v){return device(1,0xf3)&&device(1,(u8)v);}
void ps2_init(void)
{
    if(inb(0x64)==255)return;
    if(!cmd(0xad)||!cmd(0xa7))return;
    for(int i=0;i<64&&(inb(0x64)&1);i++)(void)inb(0x60);
    if(!cmd(0x20))return;int cfg=receive(0);if(cfg<0)return;
    cfg=(cfg&~3)|64; /* translation enabled, controller IRQs disabled */
    if(!cmd(0x60)||!data((u8)cfg))return;
    if(cmd(0xab)&&receive(0)==0&&cmd(0xae)) {
        if(device(0,0xf5)&&device(0,0xf0)&&device(0,2)&&device(0,0xf4)) {
            keyboard_on=1;cfg&=~16;device_add("PS/2 keyboard (i8042)","input",0);
        }
    }
    if(cmd(0xa9)&&receive(0)==0&&cmd(0xa8)&&device(1,0xf6)) {
        cfg&=~32;
        if(rate(200)&&rate(100)&&rate(80)&&device(1,0xf2)) {
            int id=receive(1);if(id==3||id==4)mouse_bytes=4;
        }
        if(device(1,0xf4)){mouse_on=1;device_add("PS/2 mouse (i8042)","input",0);}
    }
    /* Self/interface tests and enable commands may change the clock bits. */
    if(cmd(0x60))data((u8)cfg);
}
static void keyboard_led_poll(void)
{
    /* LED command/ACK state machine never blocks the timer or steals a key. */
    if(!keyboard_on)return;
    if(led_phase && ticks>=led_deadline){led_applied=led_sent;led_phase=0;}
    if(inb(0x64)&2)return;
    if(!led_phase && led_want!=led_applied) {
        led_sent=led_want;led_retries=0;led_phase=1;led_deadline=ticks+25;outb(0x60,0xed);
    } else if(led_phase==4){led_phase=1;outb(0x60,0xed);}
    else if(led_phase==2){led_phase=3;outb(0x60,(u8)led_sent);}
}

void ps2_poll(void)
{
    for(int i=0;i<64;i++) {
        u8 s=inb(0x64);if(s==255||!(s&1))break;u8 b=inb(0x60);
        if(s&0xc0){mouse_at=0;ext=0;continue;}
        if(s&32){if(mouse_on)mouse_byte(b);}else if(keyboard_on)keyboard_byte(b);
    }
    keyboard_led_poll();
}
