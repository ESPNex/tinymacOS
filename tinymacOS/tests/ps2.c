#define TMC_INPUT_TEST 1
#define TMC_PS2_TEST 1
#include "../drivers/input.c"
/* The input host support defines no-op ps2_init/poll for the xHCI contracts.
 * Rename the real implementation in this decoder contract only. */
#define ps2_init native_ps2_init
#define ps2_poll native_ps2_poll
#define ps2_setleds native_ps2_setleds
#include "../drivers/ps2.c"
u64 ticks;
static void mouse_byte(u8 b){tmc_linux_psmouse_byte(b,0); }
static Event key(int k,int ctrl,int shift)
{
 Event e;assert(input_pop(&e)&&e.type==EV_KEY&&e.a==k&&e.b==ctrl&&e.c==shift);return e;
}
static Event event(int type,int a,int b,int c)
{
 Event e;assert(input_pop(&e));assert(e.type==type&&e.a==a&&e.b==b&&e.c==c);return e;
}
int main(void)
{
 (void)boot;
 keyboard_byte(0x2a);keyboard_byte(0x1e);key('A',0,1);keyboard_byte(0x9e);
 keyboard_byte(0xaa);keyboard_byte(0x30);key('b',0,0);keyboard_byte(0xb0);
 assert(!kmods);
 keyboard_byte(0x1d);keyboard_byte(0x2e);key('c',1,0);keyboard_byte(0xae);keyboard_byte(0x9d);
 keyboard_byte(0xe0);keyboard_byte(0x1d);keyboard_byte(0x2e);key('c',1,0);keyboard_byte(0xae);keyboard_byte(0xe0);keyboard_byte(0x9d);
 keyboard_byte(0x3a);keyboard_byte(0x3a);keyboard_byte(0xba);keyboard_byte(0x1e);key('A',0,0);keyboard_byte(0x9e);
 keyboard_byte(0x2a);keyboard_byte(0x1e);key('a',0,1);keyboard_byte(0x9e);keyboard_byte(0xaa);
 keyboard_byte(0x3a);keyboard_byte(0xba);
 keyboard_byte(0x56);key('<',0,0);keyboard_byte(0xd6);
 keyboard_byte(0xe0);keyboard_byte(0x38);keyboard_byte(0x27);key('@',0,0);keyboard_byte(0xa7);keyboard_byte(0xe0);keyboard_byte(0xb8);
 keyboard_byte(0xe0);keyboard_byte(0x48);key(K_UP,0,0);keyboard_byte(0xe0);keyboard_byte(0xc8);
 keyboard_byte(0x3c);key(K_F2,0,0);keyboard_byte(0xbc);
 u8 print[]={0xe0,0x2a,0xe0,0x37,0xe0,0xb7,0xe0,0xaa};
 for(unsigned i=0;i<sizeof print;i++)keyboard_byte(print[i]);assert(!kmods);
 u8 pause[]={0xe1,0x1d,0x45,0xe1,0x9d,0xc5};for(unsigned i=0;i<sizeof pause;i++)keyboard_byte(pause[i]);
 Event e;assert(!input_pop(&e));keyboard_byte(0x1e);key('a',0,0);keyboard_byte(0x9e);
 keyboard_byte(0x47);key('7',0,0);keyboard_byte(0xc7);
 keyboard_byte(0x45);keyboard_byte(0xc5);keyboard_byte(0x47);key(K_HOME,0,0);keyboard_byte(0xc7);
 keyboard_byte(0x2a);keyboard_byte(0x47);key('7',0,1);keyboard_byte(0xc7);keyboard_byte(0xaa);
 native_ps2_setleds(1);assert(led_want==4);
 puts("PASS PS/2 keyboard: make/break, both Ctrl, Shift release 0xAA, Caps XOR Shift, autorepeat-safe Caps, E0 arrows, F2, PrintScreen and Pause fragments");
 assert(tmc_linux_psmouse_start(0));
 mouse_byte(0);assert(!input_pop(&e));
 mouse_byte(0x18);mouse_byte(251);mouse_byte(0);event(EV_MOUSE,-5,0,0);
 mouse_byte(0x28);mouse_byte(0);mouse_byte(251);event(EV_MOUSE,0,5,0);
 mouse_byte(0x09);mouse_byte(0);mouse_byte(0);event(EV_MOUSE,0,0,1);
 mouse_byte(0x08);mouse_byte(0);mouse_byte(0);event(EV_MOUSE,0,0,0);
 mouse_byte(0xc8);mouse_byte(255);mouse_byte(255);assert(!input_pop(&e));
 tmc_linux_psmouse_stop();assert(tmc_linux_psmouse_start(4));mouse_byte(0x0e);mouse_byte(2);mouse_byte(3);mouse_byte(15);
 event(EV_MOUSE,2,-3,6);event(EV_SCROLL,1,0,0);assert(!input_pop(&e));
 native_ps2_init();assert(!ps2_io_writes);
 ps2_fake_status=0;keyboard_on=1;native_ps2_poll();assert(ps2_last_byte==0xed&&led_phase==1);
 keyboard_byte(0x1e);key('a',0,0);keyboard_byte(0x9e);
 keyboard_byte(0xfa);native_ps2_poll();assert(ps2_last_byte==4&&led_phase==3);
 keyboard_byte(0xfa);assert(!led_phase&&led_applied==4);
 puts("PASS PS/2 mouse: packet synchronization, signed 9-bit X/Y, Y inversion, three buttons, overflow discard, negotiated wheel; absent-controller guard");
}
