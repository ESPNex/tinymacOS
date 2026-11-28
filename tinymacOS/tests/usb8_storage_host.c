/* SPDX-License-Identifier: GPL-2.0-only
 * Test the actual BOT/SCSI production class against an explicit wire model.
 * Not enumeration, a real disk, QEMU or physical controller certification.
 */
#include <assert.h>
#include <stdio.h>
#include "../compat/linux/usbcore_storage.c"
static struct usb_device device;
static u8 wire_cdb[16],media[256*512];
static u32 wire_tag,wire_len,residue;static unsigned phase,resets,clear_count,commands,flushes,writes;
static unsigned badsig,badtag,badresidue,phase_error,command_fail,short_cbw,short_data,csw_stall,data_stall,timeout_cbw,big_capacity,large_sector;
static unsigned clear_sequence[8],sequence,reset_fail,large_io,get_refs,put_refs;
static void fixture(void)
{
 memset(disks,0,sizeof disks);slots=1;storage_busy=0;phase=0;resets=clear_count=sequence=reset_fail=large_io=get_refs=put_refs=0;
 badsig=badtag=badresidue=phase_error=command_fail=short_cbw=short_data=csw_stall=data_stall=timeout_cbw=big_capacity=large_sector=0;
 disks[0]=(struct storage_disk){.dev=&device,.in=usb_rcvbulkpipe(&device,2),.out=usb_sndbulkpipe(&device,3),.reserved=1,.online=1,.bytes=512,.blocks=256};
}
int usb_bulk_msg(struct usb_device *dev,unsigned pipe,void *data,int len,int *actual,int timeout)
{
 assert(dev==&device&&timeout==BOT_TIMEOUT);*actual=0;
 if(!phase){
  assert(pipe==disks[0].out&&len==31&&le32get(data)==0x43425355);
  if(timeout_cbw){timeout_cbw=0;return -ETIMEDOUT;}
  if(short_cbw){short_cbw=0;*actual=30;return 0;}
  u8 *p=data;wire_tag=le32get(p+4);wire_len=le32get(p+8);memcpy(wire_cdb,p+15,16);residue=0;commands++;phase=wire_len?1:2;*actual=len;
  if(wire_cdb[0]==0x35)flushes++;
  return 0;
 }
 if(phase==1){
  assert((unsigned)len==wire_len);phase=2;
  if(data_stall){data_stall=0;residue=len;return -EPIPE;}
  if(pipe==disks[0].in)memset(data,0,len);*actual=short_data?len-1:len;residue=len-*actual;
  switch(wire_cdb[0]){
   case 0x12: {u8 *p=data;p[0]=0;memcpy(p+8,"TestUSB Canonical BOT Model  ",28);break;}
   case 0x25: be32put(data,big_capacity?0xffffffff:255);be32put((u8*)data+4,large_sector?4096:512);break;
   case 0x9e: be64put(data,((u64)1<<33)-1);be32put((u8*)data+8,large_sector?4096:512);break;
   case 0x28: case 0x2a: case 0x88: case 0x8a: {
    int cmd10=wire_cdb[0]<0x80;u64 lba=cmd10?be32get(wire_cdb+2):be64get(wire_cdb+2);unsigned blocks=cmd10?((unsigned)wire_cdb[7]<<8)|wire_cdb[8]:be32get(wire_cdb+10);assert(blocks*disks[0].bytes==(unsigned)len);if(large_io){assert(!cmd10&&lba==((u64)1<<32)+1);lba=1;}assert((lba+blocks)*disks[0].bytes<=sizeof media);
    if(wire_cdb[0]==0x88||wire_cdb[0]==0x28){assert(pipe==disks[0].in);memcpy(data,media+lba*disks[0].bytes,*actual);}
    else {assert(pipe==disks[0].out);memcpy(media+lba*disks[0].bytes,data,*actual);writes++;}
    break;
   }
   case 3: break;
   default: assert(0);
  }
  return 0;
 }
 assert(phase==2&&pipe==disks[0].in&&len==13);
 if(csw_stall){csw_stall=0;return -EPIPE;}
 phase=0;memset(data,0,len);u8 *p=data;le32put(p,badsig?0:0x53425355);le32put(p+4,wire_tag+(badtag?1:0));le32put(p+8,badresidue?wire_len+1:residue);p[12]=phase_error?2:command_fail?1:0;command_fail=0;*actual=len;return 0;
}
int usb_control_msg(struct usb_device *d,unsigned pipe,u8 request,u8 type,u16 value,u16 index,void *data,u16 len,int timeout)
{
 (void)pipe;(void)index;assert(d==&device&&timeout==BOT_TIMEOUT&&!value);
 assert(request==0xff&&type==0x21&&!data&&!len);resets++;sequence=1;phase=0;return reset_fail?-EIO:0;
}
int usb_clear_halt(struct usb_device *d,int pipe)
{
 assert(d==&device);if(sequence){assert(sequence<8);clear_sequence[sequence++]=pipe;}clear_count++;return 0;
}
struct usb_device *usb_get_dev(struct usb_device *d){get_refs++;return d;}
void usb_put_dev(struct usb_device *d){assert(d==&device);put_refs++;}
int main(void)
{
 fixture();assert(!storage_identify(&disks[0]));assert(disks[0].blocks==256&&disks[0].bytes==512);assert(!strncmp(disks[0].model,"TestUSB",7));
 fixture();big_capacity=1;large_sector=1;assert(!storage_identify(&disks[0]));assert(disks[0].blocks==((u64)1<<33)&&disks[0].bytes==4096);
 fixture();u8 data[131072];for(unsigned i=0;i<sizeof media;i++)media[i]=i*17+3;memset(data,0,sizeof data);
 assert(!tmc_usb_stack_storage_io(0,0,256,data,0));assert(!memcmp(data,media,sizeof data));assert(commands>=2);
 memset(data,0x9b,sizeof data);assert(!tmc_usb_stack_storage_io(0,0,256,data,1));assert(!memcmp(data,media,sizeof data)&&writes==2);
 unsigned old=commands;assert(tmc_usb_stack_storage_io(0,255,2,data,0)<0);assert(tmc_usb_stack_storage_io(0,~(u64)0,2,data,0)<0);assert(commands==old);
 assert(!tmc_usb_stack_storage_flush(0)&&flushes==1);
 disks[0].bytes=4096;disks[0].blocks=32;assert(tmc_usb_stack_storage_size(0)==256);
 assert(!tmc_usb_stack_storage_io(0,1,8,data,0));assert(!memcmp(data,media+512,4096));
 u8 saved[8192];memcpy(saved,media,sizeof saved);memset(data,0x73,512);
 assert(!tmc_usb_stack_storage_io(0,1,1,data,1));assert(!memcmp(media,saved,512));assert(!memcmp(media+512,data,512));assert(!memcmp(media+1024,saved+1024,sizeof saved-1024));
 assert(!tmc_usb_stack_storage_io(0,0,8,data,0));
 for(unsigned f=0;f<7;f++){
  fixture();switch(f){case 0:badsig=1;break;case 1:badtag=1;break;case 2:badresidue=1;break;case 3:phase_error=1;break;case 4:short_cbw=1;break;case 5:timeout_cbw=1;break;case 6:data_stall=1;break;}
  assert(tmc_usb_stack_storage_io(0,0,1,data,0)<0);assert(resets==1&&sequence==3&&clear_sequence[1]==disks[0].in&&clear_sequence[2]==disks[0].out);
 }
 fixture();csw_stall=1;assert(!tmc_usb_stack_storage_io(0,0,1,data,0));assert(clear_count==1&&!resets);
 fixture();short_data=1;assert(tmc_usb_stack_storage_io(0,0,1,data,0)<0);assert(!resets);/* valid residue, but block read incomplete */
 fixture();command_fail=1;old=commands;assert(tmc_usb_stack_storage_io(0,0,1,data,0)<0);assert(commands==old+2&&!resets);/* REQUEST SENSE, no data-command replay */
 fixture();large_io=1;disks[0].blocks=((u64)1<<32)+256;assert(!tmc_usb_stack_storage_io(0,((u64)1<<32)+1,1,data,0));assert(!memcmp(data,media+512,512));
 fixture();badsig=reset_fail=1;unsigned before_gen=storage_generation;assert(tmc_usb_stack_storage_io(0,0,1,data,1)<0);assert(!disks[0].online&&storage_generation==before_gen+1);old=commands;assert(tmc_usb_stack_storage_io(0,0,1,data,1)<0&&commands==old);
 fixture();struct usb_interface intf={0};usb_set_intfdata(&intf,&disks[0]);storage_disconnect(&intf);assert(disks[0].dead&&!disks[0].online&&!disks[0].dev&&put_refs==1);assert(tmc_usb_stack_storage_count()==1);assert(tmc_usb_stack_storage_io(0,0,1,data,0)<0);
 fixture();disks[0].busy=1;usb_set_intfdata(&intf,&disks[0]);storage_disconnect(&intf);assert(!put_refs&&disks[0].dev);storage_leave(&disks[0]);assert(put_refs==1&&!disks[0].dev);
 fixture();disks[0].dead=1;assert(!tmc_usb_stack_storage_size(0)||tmc_usb_stack_storage_io(0,0,1,data,0)<0);assert(tmc_usb_stack_storage_flush(0)<0);
 fixture();storage_busy=1;assert(tmc_usb_stack_storage_io(0,0,1,data,0)<0);assert(!commands||phase==0);
 puts("PASS production BOT/SCSI wire MODEL: inquiry/readiness/capacity10+16, 4Kn unaligned read/RMW preservation, 128KiB split reads, exact data, flush, bounds/overflow, CSW signature/tag/residue/phase, short/STALL/timeout/reset ordering, request sense, no replay, offline and busy rejection");
}
