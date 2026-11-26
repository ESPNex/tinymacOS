#pragma once
#include <stdint.h>
struct Boot {
    uint64_t fb, map, bytes, step, rsdp;
    uint32_t w, h, stride, rgb;
    uint64_t image, image_len, efi, efi_len, install_esp, install_len, recovery_img, recovery_len, volume, volume_len;
    unsigned char entropy[16], esp_id[16];
    uint32_t rng_ok;
    uint32_t nmodes;              /* runtime-switchable GOP modes (w<<16|h) */
    uint32_t modes[24];
    uint32_t boot_flags;        /* TMC_BOOT_VERBOSE etc. */
    uint32_t recovery;            /* set by the dedicated Recovery kernel */
    uint64_t install_4k, recovery_4k; /* verified 4096-byte FAT media, live only */
    uint32_t csr;                 /* SIP config from NVRAM: bit0 = enabled */
};
struct Elf {
    unsigned char id[16];
    uint16_t type, machine;
    uint32_t version;
    uint64_t entry, phoff, shoff;
    uint32_t flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstr;
};
struct Ph {
    uint32_t type, flags;
    uint64_t off, va, pa, filesz, memsz, align;
};
