#define TMC_INPUT_TEST 1
#include "../drivers/input.c"
#include "shortcuts.h"
#include "text_input.h"
u64 ticks;
void usb_delay_ms(unsigned ms){(void)ms;}
static Event usage(int k,int m){Event e;input_key_usage(k,m);assert(input_pop(&e)&&e.type==EV_KEY);return e;}
int main(void){
 (void)boot;
 Event cmd=usage(6,8);assert(cmd.a=='c'&&cmd.b&&key_command(&cmd)&&!key_control(&cmd)&&cmd.d==8);
 Event ctrl=usage(6,1);assert(ctrl.a=='c'&&ctrl.b&&!key_command(&ctrl)&&key_control(&ctrl)&&ctrl.d==1);
 Event gr=usage(51,0x41);assert(gr.a=='@'&&!gr.b&&!key_control(&gr)&&!key_command(&gr));
 Event mission=usage(82,1);assert(mission.a==K_UP&&key_mission(&mission));mission.d=8;assert(!key_mission(&mission));
 Event shifted=usage(17,0x2a);assert(shifted.a=='N'&&shifted.c&&key_command(&shifted)&&key_lower(shifted.a)=='n');
 assert(key_boot_recovery('r',0x80000080u));assert(key_boot_recovery('R',0x80000040u));
 assert(!key_boot_recovery('r',0x80000000u));assert(!key_boot_recovery('r',0x80000008u));assert(!key_boot_recovery(0,0x80000080u));assert(!key_boot_recovery('r',0x80));
 char utf[16]="caffè";int at=strlen(utf),selected=0;
 assert(text_edit_key(utf,sizeof utf,&at,&selected,130,0)&&at==4);
 assert(text_edit_key(utf,sizeof utf,&at,&selected,8,0)&&eq(utf,"cafè"));
 puts("PASS real input: distinct Win/Control snapshots, Shift, Italian AltGr @, Control+Up, Command+R boot eligibility, plain R/Control+R/F12 not recovery");
}
