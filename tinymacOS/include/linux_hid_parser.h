/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "base.h"
#include "hidtablet.h"
/* Genuine Linux v6.12 hid-core descriptor parser functions, NOT the complete
 * HID bus, hid-input, hid-generic or USB transport driver. Boot ABI is separate.
 * Report payloads are inspected without executing Linux raw-event dispatch. */
typedef struct TmcLinuxHidParser TmcLinuxHidParser;
typedef struct {unsigned id,type,bits,bytes,fields,application,numbered;} TmcHidReportInfo;
typedef struct {
 unsigned application,physical,logical,flags,offset,width,count,usages;
 int minimum,maximum,physical_minimum,physical_maximum,unit_exponent;
 unsigned unit;
} TmcHidFieldInfo;
typedef struct {u64 opens,failures,closes,allocations,frees,live_bytes,peak_bytes,notices,busy;} TmcHidParserStats;
/* Bounded policy: descriptor <=1024 bytes, reports <=16384 bytes, 2 MiB
 * aggregate parser heap. Calls return failure on contention or resource limit;
 * no success-stub lifecycle APIs. Caller owns object until close. */
TmcLinuxHidParser *tmc_linux_hid_parse(const u8 *,unsigned);
void tmc_linux_hid_close(TmcLinuxHidParser *);
int tmc_linux_hid_report(const TmcLinuxHidParser *,unsigned type,unsigned id,TmcHidReportInfo *);
int tmc_linux_hid_field(const TmcLinuxHidParser *,unsigned type,unsigned id,unsigned index,TmcHidFieldInfo *);
int tmc_linux_hid_usage(const TmcLinuxHidParser *,unsigned type,unsigned id,unsigned field,unsigned index,u32 *);
/* Strict packet framing: short or foreign-ID reports never get zero-padded
 * into input events. No payload is mutated or read beyond received bytes. */
int tmc_linux_hid_frame(const TmcLinuxHidParser *,unsigned type,const u8 *,unsigned,TmcHidReportInfo *);
/* Native consumer bridge: derives offsets/ranges/usages FROM Linux field model,
 * then closes parser. Existing native packet/pixel decoder remains separate.
 * Firmware Recovery continues its independent firmware-only input path. */
int tmc_linux_hid_pointer_layout(const u8 *,unsigned,unsigned packet,TabletLayout *);
void tmc_linux_hid_parser_stats(TmcHidParserStats *);
void *tmc_hid_os_alloc(size_t);
void tmc_hid_os_free(void *);
