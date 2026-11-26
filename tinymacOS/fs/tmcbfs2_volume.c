/* SPDX-License-Identifier: MIT
 * Raw 512-sector kernel transport -> aligned 4KiB authenticated envelope ->
 * 16KiB TMCBFS2 blocks. Fresh creation is installer-only, destructive rekey is
 * offline Recovery-only; mounting an unknown/corrupt volume never initializes.
 */
#include "../kernel/k.h"
#include "fs2_volume.h"
#include "vault2.h"
#include "fs2_convert.h"
#include "disklayout.h"
typedef struct {Vault2 v;Fs2Convert cv;int initialized,encrypted,disk,acknowledged;char acknowledge[9];u32 base,sectors;} Volume;
static Volume volumes[SETUP_MAX_DISKS];
static int read_unit(void*x,u64 n,void*p){Volume*v=x;return n>=v->sectors/8?-1:sata_io(v->disk,v->base+(u32)(n*8),8,p,0);}
static int write_unit(void*x,u64 n,const void*p){Volume*v=x;return n>=v->sectors/8?-1:sata_io(v->disk,v->base+(u32)(n*8),8,(void*)p,1);}
static int flush_unit(void*x){return sata_flush(((Volume*)x)->disk);}
static void*allocate(void*x,size_t n){(void)x;return n==4096?(void*)page_get():kmalloc(n);}
static void deallocate(void*x,void*p){(void)x;if(kmalloc_owns(p))kfree(p);else page_put((u64)p);}
static Volume*open_volume(int disk,u32 base,u32 sectors){
 if(disk<0||disk>=sata_count()||disk>=SETUP_MAX_DISKS||base>=sata_size(disk)||sectors>sata_size(disk)-base)return 0;
 Volume*v=volumes+disk;
 if(v->initialized&&v->base==base&&v->sectors==sectors)return v;
 vault2_lock(&v->v);fs2_convert_lock(&v->cv);erase(v,sizeof *v);v->disk=disk;v->base=base;v->sectors=sectors;
 /* Record zero's public authentication metadata also identifies an envelope
  * if BOTH redundant headers were damaged. Refuse it rather than falling back
  * to any legacy formatter or treating it as an unprotected volume. */
 static const u32 offsets[]={0,8,(FV2_START+4)*8};u8 h[512];
 for(unsigned i=0;i<COUNT(offsets);i++){
  if(offsets[i]>=sectors)continue;
  if(sata_io(disk,base+offsets[i],1,h,0))return 0;
  if(!memcmp(h,i==2?"FV2BLK03":"TMCFV003",8))v->encrypted=1;
 }
 v->initialized=1;
 Fv2Io ci={v,sectors/8,read_unit,write_unit,flush_unit,allocate,deallocate};
 int converting=fs2_convert_probe(&v->cv,ci);
 if(converting!=0){v->encrypted=1;return v;}
 if(v->encrypted){
  Fv2Io io={v,sectors/8,read_unit,write_unit,flush_unit,allocate,deallocate};
  if(base%8||vault2_probe(&v->v,io)!=1){v->v.present=v->v.poison=1;}
 }
 return v;
}
int fs2_volume_encrypted(int d,u32 base,u32 sectors){Volume*v=open_volume(d,base,sectors);return v?v->encrypted:-1;}
u64 fs2_volume_blocks(int d,u32 base,u32 sectors){Volume*v=open_volume(d,base,sectors);return !v||v->cv.present?0:!v->encrypted?sectors/32:v->v.poison?0:v->v.h.blocks;}
int fs2_volume_read(int d,u32 base,u32 sectors,u64 block,void*p){
 Volume*v=open_volume(d,base,sectors);if(!v||!p||v->cv.present)return -1;
 if(v->encrypted)return vault2_read(&v->v,block,p);
 return block>=sectors/32?-1:sata_io(d,base+(u32)(block*32),32,p,0);
}
int fs2_volume_write(int d,u32 base,u32 sectors,u64 block,const void*p){
 Volume*v=open_volume(d,base,sectors);if(!v||!p||v->cv.present)return -1;
 if(v->encrypted)return vault2_write(&v->v,block,p);
 return block>=sectors/32?-1:sata_io(d,base+(u32)(block*32),32,(void*)p,1);
}
static Volume*find_volume(int disk){
 if(disk<0||disk>=sata_count()||disk>=SETUP_MAX_DISKS)return 0;
 if(volumes[disk].initialized)return volumes+disk;
 static Layout map;Medium m={sata_io,sata_flush,disk,sata_size(disk),sata_io,sata_logical_bytes(disk)};if(layout_read(&m,&map)!=2)return 0;
 Volume*v=open_volume(disk,map.data,map.parts[1].end-map.data+1);return v;
}
int fs2_volume_locked(int disk){Volume*v=find_volume(disk);return v&&(v->cv.present?1:v->encrypted&&(!v->v.unlocked||v->v.poison));}
int fs2_volume_sector_guard(int disk,u32 l,u32 n){
 Volume*v=find_volume(disk);if(!v||!n||(!v->encrypted&&!v->cv.present))return 0;
 /* Legacy sector-addressed reads/writes must NEVER bypass the envelope,
  * whether it is locked or unlocked. GPT/system/Recovery remain public. */
 return (u64)l<v->base+(u64)v->sectors&&(u64)l+n>v->base;
}
int fs2_volume_control(int op,int disk,VaultRequest*q,VaultInfo*out,int*result){
 Volume*v=find_volume(disk);
 if(!v)return 0;
 int r=FV_STATE;
 if(v->cv.present){
  if(op==0)r=v->cv.poison?FV_CORRUPT:0;
  else if(op==4&&boot.recovery){if(v->cv.poison){Fv2Io io=v->cv.io;fs2_convert_probe(&v->cv,io);}r=fs2_convert_unlock(&v->cv,q->password);if(!r)v->acknowledged=1;}
  else if(op==9&&boot.recovery&&v->cv.unlocked&&!memcmp(q->password,v->acknowledge,9)){v->acknowledged=1;erase(v->acknowledge,9);r=0;}
  else if(op==3&&boot.recovery&&v->acknowledged){r=fs2_convert_step(&v->cv,8);if(r==1){fs2_volume_forget(disk);int ignored;fs2_volume_control(0,disk,q,out,&ignored);*result=0;return 1;}}
  *out=(VaultInfo){.available=1,.enabled=1,.format=4,.locked=!v->cv.unlocked,.state=v->cv.h.mode==CV_DECRYPT?FV_DECRYPTING:FV_ENCRYPTING,.progress=v->cv.h.count?(u32)(v->cv.h.progress*100/v->cv.h.count):0,.error=r<0?r:0};
  *result=r;return 1;
 }
 if(!v->encrypted){
  u8 header[512];int known=0;
  for(unsigned i=0;i<2;i++)if(v->sectors>=64&&!sata_io(disk,v->base+i*32,1,header,0)&&!memcmp(header,"TMCBFS2",8))known=1;
  if(!known)return 0;
  if(op==0)r=0;
  if(op==1&&boot.recovery&&disk!=volume_current()){
   if(!eq(q->password,q->replacement))r=FV_AUTH;
   else{u8 random[128]={0};if(!vault_random(random,128))r=FV_ENTROPY;
    else{Fv2Io io={v,v->sectors/8,read_unit,write_unit,flush_unit,allocate,deallocate};r=fs2_convert_begin(&v->cv,io,CV_ENCRYPT,q->password,random,q->recovery);if(!r){memcpy(v->acknowledge,q->recovery+56,9);v->acknowledged=0;}}
    erase(random,sizeof random);
   }
   if(!r){int ignored;fs2_volume_control(0,disk,q,out,&ignored);*result=0;return 1;}
  }
  *out=(VaultInfo){.available=boot.recovery&&disk!=volume_current(),.format=2,.error=r<0?r:0};*result=r;return 1;
 }
 if(op==2&&boot.recovery&&disk!=volume_current()){
  Fv2Io io={v,v->sectors/8,read_unit,write_unit,flush_unit,allocate,deallocate};r=fs2_convert_begin(&v->cv,io,CV_DECRYPT,q->password,0,q->recovery);
  if(!r){vault2_lock(&v->v);v->acknowledged=1;int ignored;fs2_volume_control(0,disk,q,out,&ignored);*result=0;return 1;}
  *out=(VaultInfo){.available=1,.enabled=1,.format=3,.state=FV_ON,.error=r};*result=r;return 1;
 }
 u8 random[72]={0};
 if(op==0||op==3)r=v->v.poison?FV_CORRUPT:0;
 else if(op==4){if(v->v.poison){Fv2Io io=v->v.io;vault2_probe(&v->v,io);}r=vault2_unlock(&v->v,q->password);}
 else if(op==5){if(!vault_random(random,40))r=FV_ENTROPY;else r=vault2_password(&v->v,q->password,q->replacement,random);}
 else if(op==6){if(!vault_random(random,72))r=FV_ENTROPY;else r=vault2_recovery(&v->v,q->password,random,q->recovery);}
 else if(op==7&&boot.recovery){vault2_lock(&v->v);r=0;}
 erase(random,sizeof random);
 *out=(VaultInfo){.available=1,.enabled=1,.format=3,.locked=!v->v.unlocked||v->v.poison,.state=FV_ON,.progress=100,.error=v->v.poison?FV_CORRUPT:r<0?r:0,.protected_sectors=(u32)(v->v.h.blocks*32)};
 *result=r;return 1;
}
void fs2_volume_forget(int disk){if(disk>=0&&disk<SETUP_MAX_DISKS){vault2_lock(&volumes[disk].v);fs2_convert_lock(&volumes[disk].cv);erase(volumes+disk,sizeof(Volume));}}

