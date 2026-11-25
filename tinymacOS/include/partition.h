#pragma once
#include "base.h"
#define DISK_PARTS 128
#define DISK_GAPS 129
enum { MAP_UNKNOWN, MAP_EMPTY, MAP_GPT, MAP_MBR };
enum { FS_RAW, FS_FAT12, FS_FAT16, FS_FAT32, FS_NTFS, FS_EXFAT, FS_TMC, FS_OTHER, FS_UNREADABLE, FS_BTRFS, FS_COUNT };
typedef struct { u32 start,sectors; int number,fs,logical; u8 type[16]; char name[48]; } DiskPartition;
typedef struct { u32 start,sectors; } DiskGap;
typedef struct {
    int scheme,safe,count,gaps,slot;
    u32 start,sectors; /* largest aligned unallocated extent; never RAW */
    DiskPartition part[DISK_PARTS];
    DiskGap gap[DISK_GAPS];
    char reason[96];
} DiskMap;
static inline const char *filesystem_name(int f){
    const char *const n[]={"RAW / non riconosciuto","FAT12","FAT16","FAT32","NTFS","exFAT","BTRFS nativo (subset)","Altro filesystem","Errore di lettura","BTRFS"};
    return f>=0&&f<(int)COUNT(n)?n[f]:n[0];
}
static inline const char *partition_scheme(int s){return s==MAP_GPT?"GPT":s==MAP_MBR?"MBR":s==MAP_EMPTY?"Senza tabella":"Tabella non valida";}
