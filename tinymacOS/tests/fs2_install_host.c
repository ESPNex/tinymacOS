/* Actual installer state machine, actual format/import core, real selected
 * extent IO; no target formatter mock and no host-preformatted target. */
#define main legacy_main
#include "partition_install_host.c"
#undef main
#include "vault2.h"
#include "fs2_volume.h"
#include "../fs/tmcbfs2_install.c"
static int invalid_source;
void*kmalloc(size_t n){return malloc(n);}
void kfree(void*p){free(p);}
int kmalloc_owns(const void*p){(void)p;return 1;}
u64 page_get(void){return (u64)malloc(4096);}
void page_put(u64 p){free((void*)p);}
#include "fs2_seed_fixture.h"
int fs2_seed_snapshot(void*p,u32 bytes){return seed_fixture(p,bytes,invalid_source);}
static void run_fs2(int scheme){
 fixture(scheme);u8 before[3][32],after[3][32];digest(before);assert(!install_begin_i(0));guard_start=install_start;guard_end=install_end;write_guard=1;
 while(isbusy())assert(!install_step_i());assert(st.state==INSTALL_DONE&&st.done==st.total);digest(after);assert(!memcmp(before,after,sizeof before));
 Medium m={sata_io,sata_flush,0,sectors};Layout map;assert(layout_read(&m,&map)==2);assert(map.data==guard_start+TC_DATA);assert(!memcmp(disk+(u64)map.data*512,"TMCBFS2",8));
 u32 data_sectors=map.rec-map.data;
 assert(data_sectors==container_span(inventory.sectors)-(TC_SECTORS-TC_DATA_N));
 NativeContainer *descriptor=(void*)(disk+(u64)(guard_start+TC_HEADER)*512);
 assert(descriptor->version==2&&container_bounds(descriptor,guard_end-guard_start));
 static DiskMap allocated;assert(partition_scan(&m,&allocated)&&allocated.safe);
 int found=0;for(int i=0;i<allocated.count;i++)if(allocated.part[i].start==guard_start){assert(allocated.part[i].sectors==guard_end-guard_start);found=1;}
 assert(found&&guard_end-guard_start>TC_SECTORS);
 assert(fs2_disk_present(0,map.data,data_sectors)==1);
 VaultRequest statusq={.disk=0};VaultInfo statusinfo;int statusresult;assert(fs2_volume_control(0,0,&statusq,&statusinfo,&statusresult)&&!statusresult&&statusinfo.format==2&&!statusinfo.enabled);assert(!fs2_disk_verify(0,map.data,data_sectors));assert(!fs2_seed_snapshot(snapshot,sizeof snapshot));assert(!fs2_install_data(0,map.data,data_sectors,snapshot,sizeof snapshot,1));
 /* Production OFFLINE backup adapter, not just the core API. */
 assert(fs2_disk_backup(0,map.data,data_sectors,0)==0);assert(!fs2_disk_backup(0,map.data,data_sectors,1));assert(fs2_disk_backup(0,map.data,data_sectors,0)==1);
 target.disk=0;target.base=map.data;target.sectors=data_sectors;
 T2Io io={0,data_sectors/32,rd,wr,fl,al,fr};Tmc2 fs;T2Entry dir,file;assert(!tmc2_mount(&fs,io));u64 shared=tmc2_used_blocks(&fs);assert(!tmc2_lookup(&fs,1,"Documents",&dir)&&!tmc2_lookup(&fs,dir.key,"proof.txt",&file));
 assert(tmc2_write(&fs,file.key,0,"CHANGED",7)==7);tmc2_close(&fs);
 assert(!fs2_disk_backup(0,map.data,data_sectors,2));assert(!fs2_install_data(0,map.data,data_sectors,snapshot,sizeof snapshot,1));
 assert(!fs2_disk_backup(0,map.data,data_sectors,1));assert(!tmc2_mount(&fs,io));assert(tmc2_used_blocks(&fs)<shared+20);tmc2_close(&fs);
 mounted=0;assert(fs2_disk_backup(0,map.data,data_sectors,1)<0&&fs2_disk_backup(0,map.data,data_sectors,2)<0&&fs2_disk_reset(0,map.data,data_sectors)<0);mounted=-1;
 assert(!fs2_disk_reset(0,map.data,data_sectors));assert(!fs2_disk_verify(0,map.data,data_sectors));assert(!fs2_disk_backup(0,map.data,data_sectors,0));
 assert(!tmc2_mount(&fs,io));u8 account[512];assert(!tmc2_account(&fs,0,account,0));for(unsigned j=0;j<512;j++)assert(!account[j]);
 T2Entry home,prefs;assert(!tmc2_lookup(&fs,1,"Users",&home)&&!tmc2_lookup(&fs,home.key,"User",&home));assert(!tmc2_lookup(&fs,home.key,"Documents",&dir));u64 cookie=0;assert(!tmc2_readdir(&fs,dir.key,&cookie,&file));
 assert(!tmc2_lookup(&fs,home.key,"Library",&prefs)&&!tmc2_lookup(&fs,prefs.key,"Preferences",&prefs)&&!tmc2_lookup(&fs,prefs.key,"com.tinymacos.appearance.plist",&file));assert(file.kind==T2_FILE&&!file.size);assert(tmc2_write(&fs,file.key,0,"defaults",8)==8);tmc2_close(&fs);
 if(scheme==MAP_GPT)assert(!memcmp(disk+1024,oldparts,3*128));else assert(!memcmp(disk+446,oldmbr+446,48));
 printf("PASS actual %s fresh TMCBFS2 installer: core formats full selected data extent, verifies files/accounts and shared-root initial backup after remount, only then activates table; existing FAT32/NTFS/RAW bytes unchanged\n",scheme==MAP_GPT?"GPT":"MBR");
}
static u32 envelope_base;
static int envrd(void*x,u64 b,void*p){(void)x;return sata_io(0,envelope_base+b*8,8,p,0);}
static int envwr(void*x,u64 b,const void*p){(void)x;return sata_io(0,envelope_base+b*8,8,(void*)p,1);}
static int envfl(void*x){(void)x;return sata_flush(0);}
int vault_random(u8*p,size_t n){static unsigned generation;generation++;for(size_t j=0;j<n;j++)p[j]=(j*37+91+generation*13)%251+1;return 1;}
static void encrypted_adapter(void){
 /* Reuse the ACTUAL installer-created partition layout above. Host creation of
  * the envelope is explicit QA setup, not encrypted-installer acceptance. */
 Medium m={sata_io,sata_flush,0,sectors};Layout map;assert(layout_read(&m,&map)==2);envelope_base=map.data;u32 extent=map.parts[1].end-map.data+1;
 u8 before[3][32],after[3][32];digest(before);memset(disk+(u64)map.data*512,0,(u64)extent*512);fs2_volume_forget(0);
 Vault2 v;Fv2Io vio={0,extent/8,envrd,envwr,envfl,al,fr};u8 entropy[128];vault_random(entropy,128);char key[65];assert(!vault2_probe(&v,vio)&&!vault2_create(&v,"encrypted-installer-test",entropy,key));vault2_lock(&v);
 assert(fs2_disk_present(0,map.data,extent)==2&&fs2_volume_locked(0));VaultRequest q={.disk=0};VaultInfo info;int result;
 assert(fs2_volume_control(0,0,&q,&info,&result)&&!result&&info.enabled&&info.locked&&info.state==FV_ON&&info.format==3);
 target.disk=0;target.base=map.data;target.sectors=extent;T2Io io={0,fs2_volume_blocks(0,map.data,extent),rd,wr,fl,al,fr};
 static u8 buffer[T2_BLOCK];int oldwrites=writes;assert(fs2_volume_read(0,map.data,extent,0,buffer)<0&&writes==oldwrites);assert(fs2_volume_sector_guard(0,map.data,32)&&fs2_volume_sector_guard(0,map.data-1,2)&&!fs2_volume_sector_guard(0,map.data-1,1));
 strset(q.password,key,sizeof q.password);assert(fs2_volume_control(4,0,&q,&info,&result)&&!result&&!info.locked&&!fs2_volume_locked(0));
 oldwrites=writes;assert(fs2_install_data(0,map.data,extent,snapshot,sizeof snapshot,0)<0&&fs2_disk_reset(0,map.data,extent)<0&&writes==oldwrites);
 Tmc2 fs;assert(io.blocks==(extent/8-FV2_START)/5);assert(!tmc2_format(&fs,io));assert(!fs2_seed_snapshot(snapshot,sizeof snapshot));assert(!tmc2_seed_import(&fs,snapshot,sizeof snapshot));assert(!tmc2_snapshot(&fs,"Installazione",0));assert(!tmc2_verify(&fs));tmc2_close(&fs);
 assert(!fs2_install_data(0,map.data,extent,snapshot,sizeof snapshot,1)&&!fs2_disk_verify(0,map.data,extent)&&!fs2_disk_backup(0,map.data,extent,1));
 assert(!tmc2_mount(&fs,io));T2Entry dir,file;assert(!tmc2_lookup(&fs,1,"Documents",&dir)&&!tmc2_lookup(&fs,dir.key,"proof.txt",&file)&&tmc2_write(&fs,file.key,0,"CHANGED",7)==7);tmc2_close(&fs);
 assert(!fs2_disk_backup(0,map.data,extent,2)&&!fs2_install_data(0,map.data,extent,snapshot,sizeof snapshot,1));
 boot.recovery=1;assert(fs2_volume_control(7,0,&q,&info,&result)&&!result&&info.locked);fs2_volume_forget(0);assert(fs2_disk_present(0,map.data,extent)==2&&fs2_volume_locked(0));strset(q.password,key,65);assert(fs2_volume_control(4,0,&q,&info,&result)&&!result&&!fs2_disk_verify(0,map.data,extent));
 /* Single damaged header recovers. Both damaged headers remain recognized by
  * block-zero authentication metadata and NEVER fall through to legacy reset. */
 disk[(u64)map.data*512]^=0x80;fs2_volume_forget(0);assert(fs2_disk_present(0,map.data,extent)==2);assert(fs2_volume_control(4,0,&q,&info,&result)&&!result&&!fs2_disk_verify(0,map.data,extent));
 disk[(u64)(map.data+8)*512]^=0x80;fs2_volume_forget(0);oldwrites=writes;assert(fs2_disk_present(0,map.data,extent)==2&&fs2_volume_locked(0)&&fs2_disk_verify(0,map.data,extent)<0&&fs2_disk_reset(0,map.data,extent)<0&&writes==oldwrites);
 assert(fs2_volume_control(0,0,&q,&info,&result)&&result<0&&info.enabled&&info.locked&&info.error<0);digest(after);assert(!memcmp(before,after,sizeof before));fs2_volume_forget(0);boot.recovery=0;
 puts("PASS actual kernel encrypted-volume adapter: aligned 4KiB native I/O, locked/unlocked access, smaller truthful logical capacity, private accounts, offline verify/checkpoint/restore, lock/reprobe/recovery, legacy sector bypass denied, corrupt envelope cannot fall through to format/reset; existing FAT32/NTFS/RAW hashes unchanged (QA envelope creation, not GUI acceptance)");
}
static void fresh_encrypted_install(int scheme){
 fixture(scheme);fs2_volume_forget(0);enrollment_clear();assert(install_begin_mode_i(0,1)<0&&!writes);st.state=INSTALL_IDLE;u8 before[3][32],after[3][32];digest(before);
 VaultRequest q={.disk=0};strset(q.password,"new-volume-pass",65);strset(q.replacement,"different-pass",65);
 assert(install_encryption_i(&q)<0&&!writes&&!enrollment.enabled);
 strset(q.password,"short",65);strset(q.replacement,"short",65);assert(install_encryption_i(&q)<0&&!writes&&!enrollment.enabled);
 strset(q.password,"new-volume-pass",65);strset(q.replacement,"new-volume-pass",65);assert(!install_encryption_i(&q)&&!writes&&enrollment.enabled&&!q.password[0]&&!q.replacement[0]);
 strset(q.password,q.recovery+56,65);assert(!install_encryption_ack_i(&q));assert(!install_begin_i(0)&&!install_cancel_i()&&!enrollment.enabled&&!writes);
 strset(q.password,"new-volume-pass",65);strset(q.replacement,"new-volume-pass",65);assert(!install_encryption_i(&q));char recovery[65];memcpy(recovery,q.recovery,65);
 strset(q.password,"badcheck",65);assert(install_encryption_ack_i(&q)<0&&!writes&&!enrollment.acknowledged);strset(q.password,recovery+56,65);assert(!install_encryption_ack_i(&q));
 assert(!install_begin_mode_i(0,1));guard_start=install_start;guard_end=install_end;write_guard=1;
 while(isbusy())assert(!install_step_i());assert(st.state==INSTALL_DONE&&!enrollment.enabled);for(unsigned j=0;j<sizeof enrollment;j++)assert(!((u8*)&enrollment)[j]);
 Medium m={sata_io,sata_flush,0,sectors};Layout map;assert(layout_read(&m,&map)==2);u32 extent=map.parts[1].end-map.data+1;
 assert(!memcmp(disk+(u64)map.data*512,"TMCFV003",8)&&fs2_disk_present(0,map.data,extent)==2);
 /* Reprobe loses the installer's cached key; both separately disclosed
  * credentials reopen the REAL installer-created private account and seed. */
 fs2_volume_forget(0);assert(fs2_volume_locked(0));VaultInfo info;int result;memset(&q,0,sizeof q);q.disk=0;strset(q.password,"wrong-password",65);
 assert(fs2_volume_control(4,0,&q,&info,&result)&&result<0&&info.locked);
 strset(q.password,recovery,65);assert(fs2_volume_control(4,0,&q,&info,&result)&&!result&&!info.locked);
 assert(!fs2_seed_snapshot(snapshot,sizeof snapshot)&&!fs2_install_data(0,map.data,extent,snapshot,sizeof snapshot,1));
 fs2_volume_forget(0);strset(q.password,"new-volume-pass",65);assert(fs2_volume_control(4,0,&q,&info,&result)&&!result&&!fs2_disk_verify(0,map.data,extent));
 int oldwrites=writes;char unused[65];u8 random[128];vault_random(random,128);assert(fs2_volume_create_fresh(0,map.data,extent,"another-pass",random,unused)<0&&writes==oldwrites);
 digest(after);assert(!memcmp(before,after,sizeof before));
 /* Explicit irreversible Recovery re-enrollment; legacy reset stays refused. */
 boot.recovery=1;boot.install_len=0;st.state=INSTALL_IDLE;oldwrites=writes;assert(fs2_disk_reset(0,map.data,extent)<0&&writes==oldwrites);
 strset(q.password,"replacement-pass",65);strset(q.replacement,"replacement-pass",65);assert(!install_encryption_i(&q)&&enrollment.reset&&writes==oldwrites);char newrecovery[65];memcpy(newrecovery,q.recovery,65);
 strset(q.password,newrecovery+56,65);assert(!install_encryption_ack_i(&q));assert(!install_encrypted_reset_i(0)&&!enrollment.enabled);digest(after);assert(!memcmp(before,after,sizeof before));
 assert(!fs2_disk_verify(0,map.data,extent)&&!fs2_disk_backup(0,map.data,extent,0));
 target.disk=0;target.base=map.data;target.sectors=extent;Tmc2 fs;T2Io nio={0,fs2_volume_blocks(0,map.data,extent),rd,wr,fl,al,fr};assert(!tmc2_mount(&fs,nio));u8 emptyaccount[512];assert(!tmc2_account(&fs,0,emptyaccount,0));for(unsigned j=0;j<512;j++)assert(!emptyaccount[j]);T2Entry home,docs,e;assert(!tmc2_lookup(&fs,1,"Users",&home)&&!tmc2_lookup(&fs,home.key,"User",&home)&&!tmc2_lookup(&fs,home.key,"Documents",&docs));u64 cookie=0;assert(!tmc2_readdir(&fs,docs.key,&cookie,&e));tmc2_close(&fs);
 fs2_volume_forget(0);strset(q.password,"new-volume-pass",65);assert(fs2_volume_control(4,0,&q,&info,&result)&&result<0&&info.locked);strset(q.password,recovery,65);assert(fs2_volume_control(4,0,&q,&info,&result)&&result<0&&info.locked);strset(q.password,"replacement-pass",65);assert(fs2_volume_control(4,0,&q,&info,&result)&&!result&&!info.locked);fs2_volume_forget(0);strset(q.password,newrecovery,65);assert(fs2_volume_control(4,0,&q,&info,&result)&&!result&&!fs2_disk_verify(0,map.data,extent));
 printf("PASS actual %s encrypted Recovery RESET: new volume key + password + independently pre-confirmed recovery, old credentials rejected, blank accounts/documents/catalogue, cold new recovery unlock, foreign extents unchanged; no secure-erase or interruption rollback claim\n",scheme==MAP_GPT?"GPT":"MBR");
 if(scheme==MAP_MBR){
  size_t bytes=(size_t)extent*512;u8*saved=malloc(bytes);assert(saved);memcpy(saved,disk+(u64)map.data*512,bytes);
  static const int faults[]={0,1,2,3,4,7,20,60};
  for(unsigned trial=0;trial<COUNT(faults);trial++){
   memcpy(disk+(u64)map.data*512,saved,bytes);fs2_volume_forget(0);strset(q.password,newrecovery,65);assert(fs2_volume_control(4,0,&q,&info,&result)&&!result);
   u8 entropy[128];char interrupted_key[65]={0};vault_random(entropy,128);fail_at=writes+faults[trial];int r=fs2_disk_rekey_reset(0,map.data,extent,"after-interruption-pass",entropy,interrupted_key);fail_at=-1;assert(r<0);
   fs2_volume_forget(0);assert(fs2_disk_present(0,map.data,extent)==2&&fs2_volume_locked(0));
   /* An interrupted destructive reset may retain old access, expose an
    * incomplete NEW filesystem, or refuse all access. It must never appear
    * plaintext, auto-format, or claim rollback/atomic success. */
   strset(q.password,newrecovery,65);assert(fs2_volume_control(4,0,&q,&info,&result));
   if(!result)assert(!fs2_disk_verify(0,map.data,extent));
   else{strset(q.password,"after-interruption-pass",65);assert(fs2_volume_control(4,0,&q,&info,&result));if(!result)(void)fs2_disk_verify(0,map.data,extent);}
   digest(after);assert(!memcmp(before,after,sizeof before));erase(interrupted_key,65);
  }
  memcpy(disk+(u64)map.data*512,saved,bytes);erase(saved,bytes);free(saved);puts("PASS 8 real adapter reset write interruptions: old access or incomplete/new/fail-closed encrypted state, never plaintext fallback or false success; unrelated extents unchanged");
 }
 fs2_volume_forget(0);boot.recovery=0;erase(newrecovery,65);erase(recovery,65);erase(&q,sizeof q);
 printf("PASS actual %s ENCRYPTED fresh installer: password validation/cancel with zero writes, recovery disclosed before writes, FV003 + seed/accounts/snapshot created by real state machine, cold password/recovery unlock, duplicate create refused, foreign extents unchanged\n",scheme==MAP_GPT?"GPT":"MBR");
}
/* Link only the real offline dispatch here; the mounted runtime is exercised
 * separately. Any attempt to take a mounted backup in this fixture is a bug. */
