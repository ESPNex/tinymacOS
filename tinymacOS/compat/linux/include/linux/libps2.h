/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <linux/tmc_ps2_compat.h>
/* The OS already exports ps2_init(void). Namespace the upstream initializer
 * at compile time, without editing its original source or header bytes. */
#define ps2_init tmc_linux_ps2_init
#include "../../../../vendor/linux-v6.12/libps2.h"
