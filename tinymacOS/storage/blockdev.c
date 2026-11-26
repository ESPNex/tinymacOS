/* SPDX-License-Identifier: MIT
 * Compatibility ABI: sata_* now dispatches block devices, not bus-specific I/O.
 * The existing installer/volume ABI is still eight 512-B/u32 devices. Never
 * truncate a larger capacity or pretend a physical 4Kn GPT is a 512-B GPT.
 */
#include "../kernel/k.h"
static Spin disk_lock;
void storage_init(void){ahci_init();nvme_init();if(ahci_count()+nvme_count()>SETUP_MAX_DISKS)boot_logstr("[storage] device table full; additional devices not exposed\n");}
int sata_count(void){return MIN(SETUP_MAX_DISKS,ahci_count()+nvme_count());}
u32 sata_size(int d){if(d<0||d>=sata_count())return 0;int a=ahci_count();return d<a?ahci_size(d):nvme_size(d-a);}
const char*sata_model(int d){if(d<0||d>=sata_count())return "";int a=ahci_count();return d<a?ahci_model(d):nvme_model(d-a);}
int sata_io(int d,u32 l,u32 n,void*p,int w){
 if(d<0||d>=sata_count()||!p||!n||n>256||(w!=0&&w!=1))return -1;
 u32 flags;spin_lock(&disk_lock,&flags);int a=ahci_count();
 int r=d<a?ahci_io(d,l,n,p,w):nvme_io(d-a,l,n,p,w);spin_unlock(&disk_lock,flags);return r;
}
int sata_flush(int d){
 if(d<0||d>=sata_count())return -1;u32 flags;spin_lock(&disk_lock,&flags);int a=ahci_count();
 int r=d<a?ahci_flush(d):nvme_flush(d-a);spin_unlock(&disk_lock,flags);return r;
}
