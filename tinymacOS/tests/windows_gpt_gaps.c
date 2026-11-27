/* Read-only sparse Windows-like GPT: ESP, genuine MSR, Windows NTFS,
 * 10 GiB unallocated, recovery NTFS. No physical disk or huge RAM image. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "disklayout.h"
static u32 total=500118192,free_at,free_n=10u*1024*2048;
static unsigned writes;
static u8 pmbr[512];static Gpt primary,backup;static Part table[128],copy[128];
static int io(int d,u64 l,u32 n,void*p,int w){
 (void)d;assert(l<total&&n<=total-l);if(w){writes++;assert(!"partition scan must not write");}
 memset(p,0,n*512u);
 if(l==0&&n==1)memcpy(p,pmbr,512);
 else if(l==1&&n==1)memcpy(p,&primary,512);
 else if(l==total-1&&n==1)memcpy(p,&backup,512);
 else if(l==2&&n==32)memcpy(p,table,sizeof table);
 else if(l==total-33&&n==32)memcpy(p,copy,sizeof copy);
 else if(n==1&&(l==table[2].start||l==table[3].start)){
  u8*b=p;memcpy(b+3,"NTFS    ",8);b[11]=0;b[12]=2;b[13]=8;u64 count=table[l==table[2].start?2:3].end-l+1;memcpy(b+40,&count,8);b[48]=4;b[510]=85;b[511]=170;
 }
 return 0;
}
static void fixture(void){
 memset(pmbr,0,512);memset(table,0,sizeof table);pmbr[510]=85;pmbr[511]=170;pmbr[450]=0xee;u32 one=1,len=total-1;memcpy(pmbr+454,&one,4);memcpy(pmbr+458,&len,4);
 const u8 basic[16]={0xa2,0xa0,0xd0,0xeb,0xe5,0xb9,0x33,0x44,0x87,0xc0,0x68,0xb6,0xb7,0x26,0x99,0xc7};
 const u8 msr[16]={0x16,0xe3,0xc9,0xe3,0x5c,0x0b,0xb8,0x4d,0x81,0x7d,0xf9,0x2d,0xf0,0x02,0x15,0xae};
 free_at=total-34-1048576-free_n;
 u32 starts[4]={2048,206848,239616,total-34-1048576},ends[4]={206847,239615,free_at-1,total-35};
 for(unsigned i=0;i<4;i++){memcpy(table[i].type,i==0?esp_type:i==1?msr:basic,16);memset(table[i].id,0x30+i,16);table[i].start=starts[i];table[i].end=ends[i];}
 primary=(Gpt){.rev=65536,.size=92,.at=1,.other=total-1,.first=34,.last=total-34,.entries=2,.num=128,.entrysize=128,.ecrc=crc32(table,sizeof table)};
 memcpy(primary.sig,"EFI PART",8);memset(primary.guid,0x52,16);backup=primary;backup.at=total-1;backup.other=1;backup.entries=total-33;primary.crc=crc32(&primary,92);backup.crc=crc32(&backup,92);memcpy(copy,table,sizeof table);
}
static void check_gap(DiskMap*x){
 assert(x->count==4&&x->gaps>=1&&x->start==((free_at+2047)&~2047u)&&x->sectors>=free_n-2047);
 int found=0;for(int i=0;i<x->gaps;i++)if(x->gap[i].start==free_at&&x->gap[i].sectors==free_n)found=1;assert(found);
 assert(x->part[1].sectors==32768&&x->part[1].fs==FS_OTHER&&!strcmp(x->part[1].name,"Microsoft Reserved (MSR)"));
 assert(x->part[2].fs==FS_NTFS&&x->part[3].fs==FS_NTFS&&!writes);
 for(int i=0;i<x->gaps;i++)for(int j=0;j<x->count;j++)assert((u64)x->gap[i].start+x->gap[i].sectors<=x->part[j].start||(u64)x->part[j].start+x->part[j].sectors<=x->gap[i].start);
}
int main(void){
 Medium m={io,0,0,total};static DiskMap x;
 fixture();pmbr[510]=pmbr[511]=0;assert(!partition_scan(&m,&x)&&!x.safe&&x.scheme==MAP_GPT&&x.pmbr_signature_repairable);check_gap(&x);assert(!writes);puts("PASS missing 55AA: both GPT copies verified and gap exposed; signature-only installer recovery flagged, ordinary writes still protected");
 fixture();pmbr[510]=0;pmbr[462+8]=1;assert(!partition_scan(&m,&x)&&!x.pmbr_signature_repairable&&!x.safe);assert(strstr(x.reason,"residua"));assert(!writes);puts("PASS missing 55AA with stale untyped MBR entry refuses repair");
 fixture();pmbr[510]=0;backup.crc^=1;assert(!partition_scan(&m,&x)&&!x.pmbr_signature_repairable&&!x.safe);assert(!writes);
 fixture();pmbr[510]=0;copy[2].attrs^=1;backup.ecrc=crc32(copy,sizeof copy);backup.crc=0;backup.crc=crc32(&backup,92);assert(!partition_scan(&m,&x)&&!x.pmbr_signature_repairable);assert(!writes);
 fixture();pmbr[510]=0;pmbr[450]=7;assert(!partition_scan(&m,&x)&&!x.pmbr_signature_repairable);assert(!writes);puts("PASS missing MBR signature never permits repair when GPT is invalid/discordant or protective entry is absent");
 fixture();assert(partition_scan(&m,&x)&&x.safe);check_gap(&x);puts("PASS Windows GPT: EFI+16MiB MSR+2 NTFS, exact 10GiB genuine gap visible/selectable; zero writes");
 backup.crc^=1;assert(!partition_scan(&m,&x)&&!x.safe);assert(strstr(x.reason,"backup"));check_gap(&x);puts("PASS degraded GPT: validated primary still exposes genuine 10GiB unallocated inventory, but writes remain forbidden");
 fixture();primary.crc^=1;assert(!partition_scan(&m,&x)&&!x.safe);assert(strstr(x.reason,"primaria"));check_gap(&x);puts("PASS validated backup inventory exposes same gap without silently repairing Windows GPT");
 fixture();copy[2].attrs^=1;backup.ecrc=crc32(copy,sizeof copy);backup.crc=0;backup.crc=crc32(&backup,92);assert(!partition_scan(&m,&x)&&!x.safe);check_gap(&x);assert(strstr(x.reason,"discordanti"));puts("PASS discordant valid GPT copies: gap visible, unsafe allocation refused");
 fixture();u32 wrong=total-2;memcpy(pmbr+458,&wrong,4);assert(!partition_scan(&m,&x)&&!x.safe);assert((x.pmbr_signature_repairable&2)&&strstr(x.reason,"MBR protettivo"));check_gap(&x);puts("PASS stale protective MBR: consent-only coverage normalization offered; scanner never writes and ordinary disk editing stays blocked");

 fixture();u32 saturate=0xffffffff;memcpy(pmbr+458,&saturate,4);assert(!partition_scan(&m,&x)&&x.pmbr_signature_repairable==2);check_gap(&x);
 fixture();pmbr[446]=128;assert(!partition_scan(&m,&x)&&!x.pmbr_signature_repairable);
 fixture();pmbr[454]=2;assert(!partition_scan(&m,&x)&&!x.pmbr_signature_repairable);
 fixture();pmbr[462+8]=1;assert(!partition_scan(&m,&x)&&!x.pmbr_signature_repairable);
 fixture();memcpy(pmbr+458,&saturate,4);backup.crc^=1;assert(!partition_scan(&m,&x)&&!x.pmbr_signature_repairable);
 puts("PASS saturated EE length repairable only with identical valid GPT; boot flag, bad start, residual entries and degraded peer stay blocked");
 fixture();table[2].end=table[3].start-1;primary.ecrc=crc32(table,sizeof table);primary.crc=0;primary.crc=crc32(&primary,92);memcpy(copy,table,sizeof copy);backup.ecrc=primary.ecrc;backup.crc=0;backup.crc=crc32(&backup,92);assert(partition_scan(&m,&x)&&x.safe&&x.sectors<free_n);assert(!writes);puts("PASS no real gap: occupied NTFS/RAW/MSR never reclassified as free space");
}
