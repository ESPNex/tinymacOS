#pragma once
#include "sha256.h"
#include "partition.h"
#define LESP 2048u
#define NESP 49152u /* 24 MiB: Install.esp payload (System-disk.vol + anchors) */
#define LDATA (LESP+NESP)
#define NDATA 6288u  /* full user-volume image (btrfs SECTORS: 144+384*16) */
#define NPART 57344u /* FS block bound: live + snapshot area (backup starts at BACKUP_OFF) */
#define NREC 20480u /* 10 MiB Recovery HD: RECOVERY.EFI image + BaseSystem.dmg */
#define BACKUP_OFF 57344u
#define MINSECT 131072u /* scan eligible from 64 MiB; install refuses post-scan if layout cannot fit */
#define CHUNK 256u
#define RESTORE_AT(z) ((z)-33u-NESP-1u)
#define REC_AT(z) (RESTORE_AT(z)-NREC)
static const u8 esp_type[] = {0x28, 0x73, 0x2a, 0xc1, 0x1f, 0xf8, 0xd2, 0x11, 0xba, 0x4b, 0, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b};
static const u8 dat_type[] = {0x34, 0xbc, 0xe9, 0x28, 0x9c, 0x36, 0x43, 0x48, 0xb5, 0xae, 0x18, 0x9c, 0x51, 0x32, 0x70, 0xd0};
static const u8 src_type[] = {0x74, 0x4d, 0x43, 0x32, 0x50, 0x61, 0x4d, 0x41, 0xb9, 0x67, 0x38, 0xda, 0xd2, 0xd7, 0xe0, 0x7d};
typedef struct PACK {
    u8 type[16], id[16];
    u64 start, end, attrs;
    u16 name[36];
} Part;
typedef struct PACK {
    u8 sig[8];
    u32 rev, size, crc, res;
    u64 at, other, first, last;
    u8 guid[16];
    u64 entries;
    u32 num, entrysize, ecrc;
    u8 pad[420];
} Gpt;
typedef struct PACK {
    u8 magic[8], disk[16];
    u32 sectors, ver;
    u8 digest[32], pad[448];
} Seal;
_Static_assert(sizeof(Seal) == 512 && sizeof(Gpt) == 512 && sizeof(Part) == 128, "media ABI");
typedef struct {
    int(*io)(int, u32, u32, void *, int);
    int(*flush)(int);
    int d;
    u32 z;
    int(*raw)(int,u32,u32,void*,int); /* optional metadata-only raw path */
} Medium;
typedef struct {
    int pv, bv, type, container;
    u32 esp,data,rec,source;
    Gpt g;
    Part parts[128];
} Layout;
u32 crc32(const void *, size_t);
int layout_read(Medium *, Layout *);
int firstaid(Medium *, int);
int save_copy(Medium *, const void *);
int has_copy(Medium *);
int restore_copy(Medium *);
int restore_os(Medium *);
int reset_data(Medium *);
int tiny_check(const void *);

int partition_scan(Medium *, DiskMap *);
int filesystem_probe(Medium *,u32,u32);
