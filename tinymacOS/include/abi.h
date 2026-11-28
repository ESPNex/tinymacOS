#pragma once
#include "base.h"
#include "diskedit.h"
#define UBASE 0x8000000000ull
#define UEND (UBASE+0x40000000ull)
#define NAPPS 17
#define LASTAPP COMMAND
#define NPROC 23
#define NCMDS 256 /* bounded full-frame proportional text at 1280x1024 */
#define MAXFILE 8192
#define PATHLEN 144
#define NSLOTS 384
/* S_REMOVE flags are checked in-kernel; rmdir never recursively removes data. */
#define REMOVE_RECURSIVE 1u
#define REMOVE_DIR_ONLY 2u
#define REMOVE_FILE_ONLY 4u
enum {REMOVE_BAD_PATH=1,REMOVE_NOT_EMPTY,REMOVE_READ_ONLY,REMOVE_IO,REMOVE_WRONG_TYPE};

#define HOME "/Users/User"
#define DOCS HOME "/Documents"
enum {DESKTOP, FINDER, TERMINAL, TEXTEDIT, SETTINGS, ABOUT, CALC, CALENDAR, MONITOR, GALLERY, INSTALLER, DISKUTIL, NOTES, STICKIES, TIMEMACHINE, NETSURF, COMMAND};
enum {S_TICKS = 3, S_SLEEP, S_EXIT, S_SPAWN, S_PS, S_KILL, S_INFO, S_ALLOC, S_FREE, S_WOPEN, S_WDRAW, S_WLIST, S_WGET, S_EVENT, S_SEND, S_INPUT, S_PRESENT, S_STAT, S_LIST, S_CREATE, S_REMOVE, S_MOVE, S_OPEN, S_CLOSE, S_READ, S_WRITE, S_POWER, S_PREF, S_STARTARG, S_THREAD, S_DEVICES, S_SESSION, S_AUTH_BEGIN, S_AUTH_STEP, S_LOCK, S_SEEK, S_SETUP_CODE, S_INSTALL, S_CONTEXT, S_DISK, S_EXEC, S_ARGV, S_STDOUT, S_RESULT, S_RETURN, S_TOUCH, S_ART, S_SNAP, S_TRACE, S_RESMODE, S_KEXT, S_NET, S_VM, S_BOOTCONFIG, S_TLS, S_WORDWAIT, S_THREADFINISH, S_INPUTDIAG, S_FILEVAULT, S_CLIPBOARD, S_DRAG};
enum {EV_KEY = 1, EV_MOUSE, EV_CLICK, EV_RESIZE, EV_THEME, EV_ACTION, EV_CLOSE, EV_OPEN, EV_CONTEXT, EV_CONTEXT_READY, EV_POINTER_ABS, EV_SCROLL, EV_DRAG_BEGIN, EV_DRAG_READY, EV_DROP, EV_SELECT};
enum {ACT_NEW = 1, ACT_OPEN, ACT_SAVE, ACT_COPY, ACT_PASTE, ACT_RENAME, ACT_DELETE, ACT_VIEW, ACT_PARENT, ACT_PROPS, ACT_PREVIEW, ACT_SYSTEM, ACT_FILE, ACT_PACKAGE, ACT_CUT, ACT_MOUNT, ACT_UNMOUNT, ACT_EJECT};
enum {K_UP = 128, K_DOWN, K_LEFT, K_RIGHT, K_DEL, K_HOME, K_END, K_F1 = 145, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10, K_F11};
enum {C_RECT = 1, C_TEXT, C_ICON, C_LINE, C_IMAGE, C_BUTTON, C_GLASS, C_RASTER, C_MONO, C_CARET, C_CURSOR, C_BODY};
/* Declarative client hit regions; appended ABI values keep existing IDs stable. */
enum {CUR_NORMAL,CUR_TEXT,CUR_SELECT,CUR_MOVE,CUR_HORIZONTAL,CUR_VERTICAL,CUR_DIAGONAL1,CUR_DIAGONAL2,CUR_UNAVAILABLE,CUR_BUSY,CUR_WORKING,CUR_HELP,CUR_PRECISION,CUR_HANDWRITING,CUR_ALTERNATE,CUR_LOCATION,CUR_PERSON,CUR_COUNT};
/* C_GLASS style: standalone dialog, without the owning window chrome. */
enum {GLASS_DIALOG = 1};
/* Reserved signed C_ICON styles: original Settings SVG masks, never legacy sprites. */
enum {ICON_SETTINGS_APPEARANCE = -100, ICON_SETTINGS_DISPLAY = -101,
      ICON_SETTINGS_PRIVACY = -102, ICON_SETTINGS_BACKUP = -103,
      ICON_SETTINGS_CLOCK = -104, ICON_SETTINGS_GENERAL = -105};
