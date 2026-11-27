/* Actual installer, GPT/MBR reader and maintenance at high LBAs, without
 * allocating/scanning a terabyte or ever opening a physical device. */
#define TMC_FS2_HOST_HELPERS 1
#include "fs2_install_host.c"
static void large_fixture(u64 total,int scheme){
 sectors=total;sparse_high=(total-524288u)&~2047ull;sparse_window=total-sparse_high;
 disk=calloc((size_t)sparse_window+64,512);assert(disk);
 memset(&st,0,sizeof st);mounted=-1;writes=0;fail_at=-1;write_guard=0;
 disk[510]=85;disk[511]=170;disk[440]=27;
 if(scheme==MAP_GPT){
  mbrpart(0,0xee,1,MIN(total-1,0xffffffffull));Part*t=(void*)(disk+1024);memset(t[0].type,0xa7,16);memset(t[0].id,0x63,16);t[0].start=2048;t[0].end=sparse_high-1;
  Gpt a={.rev=65536,.size=92,.at=1,.other=total-1,.first=34,.last=total-34,.entries=2,.num=128,.entrysize=128,.ecrc=crc32(t,16384)};
  memcpy(a.sig,"EFI PART",8);memset(a.guid,0x77,16);Gpt b=a;b.at=total-1;b.other=1;b.entries=total-33;b.crc=crc32(&b,92);a.crc=crc32(&a,92);
  assert(!sata_io(0,1,1,&a,1));assert(!sata_io(0,total-33,32,t,1));assert(!sata_io(0,total-1,1,&b,1));
 }else mbrpart(0,7,2048,sparse_high-2048);
 memset(payload,0,sizeof payload);memset(recovery,0x4b,sizeof recovery);boot=(struct Boot){.install_esp=(u64)payload,.install_len=sizeof payload,.recovery_img=(u64)recovery,.recovery_len=sizeof recovery};writes=0;
}
static void large_run(u64 total,int scheme){
 large_fixture(total,scheme);Medium m={sata_io,sata_flush,0,total};static DiskMap x;assert(partition_scan(&m,&x)&&x.safe&&x.count==1&&x.start==sparse_high);
 u8 foreign_entry[128];memcpy(foreign_entry,scheme==MAP_GPT?disk+1024:disk+446,scheme==MAP_GPT?128:16);
 guard_start=sparse_high;guard_end=sectors-34u;write_guard=1;
 refresh();assert(st.disk[0].usable&&st.disk[0].sectors==total);assert(!install_begin_i(0));
 for(int step=0;isbusy();step++){assert(step<100000);assert(!install_step_i());}assert(st.state==INSTALL_DONE&&writes>0);
 assert(!memcmp(foreign_entry,scheme==MAP_GPT?disk+1024:disk+446,scheme==MAP_GPT?128:16));
 static Layout out;assert(layout_read(&m,&out)==2&&out.container&&out.data==sparse_high+TC_DATA&&out.rec==install_rec);
 assert(!fs2_disk_verify(0,out.data,out.parts[1].end-out.data+1));assert(!fs2_disk_backup(0,out.data,out.parts[1].end-out.data+1,1));assert(fs2_disk_backup(0,out.data,out.parts[1].end-out.data+1,0)==1);assert(!fs2_disk_backup(0,out.data,out.parts[1].end-out.data+1,2));assert(!restore_os(&m));
 assert(partition_scan(&m,&x)&&x.safe&&x.count==2);
 printf("PASS actual %s installer + maintenance at %llu sectors (%llu bytes), native extent near high LBA %llu, foreign entry preserved, strict write canaries, primary/backup readback\n",scheme==MAP_GPT?"GPT":"MBR",(unsigned long long)total,(unsigned long long)total*512,(unsigned long long)sparse_high);
 if(scheme==MAP_GPT){u8 b[512];assert(!sata_io(0,total-1,1,b,0));b[16]^=1;assert(!sata_io(0,total-1,1,b,1));int before=writes;assert(!partition_scan(&m,&x)&&!x.safe);assert(install_begin_i(0)<0&&writes==before);puts("PASS high-LBA corrupt backup GPT still refuses before writes");}
 free(disk);disk=0;sparse_high=sparse_window=0;
}
int main(void){
 large_fixture(3ull*(1ull<<31),MAP_MBR);mbrpart(0,7,0xfffff000u,8192);
 Medium overflow={sata_io,sata_flush,0,sectors};static DiskMap invalid;
 assert(!partition_scan(&overflow,&invalid)&&!invalid.safe&&!writes);
 free(disk);disk=0;sparse_high=sparse_window=0;
 puts("PASS MBR extent crossing the 32-bit address limit refuses before writes on a 3TiB disk");
 large_run(1953125000u,MAP_GPT);large_run(TMC_DISK_MAX_SECTORS,MAP_GPT);large_run(2147483648ull,MAP_MBR);
 Medium too_big={sata_io,sata_flush,0,TMC_DISK_MAX_SECTORS+1u};static DiskMap x;int before=writes;assert(!partition_scan(&too_big,&x)&&!x.safe&&strstr(x.reason,"16 TiB")&&writes==before);
 puts("PASS above-16TiB capacity is explicitly rejected without I/O or writes; no signed LBA truncation at 0x80000000");return 0;
}
