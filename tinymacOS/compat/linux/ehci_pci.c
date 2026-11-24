#include "ehci-include/tmc_ehci_compat.h"
#undef module_init
#undef module_exit
#define module_init(fn) int tmc_linux_ehci_pci_init(void){return fn();}
#define module_exit(fn) void tmc_linux_ehci_pci_exit(void){fn();}
#include "../../vendor/linux-v6.12/ehci-pci.c"
