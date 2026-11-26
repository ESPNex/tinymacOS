#include "../kernel/k.h"
#include "sha256.h"
#include "disklayout.h"
#include "biosboot.h"
#include "container.h"
#include "nativefs.h"
#ifdef TMC_FS2_BACKEND
#include "tmcbfs2_seed.h"
#include "fs2_runtime.h"
#include "fs2_volume.h"
#endif
static int iszero(const void *p, u32 n)
{
    const u64 *q = p;
    for (u32 i = 0; i < n / 8; i++)
        if (q[i])
            return 0;
    return 1;
}
static u8 io[CHUNK * 512] ALIGNED(4096),mbr[512];
_Static_assert(NDATA==NF_MAX_IMAGE,"shared native image workspace geometry");
/* Both users are serialized by fs_gate. disk_edit/maintenance reject busy
 * installation; no resize call is made recursively during seed import. */
#define snapshot native_image_workspace
static Part parts[128];
static Gpt primary, backup;
static int mounted = -1;
int volume_current(void){return mounted;}
static u32 offset,mounted_extent=NPART;
int volume_extent(u32*base,u32*sectors){if(mounted<0)return -1;*base=offset;*sectors=mounted_extent;return 0;}
static int bread(u32 l, void *p)
{
    return l < MIN(NPART,mounted_extent) ? volume_cipher_io(mounted,offset+l,1,p,0) : -1;
}
static int bwrite(u32 l, const void *p)
{
    return l < MIN(NPART,mounted_extent) ? volume_cipher_io(mounted,offset+l,1,(void*)p,1) : -1;
}
int volume_mount(Block *b)
{
    if(boot.recovery)return 0; /* never mount/write the installed home in Recovery */
    for (int d = 0; d < sata_count(); d++) {
        u32 z = sata_size(d);
        if (z < NPART || sata_io(d, 0, 1, io, 0))
            continue;
#ifdef TMC_DEBUG
        logstr("[vmdbg] ilen=");
        lognum(boot.install_len);
        logstr(" io=");
        for (int i = 0; i < 8; i++) {
            u8 c = io[i];
            char t[3] = {(char)("0123456789abcdef"[c >> 4]), (char)("0123456789abcdef"[c & 15]), 0};
            logstr(t);
        }
        logstr("\n");
#endif
        if (boot.install_len && !memcmp(io, "TINYFS1\0", 8))offset = 0;
        else {
            static Layout map;
            Medium m={sata_io, sata_flush, d, z,0,sata_logical_bytes(d)};
            if (!layout_read(&m, &map) || (!boot.install_len && memcmp(map.parts[0].id, boot.esp_id, 16)))continue;
            offset = map.data;mounted_extent=map.parts[1].end-map.data+1;
        }
        mounted = d;
        if(offset)volume_vault(d);
        *b = (Block) {
            bread, bwrite, MIN(NPART,mounted_extent)
        };
        return 1;
    }
    return 0;
}
static InstallInfo st;
static u32 pos;
#ifdef TMC_FS2_BACKEND
static struct {int enabled,acknowledged,disk,reset;char password[65];u8 random[128];} enrollment;
static void enrollment_clear(void){erase(&enrollment,sizeof enrollment);}
#else
static void enrollment_clear(void){}
#endif
static DiskMap inventory;
static int coexist;
static u32 install_scale=1;
static u32 target_source(u32 z){Medium m={.z=z,.logical_bytes=install_scale*512};return medium_source(&m);}
static u32 target_rec(u32 z){return target_source(z)-NREC;}
static u32 install_start,install_end,install_esp,install_data,install_rec,install_source,install_duet;
static NativeContainer descriptor;
static Part oldparts[128];
static Gpt oldprimary,oldbackup;
static u8 oldmbr[512];

