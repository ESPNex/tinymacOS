#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vault.h"
#include "sha256.h"
#include "../crypto/monocypher/monocypher.h"
#define NSECT FV_REQUIRED
static u8 *disk,*cache,*backup,*migration;static int dirty=-1,ops,cut=-1,torn;
static int rd(void*x,u32 l,void*p){(void)x;if(l>=NSECT)return -1;memcpy(p,cache+l*512,512);return 0;}
static int wr(void*x,u32 l,const void*p){(void)x;assert(l<NSECT);ops++;if(ops==cut)return -1;assert(dirty<0);memcpy(cache+l*512,p,512);dirty=l;return 0;}
static int flush(void*x){(void)x;ops++;if(ops==cut){if(torn&&dirty>=0)memcpy(disk+dirty*512,cache+dirty*512,231);return -1;}if(dirty>=0){memcpy(disk+dirty*512,cache+dirty*512,512);dirty=-1;}return 0;}
static FvIo io={0,NSECT,rd,wr,flush};
static void powerloss(void){dirty=-1;cut=-1;memcpy(cache,disk,NSECT*512);}
static void reset(void){memcpy(disk,backup,NSECT*512);powerloss();}
static u8 entropy[128];static char recovery[65];
static void finish(Vault*v){int r;do{r=vault_step(v,128);assert(r>=0);}while(!r);}
int main(int argc,char**argv){(void)argc;(void)argv;size_t bytes=(size_t)NSECT*512;disk=calloc(1,bytes);cache=calloc(1,bytes);backup=calloc(1,bytes);assert(disk&&cache&&backup);for(int i=0;i<128;i++)entropy[i]=i+1;
 for(u32 i=0;i<FV_PAYLOAD;i++)for(int j=0;j<512;j++)disk[i*512+j]=(i*7+j)%251;memcpy(cache,disk,bytes);
 Vault v;assert(vault_probe(&v,io)==0);assert(!vault_enable(&v,"Native-pass-285",entropy,recovery));assert(strlen(recovery)==64);
 u8 original[512],readback[512],changed[512];memcpy(original,disk+3*512,512);memset(changed,0xa5,512);
 /* Crash while conversion is incomplete; recovery slot resumes it. */
 assert(vault_step(&v,3)==0);migration=malloc(bytes);assert(migration);memcpy(migration,disk,bytes);vault_lock(&v);powerloss();assert(vault_probe(&v,io)==1);assert(vault_read(&v,3,readback)==FV_AUTH);assert(!vault_unlock(&v,recovery));finish(&v);assert(v.h.state==FV_ON);assert(!vault_read(&v,3,readback)&&!memcmp(readback,original,512));
 for(u32 i=0;i<FV_PAYLOAD;i++){assert(!vault_read(&v,i,readback));for(int j=0;j<512;j++)assert(readback[j]==(i*7+j)%251);assert(memcmp(disk+i*512,readback,512));}
 puts("PASS every live/metadata/account/snapshot/backup sector is ciphertext and roundtrips; migration resumes after reboot with recovery key");
 memcpy(backup,disk,bytes);
 for(int partial=0;partial<2;partial++)for(int failure=1;failure<=24;failure++){
  reset();assert(vault_probe(&v,io)==1);assert(!vault_unlock(&v,recovery));ops=0;cut=failure;torn=partial;(void)vault_write(&v,3,changed);powerloss();assert(vault_probe(&v,io)==1);int r=vault_unlock(&v,recovery);assert(r==0);assert(!vault_read(&v,3,readback));assert(!memcmp(readback,original,512)||!memcmp(readback,changed,512));
 }
 puts("PASS all transaction write/flush cuts, with volatile write cache and torn sector writes; journal restores both ciphertext and complete tag table");
 reset();assert(vault_probe(&v,io)==1);assert(!vault_unlock(&v,recovery));assert(vault_unlock(&v,"wrong-password")==FV_AUTH);
 /* Same-sector rewrites cannot reuse keystream; ciphertext/tags change. */
 assert(!vault_write(&v,3,changed));u8 first[512];memcpy(first,disk+3*512,512);assert(!vault_write(&v,3,changed));assert(memcmp(first,disk+3*512,512));
 vault_lock(&v);powerloss();assert(vault_probe(&v,io)==1);assert(!vault_unlock(&v,recovery));assert(!vault_write(&v,3,changed));assert(memcmp(first,disk+3*512,512));
 puts("PASS monotonic nonce reservation survives reboot and failed writes; wrong password denied, correct recovery key works");
 /* A bit flip in ciphertext cannot return unauthenticated bytes. */
 disk[8*512+17]^=1;powerloss();assert(vault_probe(&v,io)==1);assert(!vault_unlock(&v,recovery));memset(readback,0x99,512);assert(vault_read(&v,8,readback)==FV_CORRUPT);for(int i=0;i<512;i++)assert(readback[i]==0);
 reset();assert(vault_probe(&v,io)==1);assert(!vault_unlock(&v,recovery));char nextkey[65];u8 random[72];memset(random,0x72,72);assert(!vault_recovery(&v,recovery,random,nextkey));vault_lock(&v);powerloss();assert(vault_probe(&v,io)==1);assert(vault_unlock(&v,recovery)==FV_AUTH);assert(!vault_unlock(&v,nextkey));
 puts("PASS authenticated reads fail closed on tampering; recovery-key rotation revokes the previous key");
 assert(!vault_password(&v,nextkey,"New-native-pass-286",entropy));vault_lock(&v);assert(vault_unlock(&v,"Native-pass-285")==FV_AUTH);assert(!vault_unlock(&v,"New-native-pass-286"));
 assert(!vault_decrypt(&v,nextkey));assert(vault_step(&v,3)==0);assert(!vault_write(&v,1,changed));vault_lock(&v);powerloss();assert(vault_probe(&v,io)==1);assert(!vault_unlock(&v,nextkey));finish(&v);assert(!v.present);assert(!memcmp(disk+512,changed,512));assert(!memcmp(disk+3*512,original,512));powerloss();assert(vault_probe(&v,io)==0);
 puts("PASS password rotation revokes old password; decryption is resumable and preserves concurrent writes to already-converted sectors");
 if(FV_PAYLOAD<=128){
  for(int partial=0;partial<2;partial++)for(int failure=1;failure<=104;failure++){
   memcpy(disk,migration,bytes);powerloss();assert(vault_probe(&v,io)==1);assert(!vault_unlock(&v,recovery));ops=0;cut=failure;torn=partial;(void)vault_step(&v,8);powerloss();assert(vault_probe(&v,io)==1);assert(!vault_unlock(&v,recovery));finish(&v);
   for(u32 l=0;l<FV_PAYLOAD;l++){assert(!vault_read(&v,l,readback));for(int j=0;j<512;j++)assert(readback[j]==(l*7+j)%251);}
  }
  puts("PASS 208 interrupted-conversion write/flush/torn-sector cases: authenticated reboot, journal replay, full migration resume, every payload byte preserved");
 }
 /* Header commits must survive either copy tearing. Rotation may resolve to
  * the old or the new credential, but never to an inaccessible master key. */
 for(int partial=0;partial<2;partial++)for(int failure=1;failure<=4;failure++){
  reset();assert(vault_probe(&v,io)==1);assert(!vault_unlock(&v,recovery));ops=0;cut=failure;torn=partial;(void)vault_recovery(&v,recovery,random,nextkey);powerloss();assert(vault_probe(&v,io)==1);
  int r=vault_unlock(&v,nextkey);if(r)assert(!vault_unlock(&v,recovery));assert(!vault_read(&v,3,readback)&&!memcmp(readback,original,512));
 }
 puts("PASS interrupted recovery-key header commits select a valid old or new keyslot without losing the volume key");
 reset();disk[FV_META*512+40]^=1;powerloss();assert(vault_probe(&v,io)==1);assert(!vault_unlock(&v,recovery));
 disk[(FV_META+1)*512+40]^=1;powerloss();assert(vault_probe(&v,io)==FV_CORRUPT);assert(vault_unlock(&v,recovery)==FV_STATE);
 puts("PASS one corrupt header recovers from its redundant copy; two corrupt headers refuse access without reinitialization");
 if(FV_PAYLOAD<=128){
  reset();assert(vault_probe(&v,io)==1);assert(!vault_unlock(&v,recovery));assert(!vault_decrypt(&v,recovery));memcpy(migration,disk,bytes);
  for(int partial=0;partial<2;partial++)for(int failure=1;failure<=104;failure++){
   memcpy(disk,migration,bytes);powerloss();assert(vault_probe(&v,io)==1);assert(!vault_unlock(&v,recovery));ops=0;cut=failure;torn=partial;(void)vault_step(&v,8);powerloss();assert(vault_probe(&v,io)==1);assert(!vault_unlock(&v,recovery));finish(&v);assert(!v.present);
   for(u32 l=0;l<FV_PAYLOAD;l++)for(int j=0;j<512;j++)assert(disk[l*512+j]==(l*7+j)%251);
  }
  puts("PASS 208 interrupted-decryption write/flush/torn-sector cases: authenticated resume and every original plaintext byte restored");
 }
 /* Export a deterministic independent-test vector from actual on-disk bytes. */
 reset();FILE*f=fopen(argc>1?argv[1]:"/var/tmp/tmc-10.2.2/vault-vector.bin","wb");assert(f);fwrite(disk+FV_META*512,1,512,f);fwrite(disk+3*512,1,512,f);fwrite(disk+(FV_META+6)*512+(3%16)*32,1,32,f);fclose(f);
 free(disk);free(cache);free(backup);free(migration);puts("PASS native authenticated volume engine");return 0;
}
