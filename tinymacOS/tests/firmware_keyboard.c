#include <assert.h>
#include <stdio.h>
#include "../vendor/tmc-ps2-keymap/raw_ps2_queue.h"
#include "../recovery/firmware_keyboard_decode.h"
int main(void){
 struct TmcRawPs2Queue q={0},q2={0};unsigned char b=99;
 assert(tmc_raw_ps2_pop(&q,&b)==0&&b==99);
 for(unsigned n=0;n<10000;n++){for(unsigned i=0;i<255;i++)tmc_raw_ps2_push(&q,i);for(unsigned i=0;i<255;i++){assert(tmc_raw_ps2_pop(&q,&b)==1);assert(b==(unsigned char)i);}assert(tmc_raw_ps2_pop(&q,&b)==0);}
 for(unsigned i=0;i<256;i++)tmc_raw_ps2_push(&q,i);
 tmc_raw_ps2_push(&q,0xe0);assert(tmc_raw_ps2_pop(&q,&b)==-1);assert(tmc_raw_ps2_pop(&q,&b)==0);
 tmc_raw_ps2_push(&q2,42);assert(tmc_raw_ps2_pop(&q2,&b)==1&&b==42);
 RecFirmwareSet1 s;rec_fw_set1_reset(&s);
 assert(!rec_fw_set1(&s,42));unsigned k=rec_fw_set1(&s,53);assert(it_character(k,s.mods,0)=='_');
 assert(!rec_fw_set1(&s,0xb5));assert(!rec_fw_set1(&s,0xaa));assert(s.mods==0);
 assert(!rec_fw_set1(&s,0xe0));assert(!rec_fw_set1(&s,0x38));k=rec_fw_set1(&s,39);assert(it_character(k,s.mods,0)=='@');
 assert(!rec_fw_set1(&s,0xa7));assert(!rec_fw_set1(&s,0xe0));assert(!rec_fw_set1(&s,0xb8));assert(s.mods==0);
 assert(rec_fw_set1(&s,86)==100);assert(!rec_fw_set1(&s,0xd6));assert(rec_fw_set1(&s,72)==96);assert(!rec_fw_set1(&s,0xc8));
 assert(!rec_fw_set1(&s,0xe0));assert(rec_fw_set1(&s,72)==82);assert(!rec_fw_set1(&s,0xe0));assert(!rec_fw_set1(&s,0xc8));
 assert(!rec_fw_set1(&s,0xe0));assert(!rec_fw_set1(&s,42));assert(s.mods==0);
 unsigned char pause[]={0xe1,0x1d,0x45,0xe1,0x9d,0xc5};for(unsigned i=0;i<sizeof(pause);i++)assert(!rec_fw_set1(&s,pause[i]));assert(s.mods==0);
 assert(rec_fw_set1(&s,58)==57);assert(!rec_fw_set1(&s,58));assert(!rec_fw_set1(&s,0xba));assert(rec_fw_set1(&s,58)==57);
 rec_fw_set1_reset(&s);assert(s.mods==0&&s.ext==0&&s.numlock==1);
 puts("PASS firmware raw ring 2550000 byte roundtrips, overflow/isolation, physical Shift/AltGr/ISO/keypad/prefix/caps");
}
