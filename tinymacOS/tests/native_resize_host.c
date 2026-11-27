/* Actual native resizer plus actual fs/btrfs.c allocation/read/write paths.
 * Disposable RAM image with selected-partition bounds and external canaries. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../fs/btrfs.c"
typedef struct PACK {char name[28];u32 date,id,base,crc,gen;} Entry;
struct Boot boot;
__attribute__((noreturn)) void retail_stop(void){abort();}
int bootfs_lookup(const char*p){(void)p;return -2;}
int bootfs_stat(int i,Dirent*d){(void)i;(void)d;return -1;}
int bootfs_open(Process*p,int i,int w){(void)p;(void)i;(void)w;return -1;}
int bootfs_read(FD*f,void*p,int n){(void)f;(void)p;(void)n;return -1;}
static u8*media;static u32 extent=65536;static int writes,failure=-1;
#define BASE 2048u
#define TOTAL (BASE+65536u+2048u)
static int io(int d,u64 l,u32 n,void*p,int w){assert(!d&&n&&l>=BASE&&l+n<=BASE+65536);if(w){if(writes++==failure)return -1;memcpy(media+l*512,p,n*512);}else memcpy(p,media+l*512,n*512);return 0;}
static int flush(int d){assert(!d);return 0;}
static int br(u32 l,void*p){assert(l<extent);return io(0,BASE+l,1,p,0);}
static int bw(u32 l,const void*p){assert(l<extent);return io(0,BASE+l,1,(void*)p,1);}
static u8*at(u32 l){return media+(BASE+l)*512;}
static void put32(u8*p,u32 v){memcpy(p,&v,4);}
static void fixture(void){
 memset(media,0,TOTAL*512);memset(media,'P',BASE*512);memset(media+(BASE+65536)*512,'S',2048*512);
 u8*h=at(0);memcpy(h,"TMCBFS1\0",8);put32(h+8,2);put32(h+12,384);put32(h+16,144);put32(h+20,1);
 Node*n=(void*)at(64);n[0]=(Node){.kind=1};n[3]=(Node){.kind=2,.size=8192,.gen=1};strcpy(n[3].name,"document.bin");
 memset(at(13),'A',1024);for(int j=0;j<8192;j++)at(144+3*16)[j]=j%251;
 u8*sb=at(128);memcpy(sb+64,"_BHRfS_M",8);put32(sb,native_crc32c(sb+32,4064));
 u8*cat=at(136);memcpy(cat,"SNAPCAT1",8);put32(cat+8,2);put32(cat+12,8);
 for(int k=0;k<8;k++){
  u32 dest=(k+1)*6288;memcpy(at(dest),at(0),6288*512);at(dest+144+3*16)[0]=k+40;
  Entry*e=(void*)(cat+32+k*48);sprintf(e->name,"snapshot%d",k);e->base=dest;e->crc=native_crc32c(at(dest),6288*512);e->id=k+1;
 }
 memcpy(at(BACKUP_OFF),at(0),6288*512);Seal*s=(void*)at(BACKUP_OFF-1);memcpy(s->magic,"TMCSNAP2",8);s->sectors=6288;s->ver=1;sha_bytes(at(BACKUP_OFF),6288*512,s->digest);
 writes=0;failure=-1;extent=65536;
}
static void verify_all(u32 size){
 NativeGeometry g;assert(native_geometry(at(0),&g)&&g.sectors==size);assert(native_image_check(at(0),&g));
 for(int j=0;j<8192;j++){assert(at(144+3*16)[j]==j%251);assert(at(g.backup+144+3*16)[j]==j%251);}
 for(int j=0;j<1024;j++)assert(at(13)[j]=='A'&&at(g.backup+13)[j]=='A');
 Entry*es=(void*)(at(136)+32);
 for(int k=0;k<8;k++){
  assert(es[k].base==(k+1)*g.image);assert(native_image_check(at(es[k].base),&g));
  assert(native_crc32c(at(es[k].base),g.image*512)==es[k].crc);for(int j=0;j<8192;j++)assert(at(es[k].base+144+3*16)[j]==(j?j%251:k+40));
 }
 Seal*s=(void*)at(g.backup-1);u8 sum[32];sha_bytes(at(g.backup),g.image*512,sum);assert(s->sectors==g.image&&!memcmp(sum,s->digest,32));
 for(u32 j=0;j<BASE*512;j++)assert(media[j]=='P');for(u32 j=(BASE+65536)*512;j<TOTAL*512;j++)assert(media[j]=='S');
}
static void load_driver(void){
 assert(native_geometry(at(0),&native_geo));active_slots=native_geo.slots;frame_sectors=native_geo.image;extent=native_geo.sectors?native_geo.sectors:65536;
 memset(nodes,0,sizeof nodes);memcpy(nodes,at(NODE0),active_slots*64);block=(Block){br,bw,extent};ovl_slot=-1;enc_state=0;
}
static int allocate_to_limit(void){
 load_driver();int created=0;char name[48];
 for(unsigned i=0;i<384;i++){sprintf(name,"/new%u",i);int id=fs_create_i(name,0);if(id<0)break;created++;Process p={0};p.fd[0]=(FD){id,nodes[id].gen,0,1};u8 value=0x72;assert(fs_write_i(&p,0,&value,1)==1);u8 got=0;p.fd[0].at=0;assert(fs_read_i(&p,0,&got,1)==1&&got==value);}
 assert(created==(int)active_slots-2);return created;
}
int main(void){
 media=malloc((size_t)TOTAL*512);assert(media);Medium m={io,flush,0,TOTAL};DiskEdit q={.start=BASE,.sectors=65536,.size=49152};
 fixture();load_driver();seeding=1;
 int volumes=seed("/Volumes",1,1,0);seeding=0;
 assert(volumes>0&&nodes[volumes].kind==1&&nodes[volumes].ro);
 puts("PASS trusted boot seed creates the backing /Volumes before public namespace protection; no public-write bypass");
 fixture();u8 beforehash[32],afterhash[32];sha_bytes(media,TOTAL*512,beforehash);assert(!native_resize(&m,&q,0)&&!writes);sha_bytes(media,TOTAL*512,afterhash);assert(!memcmp(beforehash,afterhash,32));assert(!native_resize(&m,&q,1));verify_all(49152);load_driver();assert(active_slots==288);
 puts("PASS real native 32->24MiB: 384->288 allocatable slots; all 8 snapshots relocated/CRC-verified, backup SHA-verified, accounts/files/canaries preserved");
 q.sectors=49152;q.size=65536;assert(!native_resize(&m,&q,0));assert(!native_resize(&m,&q,1));verify_all(65536);load_driver();assert(active_slots==384);
 puts("PASS native 24->32MiB: all snapshot/backup contents preserved while real inode/data capacity grows");
 int large=allocate_to_limit();fixture();q.sectors=65536;q.size=49152;assert(!native_resize(&m,&q,1));int small=allocate_to_limit();assert(large>small);
 printf("PASS actual fs_create/fs_write/fs_read enforce variable storage: %d vs %d additional 8KiB slots\n",large,small);
 fixture();((Node*)at(64))[350]=(Node){.kind=2};strcpy(((Node*)at(64))[350].name,"must-survive");sha_bytes(media,TOTAL*512,beforehash);assert(native_resize(&m,&q,1)==DE_OCCUPIED&&!writes);sha_bytes(media,TOTAL*512,afterhash);assert(!memcmp(beforehash,afterhash,32));puts("PASS allocated inode tail refuses shrink without writes");
 for(int fail=0;fail<12;fail++){fixture();failure=fail;int r=native_resize(&m,&q,1);assert(r==DE_IO);if(fail>0){NativeGeometry g;assert(!native_geometry(at(0),&g));}}
 puts("PASS injected write errors never report success; incomplete header refuses mount");
 fixture();at(6288+144+3*16)[0]^=1;assert(native_resize(&m,&q,1)==DE_PROTECTED&&!writes);puts("PASS corrupt snapshot refuses resize before any write");
 free(media);return 0;
}
