#pragma once
#include "ui.h"
#include "sha256.h"
#define KERNELDIR KCDIR
static inline int bootpath(const char *p)
{
    const char *roots[] = {"/System", "/Applications", "/bin", "/sbin", "/usr"};
    for (int i = 0; i < 5; i++) {
        int z = strlen(roots[i]);
        if (strlen(p) >= (size_t)z && !memcmp(p, roots[i], z) && (!p[z] || p[z] == '/'))return 1;
    }
    return 0;
}
static inline void exactsize(char *s, u32 bytes, int cap)
{
    char n[24];
    number(n, bytes);
    int len = strlen(n);
    for (int i = 0; i < len; i++) {
        if (i && (len - i) % 3 == 0)append(s, ".", cap);
        char k[2] = {n[i], 0};
        append(s, k, cap);
    }
    append(s, " byte", cap);
}
static inline void sizebadge(char *s, u32 bytes, int cap)
{
    *s = 0;
    if (bytes < 1024) {
        exactsize(s, bytes, cap);
        return;
    }
    u32 unit = bytes < 1048576 ? 1024 : 1048576;
    addnum(s, bytes / unit, cap);
    append(s, ",", cap);
    addnum(s, (u64)(bytes % unit) * 10 / unit, cap);
    append(s, unit == 1024 ? " KiB" : " MiB", cap);
}
static inline const char *syskind(const Dirent *d)
{
    return d->dir ? "Cartella" : eq(d->name, "KernelCache") ? "Cache del kernel" : eq(d->name,
            "boot.efi") ? "Eseguibile di avvio" : "File di sistema";
}
