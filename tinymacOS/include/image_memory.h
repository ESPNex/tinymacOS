/* SPDX-License-Identifier: MIT */
#pragma once
#include "base.h"
/* Decoder/cache workspace. Large allocations use actual free physical RAM,
 * not the fixed small-object kernel heap. Exhaustion remains an I/O failure;
 * it must never be hidden by truncating a valid stream or faking output. */
void*image_mem_alloc(size_t);
void image_mem_free(void*);
