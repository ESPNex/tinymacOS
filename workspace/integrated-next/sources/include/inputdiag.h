#pragma once
#include "base.h"
#define TMC_INPUTDIAG_VERSION 1u
#define TMC_INPUTDIAG_DEVICES 16u
/* No keystrokes, secrets or memory addresses. Best-effort monotonic counters.
 * Recovery uses independent firmware drivers; native unsupported controller
 * counts are explicit, not a claim of EHCI/OHCI/UHCI support. */
typedef struct {
    u32 pci,port,vid,pid,protocols,status;
} InputDeviceDiag;
typedef struct {
    u32 version,bytes,xhci,active_xhci,ehci,ohci,uhci,root_devices;
    u32 keyboards,pointers,control_errors,transfer_errors,bad_reports,report_fallbacks;
    u64 reports,delivered,merged,dropped;
    int32_t last_x,last_y;
    u32 last_buttons,device_count;
    InputDeviceDiag device[TMC_INPUTDIAG_DEVICES];
} InputDiag;