static int isbusy(void)
{
    return st.state >= INSTALL_SCAN && st.state <= INSTALL_VERIFY;
}
static void note(int s, const char *t)
{
    st.state = s;
    strset(st.message, t, sizeof st.message);
    logstr("[install] ");
    logstr(t);
    logstr("\n");
}
static int err(const char *t)
{
    enrollment_clear();
    note(INSTALL_ERROR, t);
    return -1;
}
static void refresh(void)
{
    st.count = sata_count();
    st.live = !!boot.install_len;
#ifdef TMC_FS2_BACKEND
    st.encryption_available = 1;
#endif
    for (int i = 0; i < st.count; i++) {
        InstallDisk *q = st.disk + i;
        q->sectors = sata_size(i);
        strset(q->model, sata_model(i), 41);
        Medium medium={sata_io,sata_flush,i,q->sectors,0,sata_logical_bytes(i)};
        partition_scan(&medium,&inventory);q->scheme=inventory.scheme;q->start=inventory.start;q->available=inventory.sectors;
        strset(q->reason,inventory.reason,sizeof q->reason);q->formats[0]=0;
        int seen[FS_COUNT]={0};for(int j=0;j<inventory.count;j++){int f=inventory.part[j].fs;if(f>=0&&f<FS_COUNT&&!seen[f]){if(q->formats[0])append(q->formats,", ",sizeof q->formats);append(q->formats,filesystem_name(f),sizeof q->formats);seen[f]=1;}}
        q->usable=st.live&&i!=mounted&&inventory.safe&&inventory.slot>=0&&
            (medium_scale(&medium)==1||(boot.install_4k&&boot.recovery_4k&&inventory.scheme==MAP_EMPTY))&&
            (inventory.scheme==MAP_EMPTY?q->sectors>=196608u:inventory.sectors>=TC_SECTORS);
        if(q->usable&&inventory.scheme!=MAP_EMPTY){
            static Layout existing;
            if(layout_read(&medium,&existing)){
                q->usable=0;strset(q->reason,"tinymacOS già presente: usa Recupero",sizeof q->reason);
            }
        }

    }
}
void install_status_i(InstallInfo *q)
{
    if (!isbusy() && st.state != INSTALL_DONE)
        refresh();
    *q = st;
}
int install_begin_i(int d)
{
    if (isbusy() || st.state == INSTALL_DONE)
        return -1;
    refresh();
    if (!st.live || boot.recovery_len != NREC * 512ull || boot.install_len != NESP * 512ull || d < 0 ||
        d >= st.count || !st.disk[d].usable)
        return err("Disco non disponibile: nessuna modifica.");
    #ifdef TMC_FS2_BACKEND
    if(enrollment.enabled&&(!enrollment.acknowledged||enrollment.disk!=d||enrollment.reset))return err("Destinazione cifrata cambiata: riconfigura FileVault.");
#endif
    st.target = d;
    st.done = pos = 0;
    Medium medium={sata_io,sata_flush,d,sata_size(d),0,sata_logical_bytes(d)};
    if(!partition_scan(&medium,&inventory)||!inventory.safe)return err("Tabella cambiata: nessuna modifica.");
    coexist=inventory.scheme!=MAP_EMPTY;install_scale=medium_scale(&medium);if(install_scale!=1&&install_scale!=8)return err("Geometria logica non supportata.");
    install_start=coexist?inventory.start:0;install_end=coexist?install_start+TC_SECTORS:medium.z;
    install_esp=coexist?install_start:LESP;install_data=coexist?install_start+TC_DATA:LDATA;
    install_rec=coexist?install_start+TC_REC:target_rec(medium.z);install_source=coexist?install_start+TC_SOURCE:target_source(medium.z);
    install_duet=coexist?install_start+TC_DUET:TMC_DUET_LBA;
    if(sata_io(d,0,1,oldmbr,0))return err("Lettura MBR non riuscita.");
    if(coexist&&inventory.scheme==MAP_GPT){
        if(sata_io(d,1,1,&oldprimary,0)||sata_io(d,medium.z-1,1,&oldbackup,0)||sata_io(d,oldprimary.entries,(oldprimary.num*128+511)/512,oldparts,0))return err("Lettura GPT non riuscita.");
    }
    st.total=install_end-install_start+2*(2*NESP+NREC+NDATA);
    note(INSTALL_SCAN,coexist?"Verifica dello spazio non allocato":"Controllo del disco vuoto");
    return 0;
}
int install_cancel_i()
{
    if (st.state != INSTALL_SCAN && st.state != INSTALL_ERROR)
        return -1;
    enrollment_clear();
    memset(&st, 0, sizeof st);
    pos = 0;
    return 0;
}
static void guid(u8 *out, u8 tag)
{
    SHA256 h;
    u8 sum[32];
    sha_init(&h);
    sha_update(&h, "Orizzonte partition ID", 22);
    sha_update(&h, snapshot + 13 * 512 + 48, 16);
    sha_update(&h, &ticks, 8);
    sha_update(&h, &st.target, 4);
    sha_update(&h, &tag, 1);
    sha_final(&h, sum);
    memcpy(out, sum, 16);
    out[7] = (out[7] & 15) | 64;
    out[8] = (out[8] & 63) | 128;
    erase(sum, 32);
}
static void part(int i, const u8 *t, u32 l, u32 end, const char *n)
{
    Part *p = parts + i;
    memcpy(p->type, t, 16);
    guid(p->id, i + 1);
    p->start = l;
    p->end = end;
    for (int j = 0; n[j] && j < 35; j++)
        p->name[j] = n[j];
}
static int unchanged(int d)
{
    if(sata_io(d,0,1,io,0)||memcmp(io,oldmbr,512))return 0;
    if(inventory.scheme==MAP_GPT){
        if(sata_io(d,1,1,io,0)||memcmp(io,&oldprimary,512)||sata_io(d,sata_size(d)-1,1,io,0)||memcmp(io,&oldbackup,512)||
           sata_io(d,oldprimary.entries,(oldprimary.num*128+511)/512,io,0)||memcmp(io,oldparts,oldprimary.num*128))return 0;
        if(sata_io(d,oldbackup.entries,(oldbackup.num*128+511)/512,io,0)||memcmp(io,oldparts,oldbackup.num*128))return 0;
    }
    return 1;
}
static int coexist_plan(void)
{
    if(!unchanged(st.target))return -1;
    descriptor=(NativeContainer){.version=1,.sectors=TC_SECTORS,.data=TC_DATA,.rec=TC_REC,.source=TC_SOURCE,.duet=TC_DUET};
    memcpy(descriptor.magic,TC_MAGIC,8);guid(descriptor.id,31);guid(descriptor.esp_id,32);
    descriptor.crc=crc32(&descriptor,sizeof descriptor);
    /* Reserved FAT sectors 9/10 are outside its allocation tables and files. */
    u8*esp=(u8*)boot.install_esp;
    memcpy(esp+TC_HEADER*512,&descriptor,512);memcpy(esp+TC_OLD_MBR*512,oldmbr,512);
    memcpy(mbr,oldmbr,512);memcpy(mbr,bios_coexist_mbr,440);
    for(int i=0;i<=432;i++){
        u64 v;memcpy(&v,mbr+i,8);
        if(v==0x1badb002aabbccddull){v=install_duet;memcpy(mbr+i,&v,8);}
        if(v==0x1badb003aabbccddull){v=install_start+TC_OLD_MBR;memcpy(mbr+i,&v,8);}
    }
    if(inventory.scheme==MAP_GPT){
        primary=oldprimary;backup=oldbackup;memcpy(parts,oldparts,sizeof parts);
        Part*p=parts+inventory.slot;memset(p,0,sizeof *p);memcpy(p->type,esp_type,16);memcpy(p->id,descriptor.esp_id,16);
        p->start=install_start;p->end=install_end-1;const char*n="tinymacOS";for(int j=0;n[j];j++)p->name[j]=n[j];
        primary.ecrc=backup.ecrc=crc32(parts,primary.num*128);primary.crc=backup.crc=0;
        primary.crc=crc32(&primary,primary.size);backup.crc=crc32(&backup,backup.size);
    }else{
        u8*p=mbr+446+inventory.slot*16;memset(p,0,16);p[1]=p[5]=254;p[2]=p[3]=p[6]=p[7]=255;p[4]=0xef;
        memcpy(p+8,&install_start,4);u32 n=TC_SECTORS;memcpy(p+12,&n,4);
    }
    return 0;
}
static void layout(u32 n)
{
    memset(parts, 0, sizeof parts);
    part(0, esp_type, LESP, LDATA - 1, "tinymacOS EFI");
    part(1, dat_type, LDATA, target_rec(n) - 1, "Macintosh HD - Dati");
    part(2, esp_type, target_rec(n), target_source(n) - 1, "Recovery");
    part(3, src_type, target_source(n), n-(install_scale==8?41:34), "Installazione locale");
    if(install_scale==8)for(unsigned i=0;i<4;i++){parts[i].start/=8;parts[i].end/=8;}
    u32 native=n/install_scale,table=(32+install_scale-1)/install_scale;
    primary = (Gpt) {
        .rev = 0x10000, .size = 92, .at = 1, .other = native - 1, .first = 2+table, .last = native-2-table, .entries = 2, .num = 128,
        .entrysize = 128, .ecrc = crc32(parts, sizeof parts)
    };
    memcpy(primary.sig, "EFI PART", 8);
    guid(primary.guid, 0);
    backup = primary;
    backup.at = native - 1;
    backup.other = 1;
    backup.entries = native-1-table;
    backup.crc = crc32(&backup, 92);
    primary.crc = crc32(&primary, 92);
    memset(mbr, 0, 512);
    if(install_scale==1)memcpy(mbr,bios_boot_mbr,440);
    mbr[450] = 0xee;
    mbr[448] = 2;
    mbr[451] = mbr[452] = mbr[453] = 255;
    struct PACK {u32 value;} *start=(void*)(mbr+454),*length=(void*)(mbr+458);
    start->value=1;length->value=native-1;
    mbr[510] = 85;
    mbr[511] = 170;
}
static int commit(int d, u32 l, u32 n, void *p)
{
    return sata_io(d, l, n, p, 1) || sata_flush(d) || sata_io(d, l, n, io, 0) || memcmp(io, p, n * 512);
}
static int install_bios(int d)
{
    if(install_scale==8)return 0; /* 4Kn BIOS requires the separate USB/SATA helper */
    u64 bytes=bios_duet_end-bios_duet;
    if(!bytes || bytes%512 || (coexist?bytes/512>install_end-install_duet:bytes/512>LESP-TMC_DUET_LBA))return -1;
    for(u32 j=0;j<bytes/512;){
        u32 n=MIN(CHUNK,(u32)(bytes/512)-j);
        if(commit(d,install_duet+j,n,(void *)(bios_duet+(u64)j*512)))return -1;
        j+=n;
    }
    return 0;
}
int install_step_i()
{
    if (!isbusy())
        return -1;
    int d = st.target;
    u32 z = sata_size(d);
#ifdef TMC_FS2_BACKEND
    if(st.state==INSTALL_DATA){
        u32 sectors=coexist?TC_DATA_N:target_rec(z)-install_data;
        int result;
        if(enrollment.enabled){char recovery[65]={0};result=fs2_install_encrypted(d,install_data,sectors,snapshot,sizeof snapshot,enrollment.password,enrollment.random,recovery);erase(recovery,sizeof recovery);enrollment_clear();}
        else result=fs2_install_data(d,install_data,sectors,snapshot,sizeof snapshot,0);
        if(result)return err("Creazione TMCBFS2 non riuscita: disco non attivato.");
        st.done+=NDATA;note(INSTALL_VERIFY,"Verifica TMCBFS2, sistema e Recupero");pos=0;
    }
#endif
    for (int j = 0; j < (st.state == INSTALL_SCAN && z > 2097152 ? 32 : 8) && isbusy(); j++) {
        u32 span = 2 * NESP + NREC, limit = st.state == INSTALL_SCAN ? install_end-install_start : st.state == INSTALL_ESP ? span : st.state ==
                                            INSTALL_DATA ? NDATA : span + NDATA;
#ifdef TMC_FS2_BACKEND
        if(st.state==INSTALL_VERIFY)limit=span;
#endif
        u32 idx = st.state == INSTALL_DATA ? span + pos : pos,
            off = idx < NESP ? idx : idx < NESP + NREC ? idx - NESP : idx < span ? idx - NESP - NREC : idx - span;
        u32 n = MIN(CHUNK, limit - pos);
        if (st.state != INSTALL_SCAN)n = MIN(n,
                                                 (idx < NESP ? NESP : idx < NESP + NREC ? NREC : idx < span ? NESP : NDATA) - off);
        if (st.state == INSTALL_SCAN) {
            if (sata_io(d, install_start+pos, n, io, 0))
                return err("Errore di lettura: nessuna scrittura.");
            if (!coexist && !iszero(io, n * 512))
                return err("Il disco contiene dati: installazione annullata senza scrivere.");
        } else {
            u8 *src = (idx < NESP ? (u8 *)(install_scale==8?boot.install_4k:boot.install_esp) : idx < NESP + NREC ? (u8 *)(install_scale==8?boot.recovery_4k:boot.recovery_img) : idx < span ?
                       (u8 *)(install_scale==8?boot.install_4k:boot.install_esp) : snapshot) + (u64)off * 512;
            u32 l = (idx < NESP ? install_esp : idx < NESP + NREC ? install_rec : idx < span ? install_source + install_scale : install_data) + off;
            if (st.state == INSTALL_VERIFY) {
                if (sata_io(d, l, n, io, 0) || memcmp(io, src, n * 512))
                    return err("Rilettura non riuscita: disco non attivato.");
            } else if (sata_io(d, l, n, src, 1))
                return err("Scrittura SATA non riuscita: disco non attivato.");
        }
        st.done += n;
        pos += n;
        if (pos != limit)
            continue;
        pos = 0;
        if (st.state == INSTALL_SCAN) {
            if (install_rec - 1 < install_data + BACKUP_OFF + NDATA)
                return err("Disco troppo piccolo per il layout: servono almeno 96 MiB.");
            if (fs_snapshot(snapshot, sizeof snapshot))
                return err("Copia del volume utente non riuscita.");
#ifdef TMC_FS2_BACKEND
            if(tmc2_seed_validate(snapshot,sizeof snapshot))return err("Volume live non importabile: nessuna scrittura eseguita.");
#endif
            if(coexist){if(coexist_plan())return err("Tabella cambiata: nessuna scrittura eseguita.");}
            else layout(z);
            note(INSTALL_ESP, "Installazione di tinymacOS e di Recupero");
        } else if (st.state == INSTALL_ESP){
            note(INSTALL_DATA, "Copia di account e documenti");
#ifdef TMC_FS2_BACKEND
            return 0; /* format/import at next step, never legacy sector copy */
#endif
        }
        else if (st.state == INSTALL_DATA) {
            if (sata_flush(d))
                return err("Impossibile sincronizzare il disco.");
            note(INSTALL_VERIFY, "Verifica dell’installazione");
        } else {
#ifdef TMC_FS2_BACKEND
            u32 sectors=coexist?TC_DATA_N:target_rec(z)-install_data;
            if(fs2_install_data(d,install_data,sectors,snapshot,sizeof snapshot,1))return err("Rilettura TMCBFS2 non riuscita: disco non attivato.");
            st.done+=NDATA;
#endif
            Seal seal = {.sectors = NESP, .ver = 1};
            memcpy(seal.magic, "TMCSYS20", 8);
            memcpy(seal.disk, coexist?descriptor.id:primary.guid, 16);
            sha_bytes((void *)(install_scale==8?boot.install_4k:boot.install_esp), boot.install_len, seal.digest);
            if(coexist&&!unchanged(d))return err("Tabella cambiata: attivazione annullata.");
            if(install_bios(d)||commit(d,install_source,1,&seal))return err("Payload di avvio non verificato.");
            if(coexist){
                if(inventory.scheme==MAP_GPT && (commit(d,backup.entries,(backup.num*128+511)/512,parts)||commit(d,backup.at,1,&backup)||commit(d,primary.entries,(primary.num*128+511)/512,parts)||commit(d,primary.at,1,&primary)))return err("Aggiornamento GPT interrotto: usa la copia GPT valida.");
                if(commit(d,0,1,mbr))return err("Aggiornamento avvio fallito: MBR originale conservato nel volume.");
            }else if(commit(d,backup.entries*install_scale,32,parts)||commit(d,backup.at*install_scale,1,&backup)||commit(d,primary.entries*install_scale,32,parts)||commit(d,primary.at*install_scale,1,&primary)||commit(d,0,1,mbr))return err("Finalizzazione GPT non riuscita. Non usare questo disco.");
            erase(snapshot, sizeof snapshot);
            enrollment_clear();
            note(INSTALL_DONE, "Installazione completata e verificata");
        }
    }
    return 0;
}

