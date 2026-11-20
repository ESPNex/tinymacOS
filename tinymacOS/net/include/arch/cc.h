#pragma once
/* lwIP arch layer for the freestanding x86-64 kernel. */
#define LWIP_NO_CTYPE_H 1   /* freestanding: use lwip digit macros, no ctype.h */
#include <stdint.h>
#include <stddef.h>
#define BYTE_ORDER LITTLE_ENDIAN
typedef int sys_prot_t;
#define LWIP_PLATFORM_DIAG(x) do { } while (0)
#define LWIP_PLATFORM_ASSERT(x) do { } while (0)
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_PACKED __attribute__((packed))
#define PACK_STRUCT_FLD_8(x) x
#define PACK_STRUCT_FLD_U8(x) x
#define PACK_STRUCT_FLD_16(x) x
#define PACK_STRUCT_FLD_U16(x) x
#define PACK_STRUCT_FLD_32(x) x
#define PACK_STRUCT_FLD_U32(x) x
