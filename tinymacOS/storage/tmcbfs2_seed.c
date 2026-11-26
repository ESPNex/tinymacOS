/* SPDX-License-Identifier: MIT
 * Bounded legacy live-session -> scalable fresh-volume import. The source is
 * immutable; no whole-volume snapshot duplication and no implicit conversion.
 */
#include "tmcbfs2_seed.h"
#include "nativefs.h"
typedef struct PACK {u8 kind,ro;u16 parent;u32 size,gen;char name[48];u32 reserved;} SeedNode;
_Static_assert(sizeof(SeedNode)==64,"seed inode ABI");
static int geometry(const void*image,size_t bytes,NativeGeometry*g){
 if(!image||bytes<512||!native_geometry(image,g)||bytes<(u64)g->image*512)return -1;
 u32 cipher;memcpy(&cipher,(const u8*)image+24,4);
 return cipher||!native_image_check(image,g)?-1:0;
}
int tmc2_seed_validate(const void*image,size_t bytes){
 NativeGeometry g;if(geometry(image,bytes,&g))return -1;
 const SeedNode*n=(const void*)((const u8*)image+NF_NODE*512);
 if(n[0].kind!=T2_DIR||n[0].parent||n[0].size)return -1;
 for(unsigned i=1;i<g.slots;i++)if(n[i].kind){
  if(n[i].ro>1||n[i].reserved||!n[i].name[0]||n[i].name[47]||eq(n[i].name,".")||eq(n[i].name,"..")||(n[i].kind==T2_DIR&&n[i].size))return -1;
  for(const char*p=n[i].name;*p;p++)if(*p=='/'||(u8)*p<32)return -1;
  for(unsigned j=1;j<i;j++)if(n[j].kind&&n[j].parent==n[i].parent&&eq(n[j].name,n[i].name))return -1;
 }
 return 0;
}
/* IDs are mapped, never assumed equal: deleted source slots and children
 * preceding their parents are both valid. The fixed map bounds the source,
 * not the new filesystem's future allocation capacity. */
static int transfer(Tmc2*c,const void*image,size_t bytes,int verify){
 NativeGeometry g;if(tmc2_seed_validate(image,bytes)||geometry(image,bytes,&g))return -1;
 const u8*raw=image;const SeedNode*n=(const void*)(raw+NF_NODE*512);
 u64 ids[NF_MAX]={1};unsigned left=0,total=1;
 for(unsigned i=1;i<g.slots;i++)if(n[i].kind)left++,total++;
 if((!verify&&tmc2_inode_count(c)!=1)||(verify&&tmc2_inode_count(c)!=total))return -1;
 while(left){unsigned progress=0;
  for(unsigned i=1;i<g.slots;i++)if(n[i].kind&&!ids[i]&&ids[n[i].parent]){
   const SeedNode*s=n+i;T2Entry e;
   if(verify){if(tmc2_lookup(c,ids[s->parent],s->name,&e)||e.kind!=s->kind||e.size!=s->size||e.reserved!=s->ro)return -1;ids[i]=e.key;}
   else{
    if(tmc2_create(c,ids[s->parent],s->name,s->kind,ids+i))return -1;
    if(s->kind==T2_FILE&&s->size&&tmc2_write(c,ids[i],0,raw+(NF_DATA+i*16)*512,s->size)!=s->size)return -1;
    if(s->ro&&tmc2_readonly(c,ids[i],1))return -1;
   }
   if(verify&&s->kind==T2_FILE){u8 buffer[512];for(u32 at=0;at<s->size;at+=sizeof buffer){unsigned take=MIN(sizeof buffer,s->size-at);if(tmc2_read(c,ids[i],at,buffer,take)!=take||memcmp(buffer,raw+(NF_DATA+i*16)*512+at,take))return -1;}}
   progress++;left--;
  }
  if(!progress)return -1;
 }
 for(unsigned slot=0;slot<2;slot++){
  u8 account[512];if(verify){if(tmc2_account(c,slot,account,0)||memcmp(account,raw+(13+slot)*512,512))return -1;}
  else if(tmc2_account(c,slot,(void*)(raw+(13+slot)*512),1))return -1;
 }
 return 0;
}
int tmc2_seed_import(Tmc2*c,const void*image,size_t bytes){return transfer(c,image,bytes,0);}
int tmc2_seed_verify(Tmc2*c,const void*image,size_t bytes){return transfer(c,image,bytes,1);}
