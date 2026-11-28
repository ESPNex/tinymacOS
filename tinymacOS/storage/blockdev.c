/* SPDX-License-Identifier: MIT
 * Common 512-byte/u64 block ABI. A grow-on-demand stable registry removes
 * the installer's eight-disk display limit from mounted image devices.
 * Physical tombstones are never reused; detached RO image slots may be reused
 * after fs_gate has invalidated the old mounted-volume generation.
 */
#include "../kernel/k.h"
extern int tmc_usb_stack_storage_count(void),tmc_usb_stack_storage_io(int,u64,unsigned,void*,int),tmc_usb_stack_storage_flush(int);
extern int tmc_usb_stack_storage_ejectable(int),tmc_usb_stack_storage_eject(int);
extern u64 tmc_usb_stack_storage_size(int);
extern unsigned tmc_usb_stack_storage_bytes(int),tmc_usb_stack_storage_generation(void);
extern const char*tmc_usb_stack_storage_model(int);
extern void tmc_usb_stack_poll(void),usb_delay_ms(unsigned);
extern u64 tmc_lx_os_clock_ms(void);
extern u32 nvme_logical_bytes(int);
extern int vol_source_write_blocked(int,u64,u32) __attribute__((weak));
extern int vol_source_busy(int,u64,u64) __attribute__((weak));
static Spin disk_lock,registry_lock;
typedef struct {int kind,index;void*context;int(*read)(void*,u64,void*,size_t);u64 bytes;} BlockSlot;
#define REGISTRY_PAGE_SLOTS 64
/* Initial physical devices need no heap allocation during early discovery. */
static BlockSlot initial[SETUP_MAX_DISKS];
typedef struct RegistryPage {struct RegistryPage*next;BlockSlot slot[REGISTRY_PAGE_SLOTS];} RegistryPage;
static RegistryPage*pages;static int count,usb_seen;static unsigned generation=1;
static BlockSlot*slot_at(int d,int create){
 if(d<0)return NULL;if(d<SETUP_MAX_DISKS)return initial+d;
 unsigned page=(d-SETUP_MAX_DISKS)/REGISTRY_PAGE_SLOTS,index=(d-SETUP_MAX_DISKS)%REGISTRY_PAGE_SLOTS;RegistryPage**link=&pages;
 while(1){if(!*link){if(!create)return NULL;RegistryPage*p=kmalloc(sizeof *p);if(!p)return NULL;memset(p,0,sizeof *p);__atomic_store_n(link,p,__ATOMIC_RELEASE);}if(!page)return &(*link)->slot[index];link=&(*link)->next;page--;}
}
/* registry_lock held; publish a fully initialized slot before its new count. */
static int append_slot(BlockSlot slot){
 if(count==0x7fffffff)return -1;BlockSlot*s=slot_at(count,1);if(!s)return -1;int id=count;*s=slot;__atomic_store_n(&count,count+1,__ATOMIC_RELEASE);__atomic_add_fetch(&generation,1,__ATOMIC_RELEASE);return id;
}
static void sync_usb(void){int n=tmc_usb_stack_storage_count();u32 flags;spin_lock(&registry_lock,&flags);while(usb_seen<n){if(append_slot((BlockSlot){.kind=3,.index=usb_seen})<0)break;usb_seen++;}spin_unlock(&registry_lock,flags);}
void storage_init(void){
 ahci_init();nvme_init();u32 flags;spin_lock(&registry_lock,&flags);
 for(int i=0;i<ahci_count();i++)if(append_slot((BlockSlot){.kind=1,.index=i})<0)break;
 for(int i=0;i<nvme_count();i++)if(append_slot((BlockSlot){.kind=2,.index=i})<0)break;
 spin_unlock(&registry_lock,flags);u64 until=tmc_lx_os_clock_ms()+1000;do{tmc_usb_stack_poll();usb_delay_ms(1);}while(tmc_lx_os_clock_ms()<until);sync_usb();
}
int sata_count(void){sync_usb();return __atomic_load_n(&count,__ATOMIC_ACQUIRE);}
static BlockSlot*device(int d){return d<0||d>=sata_count()?NULL:slot_at(d,0);}
u64 sata_size(int d){BlockSlot*s=device(d);return !s?0:s->kind==1?ahci_size(s->index):s->kind==2?nvme_size(s->index):s->kind==3?tmc_usb_stack_storage_size(s->index):s->bytes/512;}
const char*sata_model(int d){BlockSlot*s=device(d);return !s?"":s->kind==1?ahci_model(s->index):s->kind==2?nvme_model(s->index):s->kind==3?tmc_usb_stack_storage_model(s->index):"Immagine disco (sola lettura)";}
int sata_readonly(int d){BlockSlot*s=device(d);return s&&s->kind==4;}
int sata_is_image(int d){return sata_readonly(d);}
int sata_io(int d,u64 l,u32 n,void*p,int w){
 BlockSlot*s=device(d);if(!s||!p||!n||n>256||(w!=0&&w!=1))return -1;u64 z=sata_size(d);if(l>=z||n>z-l)return -1;
 /* Image callbacks recurse into physical storage: never hold disk_lock.
  * fs_gate protects image callbacks, source objects and detach/slot reuse. */
 if(s->kind==4){u32 flags;rec_lock(&fs_gate,&flags);z=s->bytes/512;int r=w||!s->read||l>=z||n>z-l?-1:s->read(s->context,l*512,p,(size_t)n*512);rec_unlock(&fs_gate,flags);return r;}
 /* Freeze check and physical write are one serialized transaction. Merely
  * checking the source list before acquiring the lock would permit races. */
 u32 writeflags=0;if(w){rec_lock(&fs_gate,&writeflags);if(vol_source_write_blocked&&vol_source_write_blocked(d,l,n)){rec_unlock(&fs_gate,writeflags);return -1;}}
 u32 flags;spin_lock(&disk_lock,&flags);int r=s->kind==1?ahci_io(s->index,l,n,p,w):s->kind==2?nvme_io(s->index,l,n,p,w):s->kind==3?tmc_usb_stack_storage_io(s->index,l,n,p,w):-1;
 spin_unlock(&disk_lock,flags);if(w)rec_unlock(&fs_gate,writeflags);return r;
}
int sata_flush(int d){BlockSlot*s=device(d);if(!s)return -1;u32 flags;spin_lock(&disk_lock,&flags);int r=s->kind==1?ahci_flush(s->index):s->kind==2?nvme_flush(s->index):s->kind==3?tmc_usb_stack_storage_flush(s->index):s->bytes?0:-1;spin_unlock(&disk_lock,flags);return r;}
u32 sata_logical_bytes(int d){BlockSlot*s=device(d);return !s?0:s->kind==2?nvme_logical_bytes(s->index):s->kind==3?tmc_usb_stack_storage_bytes(s->index):512;}
unsigned sata_generation(void){return __atomic_load_n(&generation,__ATOMIC_ACQUIRE)+tmc_usb_stack_storage_generation();}
int sata_ejectable(int d){BlockSlot*s=device(d);return s&&s->kind==3&&tmc_usb_stack_storage_ejectable(s->index);}
int sata_eject(int d){
 BlockSlot*s=device(d);if(!s||!sata_ejectable(d))return -1;u32 fsflags;rec_lock(&fs_gate,&fsflags);
 if(vol_source_busy&&vol_source_busy(d,0,sata_size(d))){rec_unlock(&fs_gate,fsflags);return -1;}
 u32 flags;spin_lock(&disk_lock,&flags);int r=tmc_usb_stack_storage_eject(s->index);spin_unlock(&disk_lock,flags);rec_unlock(&fs_gate,fsflags);return r;
}
void tmc_usbcore_os_storage_changed(void){fsrev++;}
int block_image_add(void*context,u64 bytes,int(*read)(void*,u64,void*,size_t)){
 sync_usb();if(!read||!bytes||bytes%512)return -1;u32 flags;spin_lock(&registry_lock,&flags);int d=-1;
 for(int j=0;j<count;j++){BlockSlot*s=slot_at(j,0);if(s&&s->kind==4&&!s->bytes){d=j;break;}}
 if(d<0)d=append_slot((BlockSlot){.kind=4,.context=context,.read=read,.bytes=bytes});else{*slot_at(d,0)=(BlockSlot){.kind=4,.context=context,.read=read,.bytes=bytes};__atomic_add_fetch(&generation,1,__ATOMIC_RELEASE);}
 if(d>=0)fsrev++;spin_unlock(&registry_lock,flags);return d;
}
int block_image_remove(int d){BlockSlot*s=device(d);if(!s||s->kind!=4||!s->bytes)return -1;u32 flags;spin_lock(&registry_lock,&flags);s->bytes=0;s->read=NULL;s->context=NULL;__atomic_add_fetch(&generation,1,__ATOMIC_RELEASE);fsrev++;spin_unlock(&registry_lock,flags);return 0;}
