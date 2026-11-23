#pragma once
#include "abi.h"
#include "vmabi.h"
#include "../include/kext.h"
#include "percpu.h"
#include "spinlock.h"
#include "handoff.h"
#include "io.h"
#define PAGE 4096ull
#define NX (1ull<<63)
#define ADDR 0x000ffffffffff000ull
#define NTASK 32
extern struct Boot boot;
extern u64 *kroot, totalpages, freepages, ticks, switches;
extern u32 fsrev, npci, persistent, apic_enabled, usednodes, theme, wall, dock, reduce_motion, opaque, dark;
extern u64 busy_ticks;
extern char cpuname[56];
void serial_init(void);
void logstr(const char *);
void lognum(u64);
void loghex(u64);
__attribute__((noreturn)) void retail_stop(void);
#define panic(...) retail_stop()
void mem_init(void);
u64 page_get(void);
void page_put(u64);
u64 *vm_new(void);
u64 *vm_pte(u64 *, u64, int);
u64 *vm_pte_l(u64 *, u64, int);
int vm_add(u64 *, u64, u64);
int user_ok(u64, u64, int);
void vm_drop(u64 *);
void vm_unmap(u64 *, u64);
int vm_access(u64 *,u64,u64);
void *kmalloc(size_t);
void kfree(void *);
typedef struct {
    u64 r15, r14, r13, r12, r11, r10, r9, r8, rdi, rsi, rbp, rdx, rcx, rbx, rax, vec, err, rip, cs, flags, rsp,
        ss;
} Frame;
typedef struct {
    int id, gen, at, write;
} FD;
typedef struct {
    int pid, app, dying;
    u32 memory_users; /* low bits: syscall uaccess readers; high bit: VM writer */
    u64 *root;
    int tasks;
    u64 switches, cpu_ticks;
    FD fd[12];
    Win win;
    Draw *draw;
    int ndraw;
    void *raster;              /* C_RASTER payload captured at S_WDRAW */
    u64 rasterlen;
    Event events[32];
    int head, tail;
    char arg[PATHLEN];
    ExecArgs *args;ExecResult *result;int owner,job;char program[48];
    struct {
        u64 addr, n;
    } alloc[32];
    struct { u64 address,bytes,guard; } vm[TMC_VM_SLOTS];
} Process;
typedef struct {
    Frame *f;
    int state, pi, pin, at;
    u64 wake;
    u64 wait_word; /* private compare-and-park key; never dereferenced while asleep */
    u64 fs_base; /* user thread pointer; initialized per task, restored on migration */
    u8 fx[512] ALIGNED(16);
    u8 stack[16384] ALIGNED(16);
} Task;
void uservm_info(struct TmcVMInfo *);
i64 uservm_operation(Process *,u64,u64,u64,u64);
extern Process proc[NPROC];
extern Task task[NTASK];
static inline Process *current(void)
{
    return &proc[task[cur].pi];
}
void cpu_init(void);
void irq_init(void);
void eoi(int);
void ps2_init(void);void ps2_poll(void);void ps2_setleds(int);
void ehci_init(void);void ehci_poll(void);
void uhci_init(void);void uhci_poll(void);
void input_key_usage(int,int);
void input_init(void);
void input_irq(void);
#include "inputdiag.h"
extern InputDiag input_diag;
void input_diagnostics(InputDiag *);
int input_pop(Event *);
void rtc(SysInfo *);
void platform_init(void);
int power(int);
void arch_rsp0(u64);
void enter_frame(Frame *) __attribute__((noreturn));
extern Spin sched_lock;
extern RecSpin fs_gate;
void scheduler_init(void);
int smp_count(void);
void smp_start(void);
void tlb_shoot(u64);
void kexec_x(u64, u64);
extern struct PerCpu cpus[8];
extern int idle_slots[8];
void ap_main_entry(int);
void tlb_shoot_isr(void);
extern int ncpu_online;
void cpu_tables(int);
int spawn(int, const char *);
int newthread(u64, u64);
void terminate(int);
Frame *schedule(Frame *);
Frame *dispatch(Frame *);
Frame *syscall_do(Frame *);
Process *by_pid(int);
void post(Process *, const Event *);
void fs_init(void);
int fs_stat(const char *, Dirent *);
int fs_list(const char *, Dirent *, int, int);
int fs_create(const char *, int);
int fs_remove(const char *);
int fs_remove_flags(const char *,unsigned);
int fs_move(const char *, const char *);
int fs_open(Process *, const char *, int);
int fs_close(Process *, int);
int fs_read(Process *, int, void *, int);
int fs_write(Process *, int, const void *, int);
int fs_saveprefs(void);
typedef struct {
    int (*read)(u32, void *);
    int (*write)(u32, const void *);
    u32 sectors;
} Block;
int install_finished(void);
int install_writing(void);

