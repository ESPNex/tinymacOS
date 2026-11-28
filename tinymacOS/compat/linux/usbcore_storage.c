/* SPDX-License-Identifier: GPL-2.0-only
 * USB Mass Storage Bulk-Only / transparent SCSI class, on canonical usbcore.
 * No UAS, streams or alternate HCD. Only LUN0 is exposed (multi-LUN refused).
 * Exported slots are monotonically reserved, never renumbered/reused after
 * disconnect: a stale filesystem handle can never write a replacement disk.
 */
#include <tmc_kernel.h>
#include <linux/usb.h>
#include <usb.h>
#define STORAGE_SLOTS 8
#define BOT_TIMEOUT 3000
struct storage_disk {
 struct usb_device *dev;unsigned interface,in,out;u32 tag,bytes;
 u64 blocks;int reserved,online,pending,busy,dead;char model[41];
};
static struct storage_disk disks[STORAGE_SLOTS];
static unsigned slots;
static unsigned storage_generation=1;
extern void tmc_usbcore_os_storage_changed(void);
static void storage_changed(void){storage_generation++;tmc_usbcore_os_storage_changed();}
static u8 block_buffer[4096]; /* serialized by storage_busy, including RMW */
static int storage_busy;
static u32 be32get(const u8 *p){return ((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3];}
static u64 be64get(const u8 *p){return ((u64)be32get(p)<<32)|be32get(p+4);}
static void be32put(u8 *p,u32 n){p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n;}
static void be64put(u8 *p,u64 n){be32put(p,n>>32);be32put(p+4,n);}
static u32 le32get(const u8 *p){return p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24);}
static void le32put(u8 *p,u32 n){p[0]=n;p[1]=n>>8;p[2]=n>>16;p[3]=n>>24;}
static int bot_reset(struct storage_disk *d)
{
 if(d->dead)return -ENODEV;
 int r=usb_control_msg(d->dev,usb_sndctrlpipe(d->dev,0),0xff,0x21,0,d->interface,NULL,0,BOT_TIMEOUT);
 /* BOT reset recovery ordering is class reset, clear IN, clear OUT. */
 int a=usb_clear_halt(d->dev,d->in),b=usb_clear_halt(d->dev,d->out);
 return r<0?r:a?a:b;
}
/* Returns 0 success, 1 command failed (REQUEST SENSE possible), negative
 * transport error. Never silently replay a write after transport failure. */
