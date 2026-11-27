/* Same installed EFI algorithm, not a reimplementation of the variable store. */
#define main old_boot_default_main
#include "boot_default.c"
#undef main
#include "firmware_boot.h"
#include "handoff.h"
_Static_assert(__builtin_offsetof(struct Boot,csr)==296,"retain original SIP handoff offset");
_Static_assert(__builtin_offsetof(struct Boot,firmware_magic)==304,"append runtime extension after original handoff");
#include <sys/mman.h>
#include <stdlib.h>
int main(void){
 FirmwareTarget t={.number=3,.logical_bytes=4096,.start=262144,.sectors=19531776,.gpt=1,.signature={17,34}};
 u8 shortpath[FW_BOOT_PATH_BYTES],fullpath[FW_BOOT_PATH_BYTES+6];
 assert(firmware_boot_path(&t,shortpath)==92);u64 x;memcpy(&x,shortpath+8,8);assert(x==262144);assert(shortpath[40]==2&&shortpath[41]==2&&shortpath[88]==127);
 memcpy(fullpath,(u8[]){1,1,6,0,0,3},6);memcpy(fullpath+6,shortpath,92);
 reset();assert(!EFI_ERROR(boot_install_default(&rs,shortpath,sizeof shortpath)));order_is(0,9,10);original();int before=writes;
 assert(!EFI_ERROR(boot_set_default(&rs,fullpath,sizeof fullpath))&&writes==before);order_is(0,9,10);
 puts("PASS confirmed install registers GUID HD/File OpenCore first; expanded full physical path reuses same option without NVRAM writes");
 assert(!EFI_ERROR(boot_restore_default(&rs)));order_is(9,10,65535);before=writes;
 assert(!EFI_ERROR(boot_set_default(&rs,shortpath,sizeof shortpath))&&writes==before);
 assert(!EFI_ERROR(boot_install_default(&rs,shortpath,sizeof shortpath)));order_is(0,9,10);original();assert(indexof(L"Boot0001",&global)<0);
 puts("PASS ordinary boot respects rollback; new explicit installation re-enables preference and reuses retained option; Microsoft entries unchanged");
 assert(!EFI_ERROR(boot_restore_default(&rs)));deny_order=1;assert(EFI_ERROR(boot_install_default(&rs,fullpath,sizeof fullpath)));order_is(9,10,65535);original();int idx=indexof(L"tinymacOS-default-disabled",&vendor);assert(idx>=0&&*(u32*)vars[idx].data==1);
 t.gpt=0;assert(firmware_boot_path(&t,shortpath)==92&&shortpath[40]==1&&shortpath[41]==1);for(int i=28;i<40;i++)assert(!shortpath[i]);
 t.sectors=0;assert(firmware_boot_path(&t,shortpath)<0);t.sectors=10;t.start=~0ull-4;assert(firmware_boot_path(&t,shortpath)<0);t.start=1;t.logical_bytes=2048;assert(firmware_boot_path(&t,shortpath)<0);
 puts("PASS refused write restores prior opt-out flag; native LBA path validates MBR/GPT signatures and refuses invalid geometry");
 u8*pages=mmap(0,4*4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);assert(pages!=MAP_FAILED);
 u64 a=(u64)pages;*(u64*)pages=(a+4096)|3;*(u64*)(pages+4096)=(a+8192)|3;*(u64*)(pages+8192)=(a+12288)|3;*(u64*)(pages+12288)=0x200003;
 u8 map[80]={0};u32 type=2;u64 count=4;memcpy(map,&type,4);memcpy(map+8,&a,8);memcpy(map+24,&count,8);unsigned budget=8;
 assert(firmware_tables_kept(a,4,&budget,map,40,40)&&budget==4);type=7;memcpy(map,&type,4);budget=8;assert(!firmware_tables_kept(a,4,&budget,map,40,40));
 type=2;memcpy(map,&type,4);count=3;memcpy(map+24,&count,8);budget=8;assert(!firmware_tables_kept(a,4,&budget,map,40,40));
 count=4;memcpy(map+24,&count,8);budget=2;assert(!firmware_tables_kept(a,4,&budget,map,40,40));budget=8;assert(!firmware_tables_kept(a+1,4,&budget,map,40,40));assert(!firmware_tables_kept(1ull<<36,4,&budget,map,40,40));
 munmap(pages,16384);puts("PASS EFI bridge page-table retention: entire four-level tree required, conventional/reclaimed or missing table pages, high/misaligned roots and exhausted budget refuse before firmware calls");
 return 0;
}
