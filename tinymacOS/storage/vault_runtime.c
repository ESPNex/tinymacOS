#include "../kernel/k.h"
#include "disklayout.h"
#include "fs2_volume.h"
#define MAXVAULT 16
typedef struct {Vault v;int initialized,d;u32 base,extent;} Runtime;
static Runtime volumes[MAXVAULT];
static int rawread(void*p,u32 l,void*out){Runtime*r=p;return sata_io(r->d,r->base+l,1,out,0);}
static int rawwrite(void*p,u32 l,const void*in){Runtime*r=p;return sata_io(r->d,r->base+l,1,(void*)in,1);}
static int rawflush(void*p){return sata_flush(((Runtime*)p)->d);}
Vault *volume_vault(int d)
{
#ifdef TMC_FS2_BACKEND
 extern int fs2_plain_volume(int);if(fs2_plain_volume(d))return 0;
#endif
 if(d<0||d>=sata_count()||d>=MAXVAULT)return 0;Runtime*r=volumes+d;if(r->initialized)return &r->v;
 static Layout map;Medium m={sata_io,sata_flush,d,sata_size(d)};
 if(layout_read(&m,&map)!=2)return 0;
#ifdef TMC_FS2_BACKEND
 /* Recovery must not mistake arbitrary scalable data for v1 vault metadata. */
 extern int fs2_disk_present(int,u32,u32);
 if(fs2_disk_present(d,map.data,map.parts[1].end-map.data+1)!=0)return 0;
#endif
 r->base=map.data;r->extent=map.parts[1].end-map.data+1;r->d=d;r->initialized=1;
 FvIo io={r,r->extent,rawread,rawwrite,rawflush};vault_probe(&r->v,io);return &r->v;
}
int volume_locked(void){
#ifdef TMC_FS2_BACKEND
 if(fs2_volume_locked(volume_current()))return 1;
#endif
 Vault*v=volume_vault(volume_current());return v&&v->present&&(!v->unlocked||v->poison);}
/* This is the sole data-volume path for normal FS, metadata, snapshots and
 * Recovery backup/restore. GPT/EFI/Recovery OS remain intentionally public. */
int volume_cipher_io(int d,u32 l,u32 n,void*p,int write)
{
 if(!n)return 0;
#ifdef TMC_FS2_BACKEND
 if(fs2_volume_sector_guard(d,l,n))return FV_STATE;
#endif
 Vault*v=volume_vault(d);u32 base=v?volumes[d].base:0;
 if(!v || l<base || l>=base+FV_PAYLOAD){
  if(v&&v->present&&l<base+FV_REQUIRED&&l+n>base+FV_PAYLOAD)return FV_STATE;
  if(v&&v->present&&l<base&&n>base-l)return FV_STATE;
  return sata_io(d,l,n,p,write)?FV_IO:write?sata_flush(d):0;
 }
 if(n>base+FV_PAYLOAD-l)return FV_STATE;
 if(!v->present)return sata_io(d,l,n,p,write)?FV_IO:write?sata_flush(d):0;
 for(u32 i=0;i<n;i++){int r=write?vault_write(v,l-base+i,(u8*)p+i*512):vault_read(v,l-base+i,(u8*)p+i*512);if(r)return r;}return 0;
}
static void status(Vault*v,VaultInfo*out,int error)
{
 *out=(VaultInfo){0};if(!v){out->error=FV_STATE;return;}
 out->format=1;out->available=v->io.sectors>=FV_REQUIRED;out->enabled=v->present;out->locked=v->present&&!v->unlocked;out->state=v->h.state;out->progress=(u64)v->h.progress*100/FV_PAYLOAD;out->error=v->poison?FV_CORRUPT:error;out->protected_sectors=v->present?FV_PAYLOAD:0;
}
/* Explicit authenticated abort of an activation interrupted BEFORE sector 0
 * conversion. Never a generic corrupt-header reset: require an intact plaintext
 * account, plaintext filesystem header, and an entirely unused tag table. */
static int abort_unstarted(Vault*v,const char*pass)
{
 u8 h[512],account[512],z[512]={0};
 if(!v->present||rawread(v->io.opaque,0,h)||memcmp(h,"TMCBFS1\0",8)||((u32*)h)[5]!=1||((u32*)h)[6])return FV_STATE;
 int authenticated=0;for(int i=13;i<=14;i++)if(!rawread(v->io.opaque,i,account)&&session_record_password(account,pass))authenticated=1;
 erase(account,sizeof account);if(!authenticated)return FV_AUTH;
 for(u32 l=FV_META+6;l<FV_REQUIRED;l++){if(rawread(v->io.opaque,l,h))return FV_IO;for(int j=0;j<512;j++)if(h[j])return FV_STATE;}
 for(u32 l=FV_META;l<v->io.sectors;l++)if(rawwrite(v->io.opaque,l,z)||rawflush(v->io.opaque))return FV_IO;
 FvIo io=v->io;return vault_probe(v,io);
}
int filevault_control(int op,VaultRequest*q,VaultInfo*out)
{
 u32 flags;rec_lock(&fs_gate,&flags);int d=q->disk<0?volume_current():q->disk;
#ifdef TMC_FS2_BACKEND
 int result;if(fs2_volume_control(op,d,q,out,&result)){rec_unlock(&fs_gate,flags);return result;}
#endif
 Vault*v=volume_vault(d);int r=FV_STATE;u8 entropy[128]={0};
 if(v){
  if(op==0)r=0;
  else if(op==1){u8 h[512];
   /* Reject RAM/live sessions and old per-file cipher. No implicit decrypt or
    * destructive migration of an unknown/legacy layout. */
   if(!boot.recovery&&(boot.install_len||d!=volume_current()||!session_check_password(q->password)))r=FV_AUTH;
   else r=0;
   if(r==0){if(rawread(volumes+d,0,h)||memcmp(h,"TMCBFS1\0",8)||((u32*)h)[5]!=1||((u32*)h)[6])r=FV_STATE;
    else if(!eq(q->password,q->replacement))r=FV_AUTH;
    else if(!vault_random(entropy,sizeof entropy))r=FV_ENTROPY;
    else r=vault_enable(v,q->password,entropy,q->recovery);}
  }
  else if(op==2)r=vault_decrypt(v,q->password);
  else if(op==3)r=vault_step(v,32);
  else if(op==4){if(v->poison){FvIo io=v->io;vault_probe(v,io);}r=vault_unlock(v,q->password);}
  else if(op==5){if(!vault_random(entropy,40))r=FV_ENTROPY;else r=vault_password(v,q->password,q->replacement,entropy);}
  else if(op==6){if(!vault_random(entropy,72))r=FV_ENTROPY;else r=vault_recovery(v,q->password,entropy,q->recovery);}
  else if(op==7&&boot.recovery){vault_lock(v);r=0;}
  else if(op==8)r=abort_unstarted(v,q->password);
 }
 erase(entropy,sizeof entropy);status(v,out,r<0?r:0);rec_unlock(&fs_gate,flags);return r;
}

/* Called with fs_gate held before offline partition mutations, including those
 * which later fail. No cached layout/key may target a deleted or moved extent. */
void volume_forget(int d)
{
#ifdef TMC_FS2_BACKEND
 fs2_volume_forget(d);
#endif
 if(d>=0&&d<MAXVAULT){vault_lock(&volumes[d].v);erase(&volumes[d],sizeof volumes[d]);}
}
