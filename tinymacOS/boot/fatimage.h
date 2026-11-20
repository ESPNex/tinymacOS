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
static EFI_STATUS image_recovery(EFI_HANDLE image, EFI_SYSTEM_TABLE *st, EFI_HANDLE dev,
                                 EFI_FILE_PROTOCOL *root)
{
    EFI_BOOT_SERVICES *bs = st->BootServices;
    EFI_FILE_PROTOCOL *f = 0;
    UINTN count=BASE_LEN;UINT64 file_size=0;
    u8 *data=0;void *payload=0;
    EFI_STATUS status=root->Open(root,&f,BASE_NAME,EFI_FILE_MODE_READ,0);
    if (EFI_ERROR(status))return status;
    status=EFI_VOLUME_CORRUPTED;
    if (EFI_ERROR(f->SetPosition(f,~(UINT64)0)) || EFI_ERROR(f->GetPosition(f,&file_size)) ||
        file_size!=BASE_LEN || EFI_ERROR(f->SetPosition(f,0)))goto done;
    if (EFI_ERROR(bs->AllocatePool(EfiLoaderData,count,(void **)&data))) {status=EFI_OUT_OF_RESOURCES;goto done;}
    if (EFI_ERROR(f->Read(f,&count,data)) || !check_base(data,count))goto done;
    FatImage fs;
    if (!geometry(&fs,data,BASE_FORK))goto done;
    const char *parts[] = {"EFI        ", "BOOT       ", "BOOTX64 EFI"};
    u32 c = 0, z = 0;
    for (int i = 0; i < 3; i++) {
        const u8 *e = dirent16(&fs, c, parts[i]);
        if (!e || (i < 2) != !!(e[11] & 16))goto done;
        c = field16(e + 26);
        memcpy(&z, e + 28, 4);
    }
    if (!z || z > BASE_FORK)goto done;
    if (EFI_ERROR(bs->AllocatePool(EfiLoaderData,z,&payload))) {status=EFI_OUT_OF_RESOURCES;goto done;}
    for (u32 at = 0; at < z;) {
        if (!clok(&fs, c))goto done;
        u32 n = MIN(fs.spc, z - at);
        memcpy((u8 *)payload + at, fs.b + fs.data + (c - 2)*fs.spc, n);
        c = nextcl(&fs, c);
        at += n;
    }
    if (c < 0xfff8)goto done;
    status=start_file(image,st,dev,BASE_NAME,payload,z,BASE_LEN);
done:
    f->Close(f);
    if (data)bs->FreePool(data);
    if (payload)bs->FreePool(payload);
    return status;
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