int install_finished_i()
{
    return st.state == INSTALL_DONE;
}

int install_writing_i()
{
    return st.state >= INSTALL_ESP && st.state <= INSTALL_VERIFY;
}

static Layout layout_info;
void disk_info_i(DiskInfo *r)
{
    memset(r, 0, sizeof * r);
    r->count = sata_count();
    r->current = mounted;
    for (int d = 0; d < r->count; d++) {
        Medium m={volume_cipher_io, sata_flush, d, sata_size(d),sata_io,sata_logical_bytes(d)};
        r->disk[d].sectors = m.z;
        strset(r->disk[d].model, sata_model(d), 44);
        Medium raw={sata_io,sata_flush,d,m.z,0,sata_logical_bytes(d)};partition_scan(&raw,&r->disk[d].map);
        int t = layout_read(&m, &layout_info);
        r->disk[d].layout = t;
        r->disk[d].healthy = t && layout_info.pv && layout_info.bv;
#ifdef TMC_FS2_BACKEND
        int v2=t==2?fs2_disk_present(d,layout_info.data,layout_info.parts[1].end-layout_info.data+1):0;
        if(v2)r->disk[d].backup=v2>0&&(d==mounted&&fs2_active?fs2_local_backup(0):fs2_disk_backup(d,layout_info.data,layout_info.parts[1].end-layout_info.data+1,0))>0;
        else
#endif
        r->disk[d].backup = t == 2 && has_copy(&m);
    }
}
int disk_operation_i(int op, int d)
{
    if (isbusy() || d < 0 || d >= sata_count())return -1;
    Medium m={volume_cipher_io, sata_flush, d, sata_size(d),sata_io,sata_logical_bytes(d)};
#ifdef TMC_FS2_BACKEND
    if(layout_read(&m,&layout_info)==2){
        u32 base=layout_info.data,sectors=layout_info.parts[1].end-base+1;
        int v2=fs2_disk_present(d,base,sectors);if(v2<0)return -1;
        if(v2){
            if(op==1||op==4||(boot.recovery&&op==9))return fs2_disk_verify(d,base,sectors)||verify_system_volume(&m)?-1:0;
            if(boot.recovery&&op==6)return fs2_disk_verify(d,base,sectors)||verify_system_volume(&m)||restore_os(&m)?-1:0;
            if(op==2)return d==mounted&&fs2_active?fs2_local_backup(1):-1;
            if(boot.recovery&&op==8)return fs2_disk_backup(d,base,sectors,1);
            if(boot.recovery&&op==5)return fs2_disk_backup(d,base,sectors,2);
            /* Never apply the fixed-offset v1 reset to the new format. */
            if(boot.recovery&&op==7)return fs2_disk_reset(d,base,sectors);
        }
    }
#endif
    if(boot.recovery && op>=5 && op<=9){
        if(layout_read(&m,&layout_info)!=2)return -1;
        if(op==5)return restore_copy(&m);
        if(op==6)return restore_os(&m);
        if(op==7)return reset_data(&m);
        if(op==9)return firstaid(&m,0);
        int ok=1;
        for(u32 i=0;i<NDATA;i+=CHUNK)if(volume_cipher_io(d,layout_info.data+i,MIN(CHUNK,NDATA-i),snapshot+512*i,0)){ok=0;break;}
        int r=ok?save_copy(&m,snapshot):-1;erase(snapshot,sizeof snapshot);return r;
    }
    if (op == 4 || op == 1)return firstaid(&m, op == 1);
    if (d != mounted || boot.install_len || !persistent || layout_read(&m, &layout_info) != 2)return -1;
    if (op == 2) {
        if (fs_snapshot(snapshot, sizeof snapshot))return -1;
        int r = save_copy(&m, snapshot);
        erase(snapshot, sizeof snapshot);
        return r;
    }
    if (op == 3) {
        u8 flag[512] = {0};
        memcpy(flag, "TMRECOV2", 8);
        if (commit(d, layout_info.esp + 8, 1, flag))return -1;
        return power(1);
    }
    return -1;
}

