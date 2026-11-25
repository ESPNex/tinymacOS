/* Shipping disk editor on disposable RAM media. The optional FAT fixture is
 * made with mkfs.fat/mcopy; final FATs are independently checked by fsck/mtype.
 * This does not certify power-loss atomicity or physical firmware. */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "disklayout.h"
static u8*disk,*fatfixture;
static const u32 z=524288,fa=2048,fn=131072,foreign=400000;
static int writes,failat=-1;
static u32 allowed_start,allowed_count;
static int io(int d,u32 l,u32 n,void*p,int write){
 assert(d==0&&n&&l<z&&n<=z-l);
 if(write){
  assert((l==0&&n==1)||(l>=1&&l+n<=34)||(l>=z-33&&l+n<=z)||
    (l>=allowed_start&&l+n<=(u64)allowed_start+allowed_count));
  if(writes++==failat)return -1;
  memcpy(disk+(u64)l*512,p,n*512);
 }else memcpy(p,disk+(u64)l*512,n*512);
 return 0;
}
static int flush(int d){assert(!d);return 0;}
static Medium m={io,flush,0,524288,0};
static void p32(u8*p,u32 v){memcpy(p,&v,4);}
static void mbrpart(int slot,int t,u32 a,u32 n){u8*p=disk+446+slot*16;p[4]=t;p32(p+8,a);p32(p+12,n);}
static void headers(void){
 Part*p=(void*)(disk+1024);Gpt a={.rev=65536,.size=92,.at=1,.other=z-1,.first=34,.last=z-34,.entries=2,.num=128,.entrysize=128,.ecrc=crc32(p,16384)};
 memcpy(a.sig,"EFI PART",8);memset(a.guid,0x77,16);Gpt b=a;b.at=z-1;b.other=1;b.entries=z-33;b.crc=crc32(&b,92);a.crc=crc32(&a,92);
 memcpy(disk+512,&a,512);memcpy(disk+(u64)(z-33)*512,p,16384);memcpy(disk+(u64)(z-1)*512,&b,512);
}
static void fixture(int scheme){
 memset(disk,0,(u64)z*512);writes=0;failat=-1;allowed_count=0;
 for(int i=0;i<446;i++)disk[i]=(i*23+77)&255;disk[510]=85;disk[511]=170;
 u32 starts[]={fa,foreign},counts[]={fn,32768};
 if(scheme==MAP_MBR){mbrpart(0,12,fa,fn);mbrpart(1,7,foreign,32768);}
 else{mbrpart(0,0xee,1,z-1);Part*p=(void*)(disk+1024);for(int i=0;i<2;i++){memset(p[i].type,0xa0+i,16);memset(p[i].id,0xb0+i,16);p[i].start=starts[i];p[i].end=starts[i]+counts[i]-1;}headers();}
 memcpy(disk+(u64)fa*512,fatfixture,fn*512);
 memset(disk+(u64)foreign*512,0xa5,32768*512);u8*b=disk+(u64)foreign*512;memcpy(b+3,"NTFS    ",8);b[11]=0;b[12]=2;b[13]=8;b[510]=85;b[511]=170;u64 sectors=32768;memcpy(b+40,&sectors,8);p32(b+48,4);
}
static DiskEdit selected(int op,int number,u32 size){
 static DiskMap x;assert(partition_scan(&m,&x)&&x.safe);DiskEdit q={.disk=0,.op=op,.number=number,.size=size};
 for(int j=0;j<x.count;j++)if(x.part[j].number==number){q.start=x.part[j].start;q.sectors=x.part[j].sectors;return q;}
 assert(0);return q;
}
static void hashforeign(u8 h[32]){sha_bytes(disk+(u64)foreign*512,32768*512,h);}
static void output(const char*dir,int scheme,const char*name,u32 sectors){char file[1024];snprintf(file,sizeof file,"%s/%s-%s.fat",dir,scheme==MAP_GPT?"gpt":"mbr",name);FILE*f=fopen(file,"wb");assert(f);assert(fwrite(disk+(u64)fa*512,512,sectors,f)==sectors);assert(!fclose(f));}
static void run(int scheme,const char*out){
 fixture(scheme);u8 ms[32],after[32],bootstrap[446];hashforeign(ms);memcpy(bootstrap,disk,446);
 DiskEdit q=selected(DISK_EDIT_RESIZE,1,98304);assert(!partition_edit(&m,&q,0)&&!writes);q.confirm=DISK_EDIT_CONFIRM;allowed_start=fa;allowed_count=fn;
 assert(!partition_edit(&m,&q,1));hashforeign(after);assert(!memcmp(ms,after,32)&&!memcmp(bootstrap,disk,446));
 static DiskMap x;assert(partition_scan(&m,&x)&&x.part[0].fs==FS_FAT32&&x.part[0].sectors==98304);output(out,scheme,"shrunk",98304);
 q=selected(DISK_EDIT_RESIZE,1,fn);assert(!partition_edit(&m,&q,0));q.confirm=DISK_EDIT_CONFIRM;assert(!partition_edit(&m,&q,1));output(out,scheme,"grown",fn);hashforeign(after);assert(!memcmp(ms,after,32));
 printf("PASS %s real FAT32 shrink 64->48 MiB and grow back: BPB, backup, FSINFO and partition updated; foreign NTFS/bootstrap unchanged\n",scheme==MAP_GPT?"GPT":"MBR");
 q=selected(DISK_EDIT_DELETE,2,0);assert(!partition_edit(&m,&q,0));q.confirm=DISK_EDIT_CONFIRM;allowed_count=0;assert(!partition_edit(&m,&q,1));hashforeign(after);assert(!memcmp(ms,after,32));assert(partition_scan(&m,&x)&&x.count==1);
 puts("PASS deletion of occupied NTFS removes only selected table entry, preserving its payload");
 q=(DiskEdit){.disk=0,.op=DISK_EDIT_CREATE,.start=135168,.size=65536};strcpy(q.name,"Dati nativi");assert(!partition_edit(&m,&q,0));q.confirm=DISK_EDIT_CONFIRM;allowed_start=q.start;allowed_count=q.size;assert(!partition_edit(&m,&q,1));assert(partition_scan(&m,&x)&&x.count==2&&x.part[1].fs==FS_TMC);assert(!memcmp(disk+(u64)q.start*512,"TMCBFS1",7));assert(!memcmp(bootstrap,disk,446));hashforeign(after);assert(!memcmp(ms,after,32));
 puts("PASS native fixed-slot partition formatted before GPT/MBR publication; snapshots cleared; adjacent data unchanged");
 fixture(scheme);q=selected(DISK_EDIT_DELETE,1,0);assert(!partition_edit(&m,&q,0));assert(partition_edit(&m,&q,1)==DE_PROTECTED&&!writes);q.confirm=DISK_EDIT_CONFIRM;disk[440]^=1;assert(partition_edit(&m,&q,1)==DE_STALE&&!writes);puts("PASS mandatory confirmation and stale metadata reject with zero writes");
 fixture(scheme);q=selected(DISK_EDIT_RESIZE,2,16384);assert(partition_edit(&m,&q,0)<0&&!writes);puts("PASS invalid NTFS metadata rejected by the native engine with zero writes");
 fixture(scheme);u8*b=disk+(u64)fa*512;u32 r=b[14]|b[15]<<8,f;memcpy(&f,b+36,4);u32 cluster=110000;for(int i=0;i<2;i++)p32(disk+(u64)(fa+r+i*f)*512+cluster*4,0x0fffffff);
 q=selected(DISK_EDIT_RESIZE,1,98304);assert(partition_edit(&m,&q,0)==DE_OCCUPIED&&!writes);puts("PASS allocated tail cluster rejects FAT32 shrink without boundary changes");
 fixture(scheme);b=disk+(u64)fa*512;r=b[14]|b[15]<<8;p32(disk+(u64)(fa+r)*512+4,0x03ffffff);q=selected(DISK_EDIT_RESIZE,1,98304);assert(partition_edit(&m,&q,0)==DE_PROTECTED&&!writes);puts("PASS dirty/divergent FAT copies reject resize before writes");
 for(int fault=0;fault<(scheme==MAP_GPT?66:1);fault++){
  fixture(scheme);q=selected(DISK_EDIT_DELETE,2,0);assert(!partition_edit(&m,&q,0));q.confirm=DISK_EDIT_CONFIRM;failat=fault;
  assert(partition_edit(&m,&q,1)==DE_IO);hashforeign(after);assert(!memcmp(ms,after,32));assert(!memcmp(bootstrap,disk,446));
 }
 puts("PASS injected table-write errors never report success or write foreign payload/MBR bootstrap (not an atomicity guarantee)");
}
static void ebr(void){
 fixture(MAP_MBR);memset(disk+446,0,64);mbrpart(0,15,135168,131072);u8*a=disk+135168ull*512,*b=a+65536ull*512;
 memset(a,0,512);memset(b,0,512);a[510]=b[510]=85;a[511]=b[511]=170;a[450]=b[450]=7;p32(a+454,2048);p32(a+458,16384);a[466]=15;p32(a+470,65536);p32(a+474,65536);p32(b+454,2048);p32(b+458,16384);
 static DiskMap x;assert(partition_scan(&m,&x)&&x.count==3);DiskEdit q=selected(DISK_EDIT_DELETE,5,0);assert(!partition_edit(&m,&q,0));q.confirm=DISK_EDIT_CONFIRM;allowed_start=135168;allowed_count=1;assert(!partition_edit(&m,&q,1));assert(partition_scan(&m,&x)&&x.count==2&&x.part[1].start==135168+65536+2048);
 q=selected(DISK_EDIT_DELETE,5,0);assert(!partition_edit(&m,&q,0));q.confirm=DISK_EDIT_CONFIRM;assert(!partition_edit(&m,&q,1));assert(partition_scan(&m,&x)&&x.count==1);puts("PASS first/last EBR logical deletion retains valid, empty extended container");
 q=selected(DISK_EDIT_DELETE,1,0);assert(!partition_edit(&m,&q,0));q.confirm=DISK_EDIT_CONFIRM;assert(!partition_edit(&m,&q,1));assert(partition_scan(&m,&x)&&x.count==0);puts("PASS extended partition deletion is explicit and supported");
}
int main(int argc,char**argv){assert(argc==3);disk=calloc(z,512);fatfixture=malloc(fn*512);assert(disk&&fatfixture);FILE*f=fopen(argv[1],"rb");assert(f&&fread(fatfixture,512,fn,f)==fn);fclose(f);run(MAP_GPT,argv[2]);run(MAP_MBR,argv[2]);ebr();free(disk);free(fatfixture);return 0;}
