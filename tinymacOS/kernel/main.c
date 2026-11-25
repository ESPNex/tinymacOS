#include "k.h"
#include "../boot/splash.h"
struct Boot boot;
void kmain(struct Boot *b)
{
    boot = *b;
#ifdef TMC_RECOVERY_KERNEL
    boot.recovery=1;
#else
    boot.recovery=0;
#endif
    boot_loginit();
    boot_logstr(boot.recovery?"[boot] tinymacOS separate Recovery kernel; firmware services exited\n":"[boot] tinymacOS native kernel\n");
    splash_bar(&boot, 63);
    cpu_init();
    boot_logstr("[boot] CPU protections and interrupt tables ready\n");
    splash_bar(&boot, 69);
    mem_init();
    boot_logstr("[boot] physical memory and page tables ready\n");
    device_add("UEFI GOP framebuffer", "display", 0);
    splash_bar(&boot, 75);
    platform_init();
    boot_logstr("[boot] platform enumeration complete\n");
    splash_bar(&boot, 81);
    ahci_init();
    bootfs_init();
    fs_init();
    session_init();
    boot_logstr("[boot] storage, filesystems and session initialized\n");
    splash_bar(&boot, 88);
    input_init();
    irq_init();
    boot_logstr("[boot] input and interrupt routing initialized\n");
    kext_init();     /* autoload /System/Library/Extensions (e1000, wifi …) */
    boot_logstr("[boot] extension scan complete, starting scheduler\n");
    splash_bar(&boot, 94);
    scheduler_init();
    panic("scheduler returned", 0, 0);
}