static int bot_command(struct storage_disk *d,const u8 *cdb,unsigned clen,void *data,unsigned length,int input,unsigned *transferred)
{
 u8 cbw[31]={0},csw[13]={0};int got=0,r,stalled=0;unsigned actual=0;
 if(transferred)*transferred=0;
 if(d->dead||!d->dev)return -ENODEV;
 if(!clen||clen>16||length>65536||(length&&!data))return -EINVAL;
 u32 tag=++d->tag;le32put(cbw,0x43425355);le32put(cbw+4,tag);le32put(cbw+8,length);cbw[12]=input?0x80:0;cbw[14]=clen;memcpy(cbw+15,cdb,clen);
 r=usb_bulk_msg(d->dev,d->out,cbw,sizeof cbw,&got,BOT_TIMEOUT);
 if(r||got!=sizeof cbw)goto transport;
 if(length){
  got=0;r=usb_bulk_msg(d->dev,input?d->in:d->out,data,length,&got,BOT_TIMEOUT);
  if(got<0||(unsigned)got>length)goto transport;
  actual=got;
  if(r==-EPIPE){stalled=1;r=usb_clear_halt(d->dev,input?d->in:d->out);}
  if(r)goto transport;
 }
 got=0;r=usb_bulk_msg(d->dev,d->in,csw,sizeof csw,&got,BOT_TIMEOUT);
 if(r==-EPIPE){r=usb_clear_halt(d->dev,d->in);if(r)goto transport;got=0;r=usb_bulk_msg(d->dev,d->in,csw,sizeof csw,&got,BOT_TIMEOUT);}
 if(r||got!=sizeof csw||le32get(csw)!=0x53425355||le32get(csw+4)!=tag||csw[12]>2||le32get(csw+8)>length||csw[12]==2)goto transport;
 if(transferred)*transferred=actual;
 if(csw[12]==1)return 1;
 /* Successful block commands must account for every byte. A short INQUIRY
  * or SENSE is allowed when residue agrees, but never a truncated block I/O. */
 if(stalled||actual!=length-le32get(csw+8))goto transport;
 return 0;
transport:
 if(!r)r=-EIO;
 if(bot_reset(d)&&d->online){d->online=0;storage_changed();}
 return r<0?r:-EIO;
}
static int scsi_command(struct storage_disk *d,const u8 *cdb,unsigned clen,void *data,unsigned len,int input,unsigned *actual)
{
 int r=bot_command(d,cdb,clen,data,len,input,actual);
 if(r==1){u8 sense[18]={0},cmd[6]={3,0,0,0,18,0};unsigned n;/* consume sense, no automatic write retries */
  (void)bot_command(d,cmd,6,sense,sizeof sense,1,&n);return -EIO;}
 return r;
}
static int storage_identify(struct storage_disk *d)
{
 u8 data[96]={0},cmd[16]={0x12,0,0,0,36,0};unsigned n=0;int r;
 r=scsi_command(d,cmd,6,data,36,1,&n);if(r||n<36||(data[0]&31)!=0)return -ENODEV;
 unsigned end=0;for(unsigned i=8;i<36;i++){u8 c=data[i];d->model[end++]=(c>=32&&c<127)?c:' ';}while(end&&d->model[end-1]==' ')end--;d->model[end]=0;
 /* Bounded readiness retries for initial unit attention / spin up. */
 int ready=0;for(unsigned i=0;i<3;i++){memset(cmd,0,sizeof cmd);r=scsi_command(d,cmd,6,NULL,0,0,NULL);if(!r){ready=1;break;}if(d->dead)return -ENODEV;}
 if(!ready)return -EIO;
 memset(cmd,0,sizeof cmd);cmd[0]=0x25;r=scsi_command(d,cmd,10,data,8,1,&n);if(r||n!=8)return -EIO;
 u64 last=be32get(data);u32 bytes=be32get(data+4);
 if(last==0xffffffffu){memset(cmd,0,sizeof cmd);cmd[0]=0x9e;cmd[1]=0x10;be32put(cmd+10,32);r=scsi_command(d,cmd,16,data,32,1,&n);if(r||n!=32)return -EIO;last=be64get(data);bytes=be32get(data+8);}
 if(last==~(u64)0||(bytes!=512&&bytes!=4096)||last+1>~(u64)0/bytes)return -EOVERFLOW;
 d->blocks=last+1;d->bytes=bytes;return 0;
}
static int storage_probe(struct usb_interface *intf,const struct usb_device_id *id)
{
 (void)id;struct usb_host_interface *a=intf->cur_altsetting;struct usb_device *dev=interface_to_usbdev(intf);unsigned in=0,out=0;
 if(slots>=STORAGE_SLOTS||a->desc.bInterfaceSubClass!=6||a->desc.bInterfaceProtocol!=0x50)return -ENODEV;
 for(unsigned i=0;i<a->desc.bNumEndpoints;i++){struct usb_endpoint_descriptor *e=&a->endpoint[i].desc;if(!usb_endpoint_xfer_bulk(e))continue;if(usb_endpoint_dir_in(e)){if(in)return -EINVAL;in=usb_rcvbulkpipe(dev,usb_endpoint_num(e));}else{if(out)return -EINVAL;out=usb_sndbulkpipe(dev,usb_endpoint_num(e));}}
 if(!in||!out)return -ENODEV;
 struct storage_disk *d=&disks[slots++];d->dev=usb_get_dev(dev);d->interface=a->desc.bInterfaceNumber;d->in=in;d->out=out;d->pending=1;d->reserved=1;usb_set_intfdata(intf,d);return 0;
}
static void storage_disconnect(struct usb_interface *intf)
{
 struct storage_disk *d=usb_get_intfdata(intf);usb_set_intfdata(intf,NULL);if(!d)return;
 d->online=0;d->pending=0;d->dead=1;storage_changed();
 /* The slot owns the ref until no synchronous command can hold the device. */
 if(!d->busy){usb_put_dev(d->dev);d->dev=NULL;}
}
static const struct usb_device_id storage_ids[]={ {USB_INTERFACE_INFO(8,6,0x50)},{} };
static struct usb_driver storage_driver={.name="tinymacos-bot",.probe=storage_probe,.disconnect=storage_disconnect,.id_table=storage_ids,.supports_autosuspend=0};
int tmc_usb_storage_init(void){return usb_register(&storage_driver);}
/* Called outside core dispatch, never recursively inside a sleeping hub worker. */
void tmc_usb_storage_poll(void)
{
 if(storage_busy)return;storage_busy=1;
 for(unsigned i=0;i<slots;i++){struct storage_disk *d=&disks[i];if(!d->pending||d->dead)continue;d->pending=0;d->busy=1;
  u8 lun=0;int r=usb_control_msg(d->dev,usb_rcvctrlpipe(d->dev,0),0xfe,0xa1,0,d->interface,&lun,1,BOT_TIMEOUT);
  /* GET_MAX_LUN STALL is specified for single-LUN devices. Other failures
   * or multiple LUNs are explicit unsupported, not misreported as one disk. */
  if((r==1&&lun==0)||r==-EPIPE){r=storage_identify(d);if(!r&&!d->dead){d->online=1;storage_changed();}}
  d->busy=0;if(d->dead&&d->dev){usb_put_dev(d->dev);d->dev=NULL;}
 }storage_busy=0;
}
unsigned tmc_usb_stack_storage_generation(void){return storage_generation;}
int tmc_usb_stack_storage_count(void){return slots;}
u64 tmc_usb_stack_storage_size(int i){return i>=0&&(unsigned)i<slots&&disks[i].online?disks[i].blocks*(disks[i].bytes/512):0;}
unsigned tmc_usb_stack_storage_bytes(int i){return i>=0&&(unsigned)i<slots&&disks[i].online?disks[i].bytes:0;}
const char *tmc_usb_stack_storage_model(int i){return i>=0&&(unsigned)i<slots?disks[i].model:"";}
static struct storage_disk *storage_enter(int i)
{
 if(storage_busy||i<0||(unsigned)i>=slots||!disks[i].online||disks[i].dead)return NULL;
 storage_busy=1;disks[i].busy=1;return &disks[i];
}
static void storage_leave(struct storage_disk *d){d->busy=0;if(d->dead&&d->dev){usb_put_dev(d->dev);d->dev=NULL;}storage_busy=0;}
static int storage_blocks(struct storage_disk *d,u64 lba,unsigned count,void *data,int write)
{
 while(count){unsigned n=min_t(unsigned,count,65536/d->bytes),clen;u8 cdb[16]={0};
  if(lba<=0xffffffffu&&n-1<=0xffffffffu-lba){clen=10;cdb[0]=write?0x2a:0x28;be32put(cdb+2,lba);cdb[7]=n>>8;cdb[8]=n;}else{clen=16;cdb[0]=write?0x8a:0x88;be64put(cdb+2,lba);be32put(cdb+10,n);}unsigned actual=0;
  int r=scsi_command(d,cdb,clen,data,n*d->bytes,!write,&actual);if(r||actual!=n*d->bytes)return r?r:-EIO;
  count-=n;lba+=n;data=(u8 *)data+n*d->bytes;
 }return 0;
}
int tmc_usb_stack_storage_io(int i,u64 lba,unsigned count,void *data,int write)
{
 if(!data||!count||count>256||(write!=0&&write!=1))return -EINVAL;
 struct storage_disk *d=storage_enter(i);if(!d)return -ENODEV;
 unsigned factor=d->bytes/512;u64 sectors=d->blocks*factor;int r=-EINVAL;
 if(lba>=sectors||count>sectors-lba)goto done;
 /* Public common-block coordinates are 512 bytes even for true 4Kn media.
  * Preserve surrounding physical-sector bytes with locked read/modify/write;
  * partition parsers still receive the true 4096-byte logical geometry. */
 while(count){unsigned at=lba%factor,take;
  if(at||count<factor){
   take=min_t(unsigned,count,factor-at);r=storage_blocks(d,lba/factor,1,block_buffer,0);if(r)break;
   if(write){memcpy(block_buffer+at*512,data,take*512);r=storage_blocks(d,lba/factor,1,block_buffer,1);if(r)break;}
   else memcpy(data,block_buffer+at*512,take*512);
  }else{
   take=min_t(unsigned,count/factor,65536/d->bytes)*factor;r=storage_blocks(d,lba/factor,take/factor,data,write);if(r)break;
  }
  r=0;lba+=take;count-=take;data=(u8 *)data+take*512;
 }
done: storage_leave(d);return r;
}
int tmc_usb_stack_storage_flush(int i)
{
 struct storage_disk *d=storage_enter(i);if(!d)return -ENODEV;
 u8 cdb[10]={0x35};int r=scsi_command(d,cdb,10,NULL,0,0,NULL);storage_leave(d);return r;
}

/* Logical safe removal only: never deconfigure a composite device or power
 * down its port (that could interrupt unrelated HID interfaces). Storage is
 * inaccessible until physical reconnect; SYNCHRONIZE CACHE is mandatory. */
int tmc_usb_stack_storage_ejectable(int i){return i>=0&&(unsigned)i<slots&&disks[i].online&&!disks[i].dead;}
int tmc_usb_stack_storage_eject(int i){
 struct storage_disk *d=storage_enter(i);if(!d)return -ENODEV;
 u8 cdb[10]={0x35};int r=scsi_command(d,cdb,10,NULL,0,0,NULL);
 if(!r){d->online=0;storage_changed();}storage_leave(d);return r;
}
