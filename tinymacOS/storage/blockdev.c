/* SPDX-License-Identifier: MIT
 * Compatibility ABI: sata_* dispatches AHCI/NVMe/canonical USB BOT devices.
 * Disk indices never slide after USB disconnect; tombstones have size zero.
 * The installer ABI remains eight u64/512-B devices with explicit 4Kn geometry.
 */
#include "../kernel/k.h"
extern int tmc_usb_stack_storage_count(void),tmc_usb_stack_storage_io(int,u64,unsigned,void *,int),tmc_usb_stack_storage_flush(int);
extern u64 tmc_usb_stack_storage_size(int);
extern unsigned tmc_usb_stack_storage_bytes(int),tmc_usb_stack_storage_generation(void);
extern const char *tmc_usb_stack_storage_model(int);
extern void tmc_usb_stack_poll(void),usb_delay_ms(unsigned);
extern u64 tmc_lx_os_clock_ms(void);
static Spin disk_lock;
static int ahci_disks,nvme_disks;
void storage_init(void){
 ahci_init();nvme_init();ahci_disks=ahci_count();nvme_disks=nvme_count();
 /* Canonical USB was started once, before filesystem discovery. Give deferred
  * hub enumeration/BOT probing a bounded initial window independent of HID. */
 u64 until=tmc_lx_os_clock_ms()+1000;
 do{tmc_usb_stack_poll();usb_delay_ms(1);}while(tmc_lx_os_clock_ms()<until);
 if(ahci_disks+nvme_disks+tmc_usb_stack_storage_count()>SETUP_MAX_DISKS)boot_logstr("[storage] device table full; additional devices not exposed\n");
}
int sata_count(void){return MIN(SETUP_MAX_DISKS,ahci_disks+nvme_disks+tmc_usb_stack_storage_count());}
u64 sata_size(int d){if(d<0||d>=sata_count())return 0;if(d<ahci_disks)return ahci_size(d);d-=ahci_disks;return d<nvme_disks?nvme_size(d):tmc_usb_stack_storage_size(d-nvme_disks);}
const char*sata_model(int d){if(d<0||d>=sata_count())return "";if(d<ahci_disks)return ahci_model(d);d-=ahci_disks;return d<nvme_disks?nvme_model(d):tmc_usb_stack_storage_model(d-nvme_disks);}
int sata_io(int d,u64 l,u32 n,void*p,int w){
 if(d<0||d>=sata_count()||!p||!n||n>256||(w!=0&&w!=1))return -1;
 u32 flags;spin_lock(&disk_lock,&flags);int r;
 if(d<ahci_disks)r=ahci_io(d,l,n,p,w);else if(d<ahci_disks+nvme_disks)r=nvme_io(d-ahci_disks,l,n,p,w);else r=tmc_usb_stack_storage_io(d-ahci_disks-nvme_disks,l,n,p,w);
 spin_unlock(&disk_lock,flags);return r;
}
int sata_flush(int d){
 if(d<0||d>=sata_count())return -1;u32 flags;spin_lock(&disk_lock,&flags);int r;
 if(d<ahci_disks)r=ahci_flush(d);else if(d<ahci_disks+nvme_disks)r=nvme_flush(d-ahci_disks);else r=tmc_usb_stack_storage_flush(d-ahci_disks-nvme_disks);
 spin_unlock(&disk_lock,flags);return r;
}
extern u32 nvme_logical_bytes(int);
u32 sata_logical_bytes(int d){if(d<0||d>=sata_count())return 0;if(d<ahci_disks)return 512;d-=ahci_disks;return d<nvme_disks?nvme_logical_bytes(d):tmc_usb_stack_storage_bytes(d-nvme_disks);}

unsigned sata_generation(void){return tmc_usb_stack_storage_generation();}