enum {F_SMALL, F_BODY, F_BOLD, F_TITLE, F_DISPLAY, F_MONO, F_MICRO, F_HERO, F_CAPTION, F_CLOCK, F_DISPLAY_SEMIBOLD};
#define NFONTS 11
// High-bit inks are semantic palette tokens; ordinary inks are 0x00RRGGBB.
#define INK(x) (0x80000000u+(x))
enum {I_BG = 0, I_PANEL, I_TXT, I_SUB, I_LINE, I_ACC, I_SEL, I_ACC_TXT, I_CARD, I_SOFT};
typedef struct {
    int type, a, b, c, d;
    u64 when; /* EV_KEY.d: raw HID modifier bitmap. Input occurrence ticks; retained across compositor delivery. */
    char text[PATHLEN];
} Event;
typedef struct {
    short x, y, w, h;
    u8 type, style;
    u16 radius;
    u32 ink;
    char text[100];
} Draw;
typedef struct {
    int pid, app, w, h;
    u32 seq;
    char title[48];
    unsigned transition;
} Win;
typedef struct {
    int pid, app, state, threads;
    u64 switches;
    char name[24];
    u64 cpu_ticks;
    u64 sample_ticks; /* all-CPU timer sample clock, atomic with cpu_ticks */
    u64 resident_bytes; /* committed user leaf pages, excludes tables/shared kernel */
} ProcInfo;
typedef struct {
    u64 total, free, ticks, switches;
    u32 w, h, persistent, procs, fsrev, pci, apic, files, hour, minute, day, month, year, second;
    char cpu[56], user[32];
    u32 theme, wall, dock, reduce_motion, opaque, dark, live_cd;
    u64 busy_ticks;
    u32 ncpu, current_cpu;
    u32 boot_flags;
    u64 core_busy[8], core_ticks[8];
    u32 nmodes, modes[24];       /* switchable resolutions (w<<16|h) */
    u32 recovery;                /* isolated native Recovery session */
    u32 csr;                     /* SIP: bit0 = enabled (live NVRAM config) */
    u32 current_pid;
    u32 cpu_online_mask; /* CPU slot bitmap; AP failures need not be contiguous */
    u64 ram_bytes, reserved_bytes, user_bytes; /* UEFI-described RAM; non-managed; user leaf backing */
} SysInfo;
typedef struct {
    u64 id, parent;
    int dir, ro;
    u64 size;
    char name[48];
} Dirent;
#define INSTALLER_TITLE "Installa tinymacOS Tramontana"
#define REINSTALLER_TITLE "Reinstalla tinymacOS Tramontana"
static inline const char *installer_title(int recovery){return recovery?REINSTALLER_TITLE:INSTALLER_TITLE;}
static inline const char *appname(int a)
{
    const char *const n[] = {"Scrivania", "Finder", "Terminale", "TextEdit", "Impostazioni di Sistema", "Info su tinymacOS", "Calcolatrice", "Calendario", "Monitoraggio Attività", "Galleria", INSTALLER_TITLE, "Utility Disco", "Note", "Memo", "Time Machine", "NetSurf", "Comando"};
    return a >= 0 && a < NAPPS ? n[a] : "Idle";
}

