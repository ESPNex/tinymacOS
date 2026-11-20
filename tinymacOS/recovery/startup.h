#pragma once
/* Reopening an ESP after writing its blocks is not enough to invalidate OVMF's FAT
 * cache. Select the specific ESP in BootNext and let a cold reset reacquire it.
 * Never replace another boot entry and never edit the user's permanent BootOrder. */
static int boot_once(EFI_SYSTEM_TABLE *st, EFI_HANDLE esp)
{
    static const CHAR16 description[] = L"tinymacOS - Macintosh HD";
    static const CHAR16 digits[] = L"0123456789ABCDEF";
    EFI_GUID global = EFI_GLOBAL_VARIABLE;
    EFI_RUNTIME_SERVICES *rs = st->RuntimeServices;
    UINT32 attrs = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS;
    UINT32 flags = LOAD_OPTION_ACTIVE;
    EFI_DEVICE_PATH *path = filepath(esp, L"\\EFI\\BOOT\\BOOTX64.EFI");
    UINTN n = dpsize(path);
    UINT8 wanted[1024], old[sizeof wanted];
    UINTN size = 6 + sizeof description + n;
    if (!path) return 0;
    if (n < 4 || size > sizeof wanted) {
        fw->FreePool(path);
        return 0;
    }
    UINT16 paths = n;
    memcpy(wanted, &flags, 4);
    memcpy(wanted + 4, &paths, 2);
    memcpy(wanted + 6, description, sizeof description);
    memcpy(wanted + 6 + sizeof description, path, n);
    fw->FreePool(path);
    /* The name is stable for the same target, including its GPT partition GUID. */
    UINT16 seed = crc32(wanted, size) & 0xff;
    for (int i = 0; i < 256; ++i) {
        UINT16 id = 0x7200u | ((seed + i) & 255);
        CHAR16 key[] = L"Boot7200";
        key[6] = digits[(id >> 4) & 15];
        key[7] = digits[id & 15];
        UINTN nr = sizeof old;
        EFI_STATUS r = rs->GetVariable(key, &global, 0, &nr, old);
        int created = r == EFI_NOT_FOUND;
        if (created) {
            if (EFI_ERROR(rs->SetVariable(key, &global, attrs, size, wanted))) return 0;
        } else if (EFI_ERROR(r) || nr != size || memcmp(old, wanted, size)) continue;
        if (EFI_ERROR(rs->SetVariable(L"BootNext", &global, attrs, sizeof id, &id))) {
            if (created) rs->SetVariable(key, &global, 0, 0, 0);
            return 0;
        }
        rs->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, 0);
        return 0;
    }
    return 0;
}
