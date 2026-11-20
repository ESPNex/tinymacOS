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
static inline EFI_STATUS start_file(EFI_HANDLE self, EFI_SYSTEM_TABLE *st, EFI_HANDLE dev, CHAR16 *name,
                                    void *buffer, UINTN len, UINTN dmg_bytes)
{
    EFI_DEVICE_PATH *p = filepath(dev, name);
    if (!p)return EFI_NOT_FOUND;
    EFI_HANDLE child = 0;
    EFI_STATUS s = st->BootServices->LoadImage(FALSE, self, p, buffer, len, &child);
    st->BootServices->FreePool(p);
    if (!EFI_ERROR(s)) {
        EFI_GUID lg=LOADED_IMAGE_PROTOCOL; EFI_LOADED_IMAGE *li=0;
        struct RecoveryOrigin origin={.magic=ORIGIN_MAGIC,.image_bytes=(u32)dmg_bytes};
        EFI_DEVICE_PATH *node=devpath(dev);
        for (int i=0; node && i<64 && !IsDevicePathEnd(node); i++,node=NextDevicePathNode(node)) {
            unsigned n=DevicePathNodeLength(node);if (n<4 || n>4096)break;
            if (DevicePathType(node)!=MEDIA_DEVICE_PATH)continue;
            if (DevicePathSubType(node)==MEDIA_CDROM_DP)origin.optical=1;
            if (DevicePathSubType(node)==MEDIA_HARDDRIVE_DP && n>=42)origin.partition=((HARDDRIVE_DEVICE_PATH *)node)->PartitionNumber;
        }
        if (!EFI_ERROR(st->BootServices->HandleProtocol(child,&lg,(void **)&li))) {
            li->LoadOptions=&origin;li->LoadOptionsSize=sizeof origin;
        }
        s=st->BootServices->StartImage(child,0,0);
    }
    return s;
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
    unsigned char buf[512] __attribute__((aligned(64)));
    if (EFI_ERROR(bs->HandleProtocol(d, &gd, (void **)&io)) || io->Media->BlockSize != 512 ||
        !io->Media->LogicalPartition || EFI_ERROR(io->ReadBlocks(io, io->Media->MediaId, 8, 512, buf)) ||
        memcmp(buf, "TMRECOV2", 8))return 0;
    bs->SetMem(buf, 512, 0);
    io->WriteBlocks(io, io->Media->MediaId, 8, 512, buf);
    io->FlushBlocks(io);
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
