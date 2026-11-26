/* Native allocation subset only; NOT a complete C library header. */
#ifndef TMC_LIBC_STDLIB_H
#define TMC_LIBC_STDLIB_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void *malloc(size_t);
void *calloc(size_t,size_t);
void *realloc(void *,size_t);
void *aligned_alloc(size_t,size_t);
void free(void *);
#ifdef __cplusplus
}
#endif
#endif
