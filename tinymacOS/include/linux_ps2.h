/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "base.h"
/* Genuine Linux libps2 command/ACK/response core on native i8042 transport.
 * Not complete Linux i8042/serio/atkbd/psmouse drivers. CPU0 poll owner only;
 * do not issue blocking commands from timer/receive callbacks.
 */
#define TMC_PS2_TIMEOUT 1u
#define TMC_PS2_PARITY 2u
typedef int (*TmcPs2Write)(void *,unsigned port,u8 byte); /* 0 or negative errno */
typedef void (*TmcPs2Progress)(void *);
typedef void (*TmcPs2Receive)(void *,unsigned port,u8 byte,unsigned flags);
int tmc_linux_ps2_setup(TmcPs2Write,TmcPs2Progress,TmcPs2Receive,void *);
int tmc_linux_ps2_command(unsigned port,u8 *param,unsigned capacity,unsigned command);
int tmc_linux_ps2_rx_ready(unsigned port);
int tmc_linux_ps2_receive(unsigned port,u8 byte,unsigned flags);
typedef struct {u64 setups,commands,command_errors,tx_bytes,rx_bytes,data_bytes,rx_errors,wait_polls,rejected;} TmcLinuxPs2Stats;
void tmc_linux_ps2_stats(TmcLinuxPs2Stats *);
