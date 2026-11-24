#include <tmc_kernel.h>
#undef module_init
#undef module_exit
#define module_init(fn) int tmc_canonical_hid_init(void){return fn();}
#define module_exit(fn) void tmc_canonical_hid_exit(void){fn();}
#include "../../vendor/linux-v6.12/hid-core.c"
