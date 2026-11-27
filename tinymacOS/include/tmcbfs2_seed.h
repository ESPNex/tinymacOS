#pragma once
#include "tmcbfs2.h"
/* TMCSEED2 is a bounded, checksummed installer transport, NOT a mounted
 * filesystem. It carries a verified TMCFS2 live tree and two private account
 * records. No generation-1 image, driver or conversion is accepted. */
#define T2_SEED_MAX_ENTRIES 4096u
int tmc2_seed_export(Tmc2*,void*,size_t);
int tmc2_seed_validate(const void*,size_t);
int tmc2_seed_import(Tmc2*,const void*,size_t);
int tmc2_seed_verify(Tmc2*,const void*,size_t);
