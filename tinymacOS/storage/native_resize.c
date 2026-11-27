/* SPDX-License-Identifier: MIT
 * Offline native filesystem resizer, bounded 8..32 MiB format capacity.
 * Re-packs every allocated snapshot and the independent local backup, updates
 * their checksums and limits inode allocation. Never silently drops an inode.
 * Larger legacy partitions can be shrunk to this range after decrypting.
 * A failure after the first write is NOT rollback: the live header is poisoned
 * until the final verified commit so an interrupted volume cannot auto-mount.
 */
#include "nativefs.h"
#include "nativecow.h"
#include "vault.h"
u8 native_image_workspace[NF_MAX_IMAGE*512] ALIGNED(512);
static u8 verify[4096],header[512],catalog[512];
static NativeGeometry before,after;
static Seal backupseal;
typedef struct PACK {char name[28];u32 date,id,base,crc,gen;} Entry;
static u32 snapcrc[8];static int present[10];
static int resize_cow(Medium*,DiskEdit*,int);
u32 native_crc32c(const void*v,u32 n){u32 c=~0u;const u8*p=v;while(n--){c^=*p++;for(int j=0;j<8;j++)c=(c>>1)^(0x82f63b78u&-(c&1));}return ~c;}
int native_geometry_fit(u32 sectors,NativeGeometry*g){
 if(sectors<NF_MIN_VOLUME||sectors>NF_MAX_VOLUME)return 0;
 for(u32 slots=NF_MAX;slots>=NF_MIN;slots-=8){
  u32 frame=NF_DATA+slots*16,backup=(9*frame+1+2047)&~2047u;
  if(backup+frame<=sectors){*g=(NativeGeometry){.magic="TMCGEO3",.version=1,.slots=slots,.image=frame,.backup=backup,.payload=backup+frame,.sectors=sectors};g->crc=crc32(g,sizeof *g);return 1;}
 }
 return 0;
}
int native_geometry(const void*h,NativeGeometry*g){
 const u8*p=h;u32 words[6];memcpy(words,h,sizeof words);
 if(memcmp(p,"TMCBFS1\0",8)||words[2]!=2||words[4]!=NF_DATA||words[5]>1)return 0;
 memcpy(g,p+NF_GEOM_AT,sizeof *g);
 if(!memcmp(g->magic,"TMCGEO3",8)){
  u32 sum=g->crc;g->crc=0;int good=sum==crc32(g,sizeof *g);g->crc=sum;
  NativeGeometry expected;if(!good||!native_geometry_fit(g->sectors,&expected)||memcmp(g,&expected,sizeof *g)||words[3]!=g->slots)return 0;
  return 1;
 }
 /* No accidental acceptance of a damaged variable-capacity extension. */
 for(unsigned j=0;j<sizeof *g;j++)if(p[NF_GEOM_AT+j])return 0;
 if(words[3]!=NF_MAX)return 0;
 *g=(NativeGeometry){.slots=NF_MAX,.image=NF_MAX_IMAGE,.backup=BACKUP_OFF,.payload=BACKUP_OFF+NF_MAX_IMAGE};return 1;
}
void native_geometry_store(void*h,const NativeGeometry*g){
 memcpy((u8*)h+NF_GEOM_AT,g,sizeof *g);memcpy((u8*)h+12,&g->slots,4);
}
int native_image_check(const void*v,const NativeGeometry*g){
 const u8*p=v;NativeGeometry found;if(!native_geometry(v,&found)||found.slots!=g->slots||found.image!=g->image)return 0;
 const u8*sb=p+NF_SUPER*512;u32 sum;memcpy(&sum,sb,4);
 if(memcmp(sb+64,"_BHRfS_M",8)||sum!=native_crc32c(sb+32,4064))return 0;
 const u8*nodes=p+NF_NODE*512;if(nodes[0]!=1||nodes[2]||nodes[3])return 0;
 for(u32 i=1;i<g->slots;i++){
  const u8*n=nodes+i*64;if(!n[0])continue;u32 size;memcpy(&size,n+4,4);
  if(n[0]>2||size>8192||!n[12]||n[59])return 0;
  u32 a=i,k=0;while(a&&k++<g->slots){u16 parent;memcpy(&parent,nodes+a*64+2,2);a=parent;if(a>=g->slots||nodes[a*64]!=1)return 0;}if(a)return 0;
 }
 return 1;
}
static int rd(Medium*m,u64 start,u32 limit,u32 at,u32 n,void*p){return at>=limit||n>limit-at||m->io(m->d,start+at,n,p,0);}
static int wr(Medium*m,u64 start,u32 limit,u32 at,u32 n,const void*p){
 if(!n||n>8||at>=limit||n>limit-at)return -1;
 return m->io(m->d,start+at,n,(void*)p,1)||m->flush(m->d)||rd(m,start,limit,at,n,verify)||memcmp(p,verify,n*512)?-1:0;
}
static u32 position(NativeGeometry*g,int k){return k==9?g->backup:k*g->image;}
static int readimage(Medium*m,DiskEdit*q,int k){
 u32 at=position(&before,k);
 for(u32 j=0;j<before.image;j+=8)if(rd(m,q->start,q->sectors,at+j,MIN(8,before.image-j),native_image_workspace+j*512))return DE_IO;
 return 0;
}
static void update_image(int preserve_nodes){
 /* Preserve files, inode IDs, names, account sectors and all unknown fields.
  * Never copy bytes from a removed/free inode into the new live allocation. */
 native_geometry_store(native_image_workspace,&after);
 if(!preserve_nodes&&after.slots<before.slots)memset(native_image_workspace+NF_NODE*512+after.slots*64,0,(NF_MAX-after.slots)*64);
 else if(after.image>before.image){memset(native_image_workspace+before.image*512,0,(after.image-before.image)*512);if(!preserve_nodes)memset(native_image_workspace+NF_NODE*512+before.slots*64,0,(after.slots-before.slots)*64);}
 u8*sb=native_image_workspace+NF_SUPER*512;u64 total=(u64)after.sectors*512,used=(u64)after.image*512;
 memcpy(sb+0x70,&total,8);memcpy(sb+0x78,&used,8);memcpy(sb+3256,&total,8);memcpy(sb+3264,&used,8);
 u32 crc=native_crc32c(sb+32,4064);memcpy(sb,&crc,4);
}
int native_resize(Medium*m,DiskEdit*q,int apply){
 if(!m||!m->flush||!q||!q->sectors||!q->size||q->start>=m->z||MAX(q->sectors,q->size)>m->z-q->start)return DE_INVALID;
 if(q->size>NF_MAX_VOLUME||!native_geometry_fit(q->size,&after))return DE_UNSUPPORTED;
 memset(present,0,sizeof present);memset(snapcrc,0,sizeof snapcrc);memset(&backupseal,0,sizeof backupseal);
 if(rd(m,q->start,q->sectors,0,1,header))return DE_IO;
 if(!native_geometry(header,&before)||before.payload>q->sectors)return DE_PROTECTED;
 /* Legacy per-file cipher is not relocatable without its key. Whole-volume
  * FileVault must first be decrypted through its authenticated interface. */
 u32 legacy_cipher;memcpy(&legacy_cipher,header+24,4);if(legacy_cipher)return DE_PROTECTED;
 if(q->sectors>FV_META+1){u8 b[512];for(u32 l=FV_META;l<=FV_META+1;l++){if(rd(m,q->start,q->sectors,l,1,b))return DE_IO;if(!memcmp(b,"TMCFV",5))return DE_PROTECTED;}}
 if(before.slots==after.slots)return DE_UNSUPPORTED;
 u8 cowheader[512];if(rd(m,q->start,q->sectors,NC_SUPER,1,cowheader))return DE_IO;
 if(!memcmp(cowheader,"TMCOW03",8))return resize_cow(m,q,apply);
 if(!memcmp(cowheader,"TMCCONV3",8))return DE_PROTECTED;
 if(rd(m,q->start,q->sectors,NF_CAT,1,catalog))return DE_IO;
 u32 revision,count;memcpy(&revision,catalog+8,4);memcpy(&count,catalog+12,4);
 if(memcmp(catalog,"SNAPCAT1",8)||revision!=2||count>8)return DE_PROTECTED;
 present[0]=1;Entry*entries=(void*)(catalog+32);
 for(int k=0;k<8;k++)if(entries[k].name[0]){
  int terminated=0;for(int c=0;c<28;c++)if(!entries[k].name[c])terminated=1;
  if(!terminated||k>=(int)count||entries[k].base!=(u32)(k+1)*before.image||!entries[k].crc)return DE_PROTECTED;
  present[k+1]=1;
 }
 if(before.backup&&before.backup<q->sectors){
  if(rd(m,q->start,q->sectors,before.backup-1,1,&backupseal))return DE_IO;
  if(!memcmp(backupseal.magic,"TMCSNAP2",8)){if(backupseal.ver!=1||backupseal.sectors!=before.image)return DE_PROTECTED;present[9]=1;}
 }
 /* Entire preflight, including snapshot/backup hashes and the inode tail,
  * precedes EVERY write (even an explicitly confirmed operation). */
 for(int k=0;k<10;k++)if(present[k]){
  int r=readimage(m,q,k);if(r)return r;
  if(!native_image_check(native_image_workspace,&before))return DE_PROTECTED;
  if(k>0&&k<9){u32 c=native_crc32c(native_image_workspace,before.image*512);if(c!=entries[k-1].crc)return DE_PROTECTED;}
  if(k==9){u8 sum[32];sha_bytes(native_image_workspace,before.image*512,sum);if(memcmp(sum,backupseal.digest,32))return DE_PROTECTED;}
  for(u32 i=after.slots;i<before.slots;i++)if(native_image_workspace[NF_NODE*512+i*64])return DE_OCCUPIED;
 }
 if(!apply){erase(native_image_workspace,sizeof native_image_workspace);return 0;}
 u32 limit=MAX(q->sectors,q->size);u8 incomplete[512]={0};memcpy(incomplete,"TMCRESZ3",8);
 if(wr(m,q->start,limit,0,1,incomplete))return DE_IO;
 /* Growing: last native_image_workspace first. Shrinking: first native_image_workspace first. An entire source
  * native_image_workspace is buffered before overwriting it; no unread source is clobbered. */
 for(int step=0;step<10;step++){
  int k=after.image>before.image?9-step:step;if(!present[k])continue;
  int r=readimage(m,q,k);if(r)goto failed;
  if(!k)memcpy(native_image_workspace,header,512); /* source header is now the refusal marker */
  update_image(0);
  if(k>0&&k<9)snapcrc[k-1]=native_crc32c(native_image_workspace,after.image*512);
  if(k==9){backupseal.sectors=after.image;sha_bytes(native_image_workspace,after.image*512,backupseal.digest);}
  u32 dst=position(&after,k);
  for(u32 j=0;j<after.image;j+=8){
   /* Publish the live header/catalog only after every source native_image_workspace is safe. */
   if(!k&&(j==0||j==NF_CAT)){
    if(!j){if(wr(m,q->start,limit,dst+1,7,native_image_workspace+512))goto failed;}
    else if(wr(m,q->start,limit,dst+j+1,7,native_image_workspace+(j+1)*512))goto failed;
   }else if(wr(m,q->start,limit,dst+j,MIN(8,after.image-j),native_image_workspace+j*512))goto failed;
  }
 }
 /* No backup is invented if one did not exist. The unused seal is cleared. */
 if(!present[9])memset(&backupseal,0,sizeof backupseal);
 if(wr(m,q->start,limit,after.backup-1,1,&backupseal))goto failed;
 for(int k=0;k<8;k++)if(present[k+1]){entries[k].base=(k+1)*after.image;entries[k].crc=snapcrc[k];}
 if(wr(m,q->start,limit,NF_CAT,1,catalog))goto failed;
 native_geometry_store(header,&after);
 if(wr(m,q->start,limit,0,1,header))goto failed;
 erase(native_image_workspace,sizeof native_image_workspace);return 0;
failed:erase(native_image_workspace,sizeof native_image_workspace);return DE_IO;
}
/* Independent context: editing a second disk must never replace the mounted
 * volume's CoW maps/cache, even though both operations share fs_gate. */
