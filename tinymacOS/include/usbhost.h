#pragma once
#include "base.h"
#include "hidtablet.h"
#include "hidconfig.h"
#define USB_INPUT_INTERFACES 4
#define USB_INPUT_DEVICES 8
/* Native USB 1.x/2.0 HID transport shared by UHCI and EHCI; xHCI remains independent.
 * Speed codes match xHCI: 1 full, 2 low, 3 high. DMA is below 4 GiB.
 * ctrl returns 0, -2 STALL, or -1 timeout/transport; ctrl_bytes is actual IN length. */
typedef struct UsbInput UsbInput;
typedef struct {
    u8 report[1024] ALIGNED(4096);
    TabletLayout layout;
    unsigned proto,ep,packet,interval,iface,report_length,toggle,failures;
    int armed,linux_mouse,linux_keyboard;
    u64 retry_at;
} UsbInputPipe;
struct UsbInput {
    u8 desc[1024] ALIGNED(4096);
    UsbInputPipe pipe[USB_INPUT_INTERFACES];
    void *host;
    int (*control)(UsbInput *,int,int,int,int,int);
    unsigned address,speed,max_packet0,npipe,ctrl_bytes,vid,pid;
    unsigned hub,hub_port; /* EHCI transaction translator for low/full-speed children */
};
void usb_delay_ms(unsigned);
int usb_input_enumerate(UsbInput *,unsigned);
void usb_input_report(UsbInput *,unsigned,unsigned);
void ehci_init(void);void ehci_poll(void);
void uhci_init(void);void uhci_poll(void);
void ps2_init(void);void ps2_poll(void);
