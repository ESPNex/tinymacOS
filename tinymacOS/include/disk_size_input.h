#pragma once
#include "partition.h"
/* UI and CLI decimal MB (1,000,000 bytes). Sector backends require MiB
 * alignment: floor, never exceed requested bytes, and show effective size in
 * the existing pre-write confirmation. Six fractional places preserve exact
 * byte values when reopening an existing aligned extent. */
static inline int disk_size_mb(const char *s,u64 *sectors)
{
    const u64 limit=(u64)TMC_DISK_MAX_SECTORS*512;
    u64 whole=0,fraction=0,scale=1;int digits=0;
    while(*s>='0'&&*s<='9'){
        whole=whole*10+*s++-'0';digits++;
        if(whole>limit/1000000)return -1;
    }
    if(!digits)return -1;
    if(*s=='.'||*s==','){
        s++;digits=0;
        while(*s>='0'&&*s<='9'){
            if(scale==1000000)return -1;
            fraction=fraction*10+*s++-'0';scale*=10;digits++;
        }
        if(!digits)return -1;
    }
    if(*s)return -1;
    u64 bytes=whole*1000000+fraction*(1000000/scale);
    if(bytes>limit)return -1;
    u64 aligned=(bytes/512)&~2047ull;
    if(!aligned)return -1;*sectors=aligned;return 0;
}
static inline void disk_size_mb_exact(char *out,int cap,u64 sectors)
{
    u64 bytes=(u64)sectors*512;char digits[32];
    number(digits,bytes/1000000);strset(out,digits,cap);
    unsigned remainder=bytes%1000000;
    if(remainder){
        char fraction[8]=".000000";
        for(int i=6;i>=1;i--){fraction[i]='0'+remainder%10;remainder/=10;}
        for(int i=6;i>=1&&fraction[i]=='0';i--)fraction[i]=0;
        append(out,fraction,cap);
    }
}

static inline int disk_lba(const char*s,u64*out){
 u64 n=0;if(!*s)return -1;
 for(;*s;s++){if(*s<'0'||*s>'9'||n>(TMC_DISK_MAX_SECTORS-(*s-'0'))/10)return -1;n=n*10+*s-'0';}
 if(n>TMC_DISK_MAX_SECTORS)return -1;*out=n;return 0;
}
