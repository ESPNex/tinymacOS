/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "base.h"
/* Complete upstream hid-generic.c and hid-input.c on an explicit native
 * low-level transport/event sink. Not Linux usbhid, USB core/hub or HCDs.
 * All handle operations require serialized ownership; callbacks cannot reenter
 * or destroy their handle. A busy/reentrant operation fails, never deadlocks.
 * Current generic consumer policy: <=1024-byte descriptor, <=16384-byte frame,
 * <=16-bit fields, <=16 per-application inputs, checked signed ranges and unit
 * exponents -8..8. Unsupported models fail probe; original files stay intact. */
typedef struct TmcHidGeneric TmcHidGeneric;
typedef struct {
 int (*start)(void *);void (*stop)(void *);
 int (*open)(void *);void (*close)(void *);
 /* Registration must really accept the application; negative rejects probe.
  * token distinguishes per-application input devices on a composite HID.
  * Return value applies to add only; remove is mandatory completed cleanup. */
 int (*input)(void *,unsigned token,unsigned application,int add);
 void (*event)(void *,unsigned token,unsigned type,unsigned code,int value);
 /* HID report types 0/1/2; requests GET=1, SET=9. Return received byte count
  * for GET, consumed byte count for SET, or negative errno. Optional:
  * unavailable requests return -EOPNOTSUPP, never fabricated reports. */
 int (*raw)(void *,unsigned id,u8 *,unsigned n,unsigned type,unsigned request);
 int (*output)(void *,const u8 *,unsigned);
} TmcHidGenericOps;
typedef struct {u64 probes,attached,detached,failures,reports,rejected,events,inputs,outputs,output_errors,busy;} TmcHidGenericStats;
TmcHidGeneric *tmc_hid_generic_attach(const u8 *,unsigned,const TmcHidGenericOps *,void *);
/* strict complete frame, input reports only; caller data is never mutated */
int tmc_hid_generic_report(TmcHidGeneric *,const u8 *,unsigned);
/* deferred LED worker: service outside interrupt context, after set_led */
int tmc_hid_generic_set_led(TmcHidGeneric *,unsigned code,int on);
int tmc_hid_generic_service(TmcHidGeneric *);
int tmc_hid_generic_detach(TmcHidGeneric *);
void tmc_hid_generic_stats(TmcHidGenericStats *);
