/* SPDX-License-Identifier: MIT */
#include "../kernel/k.h"
#include "image_memory.h"
void*image_mem_alloc(size_t n){return n<=4096?kmalloc(n):kernel_span_alloc(n);}
void image_mem_free(void*p){if(!p)return;if(kmalloc_owns(p))kfree(p);else kernel_span_free(p);}
