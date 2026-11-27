#pragma once
#include "base.h"
/* Standard UEFI short-form HD()/File() device path. Native LBAs, not the
 * kernel's 512-byte coordinates. GPT partition GUID or PC-AT disk signature. */
typedef struct {u32 number,logical_bytes;u64 start,sectors;u8 signature[16];u32 gpt;} FirmwareTarget;
#define FW_RUNTIME_MAGIC 0x3154524946454354ull /* TCEFIRT1 */
#define FW_BOOT_PATH_BYTES 92u
static inline int firmware_boot_path(const FirmwareTarget*t,u8 out[FW_BOOT_PATH_BYTES]){
 static const char file[]="\\EFI\\OC\\OpenCore.efi";
 if(!t||!out||!t->number||(t->logical_bytes!=512&&t->logical_bytes!=4096)||!t->sectors||t->start>~0ull-t->sectors||t->gpt>1)return -1;
 unsigned nonzero=0;for(unsigned i=0;i<(t->gpt?16u:4u);i++)nonzero|=t->signature[i];if(!nonzero)return -1;
 memset(out,0,FW_BOOT_PATH_BYTES);out[0]=4;out[1]=1;out[2]=42;
 memcpy(out+4,&t->number,4);memcpy(out+8,&t->start,8);memcpy(out+16,&t->sectors,8);memcpy(out+24,t->signature,t->gpt?16:4);out[40]=t->gpt?2:1;out[41]=t->gpt?2:1;
 const unsigned fn=sizeof file,bytes=4+2*fn;out[42]=4;out[43]=4;out[44]=bytes;
 for(unsigned i=0;i<fn;i++)out[46+2*i]=file[i];
 unsigned end=42+bytes;out[end]=0x7f;out[end+1]=0xff;out[end+2]=4;
 return end+4;
}
/* Original EFI page tables may only be reused if EVERY table page remains in
 * a non-reclaimable loader/firmware descriptor. PMM frees only type 7.
 * Walk 4 levels with a strict budget, never follow leaf or reserved bits. */
static inline int firmware_kept(u64 at,u64 bytes,const void*map,u64 size,u64 step){
 if(!map||step<40||!bytes||at>~0ull-bytes||size%step)return 0;
 for(u64 off=0;off<size;off+=step){const u8*d=(const u8*)map+off;u32 type;u64 start,pages;memcpy(&type,d,4);memcpy(&start,d+8,8);memcpy(&pages,d+24,8);
  if(type<1||type>6||pages>(~0ull-start)/4096)continue;
  if(at>=start&&at+bytes<=start+pages*4096)return 1;
 }return 0;
}
static inline int firmware_tables_kept(u64 root,unsigned level,unsigned*budget,const void*map,u64 size,u64 step){
 if(!root||!level||!budget||!*budget||root%4096||root>=1ull<<36||!firmware_kept(root,4096,map,size,step))return 0;(*budget)--;
 const u64*p=(const u64*)root;
 for(unsigned i=0;i<512;i++)if((p[i]&1)&&level>1){if(p[i]&128){if(level==4)return 0;continue;}
  if(!firmware_tables_kept(p[i]&0x000ffffffffff000ull,level-1,budget,map,size,step))return 0;
 }return 1;
}
