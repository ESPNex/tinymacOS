#include "../kernel/k.h"
#include "disklayout.h"
#define MAXVAULT 16
typedef struct {Vault v;int initialized,d;u32 base,extent;} Runtime;
static Runtime volumes[MAXVAULT];
static int rawread(void*p,u32 l,void*out){Runtime*r=p;return sata_io(r->d,r->base+l,1,out,0);}
static int rawwrite(void*p,u32 l,const void*in){Runtime*r=p;return sata_io(r->d,r->base+l,1,(void*)in,1);}
static int rawflush(void*p){return sata_flush(((Runtime*)p)->d);}
Vault *volume_vault(int d)
{
 if(d<0||d>=sata_count()||d>=MAXVAULT)return 0;Runtime*r=volumes+d;if(r->initialized)return &r->v;
 static Layout map;Medium m={sata_io,sata_flush,d,sata_size(d)};
 if(layout_read(&m,&map)!=2||map.parts[1].start!=LDATA)return 0;
 r->base=LDATA;r->extent=map.parts[1].end-LDATA+1;r->d=d;r->initialized=1;
 FvIo io={r,r->extent,rawread,rawwrite,rawflush};vault_probe(&r->v,io);return &r->v;
}
int volume_locked(void){Vault*v=volume_vault(volume_current());return v&&v->present&&(!v->unlocked||v->poison);}
/* This is the sole data-volume path for normal FS, metadata, snapshots and
 * Recovery backup/restore. GPT/EFI/Recovery OS remain intentionally public. */
int volume_cipher_io(int d,u32 l,u32 n,void*p,int write)
{
 if(!n)return 0;Vault*v=volume_vault(d);
 if(!v || l<LDATA || l>=LDATA+FV_PAYLOAD){
  if(v&&v->present&&l<LDATA+FV_REQUIRED&&l+n>LDATA+FV_PAYLOAD)return FV_STATE;
  /* Do not permit a raw span that straddles the protected range. */
  if(v&&v->present&&l<LDATA&&n>LDATA-l)return FV_STATE;
  return sata_io(d,l,n,p,write)?FV_IO:write?sata_flush(d):0;
 }
 if(n>LDATA+FV_PAYLOAD-l)return FV_STATE;
 if(!v->present)return sata_io(d,l,n,p,write)?FV_IO:write?sata_flush(d):0;
 for(u32 i=0;i<n;i++){int r=write?vault_write(v,l-LDATA+i,(u8*)p+i*512):vault_read(v,l-LDATA+i,(u8*)p+i*512);if(r)return r;}return 0;
}
static void status(Vault*v,VaultInfo*out,int error)
{
 *out=(VaultInfo){0};if(!v){out->error=FV_STATE;return;}
 out->available=v->io.sectors>=FV_REQUIRED;out->enabled=v->present;out->locked=v->present&&!v->unlocked;out->state=v->h.state;out->progress=(u64)v->h.progress*100/FV_PAYLOAD;out->error=v->poison?FV_CORRUPT:error;out->protected_sectors=v->present?FV_PAYLOAD:0;
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
 u32 flags;rec_lock(&fs_gate,&flags);int d=q->disk<0?volume_current():q->disk;Vault*v=volume_vault(d);int r=FV_STATE;u8 entropy[128]={0};
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
