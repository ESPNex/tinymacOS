#pragma once
// The only supported UDIF is the sealed, uncompressed FAT16 volume from this build.
// No generic disk-image decoder or XML interpreter is needed before the OS boots.
#include "baseseal.h"
#define BASE_NAME L"\\Recovery\\BaseSystem.dmg"
static u32 be4(const u8 *p) { return (u32)p[0]<<24 | (u32)p[1]<<16 | (u32)p[2]<<8 | p[3]; }
static u64 be8(const u8 *p) { return (u64)be4(p)<<32 | be4(p+4); }
static int check_base(const u8 *p, UINTN n)
{
    if (n!=BASE_LEN || n<BASE_FORK+512) return 0;
    const u8 *k=p+n-512;
    if (memcmp(k,"koly",4) || be4(k+4)!=4 || be4(k+8)!=512 || be8(k+24)!=0 ||
        be8(k+32)!=BASE_FORK || be8(k+216)!=BASE_FORK || be8(k+224)!=n-512-BASE_FORK ||
        be8(k+492)!=BASE_FORK/512) return 0;
    u8 sum[32];sha_bytes(p,n,sum);return !memcmp(sum,base_sha,32);
}
typedef struct {
    const u8 *b;
    u32 bytes, spc, nfat, fat, root, rootsz, data, clusters;
} FatImage;
static inline u32 field16(const u8 *p)
{
    return p[0] | (u32)p[1] << 8;
}
static int geometry(FatImage *f, const u8 *b, u32 bytes)
{
    if (bytes != BASE_FORK || field16(b + 11) != 512 || b[510] != 85 || b[511] != 170)return 0;
    u32 spc = b[13], nf = b[16], fat = field16(b + 14) * 512, fs = field16(b + 22) * 512, root = fat + nf * fs,
        rs = field16(b + 17) * 32, ds = root + ((rs + 511) & ~511u);
    if (!spc || (spc & (spc - 1)) || spc > 64 || !nf || nf > 2 || !fat || !fs || !rs || ds >= bytes ||
        fat + fs > bytes)return 0;
    u32 n = (bytes - ds) / (512 * spc);
    if (n < 4085 || n >= 65525 || 2 * (n + 2) > fs)return 0;
    *f = (FatImage) {
        b, bytes, spc * 512, fs, fat, root, rs, ds, n
    };
    return 1;
}
static int clok(FatImage *f, u32 c)
{
    return c >= 2 && c < f->clusters + 2;
}
static u32 nextcl(FatImage *f, u32 c)
{
    return field16(f->b + f->fat + c * 2);
}
static const u8 *dirent16(FatImage *f, u32 c, const char *s)
{
    for (u32 i = 0; i <= f->clusters; i++) {
        if (c && !clok(f, c))return 0;
        u32 off = c ? f->data + (c - 2) * f->spc : f->root, n = c ? f->spc : f->rootsz;
        for (u32 j = 0; j < n; j += 32) {
            const u8 *e = f->b + off + j;
            if (!*e)return 0;
            if (*e != 0xe5 && !(e[11] & 8) && !memcmp(e, s, 11))return e;
        }
        if (!c)return 0;
        c = nextcl(f, c);
    }
    return 0;
}
static int base_file(EFI_BOOT_SERVICES *bs,FatImage *fs,const char *name,u64 *addr,u64 *len)
{
    const u8 *e=dirent16(fs,0,name);if(!e || (e[11]&16))return 0;
    u32 c=field16(e+26),z;memcpy(&z,e+28,4);if(!z||z>BASE_FORK)return 0;
    EFI_PHYSICAL_ADDRESS p=0xffffffff;
    if(EFI_ERROR(bs->AllocatePages(AllocateMaxAddress,EfiLoaderData,(z+4095)/4096,&p)))return 0;
    for(u32 at=0;at<z;){if(!clok(fs,c))return 0;u32 n=MIN(fs->spc,z-at);
        memcpy((void *)(p+at),fs->b+fs->data+(c-2)*fs->spc,n);c=nextcl(fs,c);at+=n;}
    if(c<0xfff8)return 0;*addr=p;*len=z;return 1;
}
static int native_recovery(EFI_BOOT_SERVICES *bs,EFI_FILE_PROTOCOL *root,struct Boot *b)
{
    EFI_FILE_PROTOCOL *f=0;u8 *data=0;UINTN n=BASE_LEN;UINT64 size=0;int ok=0;
    if(EFI_ERROR(root->Open(root,&f,BASE_NAME,EFI_FILE_MODE_READ,0)))return 0;
    if(EFI_ERROR(f->SetPosition(f,~(UINT64)0))||EFI_ERROR(f->GetPosition(f,&size))||size!=n||EFI_ERROR(f->SetPosition(f,0)))goto done;
    if(EFI_ERROR(bs->AllocatePool(EfiLoaderData,n,(void **)&data)))goto done;
    if(EFI_ERROR(f->Read(f,&n,data))||!check_base(data,n))goto done;
    FatImage fs;if(!geometry(&fs,data,BASE_FORK))goto done;
    if(!base_file(bs,&fs,"RECOVERYELF",&b->image,&b->image_len)||!base_file(bs,&fs,"RECOVERYVOL",&b->volume,&b->volume_len))goto done;
    if(b->volume_len<48)goto done;u8 sum[32];b->volume_len-=32;sha_bytes((void *)b->volume,b->volume_len,sum);
    if(memcmp(sum,(void *)(b->volume+b->volume_len),32))goto done;ok=1;
done:
    f->Close(f);if(data)bs->FreePool(data);return ok;
}

static EFI_STATUS bad_base(EFI_SYSTEM_TABLE *st)
{
    st->ConOut->ClearScreen(st->ConOut);
    st->ConOut->OutputString(st->ConOut,L"BaseSystem.dmg: Recupero non disponibile.\r\nImmagine mancante, danneggiata o non avviabile. Recupero non eseguito.\r\nUsa una ISO tinymacOS integra. Premi Esc per tornare al firmware.\r\n");
    EFI_INPUT_KEY k;
    while (EFI_ERROR(st->ConIn->ReadKeyStroke(st->ConIn,&k)) || (k.ScanCode!=SCAN_ESC && k.UnicodeChar!=27))
        st->BootServices->Stall(20000);
    return EFI_LOAD_ERROR;
}
