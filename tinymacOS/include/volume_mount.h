/* Authenticated S_DISK 15; per-volume state, never force-unmount.
 * Existing lazy mounts default online. Offline volumes remain discoverable.
 * The kernel revalidates actual state for every operation, not the UI label. */
#pragma once
#include "abi.h"
enum {VOLUME_QUERY,VOLUME_MOUNT,VOLUME_UNMOUNT};
enum {VOLUME_OK=0,VOLUME_NOT_FOUND=-1,VOLUME_BAD_REQUEST=-2,VOLUME_PROTECTED=-3,VOLUME_BUSY=-4,VOLUME_IO=-5};
typedef struct {char target[PATHLEN];int operation,mounted,managed,status;char message[96];} VolumeMount;