/* Never called by mount, reset or Settings conversion. The installer owns a
 * new partition extent not yet published in its destination partition table. */
int fs2_volume_create_fresh(int disk,u32 base,u32 sectors,const char*pass,const u8 random[128],char recovery[65]){
 if(disk==volume_current()||base%8)return FV_STATE;
 Volume*v=open_volume(disk,base,sectors);if(!v||v->encrypted)return FV_STATE;
 Fv2Io io={v,sectors/8,read_unit,write_unit,flush_unit,allocate,deallocate};
 if(vault2_probe(&v->v,io)!=0)return FV_STATE;
 int r=vault2_create(&v->v,pass,random,recovery);
 /* Even a failed create may have written a partial envelope. Never expose
  * the same cached extent as plaintext after an ambiguous target write. */
 if(r){fs2_volume_forget(disk);return r;}
 v->encrypted=1;return 0;
}

/* Destructive Recovery re-enrollment. Success does not promise physical flash
 * sanitization or rollback protection against an externally retained image.
 * A failure after key-header retirement may destroy access: never report it as
 * an atomic old/new reset. Public partitions are not touched. */
int fs2_volume_rekey_fresh(int disk,u32 base,u32 sectors,const char*pass,const u8 random[128],char recovery[65]){
 Volume*v=open_volume(disk,base,sectors);
 if(!boot.recovery||disk==volume_current()||!v||!v->encrypted||!v->v.unlocked||v->v.poison||!pass||strlen(pass)<8||strlen(pass)>64||!random||!recovery)return FV_STATE;
 unsigned nonzero[3]={0};for(unsigned i=0;i<32;i++){nonzero[0]|=random[i];nonzero[2]|=random[48+i];if(i<16)nonzero[1]|=random[32+i];}
 if(!nonzero[0]||!nonzero[1]||!nonzero[2]||!memcmp(random,v->v.key,32)||!memcmp(random+32,v->v.h.uuid,16))return FV_ENTROPY;
 u8*p=allocate(v,FV2_UNIT);if(!p)return FV_IO;memset(p,0,FV2_UNIT);memcpy(p,"TMCFV003",8);int r=0;
 /* Invalid recognizable envelopes are durable in BOTH slots before blanking.
  * After this point failure is deliberately fail-closed, not legacy fallback. */
 for(unsigned i=0;i<2&&!r;i++)r=write_unit(v,i,p)||flush_unit(v)?FV_IO:0;
 memset(p,0,FV2_UNIT);for(unsigned i=0;i<2&&!r;i++)r=write_unit(v,i,p)||flush_unit(v)?FV_IO:0;
 erase(p,FV2_UNIT);deallocate(v,p);Fv2Io io=v->v.io;vault2_lock(&v->v);
 if(!r&&vault2_probe(&v->v,io)!=0)r=FV_CORRUPT;
 if(!r)r=vault2_create(&v->v,pass,random,recovery);
 if(r){v->v.present=v->v.poison=1;vault2_lock(&v->v);}return r;
}