// Public wrappers: one recursive filesystem gate serializes all disk state.
void install_status(InstallInfo *q)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    install_status_i(q);
    rec_unlock(&fs_gate, fl);
}

static int install_begin_mode_i(int d,int encrypted){
 if(isbusy()||st.state==INSTALL_DONE)return -1;
 int prepared=0;
#ifdef TMC_FS2_BACKEND
 prepared=!!enrollment.enabled;
#endif
 if((encrypted!=0&&encrypted!=1)||encrypted!=prepared)return err("FileVault non confermato: riconfigura prima la destinazione.");
 return install_begin_i(d);
}
int install_begin_mode(int d,int encrypted)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = install_begin_mode_i(d,encrypted);
    rec_unlock(&fs_gate, fl);
    return r;
}
int install_begin(int d){return install_begin_mode(d,0);}

int install_cancel(void)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = install_cancel_i();
    rec_unlock(&fs_gate, fl);
    return r;
}

int install_step(void)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = install_step_i();
    rec_unlock(&fs_gate, fl);
    return r;
}

int install_finished(void)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = install_finished_i();
    rec_unlock(&fs_gate, fl);
    return r;
}

int install_writing(void)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = install_writing_i();
    rec_unlock(&fs_gate, fl);
    return r;
}

void disk_info(DiskInfo *r)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    disk_info_i(r);
    rec_unlock(&fs_gate, fl);
}

