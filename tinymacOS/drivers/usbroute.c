#ifndef TMC_USBROUTE_TEST
#include "../kernel/k.h"
#include "usbhost.h"
#endif
#include "usbroute.h"
/* Intel's shared EHCI/xHCI USB2 port mux is outside the xHCI MMIO spec.
 * Bay Trail 8086:0f35 is used by Q1900M, but this is a controller-family
 * quirk, not a motherboard driver. Only documented switchable families.
 * Called once, with a running xHC, before any EHCI enumeration. */
void usb_route_intel_xhci(u32 dev)
{
    u32 id=pci_reg(dev,0);if((id&65535)!=0x8086)return;
    unsigned device=id>>16;
    if(device!=0x0f35&&device!=0x1e31&&device!=0x8c31&&device!=0x8cb1&&device!=0x9c31&&device!=0x9cb1)return;
    if((pci_reg(dev,8)>>8)!=0x0c0330)return;
    u32 sub=pci_reg(dev,0x2c);if((sub&65535)==0x104d&&(sub>>16)==0x90a8)return;
    u32 usb2=pci_reg(dev,0xd4),usb3=pci_reg(dev,0xdc);
    int companion=!!pci_find_after(0x0c0320,0);
    /* Absent/locked config registers must not become an all-ports write. */
    if(usb2==0xffffffffu||usb3==0xffffffffu)return;
    if(usb3){outl(0xcf8,dev|0xd8);outl(0xcfc,pci_reg(dev,0xd8)|usb3);}
    /* If an EHCI exists, keep firmware's USB2 mux. Forcing all shared
     * sockets to xHCI loses an otherwise-working UEFI EHCI/RMH path. */
    if(companion)usb2=0;
    if(usb2){outl(0xcf8,dev|0xd0);outl(0xcfc,pci_reg(dev,0xd0)|usb2);}
    if(usb2||usb3){usb_delay_ms(100);logstr("[usb] Intel USB ownership: firmware USB2 routing preserved when EHCI exists\n");}
}
/* Cold-boot recovery only, after BOTH xHCI and USB2 host scans. Preserve a
 * functioning USB keyboard/mouse route; never run as hotplug or board matching.
 * An EHCI/RMH path can be firmware-owned but unusable by the native host. If
 * there are no native HID interfaces anywhere, retry documented USB2 mux bits
 * on the already-running xHC rather than leaving every pointer absent. */
int usb_route_intel_xhci_recover(u32 dev,unsigned pointers,unsigned keyboards)
{
    if(pointers || keyboards)return 0;
    u32 id=pci_reg(dev,0);if((id&65535)!=0x8086)return 0;
    unsigned d=id>>16;
    if(d!=0x0f35&&d!=0x1e31&&d!=0x8c31&&d!=0x8cb1&&d!=0x9c31&&d!=0x9cb1)return 0;
    if((pci_reg(dev,8)>>8)!=0x0c0330 || !pci_find_after(0x0c0320,0))return 0;
    u32 sub=pci_reg(dev,0x2c);if((sub&65535)==0x104d&&(sub>>16)==0x90a8)return 0;
    u32 mask=pci_reg(dev,0xd4),before=pci_reg(dev,0xd0);
    if(!mask || mask==~0u || before==~0u || (before&mask)==mask)return 0;
    outl(0xcf8,dev|0xd0);outl(0xcfc,before|mask);
    u32 after=pci_reg(dev,0xd0);
    if(after==~0u || !((after^before)&mask))return 0;
    usb_delay_ms(100);
    logstr("[usb] no native USB HID: retrying documented shared USB2 ports on active xHCI\n");
    return 1;
}
