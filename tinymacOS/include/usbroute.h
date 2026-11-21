#pragma once
#include "base.h"
#ifndef TMC_INPUT_TEST
void usb_route_intel_xhci(u32);
int usb_route_intel_xhci_recover(u32,unsigned,unsigned);
#endif
