/* SPDX-License-Identifier: GPL-2.0-or-later
 * No upstream typedefs cross this bridge; layout verified by native wrapper.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
enum { NT_VSTAT,NT_VLIST,NT_VCREATE,NT_VREMOVE,NT_VMOVE,NT_VREAD,NT_VWRITE };
typedef struct {uint64_t size;int dir,ro;char name[1021];} NtVolumeEntry;
typedef struct {int op,directory,cap,skip;const char*path,*target;uint64_t offset;void*data;size_t bytes;NtVolumeEntry*entries;int readonly;} NtVolumeRequest;
long long tmc_ntfs_volume_run(NtVolumeRequest*,int,int);
