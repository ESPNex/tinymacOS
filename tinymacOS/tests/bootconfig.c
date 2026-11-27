#include "base.h"
#include "bootconfig.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 BootConfig c;for(unsigned v=0;v<2;v++){bootconfig_make(&c,v);assert(bootconfig_valid(&c));
 for(int w=800;w<=1920;w+=8)for(int h=600;h<=1080;h++){
 u32 f=bootconfig_display(w,h)|v;bootconfig_make(&c,f);assert(bootconfig_valid(&c));assert(bootconfig_width(f)==w);assert(bootconfig_height(f)==h);
 u32 next=(f&TMC_BOOT_DISPLAY_MASK)|!v;assert((next&1)==!v);assert((next&TMC_BOOT_DISPLAY_MASK)==(f&TMC_BOOT_DISPLAY_MASK));
 c.flags^=1;assert(!bootconfig_valid(&c));}}
 /* Every previously rejected width remainder survives save/reload exactly. */
 for(int w=800;w<=1920;w++){
  u32 f=bootconfig_display(w,768);bootconfig_make(&c,f);assert(bootconfig_valid(&c));assert(bootconfig_width(c.flags)==(unsigned)w);assert(bootconfig_height(c.flags)==768);
 }
 assert(bootconfig_display(1280,800)==(2u|(160u<<8)|(800u<<16)));
 assert(bootconfig_width(bootconfig_display(1366,768))==1366);
 assert(!bootconfig_flags_valid(0x10));assert(!bootconfig_flags_valid(0x80));
 assert(bootconfig_flags_valid(TMC_BOOT_CSR_OVERRIDE));assert(bootconfig_flags_valid(TMC_BOOT_CSR_MASK));assert(!bootconfig_flags_valid(TMC_BOOT_CSR_DISABLED));assert(!bootconfig_flags_valid(bootconfig_display(640,480)));assert(!bootconfig_flags_valid(bootconfig_display(1920,1200)));assert(!bootconfig_flags_valid(0x80000000));
 puts("PASS BootConfig v1 backward compatibility, 135642 legacy mode/verbose combinations + all width remainders, digest tamper, reserved/range rejection");
}