int disk_operation(int op, int d)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = disk_operation_i(op, d);
    rec_unlock(&fs_gate, fl);
    return r;
}

/* Only update the preallocated one-sector settings file in this boot disk's
 * Recovery FAT16. No arbitrary disk/path writes and no filesystem metadata edits.
 * The immutable/sealed system ESP and recovery DMG are never changed here. */
#include "bootconfig.h"
static Spin cfg_lock;
static Layout cfg_layout;
static u8 cfg_sector[512] ALIGNED(512);
static unsigned cfg16(const u8 *p){return p[0]|(unsigned)p[1]<<8;}
static u32 cfg32(const u8 *p){return cfg16(p)|(u32)cfg16(p+2)<<16;}
int bootconfig_disk(int write,u32 flags)
{
    if(boot.install_len || (write==1?flags>1:!bootconfig_flags_valid(flags)) || (write==3&&(!boot.recovery||(flags!=TMC_BOOT_CSR_OVERRIDE&&flags!=TMC_BOOT_CSR_MASK))))return -2;
    u32 irq;spin_lock(&cfg_lock,&irq);int result=-1;
    for(int d=0;d<sata_count();d++) {
        Medium medium={sata_io,sata_flush,d,sata_size(d),0,sata_logical_bytes(d)};
        if(layout_read(&medium,&cfg_layout)!=2 || (!boot.recovery&&memcmp(cfg_layout.parts[0].id,boot.esp_id,16)))continue;
        u64 base64=cfg_layout.parts[2].start;
        if(base64>0xffffffffu || cfg_layout.parts[2].end-base64+1!=NREC)break;
        u32 base=(u32)base64;
        if(sata_io(d,base,1,cfg_sector,0))break;
        unsigned factor=cfg16(cfg_sector+11)/512;
        if(cfg16(cfg_sector+510)!=0xaa55||(factor!=1&&factor!=8)||cfg_sector[13]!=1||cfg_sector[16]!=2)break;
        unsigned reserved=cfg16(cfg_sector+14),fat=cfg16(cfg_sector+22),entries=cfg16(cfg_sector+17);
        unsigned total=cfg16(cfg_sector+19);if(!total)total=cfg32(cfg_sector+32);
        unsigned root=reserved+2*fat,rootsectors=(entries*32+factor*512-1)/(factor*512),data=root+rootsectors;
        if(!reserved||!fat||!entries||entries>512||total*factor!=NREC||data>=total)break;
        u32 at=0;
        for(unsigned sector=0;sector<rootsectors*factor&&!at;sector++) {
            if(sata_io(d,base+root*factor+sector,1,cfg_sector,0))break;
            for(unsigned j=0;j<512;j+=32) {
                const u8 *p=cfg_sector+j;
                if(memcmp(p,"BOOTCFG BIN",11)||(p[11]&0x18)||cfg32(p+28)!=512)continue;
                unsigned cluster=cfg16(p+26);
                if(cluster>=2 && cluster-2<total-data)at=base+(data+cluster-2)*factor;
            }
        }
        if(!at||sata_io(d,at,1,cfg_sector,0)||!bootconfig_valid((BootConfig *)cfg_sector))break;
        result=((BootConfig *)cfg_sector)->flags;
        if(write) {
            u32 old=((BootConfig *)cfg_sector)->flags;
            u32 next=write==3?((old&~TMC_BOOT_CSR_MASK)|(flags&TMC_BOOT_CSR_MASK)):write==2?((old&~TMC_BOOT_DISPLAY_MASK)|(flags&TMC_BOOT_DISPLAY_MASK)):((old&~TMC_BOOT_VERBOSE)|flags);
            BootConfig want;bootconfig_make(&want,next);
            if(sata_io(d,at,1,&want,1)||sata_flush(d)||sata_io(d,at,1,cfg_sector,0)||memcmp(&want,cfg_sector,512))result=-1;
            else result=next;
        }
        break;
    }
    spin_unlock(&cfg_lock,irq);return result;
}

