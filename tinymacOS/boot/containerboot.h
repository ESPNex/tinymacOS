#pragma once
#include "container.h"
static NativeContainer container_boot;
static EFI_BLOCK_IO *container_io;
/* Firmware exposes native logical blocks; kernel coordinates stay 512 B. */
static int container_read(EFI_BLOCK_IO*io,u64 sector,UINTN bytes,void*out)
{
    u32 b=io->Media->BlockSize,scale=b/512;
    if((b!=512&&b!=4096)||bytes%512)return 0;
    if(!(sector%scale)&&!(bytes%b))return !EFI_ERROR(io->ReadBlocks(io,io->Media->MediaId,sector/scale,bytes,out));
    u8 unit[4096] ALIGNED(4096);u8*p=out;
    while(bytes){u32 off=(sector%scale)*512,n=MIN(bytes,b-off);
        if(EFI_ERROR(io->ReadBlocks(io,io->Media->MediaId,sector/scale,b,unit)))return 0;
        memcpy(p,unit+off,n);p+=n;bytes-=n;sector+=n/512;
    }return 1;
}
static int boot_container(EFI_BOOT_SERVICES*bs,EFI_HANDLE d)
{
    EFI_GUID g=EFI_BLOCK_IO_PROTOCOL_GUID;EFI_BLOCK_IO*io=0;NativeContainer h;
    if(EFI_ERROR(bs->HandleProtocol(d,&g,(void**)&io))||!io->Media->LogicalPartition||(io->Media->BlockSize!=512&&io->Media->BlockSize!=4096)||
       (io->Media->LastBlock+1)*(io->Media->BlockSize/512)<TC_SECTORS||!container_read(io,TC_HEADER,512,&h)||!container_bounds(&h,(io->Media->LastBlock+1)*(io->Media->BlockSize/512)))return 0;
    /* Same CRC-32 implementation as the kernel, without a link dependency. */
    u32 saved=h.crc,c=~0u;h.crc=0;
    const u8*p=(const u8*)&h;for(int j=0;j<512;j++){c^=p[j];for(int k=0;k<8;k++)c=(c>>1)^(0xedb88320u&-(c&1));}
    if(saved!=~c)return 0;h.crc=saved;container_boot=h;container_io=io;return 1;
}

static int container_recovery(EFI_BOOT_SERVICES*bs,struct Boot*b)
{
    EFI_PHYSICAL_ADDRESS addr=0xffffffff;const UINTN size=20480u*512;
    if(!container_io||EFI_ERROR(bs->AllocatePages(AllocateMaxAddress,EfiLoaderData,size/4096,&addr)))return 0;
    EFI_BLOCK_IO*io=container_io;
    if(!container_read(io,container_rec_at(&container_boot),size,(void*)addr)){bs->FreePages(addr,size/4096);return 0;}
    /* Recovery has a fixed FAT16 image; follow its directory/cluster chains,
     * not presumed offsets of the DMG file. It is then verified by BASE_SHA. */
    FatImage f={0};u8*p=(u8*)addr;u32 bps=field16(p+11),r=field16(p+14)*bps,fs=field16(p+22)*bps,root=r+p[16]*fs,rs=field16(p+17)*32,ds=root+((rs+bps-1)&~(bps-1)),spc=p[13]*bps;
    int ok=0;if((bps!=512&&bps!=4096)||!spc||(spc&(spc-1))||spc>32768||ds>=size||root+rs>size||r+fs>size)goto done;
    f=(FatImage){p,size,spc,fs,r,root,rs,ds,(size-ds)/spc};
    f.fat12=f.clusters<4085;
    if(!f.clusters||f.clusters>=65525||(f.fat12?((f.clusters+2)*3+1)/2:(f.clusters+2)*2)>fs)goto done;
    const u8*dir=dirent16(&f,0,"RECOVERY   ");if(!dir||!(dir[11]&16))goto done;
    const u8*e=dirent16(&f,field16(dir+26),"BASESY~1DMG");if(!e||(e[11]&16))goto done;
    u32 bytes;memcpy(&bytes,e+28,4);if(bytes!=BASE_LEN)goto done;
    EFI_PHYSICAL_ADDRESS buf=0xffffffff;if(EFI_ERROR(bs->AllocatePages(AllocateMaxAddress,EfiLoaderData,(bytes+4095)/4096,&buf)))goto done;
    u32 cl=field16(e+26),at=0;for(;at<bytes;){if(!clok(&f,cl))break;u32 n=MIN(spc,bytes-at);memcpy((void*)(buf+at),p+ds+(cl-2)*spc,n);at+=n;cl=nextcl(&f,cl);}
    if(at==bytes&&cl>=(f.fat12?0xff8:0xfff8))ok=native_recovery_data(bs,(void*)buf,b);
    bs->FreePages(buf,(bytes+4095)/4096);
 done:bs->FreePages(addr,size/4096);return ok;
}

static void container_boot_config(EFI_BOOT_SERVICES*bs,struct Boot*b)
{
    (void)bs;if(!container_io)return;EFI_BLOCK_IO*io=container_io;
    u8 p[512] ALIGNED(512);
    if(!container_read(io,container_rec_at(&container_boot),512,p)||p[13]!=1||p[16]!=2)return;
    u32 factor=field16(p+11)/512;if(factor!=1&&factor!=8)return;
    u32 reserved=field16(p+14),fat=field16(p+22),roots=field16(p+17),root=(reserved+2*fat)*factor,rs=(roots*32+factor*512-1)/(factor*512)*factor,data=root+rs;
    if(!reserved||!fat||roots>512||data>=20480)return;
    for(u32 i=0;i<rs;i++){
        if(!container_read(io,container_rec_at(&container_boot)+root+i,512,p))return;
        for(int j=0;j<512;j+=32)if(!memcmp(p+j,"BOOTCFG BIN",11)&&!(p[j+11]&24)){
            u32 cl=field16(p+j+26);if(cl<2||cl-2>=20480-data)return;
            if(!!container_read(io,container_rec_at(&container_boot)+data+(cl-2)*factor,512,p)&&bootconfig_valid((BootConfig*)p))b->boot_flags=((BootConfig*)p)->flags;return;
        }
    }
}
