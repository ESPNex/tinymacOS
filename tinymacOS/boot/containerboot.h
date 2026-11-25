#pragma once
#include "container.h"
static NativeContainer container_boot;
static EFI_BLOCK_IO *container_io;
static int boot_container(EFI_BOOT_SERVICES*bs,EFI_HANDLE d)
{
    EFI_GUID g=EFI_BLOCK_IO_PROTOCOL_GUID;EFI_BLOCK_IO*io=0;NativeContainer h;
    if(EFI_ERROR(bs->HandleProtocol(d,&g,(void**)&io))||!io->Media->LogicalPartition||io->Media->BlockSize!=512||
       io->Media->LastBlock<TC_SECTORS-1||EFI_ERROR(io->ReadBlocks(io,io->Media->MediaId,TC_HEADER,512,&h))||!container_bounds(&h,io->Media->LastBlock+1))return 0;
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
    if(EFI_ERROR(io->ReadBlocks(io,io->Media->MediaId,container_boot.rec,size,(void*)addr))){bs->FreePages(addr,size/4096);return 0;}
    /* Recovery has a fixed FAT16 image; follow its directory/cluster chains,
     * not presumed offsets of the DMG file. It is then verified by BASE_SHA. */
    FatImage f={0};u8*p=(u8*)addr;u32 r=field16(p+14)*512,fs=field16(p+22)*512,root=r+p[16]*fs,rs=field16(p+17)*32,ds=root+((rs+511)&~511u),spc=p[13]*512;
    int ok=0;if(field16(p+11)!=512||!spc||(spc&(spc-1))||spc>32768||ds>=size||root+rs>size||r+fs>size)goto done;
    f=(FatImage){p,size,spc,fs,r,root,rs,ds,(size-ds)/spc};
    if(f.clusters<4085||f.clusters>=65525||(f.clusters+2)*2>fs)goto done;
    const u8*dir=dirent16(&f,0,"RECOVERY   ");if(!dir||!(dir[11]&16))goto done;
    const u8*e=dirent16(&f,field16(dir+26),"BASESY~1DMG");if(!e||(e[11]&16))goto done;
    u32 bytes;memcpy(&bytes,e+28,4);if(bytes!=BASE_LEN)goto done;
    EFI_PHYSICAL_ADDRESS buf=0xffffffff;if(EFI_ERROR(bs->AllocatePages(AllocateMaxAddress,EfiLoaderData,(bytes+4095)/4096,&buf)))goto done;
    u32 cl=field16(e+26),at=0;for(;at<bytes;){if(!clok(&f,cl))break;u32 n=MIN(spc,bytes-at);memcpy((void*)(buf+at),p+ds+(cl-2)*spc,n);at+=n;cl=nextcl(&f,cl);}
    if(at==bytes&&cl>=0xfff8)ok=native_recovery_data(bs,(void*)buf,b);
    bs->FreePages(buf,(bytes+4095)/4096);
 done:bs->FreePages(addr,size/4096);return ok;
}

static void container_boot_config(EFI_BOOT_SERVICES*bs,struct Boot*b)
{
    (void)bs;if(!container_io)return;EFI_BLOCK_IO*io=container_io;
    u8 p[512] ALIGNED(512);
    if(EFI_ERROR(io->ReadBlocks(io,io->Media->MediaId,container_boot.rec,512,p))||field16(p+11)!=512||p[13]!=1||p[16]!=2)return;
    u32 reserved=field16(p+14),fat=field16(p+22),roots=field16(p+17),root=reserved+2*fat,rs=(roots*32+511)/512,data=root+rs;
    if(!reserved||!fat||roots>512||data>=20480)return;
    for(u32 i=0;i<rs;i++){
        if(EFI_ERROR(io->ReadBlocks(io,io->Media->MediaId,container_boot.rec+root+i,512,p)))return;
        for(int j=0;j<512;j+=32)if(!memcmp(p+j,"BOOTCFG BIN",11)&&!(p[j+11]&24)){
            u32 cl=field16(p+j+26);if(cl<2||cl-2>=20480-data)return;
            if(!EFI_ERROR(io->ReadBlocks(io,io->Media->MediaId,container_boot.rec+data+cl-2,512,p))&&bootconfig_valid((BootConfig*)p))b->boot_flags=((BootConfig*)p)->flags;return;
        }
    }
}
