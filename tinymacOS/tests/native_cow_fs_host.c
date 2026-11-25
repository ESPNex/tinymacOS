/* Focused integration of the actual native driver, CoW and resize engines.
 * Deliberately not a substitute for the final ISO/release regression matrix. */
#define main legacy_native_resize_test_main
#include "native_resize_host.c"
#undef main
void clock_info(SysInfo*s){memset(s,0,sizeof *s);s->year=2026;s->month=11;s->day=18;}
static void mount_actual(void){
 load_driver();memset(&mounted_cow,0,sizeof mounted_cow);assert(!cow_attach(0));
}
static void write_document(int value){
 u8 data[8192];memset(data,value,sizeof data);Process p={0};p.fd[0]=(FD){3,nodes[3].gen,0,1};assert(fs_write_i(&p,0,data,sizeof data)==sizeof data);
}
static void read_snapshot(const char*name,int value){
 char path[100];snprintf(path,sizeof path,"/.snapshots/%s/document.bin",name);Process p={0};int fd=fs_open_i(&p,path,0);assert(fd>=0);u8 data[8192];assert(fs_read_i(&p,fd,data,sizeof data)==sizeof data);for(int j=0;j<8192;j++)assert(data[j]==value);assert(!fs_snap_verify_i(name));
}
int main(void){
 media=malloc((size_t)TOTAL*512);assert(media);fixture();
 /* Start with an ordinary old-format volume and no legacy snapshots. Its
  * existing independent backup is retained and verified across both resizes. */
 memset(at(NF_CAT)+32,0,8*48);put32(at(NF_CAT)+12,0);mount_actual();write_document('A');
 int beforewrites=writes;assert(!fs_snap_create_i("one"));assert(mounted_cow.enabled&&!mounted_cow.allocated);assert(writes-beforewrites<200);
 assert(!fs_snap_create_i("two")&&!mounted_cow.allocated);
 write_document('B');assert(mounted_cow.allocated<20);read_snapshot("one",'A');read_snapshot("two",'A');
 assert(!fs_snap_create_i("three"));write_document('C');read_snapshot("three",'B');
 mount_actual();read_snapshot("one",'A');read_snapshot("three",'B');
 puts("PASS actual filesystem snapshots are shared maps, not volume copies; file writes preserve A/B generations and survive a cold map reload");
 Medium m={io,flush,0,TOTAL};DiskEdit q={.start=BASE,.sectors=65536,.size=49152};
 int oldwrites=writes;assert(!native_resize(&m,&q,0)&&writes==oldwrites);assert(!native_resize(&m,&q,1));mount_actual();assert(active_slots==288);read_snapshot("one",'A');read_snapshot("two",'A');read_snapshot("three",'B');
 q.sectors=49152;q.size=65536;assert(!native_resize(&m,&q,0));assert(!native_resize(&m,&q,1));mount_actual();assert(active_slots==384);read_snapshot("one",'A');read_snapshot("three",'B');
 puts("PASS actual CoW snapshots and their shared pool survive real native 32->24->32MiB capacity/map relocation without materializing full snapshots");
 assert(!fs_snap_restore_i("one"));read_snapshot("three",'B');u8 data[512];assert(bytes(3,0,data,512,0)==512);for(int j=0;j<512;j++)assert(data[j]=='A');
 u32 allocated=mounted_cow.allocated;assert(!fs_snap_delete_i("one"));assert(mounted_cow.allocated<=allocated);read_snapshot("two",'A');
 assert(!fs_snap_delete_i("two"));assert(!fs_snap_delete_i("three"));assert(!mounted_cow.allocated);
 puts("PASS restoring one snapshot preserves other historical views; deleting the last references returns every pool block to free space");
 free(media);return 0;
}