/* Offline edits share the same recursive gate as the installer, filesystem,
 * snapshots and FileVault. The mounted disk is rejected wholesale, including
 * its EFI/Recovery and extended partitions; reboot into RAM Recovery to edit.
 * Raw IO is intentional: never run partition metadata through FileVault. */
int disk_edit(DiskEdit *q,int apply)
{
    u32 fl;rec_lock(&fs_gate,&fl);int r;
    if(isbusy()||q->disk<0||q->disk>=sata_count()||q->disk==mounted){
        r=q->result=DE_PROTECTED;
        strset(q->message,"Disco in uso: avvia da Recupero o dal supporto live.",sizeof q->message);
    }else{
        Medium m={sata_io,sata_flush,q->disk,sata_size(q->disk),0,sata_logical_bytes(q->disk)};
        if(apply)volume_forget(q->disk);
        r=partition_edit(&m,q,apply);
    }
    rec_unlock(&fs_gate,fl);return r;
}

#ifdef TMC_FS2_BACKEND
static int encrypted_reset_target(int d,Layout*map){
 if(!boot.recovery||d<0||d>=sata_count()||d==volume_current())return 0;
 Medium m={sata_io,sata_flush,d,sata_size(d),sata_io,sata_logical_bytes(d)};
 if(layout_read(&m,map)!=2)return 0;u32 sectors=map->parts[1].end-map->data+1;
 return fs2_disk_present(d,map->data,sectors)==2&&!fs2_volume_locked(d)&&!fs2_disk_verify(d,map->data,sectors);
}
#endif
/* Live fresh-install or offline Recovery re-enrollment preparation. Disclose
 * the recovery credential BEFORE destructive work; no target I/O writes here.
 * An acknowledged preparation is bound to one disk and never a conversion. */
