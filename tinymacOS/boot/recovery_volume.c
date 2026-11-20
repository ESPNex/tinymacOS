// Boot entry of the installed Recovery partition. The actual runtime exists only
// inside the pinned BaseSystem.dmg, not in this bootstrap or the desktop ESP.
#include <efi.h>
#include <efilib.h>
#include "sha256.h"
#include "recoveryboot.h"
#include "fatimage.h"
EFI_STATUS _entry(EFI_HANDLE image, EFI_SYSTEM_TABLE *st)
{
    fw=st->BootServices;fw->SetWatchdogTimer(0,0,0,0);
    EFI_GUID lg=LOADED_IMAGE_PROTOCOL,sg=EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_LOADED_IMAGE *li;EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;EFI_FILE_PROTOCOL *root;
    if (EFI_ERROR(fw->HandleProtocol(image,&lg,(void **)&li)) ||
        EFI_ERROR(fw->HandleProtocol(li->DeviceHandle,&sg,(void **)&fs)) || EFI_ERROR(fs->OpenVolume(fs,&root)))return bad_base(st);
    EFI_STATUS result=image_recovery(image,st,li->DeviceHandle,root);
    root->Close(root);return EFI_ERROR(result)?bad_base(st):result;
}
