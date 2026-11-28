/* SPDX-License-Identifier: MIT */
#pragma once
#include "base.h"
/* Kernel-only stable image backing. All entry points run under fs_gate. */
typedef struct ImageSource ImageSource;
ImageSource*image_source_open(Process*,const char*,u64);
int image_source_read(ImageSource*,u64,void*,size_t);
void image_source_close(ImageSource*);
int image_source_native_busy(u64);
int image_source_snapshot_busy(u64);
int image_source_attached(void);
/* A source volume view is not a generational volume FD. It retains its own
 * immutable partition geometry across vol_refresh and pins its lifetime. */
int vol_source_open(const char*,const Process*,int,ForeignIo*,char[PATHLEN],int*,int*);
void vol_source_close(ForeignIo*);
int vol_source_busy(int,u64,u64);
int vol_source_write_blocked(int,u64,u32);