static int install_encryption_i(VaultRequest*q){
 int r=-1;
#ifdef TMC_FS2_BACKEND
 if(!isbusy()&&st.state!=INSTALL_DONE&&(boot.install_len||boot.recovery)){
  enrollment_clear();refresh();
  static Layout reset_map;
  int eligible=boot.recovery?encrypted_reset_target(q->disk,&reset_map):q->disk>=0&&q->disk<st.count&&st.disk[q->disk].usable;
  if(q->disk==-1){r=0;}
  else if(eligible&&q->password[64]==0&&q->replacement[64]==0&&strlen(q->password)>=8&&eq(q->password,q->replacement)&&vault_random(enrollment.random,128)){
   enrollment.enabled=1;enrollment.reset=!!boot.recovery;enrollment.disk=q->disk;strset(enrollment.password,q->password,65);
   static const char hex[]="0123456789abcdef";
   for(unsigned i=0;i<32;i++){q->recovery[2*i]=hex[enrollment.random[48+i]>>4];q->recovery[2*i+1]=hex[enrollment.random[48+i]&15];}q->recovery[64]=0;r=0;
  }else enrollment_clear();
 }
#endif
 erase(q->password,65);erase(q->replacement,65);if(r)erase(q->recovery,65);
 return r;
}
int install_encryption(VaultRequest*q){
 u32 fl;rec_lock(&fs_gate,&fl);int r=install_encryption_i(q);rec_unlock(&fs_gate,fl);return r;
}

