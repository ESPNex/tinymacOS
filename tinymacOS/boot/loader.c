#include <efi.h>
#include <efilib.h>
#include "splash.h"
#include "sha256.h"
#include "recoveryboot.h"
#include "bootconfig.h"
#include "bootconsole.h"
#include "bootdefault.h"
#define FAIL(s) do{st->ConOut->OutputString(st->ConOut,L"Impossibile avviare tinymacOS.\r\n");return EFI_LOAD_ERROR;}while(0)
static struct Boot bi;
static EFI_SYSTEM_TABLE *ginst;
static volatile int want_rec;
static BootConsole verbose_console;
static void vmsg(const char *s){if(bi.boot_flags&TMC_BOOT_VERBOSE)bootconsole_put(&bi,&verbose_console,s);}
static int arg16(const CHAR16 *s,UINTN chars)
{
    for(UINTN i=0;i+1<chars;i++)if(s[i]=='-'&&s[i+1]=='v'&&(!i||s[i-1]==' ')&&(i+2==chars||!s[i+2]||s[i+2]==' '))return 1;
    return 0;
}
static void load_boot_config(EFI_FILE_PROTOCOL *root)
{
    EFI_FILE_PROTOCOL *file=0;BootConfig cfg;UINTN n=sizeof cfg;
    if(!EFI_ERROR(root->Open(root,&file,L"\\BOOTCFG.BIN",EFI_FILE_MODE_READ,0))){
        if(!EFI_ERROR(file->Read(file,&n,&cfg))&&n==sizeof cfg&&bootconfig_valid(&cfg))bi.boot_flags=cfg.flags;
        file->Close(file);
    }
}
static void poll_rec(void)
{
    if (want_rec || !ginst)return;
    EFI_INPUT_KEY k;
    if (!EFI_ERROR(ginst->ConIn->ReadKeyStroke(ginst->ConIn,&k))) {
        if(k.UnicodeChar=='r'||k.UnicodeChar=='R'||k.ScanCode==SCAN_F12)want_rec=1;
        if((k.UnicodeChar=='v'||k.UnicodeChar=='V')&&!(bi.boot_flags&TMC_BOOT_VERBOSE)){
            bi.boot_flags|=TMC_BOOT_VERBOSE;bootconsole_clear(&bi,&verbose_console);vmsg("[loader] verbose selected with V\n");
        }
    }
}
static unsigned char mmapbuf[128 * 1024] __attribute__((aligned(16)));
static void progress(EFI_BOOT_SERVICES *bs, int *shown, int target)
{
    while (*shown < target) {
        splash_bar(&bi, ++*shown);
        poll_rec();   /* R (o F12) durante la mela: salta al Recupero */
        bs->Stall(9000);
    }
}
static int read_asset(EFI_BOOT_SERVICES *bs, EFI_FILE_PROTOCOL *f, UINTN max, int low, int high, int *shown,
                      uint64_t *addr, uint64_t *len)
{
    UINT64 z = 0;
    if (EFI_ERROR(f->SetPosition(f, ~(UINT64)0)) || EFI_ERROR(f->GetPosition(f, &z)) || !z || z > max ||
        EFI_ERROR(f->SetPosition(f, 0)))return 0;
    EFI_PHYSICAL_ADDRESS pa = 0xffffffff;
    if (EFI_ERROR(bs->AllocatePages(AllocateMaxAddress, EfiLoaderData, (z + 4095) / 4096, &pa)))return 0;
    for (UINTN at = 0; at < z;) {
        UINTN m = z - at;
        if (m > 131072)m = 131072;
        UINTN got = m;
        if (EFI_ERROR(f->Read(f, &got, (void *)(pa + at))) || got != m)return 0;
        at += got;
        progress(bs, shown, low + (high - low)*at / z);
    }
    f->Close(f);
    *addr = pa;
    *len = z;
    return 1;
}
#include "fatimage.h"
#include "containerboot.h"
EFI_STATUS _entry(EFI_HANDLE image, EFI_SYSTEM_TABLE *st)
{
    fw = st->BootServices;
    EFI_BOOT_SERVICES *bs = st->BootServices;
    bs->SetWatchdogTimer(0, 0, 0, 0);
    EFI_GUID lg = LOADED_IMAGE_PROTOCOL, sg = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID,
             gg = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID, ac = ACPI_20_TABLE_GUID;
    EFI_LOADED_IMAGE *li;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    EFI_FILE_PROTOCOL *root, *f;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *go;
    if (EFI_ERROR(bs->LocateProtocol(&gg, 0, (void **)&go)))
        FAIL("GOP unavailable");
    ginst = st;
    if (EFI_ERROR(bs->HandleProtocol(image, &lg, (void **)&li))
        || EFI_ERROR(bs->HandleProtocol(li->DeviceHandle, &sg, (void **)&fs)) || EFI_ERROR(fs->OpenVolume(fs, &root))
       )FAIL("volume");
    int native_container=boot_container(bs,li->DeviceHandle);
    EFI_HANDLE rd = native_container?0:sibling(bs, li->DeviceHandle, 3);
    load_boot_config(root);
    if(native_container)container_boot_config(bs,&bi);
    if(rd){EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *rfs=0;EFI_FILE_PROTOCOL *rr=0;
        if(!EFI_ERROR(bs->HandleProtocol(rd,&sg,(void **)&rfs))&&!EFI_ERROR(rfs->OpenVolume(rfs,&rr))){load_boot_config(rr);rr->Close(rr);}}
    int best = -1, preferred=-1, score = -1, nm = 0;
    for (UINT32 i = 0; i < go->Mode->MaxMode; i++) {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *m;
        UINTN n;
        if (EFI_ERROR(go->QueryMode(go, i, &n, &m)))
            continue;
        int w = m->HorizontalResolution, h = m->VerticalResolution, s = w * h;
        if (w == 1280 && h == 800)s += 4000000;
        if (w == 1024 && h == 768)
            s += 2000000;
        /* Keep every mode whose pixels fit the desktop framebuffers (1920x1080). */
        /* !(w & 7): the stdvga DISPI only accepts 8px-aligned widths, and
           duplicates would pin the Settings mode cycler on one entry. */
        if (w >= 800 && h >= 600 && w <= 1920 && h <= 1080 && !(w & 7) &&
            (UINT64)w * h <= 1920ULL * 1080 && m->PixelFormat <= 1) {
            if((bi.boot_flags&TMC_BOOT_DISPLAY)&&w==(int)(((bi.boot_flags>>8)&255)*8)&&h==(int)((bi.boot_flags>>16)&2047))preferred=i;
            if (s > score) {
                score = s;
                best = i;
            }
            int dup = 0;
            for (int k = 0; k < nm; k++)
                if (bi.modes[k] == (((UINT32)w << 16) | (UINT32)h))dup = 1;
            if (!dup && nm < 24)  /* hand a switchable list to the kernel */
                bi.modes[nm++] = ((UINT32)w << 16) | (UINT32)h;
        }
        bs->FreePool(m);
    }
    bi.nmodes = (UINT32)nm;
    {
        /* SIP: NVRAM tinymacOS-csr (bit0). Absent => enabled, like a fresh Mac.
         * NOTE: resolve everything through st-> — gnu-efi's external `RT`
         * symbol would enter .dynsym undefined and corrupt the image when
         * crt0 relocates (the PE ships without base relocations). */
        UINTN csz = 4;
        UINT32 csr = 1;
        EFI_GUID gvc = TMC_SIP_GUID;
        if (st->RuntimeServices->GetVariable(L"tinymacOS-csr", &gvc, NULL, &csz, &csr)
                != EFI_SUCCESS) {
            /* Legacy fallback: builds before the GUID move wrote EFI_GLOBAL_VARIABLE. */
            EFI_GUID gg = EFI_GLOBAL_VARIABLE;
            if (st->RuntimeServices->GetVariable(L"tinymacOS-csr", &gg, NULL, &csz, &csr)
                    != EFI_SUCCESS)
                csr = 1;
        }
        bi.csr = (bi.boot_flags&TMC_BOOT_CSR_OVERRIDE)?!(bi.boot_flags&TMC_BOOT_CSR_DISABLED):(csr & 1u);
    }
    if(preferred>=0 && !EFI_ERROR(go->SetMode(go,preferred)))best=preferred;
    else if (best < 0 || EFI_ERROR(go->SetMode(go, best)))
        FAIL("No compatible 32-bit GOP mode");
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *m = go->Mode->Info;
    bi.fb = go->Mode->FrameBufferBase;
    bi.w = m->HorizontalResolution;
    bi.h = m->VerticalResolution;
    bi.stride = m->PixelsPerScanLine;
    bi.rgb = m->PixelFormat == 0;
    if (bi.fb + (UINT64)bi.stride * bi.h * 4 > 0x100000000ULL)
        FAIL("GOP outside 32-bit physical map");
    UINT32 verbose=0;UINTN vsize=sizeof verbose;EFI_GUID vg=TMC_SIP_GUID;
    if(!EFI_ERROR(st->RuntimeServices->GetVariable(L"tinymacOS-verbose",&vg,0,&vsize,&verbose))&&verbose==1)bi.boot_flags|=TMC_BOOT_VERBOSE;
    /* Global Apple boot-args may be inherited from another OS/older OpenCore.
     * Our persisted BootConfig, explicit load options and V key are authoritative. */
    if(li->LoadOptions&&li->LoadOptionsSize<=2048&&arg16(li->LoadOptions,li->LoadOptionsSize/2))bi.boot_flags|=TMC_BOOT_VERBOSE;
    if(bi.boot_flags&TMC_BOOT_VERBOSE){bootconsole_clear(&bi,&verbose_console);vmsg("[loader] tinymacOS verbose boot\n");}
    splash_logo(&bi);
    splash_bar(&bi, 0);
    int displayed = 0;
    EFI_GUID rngid = EFI_RNG_PROTOCOL_GUID;
    EFI_RNG_PROTOCOL *rng = 0;
    if (!EFI_ERROR(bs->LocateProtocol(&rngid, 0, (void **)&rng)) &&
        !EFI_ERROR(rng->GetRNG(rng, 0, 16, bi.entropy)))bi.rng_ok = 1;

    int rec = recflag(bs, li->DeviceHandle) || recovery_key(st);
#ifdef TMC_RECOVERY_ENTRY
    rec=1;
#endif
    if (rec) {
recover:
        ;
        EFI_FILE_PROTOCOL *rr=root;
        if (rd) {
            EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *part=0;
            if (EFI_ERROR(bs->HandleProtocol(rd,&sg,(void **)&part)) || EFI_ERROR(part->OpenVolume(part,&rr)))return bad_base(st);
        }
        if(native_container?!container_recovery(bs,&bi):!native_recovery(bs,rr,&bi))return bad_base(st);
        goto recovery_loaded;
    }
    UINT64 filelen;
    void *file;
    UINTN got;
    if (EFI_ERROR(root->Open(root, &f, L"\\System\\Library\\KernelCollections\\KernelCache", EFI_FILE_MODE_READ,
                             0)) || !read_asset(bs, f, 8 << 20, 0, 16, &displayed, &bi.image, &bi.image_len))goto recover;
    vmsg("[loader] KernelCache read\n");
    u8 *kc = (void *)bi.image, sum[32];
    if (bi.image_len < 128 || memcmp(kc, "TMKCACHE", 8) || ((u32 *)kc)[2] != 2 || ((u32 *)kc)[3] != 62 ||
        ((u64 *)kc)[2] != 64 || ((u64 *)kc)[3] != bi.image_len - 64)goto recover;
    sha_bytes(kc + 64, bi.image_len - 64, sum);
    if (memcmp(sum, kc + 32, 32))goto recover;
    if (EFI_ERROR(root->Open(root, &f, L"\\System\\Library\\CoreServices\\System.vol", EFI_FILE_MODE_READ, 0)) ||
        !read_asset(bs, f, 32 << 20, 16, 44, &displayed, &bi.volume, &bi.volume_len) || bi.volume_len < 48)goto recover;
    bi.volume_len -= 32;
    sha_bytes((void *)bi.volume, bi.volume_len, sum);
    if (memcmp(sum, (u8 *)bi.volume + bi.volume_len, 32))goto recover;
    vmsg("[loader] kernel and system SHA-256 verified\n");
    EFI_STATUS self=root->Open(root,&f,L"\\EFI\\TMC\\BOOTX64.EFI",EFI_FILE_MODE_READ,0);
    if(EFI_ERROR(self))self=root->Open(root,&f,L"\\EFI\\BOOT\\BOOTX64.EFI",EFI_FILE_MODE_READ,0);
    if (EFI_ERROR(self) ||
        !read_asset(bs, f, 2 * 1024 * 1024, 45, 49, &displayed, &bi.efi,
                    &bi.efi_len))FAIL("Cannot read boot executable");
    // Only the installation medium contains the ready-to-copy EFI system volume.
    // Disks boot directly from their own FAT ESP; no firmware services after hand-off.
    EFI_STATUS inst = root->Open(root, &f, L"\\Install.esp", EFI_FILE_MODE_READ, 0);
    if (inst == EFI_SUCCESS) {
        if (!read_asset(bs, f, 24 * 1024 * 1024, 50, 58, &displayed, &bi.install_esp,
                        &bi.install_len))FAIL("Cannot read install volume");
        // Assemble Recovery HD from its FAT prefix and the very same BaseSystem.dmg
        // used to boot Recovery on the CD. No sidecar, second DMG or host lookup.
        u64 prefix=0,prefix_len=0,dmg=0,dmg_len=0;
        const UINTN capacity=10u<<20; /* == NREC*512 in include/disklayout.h */
        if (EFI_ERROR(root->Open(root,&f,L"\\Recovery\\Volume.hdr",EFI_FILE_MODE_READ,0)) ||
            !read_asset(bs,f,capacity-BASE_LEN,58,58,&displayed,&prefix,&prefix_len) || prefix_len%512 ||
            EFI_ERROR(root->Open(root,&f,BASE_NAME,EFI_FILE_MODE_READ,0)) ||
            !read_asset(bs,f,BASE_LEN,58,59,&displayed,&dmg,&dmg_len) || !check_base((void *)dmg,dmg_len))return bad_base(st);
        EFI_PHYSICAL_ADDRESS recbuf=0xffffffff;
        if (EFI_ERROR(bs->AllocatePages(AllocateMaxAddress,EfiLoaderData,capacity/4096,&recbuf)))FAIL("memory");
        bs->SetMem((void *)recbuf,capacity,0);
        bs->CopyMem((void *)recbuf,(void *)prefix,prefix_len);
        bs->CopyMem((void *)(recbuf+prefix_len),(void *)dmg,dmg_len);
        bs->FreePages(prefix,(prefix_len+4095)/4096);bs->FreePages(dmg,(dmg_len+4095)/4096);
        bi.recovery_img=recbuf;bi.recovery_len=capacity;
        u64 sig = 0, sz = 0;
        if (EFI_ERROR(root->Open(root, &f, L"\\Recovery\\Payload.sha", EFI_FILE_MODE_READ, 0)) ||
            !read_asset(bs, f, 64, 59, 59, &displayed, &sig, &sz) || sz != 64)goto recover;
        sha_bytes((void *)bi.install_esp, bi.install_len, sum);
        if (memcmp(sum, (void *)sig, 32))goto recover;
        sha_bytes((void *)bi.recovery_img, bi.recovery_len, sum);
        if (memcmp(sum, (u8 *)sig + 32, 32))goto recover;
        u64 n4=0,r4=0,s4=0,sn=0;
        if(EFI_ERROR(root->Open(root,&f,L"\\install-4k.esp",EFI_FILE_MODE_READ,0))||
           !read_asset(bs,f,24u<<20,59,59,&displayed,&bi.install_4k,&n4)||n4!=(24u<<20)||
           EFI_ERROR(root->Open(root,&f,L"\\recovery-4k.img",EFI_FILE_MODE_READ,0))||
           !read_asset(bs,f,10u<<20,59,59,&displayed,&bi.recovery_4k,&r4)||r4!=(10u<<20)||
           EFI_ERROR(root->Open(root,&f,L"\\Payload4K.sha",EFI_FILE_MODE_READ,0))||
           !read_asset(bs,f,64,59,59,&displayed,&s4,&sn)||sn!=64)FAIL("4Kn payload");
        sha_bytes((void*)bi.install_4k,n4,sum);if(memcmp(sum,(void*)s4,32))goto recover;
        sha_bytes((void*)bi.recovery_4k,r4,sum);if(memcmp(sum,(u8*)s4+32,32))goto recover;
        bs->FreePages(s4,1);

    } else if (inst != EFI_NOT_FOUND)FAIL("Cannot inspect boot volume");
#ifndef TMC_RECOVERY_ENTRY
    if(inst==EFI_NOT_FOUND&&(bi.boot_flags&TMC_BOOT_DEFAULT)){
        /* Register OpenCore itself, not its removable-media bootstrap. The
         * fallback alias can be rediscovered as an OS entry on later boots,
         * recursively launching the picker instead of the native stage. */
        EFI_DEVICE_PATH*target=filepath(li->DeviceHandle,L"\\EFI\\OC\\OpenCore.efi");
        if(target){EFI_STATUS result=boot_set_default(st->RuntimeServices,target,dpsize(target));bs->FreePool(target);
            if(EFI_ERROR(result))vmsg("[boot] OpenCore preference not verified; inspect firmware boot settings\n");
        }
    }
#endif
    if (want_rec)goto recover;   /* R premuto durante la mela */
    EFI_DEVICE_PATH *dp = devpath(li->DeviceHandle);
    for (int k = 0; dp && !IsDevicePathEnd(dp) && k < 64; k++, dp = NextDevicePathNode(dp)) {
        if (DevicePathType(dp) == MEDIA_DEVICE_PATH && DevicePathSubType(dp) == MEDIA_HARDDRIVE_DP &&
            DevicePathNodeLength(dp) >= 42) {
            HARDDRIVE_DEVICE_PATH *part = (void *)dp;
            if (part->SignatureType == 2)bs->CopyMem(bi.esp_id, part->Signature, 16);
        }
    }
    if(native_container)bs->CopyMem(bi.esp_id,container_boot.esp_id,16);
    root->Close(root);
    file=(u8 *)bi.image+64;filelen=got=bi.image_len-64;
    goto kernel_loaded;
recovery_loaded:
    file=(void *)bi.image;filelen=got=bi.image_len;
kernel_loaded:
    if(filelen<sizeof(struct Elf))FAIL("Short kernel image");
    struct Elf *e = file;
    if (*(UINT32 *)e->id != 0x464c457f || e->id[4] != 2 || e->machine != 62 || e->type != 2
        || e->phentsize != sizeof(struct Ph) || !e->phnum || e->phoff > got
        || e->phnum * sizeof(struct Ph) > got - e->phoff)
        FAIL("Invalid ELF64");
    struct Ph *p = (void *)((char *)file + e->phoff);
    UINT64 lo = ~0ULL, hi = 0, entry = e->entry;
    int ok = 0;
    for (UINTN i = 0; i < e->phnum; i++)
        if (p[i].type == 1 && p[i].memsz) {
            struct Ph *h = p + i;
            if (h->pa != h->va || h->pa < 0x1000000 || h->pa > 0xa000000 || h->memsz > 0xa000000 - h->pa
                || h->filesz > h->memsz || h->off > got || h->filesz > got - h->off)
                FAIL("Invalid load segment");
            if (h->pa < lo)
                lo = h->pa;
            if (h->pa + h->memsz > hi)
                hi = h->pa + h->memsz;
            if ((h->flags & 1) && entry >= h->pa && entry < h->pa + h->memsz)
                ok = 1;
        }
    if (!ok)
        FAIL("Bad entry point");
    EFI_PHYSICAL_ADDRESS dest = lo & ~4095ULL;
    UINTN pages = (hi - dest + 4095) / 4096;
    if (EFI_ERROR(bs->AllocatePages(AllocateAddress, EfiLoaderData, pages, &dest)))
        FAIL("Kernel load address not available");
    bs->SetMem((void *)dest, pages * 4096, 0);
    for (UINTN i = 0; i < e->phnum; i++)
        if (p[i].type == 1 && p[i].filesz)
            bs->CopyMem((void *)p[i].pa, (char *)file + p[i].off, p[i].filesz);
    progress(bs, &displayed,
             60); // Retain the original optical bytes as EfiLoaderData, never reclaimed by this PMM.
    for (UINTN i = 0; i < st->NumberOfTableEntries; i++)
        if (!memcmp(&st->ConfigurationTable[i].VendorGuid, &ac, 16))
            bi.rsdp = (UINTN)st->ConfigurationTable[i].VendorTable;
    if(!bi.rsdp){EFI_GUID ac1=ACPI_TABLE_GUID;
        for(UINTN i=0;i<st->NumberOfTableEntries;i++)if(!memcmp(&st->ConfigurationTable[i].VendorGuid,&ac1,16))bi.rsdp=(UINTN)st->ConfigurationTable[i].VendorTable;}
    /* OpenDuet may not publish SeaBIOS' ACPI pointer as a configuration table.
     * Standard legacy RSDP discovery, checksummed again by the native parser. */
    if(!bi.rsdp)for(UINTN a=0xe0000;a+36<=0x100000;a+=16){
        const u8 *p=(const u8 *)a;unsigned sum=0;
        if(memcmp(p,"RSD PTR ",8))continue;
        for(unsigned j=0;j<20;j++)sum+=p[j];
        if(!(sum&255)){bi.rsdp=a;break;}
    }
    vmsg("[loader] entering native kernel, ExitBootServices\n");
    for (int tries = 0; tries < 8; tries++) {
        UINTN n = sizeof(mmapbuf), key, ds;
        UINT32 ver;
        if (EFI_ERROR(bs->GetMemoryMap(&n, (EFI_MEMORY_DESCRIPTOR *)mmapbuf, &key, &ds, &ver)))
            FAIL("Memory map unavailable");
        bi.map = (UINTN)mmapbuf;
        bi.bytes = n;
        bi.step = ds;
        EFI_STATUS r = bs->ExitBootServices(image, key);
        if (!EFI_ERROR(r)) {
            __asm__ volatile("cli; cld");
            ((void(*)(struct Boot *))entry)(&bi);
            for (;;)
                __asm__ volatile("hlt");
        }
        if (r != EFI_INVALID_PARAMETER)
            break;
    }
    return EFI_LOAD_ERROR;
}
