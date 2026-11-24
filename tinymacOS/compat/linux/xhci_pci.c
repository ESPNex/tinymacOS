#include "xhci-include/tmc_xhci_compat.h"
#undef module_init
#undef module_exit
#define module_init(fn) int tmc_linux_xhci_pci_init(void){return fn();}
#define module_exit(fn) void tmc_linux_xhci_pci_exit(void){fn();}
#include "../../vendor/linux-v6.12/xhci-pci.c"
