#pragma once
#ifdef TMC_LINUX_HID_UNIT
#include <linux/tmc_hid_input.h>
#else
#error This header currently supports only the isolated HID unit
#endif