static NativeCow offline_cow;
static struct {Medium*m;u32 start,limit;} cow_view;
static int cow_read_device(void*x,u32 l,void*p){(void)x;return rd(cow_view.m,cow_view.start,cow_view.limit,l,1,p);}
static int cow_write_device(void*x,u32 l,const void*p){(void)x;return wr(cow_view.m,cow_view.start,cow_view.limit,l,1,p);}
static int cow_backup_move(Medium*m,DiskEdit*q){
 if(!present[9])return 0;
 if(readimage(m,q,9))return DE_IO;update_image(1);backupseal.sectors=after.image;sha_bytes(native_image_workspace,after.image*512,backupseal.digest);
 u32 limit=MAX(q->sectors,q->size);
 for(u32 j=0;j<after.image;j+=8)if(wr(m,q->start,limit,after.backup+j,MIN(8,after.image-j),native_image_workspace+j*512))return DE_IO;
 return 0;
}
static int resize_cow(Medium*m,DiskEdit*q,int apply){
 cow_view.m=m;cow_view.start=q->start;cow_view.limit=MAX(q->sectors,q->size);
 CowIo io={0,cow_view.limit,cow_read_device,cow_write_device};
 int r=native_cow_mount_readonly(&offline_cow,io,before.image,before.backup-1);
 if(r!=1)return DE_PROTECTED;
 if(native_cow_can_resize(&offline_cow,after.image,after.backup-1))return DE_OCCUPIED;
 for(int k=-1;k<8;k++)if(k<0||(offline_cow.active&(1u<<k))){
  for(u32 l=0;l<before.image;l++)if(native_cow_read(&offline_cow,(k+1)*before.image+l,native_image_workspace+l*512))return DE_PROTECTED;
  if(!native_image_check(native_image_workspace,&before))return DE_PROTECTED;
  for(u32 i=after.slots;i<before.slots;i++)if(native_image_workspace[NF_NODE*512+i*64])return DE_OCCUPIED;
 }
 if(rd(m,q->start,q->sectors,before.backup-1,1,&backupseal))return DE_IO;
 present[9]=!memcmp(backupseal.magic,"TMCSNAP2",8);
 if(present[9]){
  if(backupseal.sectors!=before.image||backupseal.ver!=1||readimage(m,q,9)||!native_image_check(native_image_workspace,&before))return DE_PROTECTED;
  u8 hash[32];sha_bytes(native_image_workspace,before.image*512,hash);if(memcmp(hash,backupseal.digest,32))return DE_PROTECTED;
  for(u32 i=after.slots;i<before.slots;i++)if(native_image_workspace[NF_NODE*512+i*64])return DE_OCCUPIED;
 }
 if(!apply){erase(native_image_workspace,sizeof native_image_workspace);return 0;}
 u8 incomplete[512]={0};memcpy(incomplete,"TMCRESZ3",8);
 if(wr(m,q->start,cow_view.limit,0,1,incomplete))return DE_IO;
 if(after.image>before.image&&cow_backup_move(m,q))goto cow_failed;
 if(native_cow_relayout(&offline_cow,after.image,after.backup-1))goto cow_failed;
 if(after.image<before.image&&cow_backup_move(m,q))goto cow_failed;
 if(readimage(m,q,0))goto cow_failed;memcpy(native_image_workspace,header,512);update_image(1);
 for(u32 j=0;j<after.image;j+=8){
  if(!j){if(wr(m,q->start,cow_view.limit,1,7,native_image_workspace+512))goto cow_failed;}
  else if(j==NF_CAT){if(wr(m,q->start,cow_view.limit,j+4,4,native_image_workspace+(j+4)*512))goto cow_failed;}
  else if(wr(m,q->start,cow_view.limit,j,MIN(8,after.image-j),native_image_workspace+j*512))goto cow_failed;
 }
 if(!present[9])memset(&backupseal,0,sizeof backupseal);
 if(wr(m,q->start,cow_view.limit,after.backup-1,1,&backupseal)||native_cow_finish(&offline_cow))goto cow_failed;
 native_geometry_store(header,&after);
 if(wr(m,q->start,cow_view.limit,0,1,header))goto cow_failed;
 erase(native_image_workspace,sizeof native_image_workspace);return 0;
cow_failed:erase(native_image_workspace,sizeof native_image_workspace);return DE_IO;
}
