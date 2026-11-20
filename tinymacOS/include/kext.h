#pragma once
/* Kernel extension contract: the kernel passes this table to kext_start().
 * Kexts are ELF64 relocatables (ET_REL) shipped inside .kext bundles under
 * /System/Library/Extensions; the loader resolves their few libc-style
 * undefined symbols (memcpy/memset/...) against the kernel and nothing else. */
#include "abi.h"

typedef void *(*kext_alloc_fn)(unsigned long);
typedef void (*kext_free_fn)(void *);
typedef void (*kext_log_fn)(const char *);
typedef u64 (*kext_ticks_fn)(void);
typedef void (*kext_udelay_fn)(unsigned);
typedef u32 (*kext_pci_find_fn)(u32 classcode);
typedef void *(*kext_pci_mmio_fn)(u32 dev, int bar);
typedef u32 (*kext_pci_reg_fn)(u32 dev, int off);

/* Network device registered by a driver kext; frames handed to the stack
 * with net_rx(), frames to wire with ops->tx(). */
struct netdev_ops {
    void (*tx)(void *ctx, const u8 *frame, int len);
    void *ctx;
};

typedef int (*kext_net_register_fn)(const char *name, const u8 mac[6], struct netdev_ops *ops);
typedef void (*kext_net_rx_fn)(int ifindex, const u8 *frame, int len);
typedef void (*kext_net_set_link_fn)(int ifindex, int up);

struct kext_api {
    kext_log_fn log;
    kext_alloc_fn alloc;
    kext_free_fn free;
    kext_ticks_fn ticks;
    kext_udelay_fn udelay;
    kext_pci_find_fn pci_find;
    kext_pci_mmio_fn pci_mmio;
    kext_pci_reg_fn pci_reg;
    kext_net_register_fn net_register;
    kext_net_rx_fn net_rx;
    kext_net_set_link_fn net_set_link;
};

/* Entry points every kext must export (resolved from its symtab). */
void kext_start(struct kext_api *api);
void kext_stop(void);

#define KEXT_MAX 16
struct LoadedKext {
    char id[48], ver[16], name[28];
    u32 size, state;
    void *image;
    void *raw;
    void (*start)(struct kext_api *);
    void (*stop)(void);
    void (*poll)(void);
    int (*wifi_scan)(void *, int);
    int (*wifi_status)(void *);
    int (*wifi_connect)(const char *);
};
