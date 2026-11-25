/* SPDX-License-Identifier: MIT
 * Native snapshot Copy-on-Write. New snapshots share live sectors; the first
 * overwrite preserves ONE old sector for ALL snapshots still sharing it.
 * Reference counts are reconstructed from durable maps, never trusted on disk.
 * A journal makes interrupted map/catalog/live-sector updates replayable.
 * No raw SATA path: FileVault authenticates/encrypts maps, journal and pool too.
 */
#include "nativecow.h"
static int nonzero(const void*v,u32 n){const u8*p=v;while(n--)if(*p++)return 1;return 0;}
static CowEntry*entries(NativeCow*c){return (void*)(c->catalog+32);}
static int metadata(u32 l){return !l||(l>=NF_SUPER&&l<NF_DATA);}
static int error(NativeCow*c,int e){c->poison=1;return c->error=e;}
static int readraw(NativeCow*c,u32 l,void*p){return l>=c->io.sectors||c->io.read(c->io.opaque,l,p)?NC_IO:0;}
static int writeraw(NativeCow*c,u32 l,const void*p){
 u8 b[512];if(l>=c->io.sectors||c->io.write(c->io.opaque,l,p)||readraw(c,l,b)||memcmp(p,b,512))return error(c,NC_IO);return 0;
}
static int shape(CowSuper*s,u32 frame,u32 limit){
 u32 stride=(frame*8+511)/512,start=frame+8*stride;
 if(frame<NF_DATA||frame>NF_MAX_IMAGE||limit>NPART||limit<=start||limit<=NC_JOURNAL)return 0;
 memset(s,0,sizeof *s);memcpy(s->magic,"TMCOW03",8);s->version=1;s->frame=frame;s->map_start=frame;s->map_stride=stride;s->store_start=start;s->store_count=limit-start;s->limit=limit;s->crc=crc32(s,sizeof *s);return 1;
}
static u32 manifest(NativeCow*c,int k){return crc32(c->refs[k],c->super.frame*sizeof(CowRef));}
static int catalog_ok(NativeCow*c){
 u32 rev,count;memcpy(&rev,c->catalog+8,4);memcpy(&count,c->catalog+12,4);
 if(memcmp(c->catalog,"SNAPCAT1",8)||rev!=2||count>8)return 0;
 c->active=0;CowEntry*e=entries(c);
 for(u32 k=0;k<8;k++)if(e[k].name[0]){
  int terminated=0;for(int j=0;j<28;j++)if(!e[k].name[j])terminated=1;
  if(!terminated||k>=count||!e[k].id||e[k].base!=(k+1)*c->super.frame)return 0;c->active|=1u<<k;
 }
 return 1;
}
static int rebuild(NativeCow*c,int check_manifest){
 memset(c->users,0,sizeof c->users);c->allocated=0;
 for(int k=0;k<8;k++)if(c->active&(1u<<k)){
  if(check_manifest&&entries(c)[k].crc!=manifest(c,k))return NC_CORRUPT;
  for(u32 l=0;l<c->super.frame;l++){
   CowRef r=c->refs[k][l];if(metadata(l)){if(r.location||r.crc)return NC_CORRUPT;continue;}
   if(r.location>c->super.store_count)return NC_CORRUPT;
   if(r.location&&!c->users[r.location]++)c->allocated++;
  }
 }
 return 0;
}
static int map_sector(NativeCow*c,int k,u32 logical){
 u32 row=logical/64,index=row*64,n=MIN(64,c->super.frame-index);u8 b[512]={0};memcpy(b,c->refs[k]+index,n*8);
 return writeraw(c,c->super.map_start+k*c->super.map_stride+row,b);
}
static int map_all(NativeCow*c,int k){for(u32 l=0;l<c->super.frame;l+=64)if(map_sector(c,k,l))return NC_IO;return 0;}
static int load_maps(NativeCow*c){
 u8 b[512];for(int k=0;k<8;k++)if(c->active&(1u<<k))for(u32 l=0;l<c->super.frame;l+=64){
  if(readraw(c,c->super.map_start+k*c->super.map_stride+l/64,b))return NC_IO;
  memcpy(c->refs[k]+l,b,MIN(64,c->super.frame-l)*8);
 }return 0;
}
static int journal_valid(CowJournal*j){
 u32 crc=j->crc;j->crc=0;int ok=!memcmp(j->magic,"TMCOWTX3",8)&&j->version==1&&crc==crc32(j,sizeof *j);j->crc=crc;return ok;
}
static int replay(NativeCow*c,CowJournal*j){
 if((j->mask&~c->active)||j->logical>=c->super.frame||metadata(j->logical)||
    (j->mask?(!j->location||j->location>c->super.store_count):j->location!=0))return error(c,NC_CORRUPT);
 u8 old[512];u32 crc=0;
 if(j->mask){if(readraw(c,c->super.store_start+j->location-1,old))return error(c,NC_IO);crc=crc32(old,512);}
 CowEntry*e=entries(c);
 for(int k=0;k<8;k++)if(j->mask&(1u<<k)){
  CowRef*r=&c->refs[k][j->logical];if(e[k].id!=j->ids[k]||r->crc!=crc)return error(c,NC_CORRUPT);
  r->location=j->location;if(map_sector(c,k,j->logical))return NC_IO;e[k].crc=manifest(c,k);
 }
 if(j->mask&&writeraw(c,NF_CAT,c->catalog))return NC_IO;
 /* New data is durable before publishing the transaction. Recovery can
  * finish a torn live-sector write, not merely preserve the old snapshot. */
 u8 staged[512];if(readraw(c,NC_STAGED,staged)||crc32(staged,512)!=j->new_crc)return error(c,NC_CORRUPT);
 return writeraw(c,j->logical,staged);
}
static int mount_impl(NativeCow*c,CowIo io,u32 frame,u32 limit,int repair){
 memset(c,0,sizeof *c);c->io=io;CowSuper expected;
 if(!shape(&expected,frame,limit)||limit>io.sectors)return NC_CORRUPT;
 if(readraw(c,NC_SUPER,&c->super))return NC_IO;
 if(!memcmp(c->super.magic,"TMCCONV3",8))return error(c,NC_CORRUPT);
 if(!memcmp(c->super.magic,"TMCOW03",8)){
  if(memcmp(&c->super,&expected,sizeof expected))return error(c,NC_CORRUPT);
 }else{
  if(nonzero(&c->super,sizeof c->super))return error(c,NC_CORRUPT);
  c->super=expected;return 0;
 }
 c->enabled=1;
 if(readraw(c,NF_CAT,c->catalog)||!catalog_ok(c)||load_maps(c))return error(c,NC_CORRUPT);
 CowJournal journal;if(readraw(c,NC_JOURNAL,&journal))return error(c,NC_IO);
 if(nonzero(&journal,sizeof journal)){
  if(!repair)return error(c,NC_CORRUPT);
  if(journal_valid(&journal)){if(replay(c,&journal))return c->error;}
  else if(rebuild(c,1))return error(c,NC_CORRUPT);
  /* A torn clear is safe only after complete map manifests validate. */
  u8 zero[512]={0};if(writeraw(c,NC_JOURNAL,zero)||writeraw(c,NC_STAGED,zero))return NC_IO;
 }
 if(rebuild(c,1))return error(c,NC_CORRUPT);
 return 1;
}
int native_cow_mount(NativeCow*c,CowIo io,u32 frame,u32 limit){return mount_impl(c,io,frame,limit,1);}
int native_cow_mount_readonly(NativeCow*c,CowIo io,u32 frame,u32 limit){return mount_impl(c,io,frame,limit,0);}
static u32 checksum_continue(u32 crc,const u8*p,u32 n){while(n--){crc^=*p++;for(int i=0;i<8;i++)crc=(crc>>1)^(0x82f63b78u&-(crc&1));}return crc;}
int native_cow_enable(NativeCow*c,CowIo io,u32 frame,u32 limit){
 int mounted=native_cow_mount(c,io,frame,limit);if(mounted)return mounted<0?mounted:0;
 if(readraw(c,NF_CAT,c->catalog)||!catalog_ok(c))return error(c,NC_CORRUPT);
 /* Import legacy full-image snapshots with an in-place permutation. The
  * source inverse table and a single displaced sector handle overlap/cycles;
  * no complete volume or snapshot copy is allocated in RAM or on disk. */
 u32 next=1;u8 b[512];memset(c->source,0,sizeof c->source);memset(c->moved,0,sizeof c->moved);
 for(int k=0;k<8;k++)if(c->active&(1u<<k)){
  u32 sum=~0u;
  for(u32 l=0;l<frame;l++){
   u32 src=(k+1)*frame+l;if(src>=limit||readraw(c,src,b))return error(c,NC_IO);
   sum=checksum_continue(sum,b,512);
   if(metadata(l))continue;
   if(next>c->super.store_count)return error(c,NC_SPACE);
   c->refs[k][l]=(CowRef){next,crc32(b,512)};c->source[src]=next++;
  }
  if(~sum!=entries(c)[k].crc)return error(c,NC_CORRUPT);
 }
 /* source[] maps old physical sector -> dense new store index (1-based).
  * Build the inverse in a temporary array of refcounts; there can be >65535
  * source sectors only outside this format's explicit 57344-sector bound. */
 memset(c->users,0,sizeof c->users);
 for(u32 src=0;src<limit;src++)if(c->source[src])c->users[c->source[src]]=src;
 u8 marker[512]={0};memcpy(marker,"TMCCONV3",8);if(writeraw(c,NC_SUPER,marker))return NC_IO;
 for(u32 first=1;first<next;first++){
  if(c->moved[first/8]&(1u<<(first%8)))continue;
  u32 item=first;if(readraw(c,c->users[item],b))return error(c,NC_IO);
  for(;;){
   u32 dest=c->super.store_start+item-1,displaced=c->source[dest];u8 tmp[512];
   int need=displaced&&!(c->moved[displaced/8]&(1u<<(displaced%8)));
   if(need&&readraw(c,dest,tmp))return error(c,NC_IO);
   if(writeraw(c,dest,b))return NC_IO;c->moved[item/8]|=1u<<(item%8);
   if(!need)break;memcpy(b,tmp,512);item=displaced;
  }
 }
 for(int k=0;k<8;k++)if(c->active&(1u<<k)){if(map_all(c,k))return NC_IO;entries(c)[k].crc=manifest(c,k);}
 if(writeraw(c,NF_CAT,c->catalog))return NC_IO;
 u8 zero[512]={0};if(writeraw(c,NC_JOURNAL,zero)||writeraw(c,NC_STAGED,zero)||writeraw(c,NC_SUPER,&c->super))return NC_IO;
 c->enabled=1;if(rebuild(c,1))return error(c,NC_CORRUPT);return 0;
}
int native_cow_read(NativeCow*c,u32 l,void*p){
 if(!c->enabled||c->poison)return NC_CORRUPT;
 if(l<c->super.frame)return readraw(c,l,p);
 u32 k=l/c->super.frame-1,offset=l%c->super.frame;
 if(k>=8||!(c->active&(1u<<k)))return NC_CORRUPT;
 if(metadata(offset))return readraw(c,offset,p);
 CowRef r=c->refs[k][offset];if(r.location>c->super.store_count)return error(c,NC_CORRUPT);
 u32 source=r.location?c->super.store_start+r.location-1:offset;
 if(readraw(c,source,p))return NC_IO;
 if(crc32(p,512)!=r.crc){memset(p,0,512);return error(c,NC_CORRUPT);}return 0;
}
int native_cow_write(NativeCow*c,u32 l,const void*p){
 if(!c->enabled||c->poison||l>=c->super.frame)return NC_CORRUPT;
 if(metadata(l)){
  if(l==NF_CAT){
   u8 saved[512];memcpy(saved,c->catalog,512);memcpy(c->catalog,p,512);
   if(!catalog_ok(c)||rebuild(c,1)){memcpy(c->catalog,saved,512);catalog_ok(c);rebuild(c,1);return error(c,NC_CORRUPT);}
  }
  return writeraw(c,l,p);
 }
 u8 old[512];if(readraw(c,l,old))return error(c,NC_IO);
 if(!memcmp(old,p,512))return 0;
 u32 mask=0,crc=crc32(old,512);
 for(int k=0;k<8;k++)if((c->active&(1u<<k))&&!c->refs[k][l].location){if(c->refs[k][l].crc!=crc)return error(c,NC_CORRUPT);mask|=1u<<k;}
 u32 location=0;
 if(mask)for(u32 n=0;n<c->super.store_count;n++){
  u32 i=1+(c->cursor+n)%c->super.store_count;if(!c->users[i]){location=i;c->cursor=i;break;}
 }
 if(mask&&!location)return c->error=NC_SPACE;
 if(mask&&writeraw(c,c->super.store_start+location-1,old))return NC_IO;
 if(writeraw(c,NC_STAGED,p))return NC_IO;
 CowJournal j={0};memcpy(j.magic,"TMCOWTX3",8);j.version=1;j.logical=l;j.location=location;j.mask=mask;
 for(int k=0;k<8;k++)j.ids[k]=entries(c)[k].id;j.new_crc=crc32(p,512);j.crc=crc32(&j,sizeof j);
 if(writeraw(c,NC_JOURNAL,&j)||replay(c,&j))return error(c,NC_IO);
 u8 zero[512]={0};if(writeraw(c,NC_JOURNAL,zero)||writeraw(c,NC_STAGED,zero))return NC_IO;
 if(rebuild(c,1))return error(c,NC_CORRUPT);return 0;
}
int native_cow_prepare(NativeCow*c,int k,u32*sum){
 if(!c->enabled||c->poison||k<0||k>=8||(c->active&(1u<<k)))return NC_CORRUPT;
 memset(c->refs[k],0,sizeof c->refs[k]);u8 b[512];
 for(u32 l=0;l<c->super.frame;l++)if(!metadata(l)){if(readraw(c,l,b))return NC_IO;c->refs[k][l].crc=crc32(b,512);}
 if(map_all(c,k))return NC_IO;*sum=manifest(c,k);return 0;
}
int native_cow_verify(NativeCow*c,int k){
 if(!c->enabled||c->poison||k<0||k>=8||!(c->active&(1u<<k))||entries(c)[k].crc!=manifest(c,k))return NC_CORRUPT;
 u8 b[512];for(u32 l=0;l<c->super.frame;l++)if(!metadata(l)&&native_cow_read(c,(k+1)*c->super.frame+l,b))return NC_CORRUPT;return 0;
}
u32 native_cow_used(NativeCow*c){
 u32 maps=0;for(int k=0;k<8;k++)if(c->active&(1u<<k))maps+=c->super.map_stride;
 return c->super.frame+maps+c->allocated;
}
int native_cow_can_resize(NativeCow*c,u32 frame,u32 limit){
 CowSuper target;if(!c->enabled||c->poison||!shape(&target,frame,limit)||limit>c->io.sectors)return NC_CORRUPT;
 memset(c->source,0,sizeof c->source);u32 needed=0;
 for(int k=0;k<8;k++)if(c->active&(1u<<k))for(u32 l=0;l<MIN(frame,c->super.frame);l++){
  u32 location=c->refs[k][l].location;if(location&&!c->source[location]){c->source[location]=1;needed++;}
 }
 return needed>target.store_count?NC_SPACE:0;
}
int native_cow_relayout(NativeCow*c,u32 frame,u32 limit){
 int r=native_cow_can_resize(c,frame,limit);if(r)return r;
 CowSuper old=c->super,target;shape(&target,frame,limit);
 memset(c->source,0,sizeof c->source);
 for(int k=0;k<8;k++)if(c->active&(1u<<k))for(u32 l=0;l<MIN(frame,old.frame);l++){
  u32 id=c->refs[k][l].location;if(id)c->source[id]=1;
 }
 u32 next=1;
 for(u32 id=1;id<=old.store_count;id++)if(c->source[id])c->source[id]=frame<old.frame?next++:id;
 u8 marker[512]={0};memcpy(marker,"TMCCONV3",8);if(writeraw(c,NC_SUPER,marker))return NC_IO;
 /* Shrinking compacts in ascending source order, always to earlier sectors.
  * Growing preserves IDs and shifts in descending order, always later. */
 for(u32 step=0;step<old.store_count;step++){
  u32 id=frame<old.frame?step+1:old.store_count-step;if(!c->source[id])continue;
  u8 data[512];if(readraw(c,old.store_start+id-1,data)||writeraw(c,target.store_start+c->source[id]-1,data))return error(c,NC_IO);
 }
 u8 zero[512]={0};u32 emptycrc=crc32(zero,512);
 for(int k=0;k<8;k++)if(c->active&(1u<<k)){
  for(u32 l=0;l<MIN(frame,old.frame);l++)if(c->refs[k][l].location)c->refs[k][l].location=c->source[c->refs[k][l].location];
  if(frame<old.frame)memset(c->refs[k]+frame,0,(old.frame-frame)*sizeof(CowRef));
  else for(u32 l=old.frame;l<frame;l++)c->refs[k][l]=(CowRef){0,metadata(l)?0:emptycrc};
 }
 c->super=target;
 for(int k=0;k<8;k++)if(c->active&(1u<<k)){
  if(map_all(c,k))return NC_IO;entries(c)[k].base=(k+1)*frame;entries(c)[k].crc=manifest(c,k);
 }
 if(writeraw(c,NF_CAT,c->catalog)||rebuild(c,1))return error(c,NC_CORRUPT);
 return 0;
}
int native_cow_finish(NativeCow*c){
 if(!c->enabled||c->poison)return NC_CORRUPT;u8 zero[512]={0};
 return writeraw(c,NC_JOURNAL,zero)||writeraw(c,NC_STAGED,zero)||writeraw(c,NC_SUPER,&c->super)?NC_IO:0;
}
