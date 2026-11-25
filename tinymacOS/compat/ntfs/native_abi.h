/* SPDX-License-Identifier: GPL-2.0-or-later
 * Minimal ABI of struct ntfs_device{,_operations}, pinned ntfs-3g 2022.10.3.
 * Derived from include/ntfs-3g/device.h (Anton Altaparmakov et al).
 * No Linux syscall numbers or opaque libc FILE objects cross this interface.
 * tools/build-ntfs.py verifies this declaration against the original headers.
 */
#pragma once
#include "base.h"
struct stat;
struct ntfs_device;
typedef struct ntfs_device_operations {
 int (*open)(struct ntfs_device*,int);
 int (*close)(struct ntfs_device*);
 i64 (*seek)(struct ntfs_device*,i64,int);
 i64 (*read)(struct ntfs_device*,void*,i64);
 i64 (*write)(struct ntfs_device*,const void*,i64);
 i64 (*pread)(struct ntfs_device*,void*,i64,i64);
 i64 (*pwrite)(struct ntfs_device*,const void*,i64,i64);
 int (*sync)(struct ntfs_device*);
 int (*stat)(struct ntfs_device*,struct stat*);
 int (*ioctl)(struct ntfs_device*,unsigned long,void*);
} NativeNtfsOps;
typedef struct ntfs_device {
 NativeNtfsOps *d_ops;unsigned long d_state;char *d_name;void *d_private;
 int d_heads,d_sectors_per_track;
} NativeNtfsDevice;
