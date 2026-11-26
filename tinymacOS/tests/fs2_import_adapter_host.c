/* Actual kernel volume and offline import adapters on distinct RAM devices. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../fs/tmcbfs2_install.c"
#include "vault2.h"
#include "disklayout.h"
struct Boot boot;
static u8*media[2];static int mounted_disk=-1;static u64 writes[2];
#define SECTORS 36864u
#define BASE 2048u
#define EXTENT 32768u
int sata_count(void){return 2;}
u32 sata_size(int d){assert(d>=0&&d<2);return SECTORS;}
int volume_current(void){return mounted_disk;}
int sata_io(int d,u32 l,u32 n,void*p,int write){assert(d>=0&&d<2&&l<SECTORS&&n<=SECTORS-l);if(write){writes[d]++;memcpy(media[d]+(u64)l*512,p,n*512);}else memcpy(p,media[d]+(u64)l*512,n*512);return 0;}
int sata_flush(int d){assert(d>=0&&d<2);return 0;}
void*kmalloc(size_t n){return malloc(n);}
void kfree(void*p){free(p);}
int kmalloc_owns(const void*p){(void)p;return 1;}
u64 page_get(void){return (u64)malloc(4096);}
void page_put(u64 p){free((void*)p);}
int vault_random(u8*p,size_t n){static unsigned g;g++;for(size_t j=0;j<n;j++)p[j]=(j*19+g*37)%251+1;return 1;}
/* open_volume already has exact extents cached in these tests. */
int layout_read(Medium*m,Layout*l){(void)m;(void)l;return 0;}
static T2Io fsio(ImportMedium*m){return (T2Io){m,fs2_volume_blocks(m->disk,m->base,m->sectors),import_read,import_write,import_flush,al,fr};}
static void run(int encrypted_source,int encrypted_dest){
 for(unsigned d=0;d<2;d++){memset(media[d],0,SECTORS*512);memset(media[d],0x7c,BASE*512);memset(media[d]+(BASE+EXTENT)*512,0x6b,(SECTORS-BASE-EXTENT)*512);fs2_volume_forget(d);}
 ImportMedium sm={0,BASE,EXTENT},dm={1,BASE,EXTENT};char recovery[2][65];u8 random[128];
 for(int d=0;d<2;d++)if(d?encrypted_dest:encrypted_source){vault_random(random,128);assert(!fs2_volume_create_fresh(d,BASE,EXTENT,"adapter-test-password",random,recovery[d]));}
 Tmc2 s,t;assert(!tmc2_format(&s,fsio(&sm))&&!tmc2_format(&t,fsio(&dm)));u64 id;assert(!tmc2_create(&s,1,"source-file",T2_FILE,&id));static u8 a[131072],b[131072];for(unsigned j=0;j<sizeof a;j++)a[j]=(j*23+51)%251;assert(tmc2_write(&s,id,0,a,sizeof a)==sizeof a);assert(!tmc2_account(&s,0,a,1)&&!tmc2_snapshot(&s,"External checkpoint",0));assert(!tmc2_create(&t,1,"destination-old",T2_FILE,&id)&&!tmc2_snapshot(&t,"Destination preserved",0));tmc2_close(&s);tmc2_close(&t);
 u64 sw=writes[0],dw=writes[1],needed=0;boot.recovery=0;assert(fs2_external_import(0,BASE,EXTENT,1,BASE,EXTENT,"External checkpoint",1,&needed)<0&&writes[1]==dw);boot.recovery=1;
 assert(fs2_external_import(0,BASE,EXTENT,0,BASE,EXTENT,"External checkpoint",1,&needed)<0);mounted_disk=1;assert(fs2_external_import(0,BASE,EXTENT,1,BASE,EXTENT,"External checkpoint",1,&needed)<0);mounted_disk=-1;
 assert(!fs2_external_import(0,BASE,EXTENT,1,BASE,EXTENT,"External checkpoint",0,&needed)&&needed>8&&writes[0]==sw&&writes[1]==dw);
 assert(!fs2_external_import(0,BASE,EXTENT,1,BASE,EXTENT,"External checkpoint",1,&needed)&&writes[0]==sw);assert(!tmc2_mount(&t,fsio(&dm)));T2Entry e;assert(!tmc2_lookup(&t,1,"source-file",&e)&&e.size==sizeof a&&tmc2_read(&t,e.key,0,b,sizeof b)==sizeof b&&!memcmp(a,b,sizeof a));assert(!tmc2_account(&t,0,b,0)&&!memcmp(a,b,512)&&!tmc2_snapshot_verify(&t,"Destination preserved"));tmc2_close(&t);
 for(unsigned d=0;d<2;d++){for(unsigned j=0;j<BASE*512;j++)assert(media[d][j]==0x7c);for(unsigned j=(BASE+EXTENT)*512;j<SECTORS*512;j++)assert(media[d][j]==0x6b);}
 /* Encrypted source requires its independent unlock after a cold reprobe. */
 if(encrypted_source){fs2_volume_forget(0);dw=writes[1];assert(fs2_external_import(0,BASE,EXTENT,1,BASE,EXTENT,"External checkpoint",1,&needed)<0&&writes[1]==dw);VaultRequest q={.disk=0};VaultInfo info;int result;strcpy(q.password,recovery[0]);assert(fs2_volume_control(4,0,&q,&info,&result)&&!result);assert(!fs2_external_import(0,BASE,EXTENT,1,BASE,EXTENT,"External checkpoint",0,&needed));}
 if(encrypted_dest){fs2_volume_forget(1);assert(fs2_volume_encrypted(1,BASE,EXTENT)==1&&fs2_volume_locked(1));VaultRequest q={.disk=1};VaultInfo info;int result;strcpy(q.password,recovery[1]);assert(fs2_volume_control(4,1,&q,&info,&result)&&!result);assert(!tmc2_mount(&t,fsio(&dm))&&!tmc2_lookup(&t,1,"source-file",&e));tmc2_close(&t);}
 printf("PASS actual external import adapters source=%s destination=%s: explicit Recovery/offline/distinct disks, read-only preview, 128KiB+account, preserved target checkpoint, no source writes, unchanged outside data extent, independent cold unlocks\n",encrypted_source?"FV003":"plain",encrypted_dest?"FV003":"plain");
 for(int d=0;d<2;d++)fs2_volume_forget(d);
}
int main(void){for(unsigned d=0;d<2;d++){media[d]=malloc(SECTORS*512);assert(media[d]);}for(int s=0;s<2;s++)for(int d=0;d<2;d++)run(s,d);for(unsigned d=0;d<2;d++)free(media[d]);return 0;}

u32 sata_logical_bytes(int d){(void)d;return 512;}
