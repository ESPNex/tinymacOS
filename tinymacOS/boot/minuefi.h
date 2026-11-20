#pragma once
// Only the UEFI services used by the two retail programs. No GNU-EFI console or debug library.
static EFI_BOOT_SERVICES *fw;
static inline EFI_DEVICE_PATH *devpath(EFI_HANDLE d)
{
    EFI_GUID g = EFI_DEVICE_PATH_PROTOCOL_GUID;
    EFI_DEVICE_PATH *p = 0;
    return !EFI_ERROR(fw->HandleProtocol(d, &g, (void **)&p)) ? p : 0;
}
static inline UINTN dpsize(EFI_DEVICE_PATH *p)
{
    UINTN n = 0;
    for (int j = 0; p && j < 96; j++) {
        int z = DevicePathNodeLength(p);
        if (z < 4 || z > 4096)return 0;
        n += z;
        if (IsDevicePathEnd(p))return n;
        p = NextDevicePathNode(p);
    }
    return 0;
}
static inline EFI_DEVICE_PATH *filepath(EFI_HANDLE dev, const CHAR16 *file)
{
    EFI_DEVICE_PATH *p = devpath(dev);
    UINTN z = dpsize(p), n = 0;
    while (file[n])n++;
    if (z < 4 || n > 512)return 0;
    UINTN sz = 4 + 2 * (n + 1);
    u8 *b = 0;
    if (EFI_ERROR(fw->AllocatePool(EfiLoaderData, z + sz, (void **)&b)))return 0;
    memcpy(b, p, z - 4);
    FILEPATH_DEVICE_PATH *f = (void *)(b + z - 4);
    f->Header.Type = MEDIA_DEVICE_PATH;
    f->Header.SubType = MEDIA_FILEPATH_DP;
    SetDevicePathNodeLength(&f->Header, sz);
    memcpy(f->PathName, file, sz - 4);
    EFI_DEVICE_PATH *end = (void *)(b + z + sz - 4);
    SetDevicePathEndNode(end);
    return (void *)b;
}