enum { FX_NONE, FX_SLIDE, FX_SHEET, FX_SELECT, FX_DISSOLVE };
static inline int appicon(int app)
{
    static const int ids[] = {14, 1, 2, 3, 4, 5, 16, 17, 18, 19, 38, 39, 40, 41, 42, 0, 0};
    return app >= 0 && app < NAPPS ? ids[app] : 0;
}
static inline const char *bundlename(int a)
{
    static const char *const names[] = {"", "Finder", "Terminal", "TextEdit", "System Settings", "About This Computer", "Calculator", "Calendar", "Activity Monitor", "Gallery", "Install tinymacOS", "Disk Utility", "Notes", "Stickies", "Time Machine", "NetSurf", "Command"};
    return a >= 1 && a <= LASTAPP ? names[a] : "";
}
typedef struct {
    char name[36], kind[20];
} DeviceInfo;

// Session state is authoritative in the kernel; passwords never appear in window lists or log messages.
enum {SESSION_SETUP, SESSION_LOCKED, SESSION_OPEN, SESSION_BROKEN};
typedef struct {
    u32 state, working, wait_seconds, fraction, needs_seed, volume_locked;
    char name[32];
} SessionInfo;
typedef struct {
    char name[32], password[65];
    u32 op; // 0 = normal authentication, 1 = reset an unreadable account (user confirmed).
} AuthRequest;

typedef struct {
    char id[44], ver[16], name[28];
    u32 state, size;             /* state: 1 loaded */
} KextInfo;

typedef struct {
    char name[8];
    u8 mac[6];
    u8 up, wifi;                 /* wifi=1 for the simulated 802.11 radio */
    u32 ip, mask;
    u32 rx, tx;
} NetIfInfo;

typedef struct {
    u32 ip, count;               /* in: target + echo requests */
    u32 ok, rtt[5];              /* out: replies + round trips in ms */
} PingResult;

typedef struct {
    char ssid[32];
    s8 rssi;
    u8 channel, secure;
} WifiAp;

typedef struct {
    u32 state;                   /* 0 idle, 1 associated */
    char ssid[32];
    u32 ip;
    s8 rssi;
    u8 channel;
    char note[44];               /* honest label of the simulated radio */
} WifiStatus;

#define NWALL 13
#define NTHEME 8
/* Fixed read-only resource allowlist, shared by WindowServer and S_ART. */
static inline const char *cursorname(unsigned v)
{
 static const char *const names[]={"Normal.cur","Text.cur","Select.cur","Move.cur","Horizontal Resize.cur","Vertical Resize.cur","Diagonal Resize 1.cur","Diagonal Resize 2.cur","Unavailable.cur","Busy.ani","Working.ani","Help.cur","Precision.cur","Handwriting.cur","Alternate.cur","Location.cur","Person.cur"};
 return v<CUR_COUNT?names[v]:"";
}
static inline const char *wallname(unsigned v)
{
    static const char *const a[] = {"Prisma", "Alba", "Laguna", "Aurora", "Prisma glaciale", "Aurora boreale", "Rame vespertino", "Riviera", "Levante", "Meriggio", "Notturno", "Brezza", "Tramontana"};
    return a[v % NWALL];
}

#include "partition.h"
#define SETUP_MAX_DISKS 8
enum { INSTALL_IDLE, INSTALL_SCAN, INSTALL_ESP, INSTALL_DATA, INSTALL_VERIFY, INSTALL_DONE, INSTALL_ERROR };
typedef struct {
    u64 sectors;
    int usable,scheme;
    int repair_mbr_signature; /* flags: 1 missing 55AA, 2 stale EE coverage; explicit fresh-install consent */
    u64 start,available;
    char formats[72],reason[96];
    char model[41];
} InstallDisk;
typedef struct {
    int count, live, state, target;
    int encryption_available; /* fresh FV003 installation only */
    u64 done, total;
    char message[112];
    InstallDisk disk[SETUP_MAX_DISKS];
    int boot_status; /* 1: verified EFI primary; 0: unavailable/BIOS; -1: EFI update refused */
} InstallInfo;

