/* SPDX-License-Identifier: MIT
 * Explicit fresh-target formatting, called only after installer preflight and
 * before activation of the destination GPT/MBR. Never a mount-time fallback.
 */
#include "../kernel/k.h"
#include "tmcbfs2_seed.h"
#include "fs2_volume.h"
static struct {int disk;u64 base,sectors;} target;
static int rd(void*x,u64 b,void*p){(void)x;return fs2_volume_read(target.disk,target.base,target.sectors,b,p);}
static int wr(void*x,u64 b,const void*p){(void)x;return fs2_volume_write(target.disk,target.base,target.sectors,b,p);}
static int fl(void*x){(void)x;return sata_flush(target.disk);}
static void*al(void*x,size_t n){(void)x;return n==4096?(void*)page_get():kmalloc(n);}
static void fr(void*x,void*p){(void)x;if(!p)return;if(kmalloc_owns(p))kfree(p);else page_put((u64)p);}
static int install_seed(int disk,u64 base,u64 sectors,const void*image,size_t bytes,int verify,int fresh){
 if(disk<0||disk>=sata_count()||base>=sata_size(disk)||sectors>sata_size(disk)-base||sectors<2048||tmc2_seed_validate(image,bytes))return -1;
 if(!verify&&!fresh&&fs2_volume_encrypted(disk,base,sectors)!=0)return -1;
 target.disk=disk;target.base=base;target.sectors=sectors;
 T2Io io={0,fs2_volume_blocks(disk,base,sectors),rd,wr,fl,al,fr};Tmc2 fs;int r;
 if(verify){if(tmc2_mount(&fs,io))return -1;r=tmc2_seed_verify(&fs,image,bytes)||tmc2_snapshot_verify(&fs,"Installazione");}
 else{
  if(tmc2_format(&fs,io))return -1;
  r=tmc2_seed_import(&fs,image,bytes)||tmc2_seed_verify(&fs,image,bytes)||tmc2_snapshot(&fs,"Installazione",0)||tmc2_snapshot_verify(&fs,"Installazione");
 }
 tmc2_close(&fs);return r?-1:0;
}

int fs2_install_data(int d,u64 b,u64 n,const void*image,size_t bytes,int verify){
 return install_seed(d,b,n,image,bytes,verify,0);
}
int fs2_install_encrypted(int d,u64 b,u64 n,const void*image,size_t bytes,const char*pass,const u8 random[128],char recovery[65]){
 if(tmc2_seed_validate(image,bytes)||fs2_volume_create_fresh(d,b,n,pass,random,recovery))return -1;
 return install_seed(d,b,n,image,bytes,0,1);
}

/* Read-only offline validation. A different mount is never used for writes to
 * a currently mounted filesystem: its in-memory allocation maps would stale. */
int fs2_disk_verify(int disk,u64 base,u64 sectors){
 if(disk<0||disk>=sata_count()||base>=sata_size(disk)||sectors>sata_size(disk)-base)return -1;
 target.disk=disk;target.base=base;target.sectors=sectors;
 Tmc2 fs;T2Io io={0,fs2_volume_blocks(disk,base,sectors),rd,wr,fl,al,fr};if(tmc2_mount(&fs,io))return -1;
 int r=tmc2_verify(&fs);tmc2_close(&fs);return r;
}
int fs2_disk_present(int disk,u64 base,u64 sectors){
 if(sectors<64)return 0;int encrypted=fs2_volume_encrypted(disk,base,sectors);if(encrypted)return encrypted<0?-1:2;u8 h[512];int failed=0;
 for(unsigned slot=0;slot<2;slot++){
  if(sata_io(disk,base+slot*32,1,h,0)){failed=1;continue;}
  if(!memcmp(h,"TMCBFS2",8))return 1;
 }
 return failed?-1:0;
}

static const T2Seed reset_seed[]={
  {0,T2_DIR,"Users"},{1,T2_DIR,"User"},{2,T2_DIR,"Desktop"},{2,T2_DIR,"Documents"},
  {2,T2_DIR,"Downloads"},{2,T2_DIR,"Pictures"},{0,T2_DIR,"Volumes"},
  {0,T2_DIR,"Library"},{8,T2_DIR,"Application Support"},{8,T2_DIR,"Preferences"},
  {2,T2_DIR,"Library"},{11,T2_DIR,"Preferences"},{11,T2_DIR,"Application Support"},
  {12,T2_FILE,"com.tinymacos.appearance.plist"},{10,T2_FILE,"com.tinymacos.system.plist"}
 };

/* Explicit Recovery factory reset. The core publishes the entire new home,
 * blank account and empty snapshot catalogue together. This is logical removal,
 * NOT sanitization of old plaintext on the physical device. */