int fs2_active;int fs2_local_backup(int op){(void)op;assert(!"offline test used mounted backup");return -1;}
int power(int op){(void)op;assert(!"offline test attempted reboot");return -1;}
static void retired_offline_refusal(void){
 fixture(MAP_GPT);assert(!install_begin_i(0));guard_start=install_start;guard_end=install_end;write_guard=1;while(isbusy())assert(!install_step_i());
 Medium m={sata_io,sata_flush,0,sectors};Layout map;assert(layout_read(&m,&map)==2);u64 extent=map.parts[1].end-map.data+1;st.state=INSTALL_IDLE;boot.recovery=1;boot.install_len=0;mounted=-1;
 const char*magic[]={"TMCBFS1","TINYFS1"};
 for(unsigned k=0;k<2;k++){
  memset(disk+map.data*512,0,64*512);memcpy(disk+map.data*512,magic[k],8);fs2_volume_forget(0);u8 before[32],after[32];sha_bytes(disk,sectors*512,before);int w=writes;
  assert(!fs2_disk_present(0,map.data,extent));
  for(int op=1;op<=9;op++)assert(disk_operation_i(op,0)<0);
  assert(fs2_disk_verify(0,map.data,extent)<0&&fs2_disk_reset(0,map.data,extent)<0&&fs2_disk_backup(0,map.data,extent,1)<0&&fs2_disk_backup(0,map.data,extent,2)<0);
  VaultRequest q={.disk=0};strset(q.password,"legacy-will-not-be-converted",65);strset(q.replacement,q.password,65);assert(install_encryption_i(&q)<0&&!enrollment.enabled);
  sha_bytes(disk,sectors*512,after);assert(!memcmp(before,after,32)&&writes==w);
 }
 boot.recovery=0;puts("PASS actual Recovery/offline dispatcher rejects verify/repair/reset/backup/restore/encryption of both retired native formats; whole-disk SHA identical and zero writes");
}
#ifndef TMC_FS2_HOST_HELPERS
static void run_stale_pmbr(void){
 for(int missing=0;missing<2;missing++)for(int saturated=0;saturated<2;saturated++){
  fixture(MAP_GPT);u32 wrong=saturated?0xffffffffu:sectors-2;memcpy(disk+458,&wrong,4);if(missing)disk[510]=disk[511]=0;
  refresh();assert(st.disk[0].usable&&(st.disk[0].repair_mbr_signature&2)&&!writes);
  assert(!install_begin_i(0)&&!writes);assert(!install_cancel_i()&&!writes);assert(!memcmp(disk+458,&wrong,4));
  u8 before[3][32],after[3][32];digest(before);assert(!install_begin_i(0));guard_start=install_start;guard_end=install_end;write_guard=1;
  while(isbusy())assert(!install_step_i());assert(st.state==INSTALL_DONE);u32 length;memcpy(&length,disk+458,4);assert(length==sectors-1&&disk[510]==85&&disk[511]==170);
  digest(after);assert(!memcmp(before,after,sizeof before));assert(!memcmp(disk+1024,oldparts,3*128));assert(!memcmp(disk+(u64)(guard_start+TC_OLD_MBR)*512,oldmbr,512));
  assert(!memcmp(disk+440,oldmbr+440,18)&&!memcmp(disk+462,oldmbr+462,48));
  Medium m={sata_io,sata_flush,0,sectors};static DiskMap map;assert(partition_scan(&m,&map)&&map.safe&&!map.pmbr_signature_repairable);
 }
 fixture(MAP_GPT);u32 wrong=sectors-2;memcpy(disk+458,&wrong,4);disk[(u64)(sectors-1)*512+16]^=1;assert(install_begin_i(0)<0&&!writes);
 puts("PASS actual stale/saturated PMBR installer: cancel writes nothing, consent repairs coverage (and missing signature), preserves all occupied extents/existing GPT entries/disk ID, saves exact original MBR; degraded GPT refuses");
}
static void run_missing_signature(void){
 fixture(MAP_GPT);disk[510]=disk[511]=0;refresh();assert(st.disk[0].usable&&st.disk[0].repair_mbr_signature&&!writes);
 assert(!install_begin_i(0)&&!writes);assert(!install_cancel_i()&&!writes&&disk[510]==0&&disk[511]==0);
 fixture(MAP_GPT);disk[510]=disk[511]=0;u8 before[3][32],after[3][32];digest(before);assert(!install_begin_i(0));
 guard_start=install_start;guard_end=install_end;write_guard=1;while(isbusy())assert(!install_step_i());
 assert(st.state==INSTALL_DONE&&disk[510]==85&&disk[511]==170);digest(after);assert(!memcmp(before,after,sizeof before));
 assert(!memcmp(disk+1024,oldparts,3*128));assert(!memcmp(disk+(u64)(guard_start+TC_OLD_MBR)*512,oldmbr,512));
 Medium m={sata_io,sata_flush,0,sectors};static DiskMap map;assert(partition_scan(&m,&map)&&map.safe&&!map.pmbr_signature_repairable);
 fixture(MAP_GPT);disk[510]=disk[511]=0;disk[(u64)(sectors-1)*512+16]^=1;assert(install_begin_i(0)<0&&!writes);
 puts("PASS actual FS2 missing-55AA installer: preflight/cancel zero writes, confirmed install restores only signature with planned boot/table activation, existing extents/entries unchanged, old MBR saved, degraded peer refused");
}
static void high_fs2_adapter(void){
 u64 original=sectors;sectors=3ull*(1ull<<31);sparse_high=sectors-524288;sparse_window=524288;
 disk=calloc(sparse_window+64,512);assert(disk);mounted=-1;write_guard=0;fail_at=-1;writes=0;
 assert(!fs2_seed_snapshot(snapshot,sizeof snapshot));
 assert(!fs2_install_data(0,sparse_high+2048,sparse_window-4096,snapshot,sizeof snapshot,0));
 assert(!fs2_install_data(0,sparse_high+2048,sparse_window-4096,snapshot,sizeof snapshot,1));
 fs2_volume_forget(0);
 static u8 sent[T2_BLOCK],got[T2_BLOCK];for(unsigned i=0;i<sizeof sent;i++)sent[i]=(i*37+9)%251;
 u64 block=(sparse_high-2048)/32;assert(block*32>0xffffffffull);
 assert(!fs2_volume_write(0,2048,sectors-4096,block,sent));
 assert(!fs2_volume_read(0,2048,sectors-4096,block,got)&&!memcmp(sent,got,sizeof sent));
 fs2_volume_forget(0);free(disk);disk=0;sparse_high=sparse_window=0;sectors=original;
 puts("PASS actual FS2 format/import/verify at >2TiB base and 16KiB write/read at >32-bit relative sector offset on a 3TiB device; no LBA narrowing");
}
int main(void){
 high_fs2_adapter();
 disk=calloc(sectors,512);assert(disk);run_stale_pmbr();run_missing_signature();run_fs2(MAP_GPT);run_fs2(MAP_MBR);encrypted_adapter();fresh_encrypted_install(MAP_GPT);fresh_encrypted_install(MAP_MBR);failure_tests(MAP_GPT);failure_tests(MAP_MBR);scanner_tests();retired_offline_refusal();
 fixture(MAP_GPT);invalid_source=1;assert(!install_begin_i(0));while(isbusy())install_step_i();assert(st.state==INSTALL_ERROR&&!writes);invalid_source=0;
 fixture(MAP_GPT);assert(!install_begin_i(0));while(st.state==INSTALL_SCAN)assert(!install_step_i());fail_at=writes+3;while(isbusy())install_step_i();assert(st.state==INSTALL_ERROR&&!memcmp(disk+1024,oldparts,3*128));
 free(disk);puts("PASS invalid source refuses before any target write; injected target failure never activates the new volume");return 0;
}

#endif
