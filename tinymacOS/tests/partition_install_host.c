/* Executes the ACTUAL installer state machine and maintenance readers against
 * disposable RAM media; raw writes are independently range-checked. */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "../fs/install.c"
struct Boot boot;
u64 ticks=987654321;
u32 persistent;
static u8 *disk;
static u32 sectors=524288,guard_start,guard_end;
static int writes,fail_at=-1,write_guard;
/* Optional sparse high-LBA window for tests/large_disk_host.c. */
static u32 sparse_high,sparse_window;
static u8 payload[24*1024*1024],recovery[10*1024*1024];
int sata_count(void){return 1;}
u32 sata_size(int d){assert(d==0);return sectors;}
const char*sata_model(int d){(void)d;return "Disposable partition fixture";}
int sata_flush(int d){(void)d;return 0;}
int sata_io(int d,u32 l,u32 n,void*p,int wr){
 assert(d==0&&l<sectors&&n<=sectors-l);
 u8 *where;
 if(sparse_high){
  if(l>=sparse_high){assert(n<=sparse_window-(l-sparse_high));where=disk+(64ull+l-sparse_high)*512;}
  else if(l<64&&n<=64-l)where=disk+(u64)l*512;
  else{assert(!wr);memset(p,0,(size_t)n*512);return 0;}
 }else where=disk+(u64)l*512;
 if(wr){
  if(write_guard)assert((l>=guard_start&&l<guard_end&&n<=guard_end-l)||(l==0&&n==1)||(inventory.scheme==MAP_GPT&&((l==1&&n==1)||(l==2&&n==32)||(l==sectors-33&&n==32)||(l==sectors-1&&n==1))));
  if(fail_at>=0&&writes++==fail_at)return -1;
  if(fail_at<0)writes++;
  memcpy(where,p,n*512);
 }else memcpy(p,where,n*512);return 0;
}
int volume_cipher_io(int d,u32 l,u32 n,void*p,int w){return sata_io(d,l,n,p,w);}
Vault*volume_vault(int d){(void)d;return 0;}
int fs_snapshot(void*p,u32 n){assert(n==NDATA*512);memset(p,0,n);memcpy(p,"TMCBFS1\0",8);u32*h=p;h[2]=2;h[3]=NSLOTS;h[4]=144;h[5]=1;((u8*)p)[64*512]=1;memcpy((u8*)p+128*512+64,"_BHRfS_M",8);return 0;}
static void put32(u8*p,u32 v){memcpy(p,&v,4);}
static void put16(u8*p,u16 v){memcpy(p,&v,2);}
static void mbrpart(int i,u8 t,u32 a,u32 n){u8*p=disk+446+16*i;p[4]=t;put32(p+8,a);put32(p+12,n);}
static void fat32(u32 at,u32 total){u8*b=disk+(u64)at*512;memcpy(b,"\xeb\x58\x90MSWIN4.1",11);put16(b+11,512);b[13]=1;put16(b+14,32);b[16]=2;put32(b+32,total);put32(b+36,1024);put32(b+44,2);b[510]=85;b[511]=170;}
static void ntfs(u32 at,u32 total){u8*b=disk+(u64)at*512;memcpy(b+3,"NTFS    ",8);put16(b+11,512);b[13]=8;u64 n=total;memcpy(b+40,&n,8);put32(b+48,4);b[510]=85;b[511]=170;}
static void headers(void){
 Part*p=(Part*)(disk+1024);Gpt a={.rev=65536,.size=92,.at=1,.other=sectors-1,.first=34,.last=sectors-34,.entries=2,.num=128,.entrysize=128,.ecrc=crc32(p,16384)};
 memcpy(a.sig,"EFI PART",8);memset(a.guid,0x77,16);Gpt b=a;b.at=sectors-1;b.other=1;b.entries=sectors-33;b.crc=crc32(&b,92);a.crc=crc32(&a,92);
 memcpy(disk+512,&a,512);memcpy(disk+(u64)(sectors-33)*512,p,16384);memcpy(disk+(u64)(sectors-1)*512,&b,512);
}
static void fixture(int scheme){
 memset(disk,0,(u64)sectors*512);memset(&st,0,sizeof st);mounted=-1;writes=0;fail_at=-1;write_guard=0;
 for(int i=0;i<440;i++)disk[i]=(i*23+77)&255;disk[440]=19;disk[444]=81;disk[510]=85;disk[511]=170;
 u32 a[]={2048,135168,153600},n[]={131072,16384,4096};
 if(scheme==MAP_MBR){for(int i=0;i<3;i++)mbrpart(i,i==0?12:i==1?7:0xda,a[i],n[i]);disk[446]=128;}
 else{mbrpart(0,0xee,1,sectors-1);Part*p=(Part*)(disk+1024);for(int i=0;i<3;i++){memset(p[i].type,0xa0+i,16);memset(p[i].id,0xb0+i,16);p[i].start=a[i];p[i].end=a[i]+n[i]-1;p[i].name[0]='A'+i;}headers();}
 fat32(a[0],n[0]);ntfs(a[1],n[1]);memset(disk+(u64)a[2]*512,0x63,n[2]*512);
 memset(payload,0,sizeof payload);memset(recovery,0x4b,sizeof recovery);boot=(struct Boot){.install_esp=(u64)payload,.install_len=sizeof payload,.recovery_img=(u64)recovery,.recovery_len=sizeof recovery};
}
static void digest(u8 out[3][32]){u32 a[]={2048,135168,153600},n[]={131072,16384,4096};for(int j=0;j<3;j++)sha_bytes(disk+(u64)a[j]*512,n[j]*512,out[j]);}
static void run(int scheme){
 fixture(scheme);Medium m={sata_io,sata_flush,0,sectors};static DiskMap x;assert(partition_scan(&m,&x)&&x.safe&&x.scheme==scheme&&x.count==3&&x.sectors>=TC_SECTORS);
 assert(x.part[0].fs==FS_FAT32&&x.part[1].fs==FS_NTFS&&x.part[2].fs==FS_RAW);u8 before[3][32],after[3][32],table[64];digest(before);memcpy(table,disk+446,64);
 guard_start=x.start;guard_end=x.start+TC_SECTORS;write_guard=1;assert(!install_begin_i(0));int steps=0;
 while(isbusy()){assert(!install_step_i());assert(++steps<100000);}assert(st.state==INSTALL_DONE);digest(after);assert(!memcmp(before,after,sizeof before));
 if(scheme==MAP_MBR)assert(!memcmp(table,disk+446,48));else{Part*p=(Part*)(disk+1024);assert(!memcmp(p,oldparts,3*128));}
 assert(!memcmp(disk+(u64)(guard_start+TC_OLD_MBR)*512,oldmbr,512));assert(!memcmp(disk+440,oldmbr+440,6));
 static Layout layout;assert(layout_read(&m,&layout)==2&&layout.container&&layout.data==guard_start+TC_DATA&&layout.rec==guard_start+TC_REC);
 assert(!firstaid(&m,0));assert(!fs_snapshot(snapshot,sizeof snapshot));assert(!save_copy(&m,snapshot));assert(has_copy(&m));assert(!restore_copy(&m));assert(!restore_os(&m));
 assert(partition_scan(&m,&x)&&x.safe&&x.count==4);digest(after);assert(!memcmp(before,after,sizeof before));
 printf("PASS %s actual installer, bounded writes, FAT32/NTFS/RAW hashes unchanged, original MBR saved, relocated mount/firstaid/backup/restore\n",scheme==MAP_GPT?"GPT":"MBR");
}
static void failure_tests(int scheme)
{
    static const int failures[]={0,1,190,191,192,300,400,485,488,489,490,491,492,493,494,495,496,497,498,499,500,501,502,503,504};
    for(unsigned k=0;k<COUNT(failures);k++){
        fixture(scheme);u8 before[3][32],after[3][32];digest(before);assert(!install_begin_i(0));
        guard_start=install_start;guard_end=install_end;write_guard=1;fail_at=failures[k];
        for(int steps=0;isbusy();steps++){int r=install_step_i();assert(steps<100000);if(r<0)assert(st.state==INSTALL_ERROR);}
        digest(after);assert(!memcmp(before,after,sizeof before));
        if(st.state==INSTALL_DONE)assert(writes<=fail_at);else assert(st.state==INSTALL_ERROR);
    }
    fixture(scheme);assert(!install_begin_i(0));disk[444]^=1;while(isbusy())install_step_i();assert(st.state==INSTALL_ERROR&&!writes);puts("PASS metadata changed after selection: refusal before first write");
    printf("PASS %s %zu injected write failures preserve every existing FAT32/NTFS/RAW byte\n",scheme==MAP_GPT?"GPT":"MBR",COUNT(failures));
}
int main(void){
 disk=calloc(sectors,512);assert(disk);run(MAP_GPT);run(MAP_MBR);failure_tests(MAP_GPT);failure_tests(MAP_MBR);
 Medium m={sata_io,sata_flush,0,sectors};static DiskMap x;
 fixture(MAP_GPT);disk[512+16]^=1;assert(!partition_scan(&m,&x)&&!x.safe);assert(install_begin_i(0)<0&&!writes);puts("PASS degraded GPT refuses allocation without writes");
 fixture(MAP_MBR);mbrpart(2,0xda,140000,4096);assert(!partition_scan(&m,&x)&&!x.safe);assert(install_begin_i(0)<0&&!writes);puts("PASS overlapping MBR refuses allocation without writes");
 fixture(MAP_MBR);memset(disk+446+16*2,0,16);mbrpart(1,7,135168,sectors-135168);assert(partition_scan(&m,&x)&&x.sectors<TC_SECTORS);assert(install_begin_i(0)<0&&!writes);puts("PASS occupied/RAW partitions are never allocation candidates");
 fixture(MAP_MBR);mbrpart(2,15,153600,100000);u8*e=disk+153600ull*512;memset(e,0,512);e[510]=85;e[511]=170;e[450]=7;put32(e+454,2048);put32(e+458,16384);ntfs(155648,16384);
 assert(partition_scan(&m,&x)&&x.safe&&x.count==4&&x.part[3].logical&&x.part[3].fs==FS_NTFS);
 for(int i=0;i<x.gaps;i++)assert(x.gap[i].start+x.gap[i].sectors<=153600||x.gap[i].start>=253600);puts("PASS logical NTFS via EBR is inventoried; entire extended partition remains reserved");
 e[466]=15;put32(e+470,10);put32(e+474,99990);u8*f=e+10*512;memcpy(f,e,512);put32(f+470,10);
 assert(!partition_scan(&m,&x)&&!x.safe);puts("PASS cyclic/malformed EBR refuses allocation");free(disk);return 0;
}
