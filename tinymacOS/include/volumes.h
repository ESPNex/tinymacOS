/* Kernel-only namespace of real read/write data volumes. fs_gate held. */
#pragma once
#include "volume_mount.h"
int vol_manage(VolumeMount*,const Process*,int);
int fs_volume_manage(VolumeMount*);
int vol_normalize(const char*,char[PATHLEN]);
int vol_reserved(const char*);
int vol_stat(const char*,Dirent*);
int vol_list(const char*,Dirent*,int,int);
int vol_open(Process*,const char*,int);
int vol_read(Process*,int,void*,int);
int vol_seek(Process*,int,u64);
int vol_exportable(const char*);
void vol_refresh(void);

int vol_write(Process*,int,const void*,int);
int vol_create(const char*,int);
int vol_remove(const char*,unsigned);
int vol_move(const char*,const char*);