static inline int darktheme(unsigned t)
{
    return t == 1 || t == 3 || t == 4 || t == 6;
}
static inline const char *themename(unsigned t)
{
    /* Each theme is an accent colour; light/dark is the separate `dark` pref. */
    static const char *const n[] = {"Blu", "Azzurro", "Teal", "Menta", "Rame", "Riviera", "Levante", "Tramontana"};
    return n[t % NTHEME];
}
static inline u32 accent(unsigned t)
{
    static const u32 c[] = {0x2446c7, 0x00a8d2, 0x006d58, 0x80ebc6, 0xf2ac80, 0xc35f32, 0xe0973f, 0x526c86};
    return c[t % NTHEME];
}
// A client can supply only a mask of its own file actions, never a shell command.
#define AMASK(a) (1u<<(a))

#define KCDIR "/System/Library/KernelCollections"
#define CORE "/System/Library/CoreServices"
static inline const char *apploc(int a)
{
    return a == FINDER || a == ABOUT ? CORE : a == TERMINAL || a == MONITOR ||
           a == DISKUTIL ? "/Applications/Utilities" : "/Applications";
}
static inline int lastapp(int live)
{
    /* App ceiling is Time Machine on both media (14 live cells incl. the
       installer, 13 installed cells after the installer is skipped). The
       installer's live-only policy lives in the callers' live_cd checks:
       library/search/open -a skip it on installed, launch() and F11 refuse it. */
    (void)live;
    return TIMEMACHINE;
}
#define ARGMAX 14
#define PIPEMAX 8192
typedef struct {
    int argc, inlen;
    char cwd[PATHLEN], argv[ARGMAX][PATHLEN];
    char in[PIPEMAX];
} ExecArgs;
typedef struct {
    int done, status, bytes;
    char out[PIPEMAX + 1];
} ExecResult;
typedef struct {
    int count, current;
    struct {
        u64 sectors;
        int layout, healthy, backup;
        char model[44];
        u64 region_start[4],region_sectors[4]; /* verified native layout, not UI estimates */
        DiskMap map;
    } disk[8];
} DiskInfo;


// Snapshot catalogue entry exposed to userspace (APFS-style browsing).
typedef struct PACK {
    char name[28];
    unsigned date, id;
} SnapInfo;

// Volume/container stats for Disk Utility, Terminal and Time Machine.
typedef struct {
    u64 total, used;
    u32 snaps, gen;
} FsInfo;

// FileVault-style encryption status: state 0 off / 1 armed, locked flag,
// number of user files protected (Node.reserved).
typedef struct {
    u32 state, locked, files;
} EncInfo;

static inline unsigned theme_wallpaper(unsigned t)
{
    static const unsigned ids[NTHEME]={0,11,4,5,6,7,8,12};
    return ids[t%NTHEME];
}

/* Native volume encryption, NOT Apple's on-disk FileVault format. */
typedef struct {int disk;char password[65],replacement[65],recovery[65];} VaultRequest;
typedef struct {int available,enabled,locked,state,progress,error;u64 protected_sectors;int format; /* 1=FV002/legacy, 2=plain FS2, 3=FV003 */} VaultInfo;

/* S_DISK 12=read-only external checkpoint preflight, 13=confirmed import.
 * Recovery-only; replaces destination files AND private account together. */
#define DISK_IMPORT_CONFIRM 0x544d4933u
typedef struct {
 int source,destination;u32 confirm,reserved;u64 staging_blocks;
 char checkpoint[48],message[160];
} DiskImport;

#include "clipboard.h"
