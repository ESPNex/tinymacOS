/* New REAL exFAT partition, published after verified copy. No VHD. */
#pragma once
#include "abi.h"
typedef struct {char source[PATHLEN],destination[PATHLEN];u64 bytes;DiskEdit part;int result;char message[160];} VolumeExport;
