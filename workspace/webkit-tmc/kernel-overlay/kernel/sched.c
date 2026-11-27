#include "k.h"
#include "wordwaitabi.h"
extern volatile u32 *lapic_mmio(void);
Process proc[NPROC];
Task task[NTASK];
u64 ticks, switches, busy_ticks;
Spin sched_lock;
static int nextpid = 1;
static Frame *mkframe(int ti, int pi, u64 ip, u64 sp, int user)
{
    Task *t = task + ti;
    fx_new(t->fx);
    t->state = 1;
    t->pi = pi;
    t->pin = -1;
    t->at = -1;
    t->wake = 0;
    t->wait_word = 0;
    t->fs_base = 0;
    t->f = (Frame *)((u8 *)t->stack + sizeof t->stack - sizeof(Frame));
    memset(t->f, 0, sizeof(Frame));
    t->f->rip = ip;
    t->f->cs = user ? 35 : 8;
    t->f->ss = user ? 27 : 16;
    t->f->rsp = sp;
    t->f->flags = 0x202;
    return t->f;
}
static int tid(void)
{
    for (int i = 1; i < NTASK; i++)
        if (!task[i].state)
            return i;
    return -1;
}
static void idle(void)
{
#ifdef TMC_DEBUG
    u64 f;
    asm volatile("pushfq; popq %0" : "=r"(f));
    logstr("[idle] cpu=");
    lognum(pc()->cpu);
    logstr(" rflags=");
    loghex(f);
    logstr("\n");
#endif
    for (;;)
        asm volatile("hlt");
}
static u8 idle_runstack[8192] ALIGNED(16);
Process *by_pid(int pid)
{
    for (int i = 1; i < NPROC; i++)
        if (proc[i].pid == pid && !proc[i].dying)
            return proc + i;
    return 0;
}
static void post_l(Process *p, const Event *e)
{
    if (!p)
        return;
    int n = (p->head + 1) % 32;
    if (n != p->tail) {
        p->events[p->head] = *e;
        p->head = n;
    }
}
void post(Process *p, const Event *e)
{
    if (!p)
        return;
    u32 fl;
    spin_lock(&sched_lock, &fl);
    post_l(p, e);
    spin_unlock(&sched_lock, fl);
}
static int load(Process *p, const char *path, u64 *entry)
{
    logstr("[load] start\n");
    const u8 *b;
    u32 len;
    if (!bootfile(path, &b, &len) || len < 56)return -1;
    const u32 *h = (const u32 *)b;
    if (h[0] != 0xfeedfacf || h[1] != 0x1000007 || h[3] != 2 || h[4] > 24 || h[5] > len - 32)return -1;
    u64 off = 32, ep = ~0ull, textva = 0, textoff = 0, textbytes = 0;
    for (u32 i = 0; i < h[4]; i++) {
        if (off + 8 > 32 + h[5])return -1;
        const u32 *q = (void *)(b + off);
        u32 z = q[1];
        if (z < 8 || z > 32 + h[5] - off)return -1;
        if (q[0] == 0x19) {
            if (z != 72)return -1;
            const u64 *v = (void *)(q + 6);
            u64 a = v[0], ms = v[1], fo = v[2], fs = v[3];
            u32 fl = q[15];
            if (a < UBASE || a >= UBASE + 0x8000000 || a % 4096 || !ms || ms > UBASE + 0x8000000 - a || fs > ms ||
                fo > len || fs > len - fo || fl > 7 || (fl & 6) == 6)return -1;
            if (fl & 4) {
                textva = a;
                textoff = fo;
                textbytes = fs;
            }
            for (u64 j = 0; j < ms; j += PAGE) {
                if (!vm_add(p->root, a + j, (fl & 4 ? 0 : NX) | (fl & 2)))return -1;
                if (j < fs)memcpy((void *)(*vm_pte(p->root, a + j, 0)&ADDR), b + fo + j, MIN(PAGE, fs - j));
            }
        } else if (q[0] == 0x80000028 && z == 24)memcpy(&ep, q + 2, 8);
        off += z;
    }
    if (off != 32 + h[5] || ep < textoff || ep - textoff >= textbytes)return -1;
    *entry = textva + ep - textoff;
    return 0;
}
static int spawn_locked(int app, const char *path, const char *arg)
{
    if (app < 0 || app >= NAPPS)
        return -1;
    int pi = -1, ti = tid();
    logstr("[spawn] tid\n");
    for (int i = 1; i < NPROC; i++)
        if (!proc[i].pid) {
            pi = i;
            break;
        }
    if (pi < 0 || ti < 0)
        return -1;
    Process *p = proc + pi;
    memset(p, 0, sizeof * p);
    p->pid = nextpid++;
    p->app = app;
    p->tasks = 1;
    p->root = vm_new();
    logstr("[spawn] vmnew\n");
    u64 ip;
    if (!p->root || load(p, path, &ip) < 0)
        goto bad;
    logstr("[spawn] loaded\n");
    for (int k = 1; k <= 24; k++)
        if (!vm_add(p->root, UEND - k * PAGE, 2 | NX))
            goto bad;
    logstr("[spawn] vmadd\n");
    if (arg)
        strset(p->arg, arg, sizeof p->arg);
    mkframe(ti, pi, ip, UEND - 16, 1);
    logstr("[spawn] framed\n");
    logstr("[spawn] ");
    logstr(appname(app));
    logstr(" pid=");
    lognum(p->pid);
    logstr(" cr3=");
    loghex((u64)p->root);
    logstr("\n");
    return p->pid;
bad:
    vm_drop(p->root);
    memset(p, 0, sizeof * p);
    return -1;
}
int spawn(int app, const char *arg)
{
    if (app < 0 || app > LASTAPP)return -1;
    if(boot.recovery&&app!=DESKTOP&&app!=TERMINAL&&app!=DISKUTIL&&app!=INSTALLER&&
       !(app==SETTINGS&&arg&&eq(arg,"@vault")))return -1;
    char path[PATHLEN];
    execpath(app, path);
    u32 fl;
    spin_lock(&sched_lock, &fl);
    int r = spawn_locked(app, path, arg);
    spin_unlock(&sched_lock, fl);
    return r;
}
int spawn_command(Process *owner, const char *path, const ExecArgs *args)
{
    if (owner->job || args->argc < 1 || args->argc > ARGMAX || args->inlen < 0 || args->inlen > PIPEMAX ||
        args->cwd[PATHLEN - 1])return -1;
    for (int j = 0; j < args->argc; j++)if (args->argv[j][PATHLEN - 1])return -1;
    if (memcmp(path, "/bin/", 5) && memcmp(path, "/usr/bin/", 9) && memcmp(path, "/usr/sbin/", 10)
            && memcmp(path, "/sbin/", 6))return -1;
    const char *name = path;
    for (const char *q = path; *q; q++)if (*q == '/')name = q + 1;
    const char *base = !memcmp(path, "/bin/", 5) ? path + 5 : !memcmp(path, "/usr/bin/", 9) ? path + 9 :
                       !memcmp(path, "/sbin/", 6) ? path + 6 : path + 10;
    if (name != base || !name[0] || eq(name, ".") || eq(name, ".."))return -1;
    ExecArgs *a = kmalloc(sizeof * a);
    ExecResult *r = kmalloc(sizeof * r);
    if (!a || !r) {
        kfree(a);
        kfree(r);
        return -1;
    }
    memcpy(a, args, sizeof * a);
    memset(r, 0, sizeof * r);
    strset(a->argv[0], name, PATHLEN);
    u32 fl;
    spin_lock(&sched_lock, &fl);
    int pid = spawn_locked(COMMAND, path, 0);
    Process *p = by_pid(pid);
    if (!p) {
        kfree(a);
        kfree(r);
        return -1;
    }
    if (p) {
        p->args = a;
        p->owner = owner->pid;
        strset(p->program, name, 48);
        owner->job = pid;
        owner->result = r;
    }
    spin_unlock(&sched_lock, fl);
    return pid;
}
static void job_exit_l(Process *p, int status)
{
    Process *o = by_pid(p->owner);
    if (o && o->result && o->job == p->pid && !o->result->done) {
        o->result->status = status;
        o->result->done = 1;
    }
}
void job_exit(Process *p, int status)
{
    u32 fl;
    spin_lock(&sched_lock, &fl);
    job_exit_l(p, status);
    spin_unlock(&sched_lock, fl);
}
static int newthread_l(u64, u64);
int newthread(u64 entry, u64 arg)
{
    u32 fl;
    spin_lock(&sched_lock, &fl);
    int r = newthread_l(entry, arg);
    spin_unlock(&sched_lock, fl);
    return r;
}
static int newthread_l(u64 entry, u64 arg)
{
    Process *p = current();
    u64 *pte = vm_pte(p->root, entry, 0);
    if (!pte || !(*pte & 1) || (*pte & NX))
        return -1;
    int t = tid();
    if (t < 0)
        return -1;
    u64 top = UEND - (t + 1) * 0x20000ull;
    for (int i = 1; i <= 12; i++) {
        u64 a = top - i * PAGE;
        u64 *e = vm_pte(p->root, a, 0);
        if (e && (*e & 1))
            continue;
        if (!vm_add(p->root, a, 2 | NX))
            return -1;
    }
    Frame *f = mkframe(t, task[cur].pi, entry, top - 8, 1);
    f->rdi = arg;
    p->tasks++;
    return t;
}
void terminate(int pi)
{
    if (pi <= 0 || pi >= NPROC)
        return;
    Process *p = proc + pi;
    if (p->app == COMMAND)job_exit_l(p, 1);
    p->dying = 1;
    for (int i = 1; i < NTASK; i++)
        if (task[i].state && task[i].pi == pi)
            task[i].state = 3;
}
static int task_live(int j)
{
    // at >= 0: some CPU is executing this task or still sits on its kernel
    // stack after a switch; it becomes free only after that CPU's next pick.
    return task[j].at >= 0;
}
static void reap(void)
{
    for (int i = 1; i < NPROC; i++)
        if (proc[i].dying && task[cur].pi != i) {
            int busy = 0;
            for (int j = 1; j < NTASK; j++)
                if (task[j].pi == i && task_live(j))
                    busy = 1;
            if (busy)
                continue;
            vm_drop(proc[i].root);
            kfree(proc[i].draw);
            if (proc[i].raster) kfree(proc[i].raster);
            kfree(proc[i].args);
            kfree(proc[i].result);
            for (int j = 1; j < NTASK; j++)
                if (task[j].pi == i) {
                    task[j].state = 0;
                    task[j].pi = 0;
                }
            memset(&proc[i], 0, sizeof proc[i]);
        }
    for (int j = 1; j < NTASK; j++)
        if (j != cur && task[j].state == 3 && !task_live(j) && !proc[task[j].pi].dying) {
            task[j].state = 0;
            task[j].pi = 0;
        }
}
int idle_slots[8];
static int left_behind[8];
Frame *schedule(Frame *f)
{
    // sched_lock is held (IRQs off). Release the previous task's CPU claim
    // first: its kernel stack is now free, this CPU runs elsewhere.
    int me = pc()->cpu;
    if (left_behind[me] >= 0) {
        if (task[left_behind[me]].at == me)
            task[left_behind[me]].at = -1;
        left_behind[me] = -1;
    }
    task[cur].f = f;
    reap();
    int next = 0;
    for (int run = 1; run <= NTASK; run++) {
        int j = (cur + run) % NTASK;
        if (!j || (task[j].pin >= 0 && (task[j].pin != me || j == idle_slots[me])))
            continue;
        if (task[j].at >= 0 && task[j].at != me && j != cur)
            continue;
        if (task[j].state == 2 && ticks >= task[j].wake) {
            /* WAIT prepared the timeout result before parking. Wake may
             * override it only while holding the same scheduler lock. */
            task[j].wait_word = 0;
            task[j].state = 1;
        }
        if (task[j].state != 1)continue;
        int app = proc[task[j].pi].app;
        if (task[j].pi && app != DESKTOP && !session_open())continue;
        if (task[j].state == 1) {
            next = j;
            break;
        }
    }
    if (!next)
        next = idle_slots[me];
    if (next != cur) {
        left_behind[me] = cur;
        task[next].at = me;
        if (f->vec == 32 && (f->cs & 3) == 3 && switches < 120) {
            logstr("[timer preempt] pid=");
            lognum(current()->pid);
            logstr(" interrupted CS=");
            loghex(f->cs);
            logstr(" -> ");
            lognum(proc[task[next].pi].pid);
            logstr("\n");
        }
        asm volatile("fxsave64 %0":"=m"(task[cur].fx));
        asm volatile("fxrstor64 %0"::"m"(task[next].fx));
        cur = next;
        wrmsr(0xc0000100,task[cur].fs_base);
        switches++;
        Process *p = current();
        p->switches++;
        asm volatile("mov %0,%%cr3"::"r"(p->root):"memory");
        arch_rsp0((u64)(task[cur].stack + sizeof task[cur].stack));
    } else {
        task[cur].at = me;
        left_behind[me] = -1;
    }
    return task[cur].f;
}
// Kernel-only tasks use their own running stacks and a DPL-0 software wait gate.
static u8 kernel_worker_stacks[2][65536] ALIGNED(16);
static int workers;
static int kernel_thread(void(*entry)(void))
{
    int ti = tid();
    if (ti < 0 || workers >= 2)
        return -1;
    u8 *sp = kernel_worker_stacks[workers++] + sizeof kernel_worker_stacks[0];
    mkframe(ti, 0, (u64)entry, (u64)sp - 8, 0);
    return ti;
}
static void clock_worker(void)
{
    logstr("[sched] kernel RTC worker online\n");
    int hb = 0;
    for (;;) {
        asm volatile("cli":::"memory");
        clock_sample();
        if ((++hb & 3) == 0) {
            logstr("[hb] cpu=");
            lognum(pc()->cpu);
            logstr(" ncpu=");
            lognum(ncpu_online);
            logstr(" t0=");
            lognum(cpus[0].core_ticks);
            logstr(" svr0=");
            loghex(lapic_mmio()[0xF0 / 4]);
            logstr(" t1=");
            lognum(cpus[1].core_ticks);
            logstr(" pid=");
            lognum(current()->pid);
            logstr("\n");
        }
        asm volatile("sti; int $129"::"D"(50ul):"memory");
    }
}
static u8 ap_idle_stacks[7][8192] ALIGNED(16);
void scheduler_init(void)
{
    logstr("[sched] init\n");
    clock_sample();
    fx_setup();
    proc[0].root = kroot;
    proc[0].app = -1;
    mkframe(0, 0, (u64)idle, (u64)(idle_runstack + sizeof idle_runstack) - 8, 0);
    task[0].pin = 0;
    idle_slots[0] = 0;
    for (int i = 0; i < 8; i++)
        left_behind[i] = -1;
    cur = 0;
    // Reserve an idle task for every AP before ordinary tasks can claim slots.
    int planned = smp_count();
    for (int c = 1; c < planned && c < 8; c++) {
        int ti = -1;
        for (int j = NTASK - 1; j >= 1; j--)
            if (!task[j].state) {
                ti = j;
                break;
            }
        if (ti < 0)
            break;
        u8 *st = ap_idle_stacks[c - 1] + 8192;
        mkframe(ti, 0, (u64)idle, (u64)st - 8, 0);
        task[ti].pin = c;
        idle_slots[c] = ti;
    }
    logstr("[sched] idles ready\n");
    arch_rsp0((u64)(task[0].stack + sizeof task[0].stack));
    if (spawn(DESKTOP, 0) < 0)
        panic("cannot start session", 0, 0);
    if (kernel_thread(clock_worker) < 0)
        panic("cannot start clock worker", 0, 0);
    extern void tmc_usb_kernel_worker(void);
    int usb_thread=kernel_thread(tmc_usb_kernel_worker);
    if(usb_thread<0)panic("cannot start USB worker",0,0);
    task[usb_thread].pin=0;

    logstr("[sched] desktop up\n");
    smp_start();
    logstr("[sched] smp done\n");
    boot_logstr("[boot] CPUs online: ");boot_lognum(ncpu_online);boot_logstr("\n");
    boot_progress(100);
    boot_logstr("[boot] services ready; handoff to login\n");
#ifdef TMC_DEBUG
    {
        int rr = fs_snap_create("boot");
        logstr("[snapdbg] create=");
        lognum(rr < 0 ? 9 : rr);
        SnapInfo si[8];
        int n = fs_snap_list(si, 8);
        logstr(" list=");
        lognum(n);
        logstr(" verify=");
        lognum(fs_snap_verify("boot"));
        logstr("\n");
        for (int i = 0; i < n; i++) {
            logstr("  snap: ");
            logstr(si[i].name);
            logstr(" id=");
            lognum(si[i].id);
            logstr("\n");
        }
        Dirent dd;
        int st = fs_stat("/.snapshots/boot" HOME "/Welcome.txt", &dd);
        logstr(" snapstat=");
        lognum(st < 0 ? 9 : 1);
        logstr(" size=");
        lognum(st < 0 ? 0 : (u64)dd.size);
        // live divergence: create a file AFTER the snapshot; image must not see it
        int mk = fs_create(HOME "/snaptest.txt", 0);
        int inv = fs_stat("/.snapshots/boot" HOME "/snaptest.txt", &dd);
        int lv = fs_stat(HOME "/snaptest.txt", &dd);
        logstr(" mk=");
        lognum(mk < 0 ? 9 : 1);
        logstr(" inimg=");
        lognum(inv < 0 ? 0 : 1);
        logstr(" live=");
        lognum(lv < 0 ? 0 : 1);
        logstr("\n");
        // read content from the snapshot image
        Process *pp = current();
        int fd = fs_open(pp, "/.snapshots/boot" HOME "/Welcome.txt", 0);
        char b0[16] = {0};
        int rdr = fd >= 0 ? fs_read(pp, fd, b0, 15) : -9;
        if (fd >= 0)fs_close(pp, fd);
        logstr(" rd=");
        lognum(rdr < 0 ? 9 : rdr);
        logstr(" txt=");
        logstr(b0);
        logstr("\n");
        // restore: live test file must disappear again
        int rz = fs_snap_restore("boot");
        int lv2 = fs_stat(HOME "/snaptest.txt", &dd);
        logstr(" restore=");
        lognum(rz < 0 ? 9 : 1);
        logstr(" liveafter=");
        lognum(lv2 < 0 ? 0 : 1);
        logstr("\n");
        // FileVault-style encryption: arm, lock (read denial), unlock, decrypt.
        int em = fs_create(HOME "/enc.txt", 0);
        int ef = em >= 0 ? fs_open(pp, HOME "/enc.txt", 1) : -1;
        int ew = ef >= 0 ? fs_write(pp, ef, "TopSecret-ABC", 14) : -1;
        if (ef >= 0)fs_close(pp, ef);
        EncInfo eia, eib;
        int e1 = fs_enc(1, "prova-di-pass", 0);
        fs_enc(0, 0, &eia);
        int e2 = fs_enc(3, 0, 0);
        int fd2 = fs_open(pp, HOME "/enc.txt", 0);
        char b1[16] = {0};
        int r1 = fd2 >= 0 ? fs_read(pp, fd2, b1, 14) : -1;
        if (fd2 >= 0)fs_close(pp, fd2);
        int e3 = fs_enc(4, "prova-di-pass", 0);
        int fd3 = fs_open(pp, HOME "/enc.txt", 0);
        char b2[16] = {0};
        int r2 = fd3 >= 0 ? fs_read(pp, fd3, b2, 14) : -1;
        if (fd3 >= 0)fs_close(pp, fd3);
        int e4 = fs_enc(2, "prova-di-pass", 0);
        int fd4 = fs_open(pp, HOME "/enc.txt", 0);
        char b3[16] = {0};
        int r3 = fd4 >= 0 ? fs_read(pp, fd4, b3, 14) : -1;
        if (fd4 >= 0)fs_close(pp, fd4);
        fs_enc(0, 0, &eib);
        logstr("[encdbg] mk=");
        lognum(em < 0 ? 9 : 1);
        logstr(" w=");
        lognum(ew < 0 ? 9 : ew);
        logstr(" enc=");
        lognum(e1 < 0 ? 9 : 0);
        logstr(" files=");
        lognum(eia.files);
        logstr(" lock=");
        lognum(e2 < 0 ? 9 : 0);
        logstr(" deny=");
        lognum(r1 < 0 ? 1 : 0);
        logstr(" ul=");
        lognum(e3 < 0 ? 9 : 0);
        logstr(" rd=");
        lognum(r2 == 14 ? 14 : (r2 < 0 ? 9 : r2));
        logstr(" txt=");
        logstr(b2);
        logstr(" dec=");
        lognum(e4 < 0 ? 9 : 0);
        logstr(" plain=");
        lognum(r3 == 14 ? 14 : (r3 < 0 ? 9 : r3));
        logstr(" fin=");
        lognum(eib.state);
        logstr("\n");
    }
#endif
    enter_frame(task[0].f);
}
