#pragma once
#ifdef TMC_LINUX_HID_UNIT
#include <linux/tmc_hid_input.h>
#else
#pragma once
#ifdef TMC_LINUX_CONFIG_UNIT
#include <linux/tmc_usb_config.h>
#else
#include "tmc_compat.h"
#endif

#endif
