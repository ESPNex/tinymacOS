/* Opt-in target libc allocation symbols, not linked into hosted tests or
 * existing production applications. Startup must attach the native TCB first. */
#include <stdlib.h>
#include "tmc_heap.h"
void *malloc(size_t n){return tmc_heap_alloc(n);}
void *calloc(size_t n,size_t s){return tmc_heap_calloc(n,s);}
void *realloc(void *p,size_t n){return tmc_heap_realloc(p,n);}
void *aligned_alloc(size_t a,size_t n){return tmc_heap_aligned_alloc(a,n);}
void free(void *p){tmc_heap_free(p);}
