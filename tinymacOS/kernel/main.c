#include "k.h"
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
    boot_progress(63);
    cpu_init();
    boot_logstr("[boot] CPU protections and interrupt tables ready\n");
    boot_progress(69);
    mem_init();
    boot_logstr("[boot] physical memory and page tables ready\n");
    device_add("UEFI GOP framebuffer", "display", 0);
    boot_progress(75);
    platform_init();
    boot_logstr("[boot] platform enumeration complete\n");
    boot_progress(81);
    ahci_init();
    bootfs_init();
    fs_init();
    session_init();
    boot_logstr("[boot] storage, filesystems and session initialized\n");
    boot_progress(88);
    input_init();
    irq_init();
    boot_logstr("[boot] input and interrupt routing initialized\n");
    kext_init();     /* autoload /System/Library/Extensions (e1000, wifi …) */
    boot_logstr("[boot] extension scan complete, starting scheduler\n");
    boot_progress(94);
    scheduler_init();
    panic("scheduler returned", 0, 0);
}