static int install_encryption_ack_i(VaultRequest*q){
 int r=-1;
#ifdef TMC_FS2_BACKEND
 if(!isbusy()&&enrollment.enabled&&q->disk==enrollment.disk&&q->password[64]==0){
  char expected[9];static const char hex[]="0123456789abcdef";
  for(unsigned i=0;i<4;i++){expected[2*i]=hex[enrollment.random[76+i]>>4];expected[2*i+1]=hex[enrollment.random[76+i]&15];}expected[8]=0;
  if(eq(q->password,expected)){enrollment.acknowledged=1;r=0;}erase(expected,sizeof expected);
 }
#endif
 erase(q->password,65);return r;
}
int install_encryption_ack(VaultRequest*q){u32 fl;rec_lock(&fs_gate,&fl);int r=install_encryption_ack_i(q);rec_unlock(&fs_gate,fl);return r;}

static int install_encrypted_reset_i(int disk){
 int r=-1;
#ifdef TMC_FS2_BACKEND
 static Layout map;
 if(!isbusy()&&enrollment.enabled&&enrollment.acknowledged&&enrollment.reset&&enrollment.disk==disk&&encrypted_reset_target(disk,&map)){
  char recovery[65]={0};u32 n=map.parts[1].end-map.data+1;
  r=fs2_disk_rekey_reset(disk,map.data,n,enrollment.password,enrollment.random,recovery);erase(recovery,sizeof recovery);
 }
 enrollment_clear();
#endif
 return r;
}
int install_encrypted_reset(int disk){u32 fl;rec_lock(&fs_gate,&fl);int r=install_encrypted_reset_i(disk);rec_unlock(&fs_gate,fl);return r;}

/* Recovery external backup import: boot/system/partition extents are NEVER
 * copied. Only explicitly selected distinct offline data volumes participate. */
int disk_import(DiskImport*q,int commit){
 if(!q)return -1;q->staging_blocks=0;
 strset(q->message,"Importazione rifiutata: usa Recupero e due dischi dati FS2 distinti e sbloccati.",sizeof q->message);
#ifndef TMC_FS2_BACKEND
 (void)commit;return -1;
#else
 if(!boot.recovery||(commit!=0&&commit!=1)||q->reserved||(commit&&q->confirm!=DISK_IMPORT_CONFIRM)||!q->checkpoint[0]||q->checkpoint[47]||q->source<0||q->destination<0||q->source==q->destination||q->source==mounted||q->destination==mounted||q->source>=sata_count()||q->destination>=sata_count())return -1;
 u32 fl;rec_lock(&fs_gate,&fl);int r=-1;
 if(!isbusy()){
  Medium sm={sata_io,sata_flush,q->source,sata_size(q->source),sata_io,sata_logical_bytes(q->source)};
  Medium dm={sata_io,sata_flush,q->destination,sata_size(q->destination),sata_io,sata_logical_bytes(q->destination)};
  /* Large partition inventories live on the heap, not the kernel stack. */
  Layout*s=kmalloc(sizeof *s),*d=kmalloc(sizeof *d);
  if(s&&d&&layout_read(&sm,s)==2&&layout_read(&dm,d)==2){
   r=fs2_external_import(q->source,s->data,s->parts[1].end-s->data+1,q->destination,d->data,d->parts[1].end-d->data+1,q->checkpoint,commit,&q->staging_blocks);
   strset(q->message,r?"Importazione non completata: verifica checkpoint, sblocco e spazio libero. Rimonta il disco prima di riprovare.":commit?"Checkpoint esterno importato: documenti e account sostituiti insieme. Snapshot del destinatario conservate.":"Anteprima verificata. --confirm sostituisce documenti E account sul destinatario; non modifica il disco sorgente.",sizeof q->message);
  }
  kfree(s);kfree(d);
 }
 rec_unlock(&fs_gate,fl);return r;
#endif
}
