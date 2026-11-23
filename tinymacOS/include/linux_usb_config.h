/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "base.h"
#include "hidconfig.h"
/* Complete Linux v6.12 USB core/config.c. Private descriptor objects do not
 * pretend to be the bounded URB adapter's general usb_device ABI. */
typedef int (*TmcConfigRead)(void *,unsigned type,unsigned index,void *,unsigned length);
typedef struct TmcLinuxConfig TmcLinuxConfig;
/* Linux speed numbers: low=1, full=2, high=3, super=5, super-plus=6.
 * Reader returns actual descriptor bytes or a negative errno, never bool. */
TmcLinuxConfig *tmc_linux_config_open(unsigned speed,unsigned configurations,TmcConfigRead,void *);
void tmc_linux_config_close(TmcLinuxConfig *);
int tmc_linux_config_bos(TmcLinuxConfig *);
unsigned tmc_linux_config_count(const TmcLinuxConfig *);
unsigned tmc_linux_config_interfaces(const TmcLinuxConfig *,unsigned configuration);
unsigned tmc_linux_config_alternates(const TmcLinuxConfig *,unsigned configuration,unsigned interface_index);
typedef struct {
 unsigned number,alternate,cls,subclass,protocol,endpoints,configuration;
 const u8 *extra;unsigned extra_length;
} TmcLinuxConfigInterface;
typedef struct {
 unsigned address,attributes,packet,interval,burst,mult,bytes_per_interval;
 u32 ssp_bytes_per_interval;
} TmcLinuxConfigEndpoint;
int tmc_linux_config_interface(const TmcLinuxConfig *,unsigned,unsigned,unsigned,TmcLinuxConfigInterface *);
int tmc_linux_config_endpoint(const TmcLinuxConfig *,unsigned,unsigned,unsigned,unsigned,TmcLinuxConfigEndpoint *);
int tmc_linux_usb_config_hid(const u8 *,unsigned,unsigned speed,HidInterface *,unsigned);
int tmc_linux_usb_config_select(const u8 *,unsigned,unsigned,HidInterface *,unsigned,unsigned *configuration,int *hub);
typedef struct {u64 opens,failures,descriptor_reads,configs,interfaces,endpoints,bos,notices,allocations,frees;} TmcLinuxConfigStats;
void tmc_linux_config_stats(TmcLinuxConfigStats *);
/* Kernel or host-fixture services, not fabricated descriptor/I/O success. */
void *tmc_config_os_alloc(size_t);
void tmc_config_os_free(void *);
void tmc_config_os_delay(unsigned);
