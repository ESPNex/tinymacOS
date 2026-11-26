/* Actual installer and kernel conversion adapter; only block transport is RAM.
 * Separate real NVMe/UEFI guest acceptance is mandatory for native 4Kn boot. */
#define TMC_FS2_HOST_HELPERS 1
#include "fs2_install_host.c"
u32 sata_logical_bytes(int d){(void)d;return 4096;}
int main(void){
 sectors=262144;disk=calloc(sectors,512);assert(disk);boot=(struct Boot){.install_esp=(u64)payload,.install_len=sizeof payload,.recovery_img=(u64)recovery,.recovery_len=sizeof recovery,.install_4k=(u64)payload,.recovery_4k=(u64)recovery};
 boot.recovery=0;mounted=-1;memset(&st,0,sizeof st);fs2_volume_forget(0);assert(!install_begin_i(0));while(isbusy())assert(!install_step_i());assert(st.state==INSTALL_DONE&&install_scale==8);
 Gpt*h=(void*)(disk+4096);assert(!memcmp(h->sig,"EFI PART",8)&&h->at==1&&h->other==sectors/8-1&&h->entries==2&&h->first==6);assert(!memcmp(disk+512,(u8[512]){0},512));
 Part*p=(void*)(disk+8192);assert(p[0].start==LESP/8&&p[1].start==LDATA/8);u32 pmbrsize;memcpy(&pmbrsize,disk+458,4);assert(pmbrsize==sectors/8-1);
 Medium medium={sata_io,sata_flush,0,sectors,sata_io,4096};DiskMap inv;Layout map;assert(partition_scan(&medium,&inv)&&inv.safe&&inv.count==4);assert(inv.part[0].start==LESP);assert(layout_read(&medium,&map)==2&&map.pv&&map.bv&&map.rec%8==0&&map.source%8==0);assert(!verify_system_volume(&medium));
 assert(!fs2_seed_snapshot(snapshot,sizeof snapshot));u32 extent=map.parts[1].end-map.data+1;assert(!fs2_disk_verify(0,map.data,extent));
 puts("PASS actual 4Kn fresh installer: GPT headers at physical 4096-byte LBAs, native entry/PMBR units, mirrored arrays, aligned full data extent, decoded inventory and sealed-system verification");
 u8 outside[2][32],after[2][32];sha_bytes(disk,(u64)map.data*512,outside[0]);sha_bytes(disk+(u64)(map.data+extent)*512,(u64)(sectors-map.data-extent)*512,outside[1]);
 boot.recovery=1;boot.install_len=0;VaultRequest q={.disk=0};VaultInfo v;int r;strset(q.password,"convert-volume-password",65);strset(q.replacement,q.password,65);
 assert(fs2_volume_control(1,0,&q,&v,&r)&&!r&&v.format==4);char key[65];memcpy(key,q.recovery,65);int previous=writes;assert(fs2_volume_control(3,0,&q,&v,&r)&&r<0&&writes==previous);
 strset(q.password,key+56,65);assert(fs2_volume_control(9,0,&q,&v,&r)&&!r);assert(fs2_volume_control(3,0,&q,&v,&r)&&!r);
 fs2_volume_forget(0);assert(fs2_volume_locked(0));strset(q.password,key,65);assert(fs2_volume_control(4,0,&q,&v,&r)&&!r);
 while(v.format==4){assert(fs2_volume_control(3,0,&q,&v,&r)&&!r);}assert(v.format==3&&v.enabled);
 fs2_volume_forget(0);strset(q.password,key,65);assert(fs2_volume_control(4,0,&q,&v,&r)&&!r);assert(!fs2_disk_verify(0,map.data,extent));assert(!fs2_install_data(0,map.data,extent,snapshot,sizeof snapshot,1));
 strset(q.password,"convert-volume-password",65);assert(fs2_volume_control(2,0,&q,&v,&r)&&!r&&v.format==4);while(v.format==4)assert(fs2_volume_control(3,0,&q,&v,&r)&&!r);assert(v.format==2&&!v.enabled);assert(!fs2_disk_verify(0,map.data,extent)&&!fs2_install_data(0,map.data,extent,snapshot,sizeof snapshot,1));
 sha_bytes(disk,(u64)map.data*512,after[0]);sha_bytes(disk+(u64)(map.data+extent)*512,(u64)(sectors-map.data-extent)*512,after[1]);assert(!memcmp(outside,after,sizeof outside));
 puts("PASS production Recovery adapter: real same-extent conversion/deactivation on 4Kn layout, kernel recovery-key acknowledgement before steps, restart/resume authentication, imported account/file/snapshot preserved and every public extent byte-identical");free(disk);return 0;
}
