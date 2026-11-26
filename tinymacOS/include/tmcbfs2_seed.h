#pragma once
#include "tmcbfs2.h"
/* Fresh-install import of a validated, plaintext legacy live-session image.
 * Not an in-place converter. Validate BEFORE any target format/write. */
int tmc2_seed_validate(const void*,size_t);
int tmc2_seed_import(Tmc2*,const void*,size_t);
int tmc2_seed_verify(Tmc2*,const void*,size_t);