void video_present(const u32 *, int, int, int, int, int, int);
int video_setmode(int, int);
void fx_setup(void);
void fx_new(void *);

void device_add(const char *, const char *, void *);
int device_list(DeviceInfo *, int);
void clock_sample(void);
void clock_info(SysInfo *);

void bootfs_init(void);
int bootfs_lookup(const char *);
int bootfs_stat(int, Dirent *);
int bootfs_list(int, Dirent *, int, int);
int bootfs_open(Process *, int, int);
int bootfs_read(FD *, void *, int);
int fs_seek(Process *, int, u64);
int account_sector(int, void *, int);
/* kext + network (kernel/kext.c, kernel/net.c) */
void kext_init(void);
int kext_load_path(const char *dir, char *err, int errcap);
int kext_unload(const char *id, char *err, int errcap);
int kext_stat(KextInfo *out, int cap);
void kext_net_tick(void);
struct LoadedKext *kext_find_wifi(void);
void net_init(void);
void net_tick(void);
int net_register(const char *name, const u8 mac[6], struct netdev_ops *ops);
void net_rx(int i, const u8 *frame, int len);
void net_set_link(int i, int up);
int net_ping(u32 ipbe, int count, u32 *rtt, int cap, u32 *okout);
int net_ifinfo(NetIfInfo *out, int cap);
int net_wifi_op(int sub, void *buf, int cap, const char *ssid);
int net_resolve(const char *host, u32 *ipout);
int net_connect(const char *host, u16 port);
int net_send(int h, const void *buf, int len);
int net_recv(int h, void *buf, int cap);
int net_recv_ex(int h, void *buf, int cap);
int net_status(int h);
int net_closesock(int h);
void session_init(void);
int session_open(void);
void session_status(SessionInfo *);
int session_begin(const AuthRequest *);
int session_work(void);
int session_lock(void);

int enrollment_salt(u8 [16]);
int decode_salt(const char [33], u8 [16]);
int session_setup_code(const char [33]);

u32 pci_find(u32 class24);
u32 pci_find_after(u32 class24,u32 after);
u32 pci_reg(u32 dev,int off);
void *pci_mmio(u32 dev,int bar);
void ahci_init(void);
int sata_count(void);
u32 sata_size(int);
const char *sata_model(int);
int sata_io(int,u32,u32,void*,int);
int sata_flush(int);
int volume_mount(Block*);
int fs_snapshot(void*,u32);
int fs_snap_create(const char *);
int fs_snap_list(void *, int);
int fs_snap_restore(const char *);
int fs_snap_verify(const char *);
int fs_snap_delete(const char *);
int fs_snap_info(FsInfo *);
int fs_enc(int, const char *, EncInfo *);
void hid_absolute(int x, int y, int buttons, int wheel);
void hid_relative(int x, int y, int buttons, int wheel);
void hid_mouse(const u8*);void hid_keys(const u8[8]);void input_repeat(void);
int input_led_take(void);void kbd_setleds(int);
void install_status(InstallInfo*);int install_begin(int);int install_step(void);int install_cancel(void);

int bootfile(const char*,const u8**,u32*);void execpath(int,char*);int spawn_command(Process*,const char*,const ExecArgs*);void job_exit(Process*,int);
void disk_info(DiskInfo*);int disk_operation(int,int);int fs_touch(const char*);
/* Retail logs stay in a bounded boot ring; GOP only until first frame and only
 * when requested. COM1 remains a diagnostic-build-only channel. */
void boot_loginit(void);void boot_logstr(const char *);void boot_lognum(u64);void boot_console_stop(void);int boot_logcopy(char *,unsigned,unsigned);
int bootconfig_disk(int,u32);

/* Detailed runtime tracing is never linked into retail images. */
#ifndef TMC_DEBUG
#define logstr(...) ((void)0)
#define lognum(...) ((void)0)
#define loghex(...) ((void)0)
#endif
