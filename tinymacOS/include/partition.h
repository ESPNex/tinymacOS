#pragma once
#include "base.h"
/* Kernel/user coordinates are unsigned 64-bit counts of 512-byte sectors.
 * A 16 TiB supported-media ceiling bounds filesystem allocation-map memory. */
#define TMC_DISK_MAX_SECTORS (1ull<<35)
#define TMC_DISK_MAX_MIB (TMC_DISK_MAX_SECTORS/2048u)
#define DISK_PARTS 128
#define DISK_GAPS 129
enum { MAP_UNKNOWN, MAP_EMPTY, MAP_GPT, MAP_MBR };
enum { FS_RAW, FS_FAT12, FS_FAT16, FS_FAT32, FS_NTFS, FS_EXFAT, FS_UNSUPPORTED_NATIVE, FS_OTHER, FS_UNREADABLE, FS_BTRFS, FS_TMC2, FS_COUNT };
typedef struct { u64 start,sectors; int number,fs,logical; u8 type[16]; char name[48]; } DiskPartition;
typedef struct { u64 start,sectors; } DiskGap;
typedef struct {
    int scheme,safe,count,gaps,slot;
    int pmbr_signature_repairable; /* flags: 1 missing 55AA, 2 stale EE coverage; identical GPTs; installer consent only */
    u64 start,sectors; /* largest aligned unallocated extent; never RAW */
    DiskPartition part[DISK_PARTS];
    DiskGap gap[DISK_GAPS];
    char reason[96];
} DiskMap;
static inline const char *filesystem_name(int f){
    const char *const n[]={"RAW / non riconosciuto","FAT12","FAT16","FAT32","NTFS","exFAT","TMCFS legacy — non supportato","Altro filesystem","Errore di lettura","Btrfs (Linux)","TMCFS2"};
    return f>=0&&f<(int)COUNT(n)?n[f]:n[0];
}
static inline const char *partition_scheme(int s){return s==MAP_GPT?"GPT":s==MAP_MBR?"MBR":s==MAP_EMPTY?"Senza tabella":"Tabella non valida";}

static inline int filesystem_native(int f){return f==FS_TMC2;}
