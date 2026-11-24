#include <tmc_kernel.h>
#undef module_init
#undef module_exit
#define module_init(fn) int tmc_canonical_usbhid_init(void){return fn();}
#define module_exit(fn) void tmc_canonical_usbhid_exit(void){fn();}
#include "../../vendor/linux-v6.12/usbhid/hid-core.c"
