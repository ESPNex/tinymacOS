/* SPDX-License-Identifier: GPL-2.0-only */
/* Physical EHCI OS services for the sole canonical USB stack. */
#include "../kernel/k.h"
#include "linux_ehci_hcd.h"
extern void usb_delay_ms(unsigned);
u8 tmc_ehci_os_in8(u16 port){return inb(port);}
u16 tmc_ehci_os_in16(u16 port){return inw(port);}
u32 tmc_ehci_os_in32(u16 port){return inl(port);}
void tmc_ehci_os_out8(u16 port,u8 value){outb(port,value);}
void tmc_ehci_os_out16(u16 port,u16 value){outw(port,value);}
void tmc_ehci_os_out32(u16 port,u32 value){outl(port,value);}
u32 tmc_ehci_os_pci_next(u32 class,u32 after){return pci_find_after(class,after);}
u32 tmc_ehci_os_pci_read(u32 address,unsigned off){return pci_reg(address,off);}
void tmc_ehci_os_pci_write(u32 address,unsigned off,u32 value){u32 f=irq_save();outl(0xcf8,address|(off&252));outl(0xcfc,value);irq_restore(f);}
void tmc_ehci_os_pci_write16(u32 address,unsigned off,u16 value){u32 f=irq_save();outl(0xcf8,address|(off&252));outw(0xcfc+(off&3),value);irq_restore(f);}
void tmc_ehci_os_pci_write8(u32 address,unsigned off,u8 value){u32 f=irq_save();outl(0xcf8,address|(off&252));outb(0xcfc+(off&3),value);irq_restore(f);}
const char *tmc_ehci_os_board_name(void){return NULL;} /* DMI inventory not implemented; absence is not an invented board ID. */
void tmc_ehci_os_delay_us(unsigned us)
{
 /* Independent PIT channel 2, including pre-IRQ startup and IF-disabled reset.
  * Use the calibrated millisecond delay conservatively for sub-ms requests. */
 usb_delay_ms((us+999)/1000);
}
void *tmc_ehci_os_mmio(u32 address,unsigned bar){return pci_mmio(address,bar);}
u32 tmc_ehci_os_read32(const volatile void *p){return *(const volatile u32 *)p;}
void tmc_ehci_os_write32(volatile void *p,u32 value){*(volatile u32 *)p=value;asm volatile("":::"memory");}
u64 tmc_ehci_os_clock_ns(void){extern u64 tmc_lx_os_clock_ns(void);return tmc_lx_os_clock_ns();}
void tmc_ehci_os_fatal(void){logstr("Linux EHCI fatal invariant\n");for(;;)asm volatile("cli; hlt");}