int fs2_disk_reset(int disk,u64 base,u64 sectors){
 /* Reset/enrollment/key-erasure policy for encrypted homes is not exposed yet.
  * Do not strand a blank account behind the previous owner's wrapping key. */
 if(fs2_volume_encrypted(disk,base,sectors)!=0)return -1;
 if(disk<0||disk>=sata_count()||disk==volume_current()||base>=sata_size(disk)||sectors>sata_size(disk)-base)return -1;

 target.disk=disk;target.base=base;target.sectors=sectors;
 Tmc2 fs;T2Io io={0,fs2_volume_blocks(disk,base,sectors),rd,wr,fl,al,fr};if(tmc2_mount(&fs,io))return -1;
 int r=tmc2_verify(&fs)||tmc2_reset(&fs,reset_seed,COUNT(reset_seed))||tmc2_collect(&fs)||tmc2_verify(&fs);
 tmc2_close(&fs);return r?-1:0;
}

/* fs_gate serializes callers. Never mutate an offline mount while the runtime
 * has allocation maps open for that volume. Atomic replacement retains the
 * previous checkpoint until the new catalogue root is durably published. */
int fs2_disk_backup(int disk,u64 base,u64 sectors,int op){
 if(disk<0||disk>=sata_count()||base>=sata_size(disk)||sectors>sata_size(disk)-base||op<0||op>2||(op&&disk==volume_current()))return -1;
 target.disk=disk;target.base=base;target.sectors=sectors;Tmc2 fs;T2Io io={0,fs2_volume_blocks(disk,base,sectors),rd,wr,fl,al,fr};if(tmc2_mount(&fs,io))return -1;
 int r;T2Entry e;
 if(!op)r=!tmc2_snapshot_find(&fs,"Copia locale",&e);
 else if(op==1)r=tmc2_verify(&fs)||tmc2_checkpoint(&fs,"Copia locale")?-1:0;
 else r=tmc2_snapshot_verify(&fs,"Copia locale")||tmc2_snapshot_restore(&fs,"Copia locale")?-1:0;
 tmc2_close(&fs);return r;
}

int fs2_disk_rekey_reset(int disk,u64 base,u64 sectors,const char*pass,const u8 random[128],char recovery[65]){
 if(!boot.recovery||disk==volume_current()||fs2_volume_encrypted(disk,base,sectors)!=1||fs2_volume_locked(disk)||fs2_disk_verify(disk,base,sectors))return -1;
 if(fs2_volume_rekey_fresh(disk,base,sectors,pass,random,recovery))return -1;
 target.disk=disk;target.base=base;target.sectors=sectors;Tmc2 fs;
 T2Io io={0,fs2_volume_blocks(disk,base,sectors),rd,wr,fl,al,fr};
 if(tmc2_format(&fs,io))return -1;
 int r=tmc2_reset(&fs,reset_seed,COUNT(reset_seed))||tmc2_collect(&fs)||tmc2_verify(&fs);tmc2_close(&fs);return r?-1:0;
}

/* Independent source/destination contexts are mandatory: the older installer
 * adapter above intentionally owns a single target. Both remain unmounted by
 * the runtime and serialized by fs_gate for the entire check/copy/publication. */
typedef struct {int disk;u64 base,sectors;} ImportMedium;
static int import_read(void*x,u64 b,void*p){ImportMedium*m=x;return fs2_volume_read(m->disk,m->base,m->sectors,b,p);}
static int import_write(void*x,u64 b,const void*p){ImportMedium*m=x;return fs2_volume_write(m->disk,m->base,m->sectors,b,p);}
static int import_flush(void*x){return sata_flush(((ImportMedium*)x)->disk);}
static int source_no_write(void*x,u64 b,const void*p){(void)x;(void)b;(void)p;return -1;}
static int source_no_flush(void*x){(void)x;return -1;}
int fs2_external_import(int sd,u64 sb,u64 sn,int dd,u64 db,u64 dn,const char*name,int commit,u64*blocks){
 if(!boot.recovery||sd<0||dd<0||sd==dd||sd==volume_current()||dd==volume_current()||sd>=sata_count()||dd>=sata_count()||(commit!=0&&commit!=1))return -1;
 ImportMedium src={sd,sb,sn},dst={dd,db,dn};Tmc2 s,d;
 T2Io si={&src,fs2_volume_blocks(sd,sb,sn),import_read,source_no_write,source_no_flush,al,fr};
 T2Io di={&dst,fs2_volume_blocks(dd,db,dn),import_read,import_write,import_flush,al,fr};
 if(tmc2_mount(&s,si))return -1;
 if(tmc2_mount(&d,di)){tmc2_close(&s);return -1;}
 int r=tmc2_import_check(&d,&s,name,blocks);
 if(!r&&commit)r=tmc2_import_snapshot(&d,&s,name);
 tmc2_close(&d);tmc2_close(&s);return r;
}
