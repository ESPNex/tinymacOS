#pragma once
#include <stdint.h>
#include <stddef.h>
typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int64_t i64;
#define PACK __attribute__((packed))
#define ALIGNED(n) __attribute__((aligned(n)))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
void *memset(void *, int, size_t);
void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
int memcmp(const void *, const void *, size_t);
size_t strlen(const char *);
int eq(const char *, const char *);
void strset(char *, const char *, int);
void append(char *, const char *, int);
void number(char *, u64);
int val(const char *);
