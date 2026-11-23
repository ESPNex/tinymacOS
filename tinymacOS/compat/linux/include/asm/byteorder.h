#pragma once
#ifdef TMC_LINUX_HID_UNIT
#include <linux/tmc_hid_input.h>
#else
/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#ifdef TMC_LINUX_CONFIG_UNIT
#include <linux/tmc_usb_config.h>
#else
#include <linux/tmc_compat.h>
#endif

#endif
