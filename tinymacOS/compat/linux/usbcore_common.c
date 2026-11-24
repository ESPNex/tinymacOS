#include <tmc_kernel.h>
#undef subsys_initcall
#undef module_exit
#define subsys_initcall(fn) int tmc_usbcommon_init(void){return fn();}
#define module_exit(fn) void tmc_usbcommon_exit(void){fn();}
#include "/home/user/tinymacOS/vendor/linux-v6.12/usbcore-common.c"
