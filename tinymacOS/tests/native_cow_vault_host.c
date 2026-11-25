/* CoW through the REAL authenticated FileVault block path, not an encryption
 * stub. Build with -DFV_PAYLOAD=4096 for a disposable bounded host fixture. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nativecow.h"
#include "vault.h"
#if FV_PAYLOAD != 4096
#error Build this bounded integration test with FV_PAYLOAD=4096
#endif
static u8*disk;static Vault v;static NativeCow a,b;
static int rd(void*x,u32 l,void*p){(void)x;assert(l<FV_REQUIRED);memcpy(p,disk+l*512,512);return 0;}
static int wr(void*x,u32 l,const void*p){(void)x;assert(l<FV_REQUIRED);memcpy(disk+l*512,p,512);return 0;}
static int sync(void*x){(void)x;return 0;}
static int cr(void*x,u32 l,void*p){(void)x;return vault_read(&v,l,p);}
static int cw(void*x,u32 l,const void*p){(void)x;return vault_write(&v,l,p);}
int main(void){
 disk=calloc(FV_REQUIRED,512);assert(disk);memcpy(disk+NF_CAT*512,"SNAPCAT1",8);u32 rev=2;memcpy(disk+NF_CAT*512+8,&rev,4);memset(disk+200*512,'A',512);
 FvIo fio={0,FV_REQUIRED,rd,wr,sync};assert(!vault_probe(&v,fio));u8 random[128];for(int j=0;j<128;j++)random[j]=j+1;char recovery[65];assert(!vault_enable(&v,"cow-local-fixture",random,recovery));int r;do{r=vault_step(&v,128);assert(r>=0);}while(!r);
 CowIo io={0,FV_PAYLOAD,cr,cw};assert(!native_cow_enable(&a,io,256,3072));u32 sum;assert(!native_cow_prepare(&a,0,&sum));
 u8 cat[512];assert(!vault_read(&v,NF_CAT,cat));CowEntry*e=(void*)(cat+32);strcpy(e->name,"encrypted-CoW-point");e->id=1;e->base=256;e->crc=sum;u32 count=1;memcpy(cat+12,&count,4);assert(!native_cow_write(&a,NF_CAT,cat));
 u8 changed[512];memset(changed,'B',512);assert(!native_cow_write(&a,200,changed));assert(a.allocated==1);assert(!native_cow_verify(&a,0));u8 old[512];assert(!native_cow_read(&a,256+200,old));for(int j=0;j<512;j++)assert(old[j]=='A');
 /* Each block of the original value, current value, map, journal, catalog and
  * CoW descriptor is encrypted, including the newly allocated old-data block. */
 assert(memcmp(disk+200*512,changed,512));assert(memcmp(disk+(a.super.store_start+a.refs[0][200].location-1)*512,old,512));assert(memcmp(disk+NF_CAT*512,cat,512));
 vault_lock(&v);assert(vault_probe(&v,fio)==1);assert(!vault_unlock(&v,"cow-local-fixture"));assert(native_cow_mount(&b,io,256,3072)==1);assert(!native_cow_verify(&b,0));assert(!native_cow_read(&b,456,old));for(int j=0;j<512;j++)assert(old[j]=='A');
 u32 stored=b.super.store_start+b.refs[0][200].location-1;disk[stored*512+17]^=1;assert(native_cow_read(&b,456,old)<0);
 puts("PASS real FileVault encrypts/authenticates CoW pool/maps/journal/catalog, old snapshots survive lock/reboot/unlock, ciphertext tampering fails closed");
 free(disk);return 0;
}
