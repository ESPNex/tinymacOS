#include "base.h"
#include "bootconfig.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 BootConfig c;for(unsigned v=0;v<2;v++){bootconfig_make(&c,v);assert(bootconfig_valid(&c));
 for(int w=800;w<=1920;w+=8)for(int h=600;h<=1080;h++){
 u32 f=bootconfig_display(w,h)|v;bootconfig_make(&c,f);assert(bootconfig_valid(&c));assert(((f>>8)&255)*8==w);assert(((f>>16)&2047)==h);
 u32 next=(f&TMC_BOOT_DISPLAY_MASK)|!v;assert((next&1)==!v);assert((next&TMC_BOOT_DISPLAY_MASK)==(f&TMC_BOOT_DISPLAY_MASK));
 c.flags^=1;assert(!bootconfig_valid(&c));}}
 assert(!bootconfig_flags_valid(4));assert(!bootconfig_flags_valid(bootconfig_display(640,480)));assert(!bootconfig_flags_valid(bootconfig_display(1920,1200)));assert(!bootconfig_flags_valid(0x80000000));
 puts("PASS BootConfig v1 backward compatibility, 135642 mode/verbose combinations, digest tamper, reserved/range rejection");
}
