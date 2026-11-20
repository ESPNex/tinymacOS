#include "k.h"
#include "../boot/splash.h"
struct Boot boot;
void kmain(struct Boot *b)
{
    boot = *b;
    splash_bar(&boot, 63);
    cpu_init();
    splash_bar(&boot, 69);
    mem_init();
    device_add("UEFI GOP framebuffer", "display", 0);
    splash_bar(&boot, 75);
    platform_init();
    splash_bar(&boot, 81);
    ahci_init();
    bootfs_init();
    fs_init();
    session_init();
    splash_bar(&boot, 88);
    input_init();
    irq_init();
    kext_init();     /* autoload /System/Library/Extensions (e1000, wifi …) */
    logstr("[k] pre-sched\n");
    splash_bar(&boot, 94);
    scheduler_init();
    panic("scheduler returned", 0, 0);
}
