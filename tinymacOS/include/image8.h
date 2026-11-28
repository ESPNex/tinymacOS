/* SPDX-License-Identifier: MIT */
#pragma once
#include "abi.h"
/* Largest whole-sector byte range representable by the u64 byte ABI.
 * Native writable filesystems keep their own format-specific capacity rules. */
#define IMAGE8_ADDRESS_LIMIT (~(u64)511)
#define IMAGE8_MAX_BYTES IMAGE8_ADDRESS_LIMIT
#define IMAGE8_INLINE_RUNS 128u
#define IMAGE8_ZERO 0u
#define IMAGE8_RAW 1u
#define IMAGE8_IGNORE 2u
#define IMAGE8_ADC 0x80000004u
#define IMAGE8_ZLIB 0x80000005u
#define IMAGE8_BZIP2 0x80000006u
#define IMAGE8_LZFSE 0x80000007u
#define IMAGE8_LZMA 0x80000008u
typedef struct {u64 sector,count,offset,length;u32 type;} Image8Run;
typedef struct Image8 Image8;
struct Image8 {
 void *context;int(*source)(void*,u64,void*,size_t);u64 file_bytes,bytes;
 u64 runs;Image8Run run[IMAGE8_INLINE_RUNS];
 /* Grow-on-demand radix pages, not a fixed run/XML count. The inline table
  * preserves allocation-free boot/host use for small maps. */
 void*run_pages;unsigned run_levels;u64 run_capacity;
 void*(*alloc)(void*,size_t);void(*free)(void*,void*);
 /* Optional compressed-run positional reader. Must validate the ENTIRE
  * compressed stream before returning bytes, including its checksum/end. */
 int(*codec)(Image8*,const Image8Run*,u64,void*,size_t);
};
int image8_decode(Image8*,void*,u64,int(*)(void*,u64,void*,size_t),u8*,size_t);
int image8_decode_codecs(Image8*,void*,u64,int(*)(void*,u64,void*,size_t),u8*,size_t,int(*)(Image8*,const Image8Run*,u64,void*,size_t));
/* Initialize a fresh/released Image8. With alloc/free, always call release
 * after success OR failure; failed parses can own partial metadata pages. */
int image8_decode_alloc(Image8*,void*,u64,int(*)(void*,u64,void*,size_t),u8*,size_t,int(*)(Image8*,const Image8Run*,u64,void*,size_t),void*(*)(void*,size_t),void(*)(void*,void*));
Image8Run*image8_run(Image8*,u64);
void image8_release(Image8*);
int image8_read(void*,u64,void*,size_t);
/* Native streaming codecs. Memory bounded independently of uncompressed run
 * length; a stream error never publishes a successful partial read. */
int image8_codec_read(Image8*,const Image8Run*,u64,void*,size_t);
int image8_codec_supported(u32);
enum {IMAGE_ATTACH,IMAGE_DETACH,IMAGE_INFO,IMAGE_NEXT};
typedef struct {int operation,disk;char path[PATHLEN];char message[160];} ImageRequest;
