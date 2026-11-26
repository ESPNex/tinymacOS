#pragma once
#include "minuefi.h"
#include "recovery_origin.h"
// Private vendor GUID for the tinymacOS SIP variable (tinymacOS-csr).
// Firmwares commonly reject unknown names in EFI_GLOBAL_VARIABLE: with a
// private GUID the variable persists everywhere, like any vendor's own keys.
// 2.3.0 and earlier wrote it in the global namespace; readers keep a fallback.
#define TMC_SIP_GUID {0x3f9a25c4, 0x71b2, 0x4e83, {0xa4, 0xc1, 0x5e, 0x9d, 0x27, 0x8b, 0x6c, 0x10}}
// Match recovery to the same hardware device, not the first volume with a familiar filename.
static inline UINTN hd_prefix(EFI_DEVICE_PATH *p, int *number)
{
    UINTN n = 0;
    for (int i = 0; p && !IsDevicePathEnd(p) && i < 64; i++, p = NextDevicePathNode(p)) {
        int z = DevicePathNodeLength(p);
        if (z < 4 || z > 4096)return 0;
        if (DevicePathType(p) == MEDIA_DEVICE_PATH && DevicePathSubType(p) == MEDIA_HARDDRIVE_DP && z >= 42) {
            *number = ((HARDDRIVE_DEVICE_PATH *)p)->PartitionNumber;
            return n;
        }
        n += z;
    }
    return 0;
}
static inline EFI_HANDLE sibling(EFI_BOOT_SERVICES *bs, EFI_HANDLE dev, int which)
{
    EFI_DEVICE_PATH *dp = devpath(dev);
    int num = 0;
    UINTN n = hd_prefix(dp, &num), count = 0;
    EFI_HANDLE *handles = 0;
    EFI_GUID fs = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    if (!n || EFI_ERROR(bs->LocateHandleBuffer(ByProtocol, &fs, 0, &count, &handles)))return 0;
    EFI_HANDLE found = 0;
    for (UINTN j = 0; j < count; j++) {
        int b = 0;
        EFI_DEVICE_PATH *p = devpath(handles[j]);
        if (hd_prefix(p, &b) == n && b == which && !memcmp(dp, p, n)) {
            found = handles[j];
            break;
        }
    }
    bs->FreePool(handles);
    return found;
}
static inline int recflag(EFI_BOOT_SERVICES *bs, EFI_HANDLE d)
{
    EFI_GUID gd = EFI_BLOCK_IO_PROTOCOL_GUID;
    EFI_BLOCK_IO *io = 0;
    unsigned char buf[4096] __attribute__((aligned(4096)));
    if(EFI_ERROR(bs->HandleProtocol(d,&gd,(void**)&io))||!io->Media->LogicalPartition)return 0;
    UINTN block=io->Media->BlockSize;if(block!=512&&block!=4096)return 0;
    UINT64 lba=4096/block;
    if(EFI_ERROR(io->ReadBlocks(io,io->Media->MediaId,lba,block,buf))||memcmp(buf,"TMRECOV2",8))return 0;
    bs->SetMem(buf,512,0); /* preserve all adjacent bytes of a native 4KiB block */
    if(EFI_ERROR(io->WriteBlocks(io,io->Media->MediaId,lba,block,buf))||EFI_ERROR(io->FlushBlocks(io)))return 0;
    return 1;
}
static inline int recovery_key(EFI_SYSTEM_TABLE *st)
{
    for (int j = 0; j < 45; j++) {
        EFI_INPUT_KEY k;
        if (!EFI_ERROR(st->ConIn->ReadKeyStroke(st->ConIn, &k)) && (k.UnicodeChar == 'r' || k.UnicodeChar == 'R' ||
                k.ScanCode == SCAN_F12))return 1;
        st->BootServices->Stall(20000);
    }
    return 0;
}
