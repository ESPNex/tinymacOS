#include "k.h"
#include "tlsabi.h"
#include "wordwaitabi.h"
#include "threadfinishabi.h"
#include "netabi.h"
#include "uaccess_gate.h"
static int string(u64 ptr, char *out, int cap)
{
    for (int i = 0; i < cap; i++) {
        if (!user_ok(ptr + i, 1, 0))
            return 0;
        out[i] = *(const char *)(ptr + i);
        if (!out[i])
            return 1;
    }
    return 0;
}
/* SIP: userspace may not modify system-owned trees while NVRAM bit0 is set.
 * Kernel-internal writers (installer, snapshots, seeds) never enter here. */
static int sip_block(const char *p)
{
    if (!(boot.csr & 1)) return 0;
    static const char *const roots[] = {"/System", "/usr", "/bin", "/sbin"};
    for (int i = 0; i < 4; i++) {
        const char *r = roots[i];
        int j = 0;
        while (r[j] && p[j] == r[j]) j++;
        if (!r[j] && (!p[j] || p[j] == '/')) return 1;
    }
    return 0;
}
static Spin alloc_lock;
static void info(SysInfo *s)
{
    memset(s, 0, sizeof * s);
    s->total = totalpages * PAGE;
    s->free = freepages * PAGE;
    s->ticks = ticks;
    s->switches = switches;
    s->w = boot.w;
    s->h = boot.h;
    s->nmodes = boot.nmodes > 24 ? 24 : boot.nmodes;
    for (u32 i = 0; i < s->nmodes; i++)s->modes[i] = boot.modes[i];
#ifdef TMC_DEBUG
    {
        static int mdump;
        if (!mdump) {
            mdump = 1;
            logstr("[modes]");
            for (u32 i = 0; i < s->nmodes; i++) {
                char nb[16];
                logstr(" ");
                number(nb, boot.modes[i] >> 16);
                logstr(nb);
                logstr("x");
                number(nb, boot.modes[i] & 0xffff);
                logstr(nb);
            }
            logstr("\n");
        }
    }
#endif
    s->persistent = persistent;
    if (session_open()) {
        SessionInfo q;
        session_status(&q);
        strset(s->user, q.name, 32);
    }
    s->live_cd = !!boot.install_len;
    s->fsrev = fsrev;
    s->pci = npci;
    s->apic = apic_enabled;
    s->files = usednodes;
    s->theme = theme;
    s->wall = wall;
    s->dock = dock;
    s->reduce_motion = reduce_motion;
    s->opaque = opaque;
    s->dark = dark;
    s->csr = boot.csr;
    s->busy_ticks = busy_ticks;
    strset(s->cpu, cpuname, sizeof s->cpu);
    u32 ifl;
    spin_lock(&sched_lock, &ifl);
    for (int i = 1; i < NPROC; i++)
        if (proc[i].pid && !proc[i].dying)
            s->procs++;
    spin_unlock(&sched_lock, ifl);
    clock_info(s);
    s->ncpu = ncpu_online > 8 ? 8 : ncpu_online;
    s->current_cpu=pc()->cpu;
    s->boot_flags=boot.boot_flags;
    for (int c = 0; c < 8; c++) {
        s->core_busy[c] = cpus[c].core_busy;
        s->core_ticks[c] = cpus[c].core_ticks;
    }
}
static i64 alloc(Process *p, u64 n)
{
    if (!n || n > 8 * 1024 * 1024)
        return -1;
    n = (n + 4095) / 4096;
    int slot = -1;
    u64 a = UBASE + 0x10000000;
    for (int i = 0; i < 32; i++)
        if (!p->alloc[i].n) {
            slot = i;
            break;
        }
    if (slot < 0)
        return -1;
    for (int i = 0; i < 32;) {
        if (p->alloc[i].n && a < p->alloc[i].addr + p->alloc[i].n * PAGE && a + n * PAGE > p->alloc[i].addr) {
            a = p->alloc[i].addr + p->alloc[i].n * PAGE;
            i = 0;
        } else
            i++;
    }
    if (a + n * PAGE > UBASE + 0x30000000)
        return -1;
    for (u64 k = 0; k < n; k++)
        if (!vm_add(p->root, a + k * PAGE, NX | 2)) {
            while (k--)
                vm_unmap(p->root, a + k * PAGE);
            return -1;
        }
    p->alloc[slot].addr = a;
    p->alloc[slot].n = n;
    return a;
}
/* sched_lock held. Keys are numeric and private to this address space; never
 * dereference a sleeping task's user address. Also shared by final handoff. */
static i64 wake_word_locked(int pi,u64 address,u64 count)
{
    i64 awakened=0;
    for(int j=1;j<NTASK && (u64)awakened<count;j++){
        Task *t=task+j;
        if(t->pi!=pi || t->state!=2 || t->wait_word!=address)continue;
        t->wait_word=0;t->state=1;
        if(t->wake<=ticks)t->f->rax=TMC_WORD_TIMEOUT;
        else {t->f->rax=0;awakened++;}
    }
    return awakened;
}
static Frame *syscall_inner(Frame *f)
{
    u64 a = f->rdi, b = f->rsi, c = f->rdx, d = f->r10, n = f->rax;
    Process *p = current();
    int desktop = p->app == DESKTOP;
    char s[PATHLEN], t[PATHLEN];
    i64 r = -1;
#define OUT(x,z) user_ok(x,z,1)
#define IN(x,z) user_ok(x,z,0)
    // No application, including the privileged compositor, can open or enumerate
    // session data before authentication.
    if (!session_open() && (n == S_SPAWN || n == S_PS || n == S_KILL || (n >= S_WOPEN && n <= S_SEND) ||
                            (n >= S_STAT &&
                             n <= S_WRITE) || n == S_PREF || n == S_STARTARG || n == S_SEEK)) {
        f->rax = -1;
        return f;
    }
    switch (n) {
    case S_SESSION:
        if (desktop && OUT(a, sizeof(SessionInfo))) {
            session_status((SessionInfo *)a);
            r = 0;
        }
        break;
    case S_CONTEXT:
        if (session_open() && p->app == FINDER) {
            for (int i = 1; i < NPROC; i++)if (proc[i].pid && proc[i].app == DESKTOP && !proc[i].dying) {
                    post(proc + i, &(Event) {
                        .type = EV_CONTEXT_READY, .a = p->pid, .b = a & 0x7ffe, .d = (int)b
                    });
                    r = 0;
                    break;
                }
        }
        break;
    case S_ART:
        if (desktop && a <= NWALL) {
            char path[PATHLEN] = "/System/Library/Fonts/Interface.tfn";
            if (a) {
                strset(path, "/System/Library/Desktop Pictures/", PATHLEN);
                append(path, wallname(a - 1), PATHLEN);
                append(path, ".qoi", PATHLEN);
            }
            const u8 *v;
            u32 z;
            if (bootfile(path, &v, &z)) {
                if (!d)r = z;
                else if (d <= 65536 && b <= z && d <= z - b && OUT(c, d)) {
                    memcpy((void *)c, v + b, d);
                    r = d;
                }
            }
        }
        break;
    case S_TOUCH:
        if (session_open() && string(a, s, sizeof s))r = fs_touch(s);
        break;
    case S_SNAP:
        // APFS-style snapshot control: 0 list, 1 create, 2 restore, 3 verify, 4 delete.
        if (!session_open())break;
        if (a == 0 && d <= 16 && OUT(c, d * sizeof(SnapInfo)))
            r = fs_snap_list((void *)c, (int)d);
        else if (a == 5 && OUT(c, sizeof(FsInfo)))
            r = fs_snap_info((void *)c);
        else if (a >= 1 && a <= 4 && string(b, s, sizeof s)) {
            if (a == 1) r = fs_snap_create(s);
            else if (a == 2) r = fs_snap_restore(s);
            else if (a == 3) r = fs_snap_verify(s);
            else r = fs_snap_delete(s);
        } else if (a == 6) {
            // 6: encryption control. b=0 status (d=EncInfo*), b=3 lock,
            // b=1/2/4 encrypt/decrypt/unlock (c=passphrase).
            if (b == 0 && OUT(d, sizeof(EncInfo)))
                r = fs_enc(0, 0, (void *)d);
            else if (b == 3)
                r = fs_enc(3, 0, 0);
            else if (b >= 1 && b <= 4 && string(c, s, sizeof s))
                r = fs_enc((int)b, s, 0);
        }
        break;
    case S_RESMODE:
        // Cycle the GOP at runtime (Settings → Schermo). Fonts stay fixed.
        r = a < boot.nmodes ? video_setmode((int)(boot.modes[a] >> 16),
                                            (int)(boot.modes[a] & 0xffff)) : -1;
        break;
    case S_KEXT: {
        /* a=op: 0 stat (b=KextInfo[], c=cap) 1 load (b=path, c=err buf128)
         *       2 unload (b=id, c=err buf128) */
        char err[128];
        err[0] = 0;
        if (a == 0) {
            if (c <= 8 && OUT(b, (u64)c * sizeof(KextInfo)))
                r = kext_stat((KextInfo *)b, (int)c);
        } else if (a == 1) {
            if (string(b, s, sizeof s))
                r = kext_load_path(s, err, sizeof err);
            else r = -1;
            if (c && OUT(c, 128)) memcpy((void *)c, err, 128);
        } else if (a == 2) {
            if (string(b, s, sizeof s))
                r = kext_unload(s, err, sizeof err);
            else r = -1;
            if (c && OUT(c, 128)) memcpy((void *)c, err, 128);
        } else r = -1;
        break;
    }
    case S_BOOTCONFIG:
        if(!session_open())break;
        if(a==0)r=bootconfig_disk(0,0);
        else if(a==1 && b<=1)r=bootconfig_disk(1,(u32)b);
        else if(a==2 && c<=4096 && d<=16384 && OUT(b,c))r=boot_logcopy((char *)b,c,d);
        break;
    case S_VM:
        if(a==TMC_VM_QUERY) {
            if(OUT(b,sizeof(struct TmcVMInfo))) {
                struct TmcVMInfo caps;uservm_info(&caps);
                *(struct TmcVMInfo *)b=caps;r=0;
            }
        } else r=uservm_operation(p,a,b,c,d);
        break;
    case S_NET: {
        /* a=op: 0 interfaces (b=NetIfInfo[], c=cap) 1 ping (b=PingResult in/out)
         *       2 wifi (c=sub: 0 status b=WifiStatus, 1 scan b=WifiAp[8],
         *              2 connect d=ssid) */
        if (a == 0) {
            if (c <= 4 && OUT(b, (u64)c * sizeof(NetIfInfo)))
                r = net_ifinfo((NetIfInfo *)b, (int)c);
        } else if (a == 1) {
            PingResult pr;
            if (!user_ok(b, sizeof pr, 0)) break;
            pr = *(const PingResult *)b;
            if (pr.count < 1) pr.count = 1;
            if (pr.count > 5) pr.count = 5;
            u32 ok = 0, rtt[5] = {0, 0, 0, 0, 0};
            r = net_ping(pr.ip, (int)pr.count, rtt, 5, &ok);
            pr.ok = ok;
            for (u32 i = 0; i < ok && i < 5; i++) pr.rtt[i] = rtt[i];
            if (r >= 0 && OUT(b, sizeof pr)) *(PingResult *)b = pr;
        } else if (a == 2) {
            char ssid[32];
            ssid[0] = 0;
            if (c == 2 && d) {
                if (!string(d, ssid, sizeof ssid)) ssid[0] = 0;
            }
            if (c == 0) {
                if (OUT(b, sizeof(WifiStatus)))
                    r = net_wifi_op(0, (void *)b, 0, 0);
            } else if (c == 1) {
                if (OUT(b, 8 * sizeof(WifiAp)))
                    r = net_wifi_op(1, (void *)b, 8, 0);
            } else if (c == 2) {
                r = net_wifi_op(2, 0, 0, ssid);
            } else r = -1;
        } else if (a == 3) {
            /* resolve: b=host, c=u32* out (puntatore IP in ordine lwIP) */
            char host[256];
            u32 ip = 0;
            if (!b || !string(b, host, sizeof host) || !OUT(c, 4)) break;
            r = net_resolve(host, &ip);
            if (r >= 0) *(u32 *)(u64)c = ip;
        } else if (a == 4) {
            /* connect: b=host, c=port -> handle >= 0 */
            char host[256];
            if (!b || !string(b, host, sizeof host) || c < 1 || c > 65535) break;
            r = net_connect(host, (u16)c);
        } else if (a == 5) {
            /* send: b=buf, c=len, d=handle */
            if (!b || !c || c > 65536 || !user_ok(b, (u64)c, 0)) break;
            r = net_send((int)d, (const void *)b, (int)c);
        } else if (a == 6) {
            /* recv: b=buf, c=cap, d=handle -> n>=0, 0=vuoto, -1=chiuso */
            if (!b || !c || c > 65536 || !OUT(b, (u64)c)) break;
            r = net_recv((int)d, (void *)b, (int)c);
        } else if (a == 7) {
            /* close: d=handle */
            r = net_closesock((int)d);
        } else if (a == TMC_NET_RECV_EX) {
            r = TMC_NET_IO;
            if (!b || !c || c > 65536 || d > 0x7fffffff || !OUT(b, (u64)c)) break;
            r = net_recv_ex((int)d, (void *)b, (int)c);
        } else if (a == TMC_NET_STATUS) {
            r = d > 0x7fffffff ? TMC_NET_EBADHANDLE : net_status((int)d);
        } else r = -1;
        break;
    }
    case S_TRACE:
#ifdef TMC_DEBUG
        // Debug-only guest trace: [user:<app>] <text> on COM1. Retail has no
        // diagnostic serial channel, so the handler is compiled out there.
        if (string(a, s, sizeof s)) {
            logstr("[user:");
            logstr(appname(p->app));
            logstr("] ");
            logstr(s);
            logstr("\n");
            r = 0;
        }
#endif
        break;
    case S_DISK:
        if (!session_open())break;
        if (a == 0 && OUT(b, sizeof(DiskInfo))) {
            disk_info((void *)b);
            r = 0;
        } else if (a == 4)r = disk_operation(4, (int)b);
        else if ((p->app == DISKUTIL || (a == 1 && p->app == COMMAND &&
                  (eq(p->program, "diskutil") || eq(p->program, "fsck_apfs"))) || (desktop &&
                  a == 3)) && a >= 1 && a <= 3)r = disk_operation((int)a, (int)b);
        break;
    case S_EXEC:
        if (session_open() && p->app == TERMINAL && string(a, s, sizeof s) &&
            IN(b, sizeof(ExecArgs)))r = spawn_command(p, s, (void *)b);
        break;
    case S_ARGV:
        if (p->args && p->app == COMMAND && OUT(a, sizeof(ExecArgs))) {
            memcpy((void *)a, p->args, sizeof(ExecArgs));
            r = 0;
        }
        break;
    case S_STDOUT:
        if (p->app == COMMAND && b <= PIPEMAX && IN(a, b)) {
            Process *owner = by_pid(p->owner);
            if (owner && owner->result && owner->job == p->pid) {
                ExecResult *q = owner->result;
                if (b <= PIPEMAX - (u32)q->bytes) {
                    memcpy(q->out + q->bytes, (void *)a, b);
                    q->bytes += b;
                    r = b;
                }
            }
        }
        break;
    case S_RESULT:
        if (p->app == TERMINAL && p->result && OUT(a, sizeof(ExecResult))) {
            r = 0;
            if (p->result->done) {
                memcpy((void *)a, p->result, sizeof(ExecResult));
                kfree(p->result);
                p->result = 0;
                p->job = 0;
                r = 1;
            }
        }
        break;
    case S_RETURN:
        if (p->app == COMMAND) {
            job_exit(p, (int)a);
            u32 fl;spin_lock(&sched_lock,&fl);
            terminate(task[cur].pi);
            Frame *next=schedule(f);spin_unlock(&sched_lock,fl);return next;
        }
        break;
    case S_INSTALL:
        if (session_open() && p->app == INSTALLER) {
            if (a == 0 && OUT(b, sizeof(InstallInfo))) {
                install_status((InstallInfo *)b);
                r = 0;
            } else if (a == 1)r = install_begin((int)b);
            else if (a == 2)r = install_step();
            else if (a == 3)r = install_cancel();
        }
        break;
    case S_SETUP_CODE:
        if (desktop && IN(a, 33))r = session_setup_code((const char *)a);
        break;
    case S_AUTH_BEGIN:
        if (desktop && IN(a, sizeof(AuthRequest)))r = session_begin((const AuthRequest *)a);
        break;
    case S_AUTH_STEP:
        if (desktop)r = session_work();
        break;
    case S_LOCK:
        if (desktop)r = session_lock();
        break;
    case S_SEEK:
        r = fs_seek(p, a, b);
        break;
    case S_TICKS:
        r = ticks;
        break;
    case S_SLEEP: {
        u32 fl;
        spin_lock(&sched_lock, &fl);
        task[cur].state = 2;
        task[cur].wake = ticks + MIN(a, 360000ull);
        task[cur].wait_word = 0;
        f->rax = 0;
        Frame *nf = schedule(f);
        spin_unlock(&sched_lock, fl);
        return nf;
    }
    case S_EXIT: {
        u32 fl;
        spin_lock(&sched_lock, &fl);
        if (a == 1 && p->tasks > 1) {
            task[cur].state = 3;
            p->tasks--;
        } else
            terminate(task[cur].pi);
        Frame *nf = schedule(f);
        spin_unlock(&sched_lock, fl);
        return nf;
    }
    case S_SPAWN:
        if (a >= FINDER && a <= LASTAPP && (!b || string(b, s, sizeof s)))
            r = spawn(a, b ? s : 0);
        break;
    case S_DEVICES:
        if (b <= 16 && OUT(a, b * sizeof(DeviceInfo)))
            r = device_list((DeviceInfo *)a, b);
        break;
    case S_TLS:
        if(a==TMC_TLS_QUERY)r=TMC_TLS_ABI_VERSION;
        else if(a==TMC_TLS_GET)r=task[cur].fs_base;
        else if(a==TMC_TLS_SET && (!b || (!(b&7) && OUT(b,8)))) {
            task[cur].fs_base=b;wrmsr(0xc0000100,b);r=0;
        }
        break;
    case S_WORDWAIT:
        if(a==TMC_WORD_QUERY){r=TMC_WORD_ABI_VERSION;break;}
        r=TMC_WORD_INVALID;
        /* User mappings are pinned by syscall_do during validation/compare.
         * Sleeping retains just a numeric key, not a dereferenceable pointer. */
        if((a!=TMC_WORD_WAIT && a!=TMC_WORD_WAKE) || (b&3) || c>0xffffffffull || !OUT(b,4))break;
        {
            u32 fl;spin_lock(&sched_lock,&fl);
            if(a==TMC_WORD_WAIT){
                if(__atomic_load_n((u32 *)b,__ATOMIC_ACQUIRE)!=(u32)c)r=TMC_WORD_CHANGED;
                else if(d && ticks>=d)r=TMC_WORD_TIMEOUT;
                else {
                    task[cur].wait_word=b;task[cur].wake=d?d:~0ull;
                    task[cur].state=2;f->rax=TMC_WORD_TIMEOUT;
                    Frame *next=schedule(f);spin_unlock(&sched_lock,fl);return next;
                }
            } else {
                r=wake_word_locked(task[cur].pi,b,c);
            }
            spin_unlock(&sched_lock,fl);
        }
        break;
    case S_THREADFINISH:
        if(a==TMC_THREAD_FINISH_QUERY){r=TMC_THREAD_FINISH_VERSION;break;}
        if(a==TMC_THREAD_FINISH && !c && !d && !(b&3) && OUT(b,4)){
            u32 fl;spin_lock(&sched_lock,&fl);
            /* The user stack/TCB will never be used again by this thread.
             * Its kernel stack is separately protected by Task.at/reaping.
             * User mapping writes remain pinned until syscall_do leaves. */
            task[cur].fs_base=0;wrmsr(0xc0000100,0);
            __atomic_store_n((u32 *)b,0,__ATOMIC_RELEASE);
            wake_word_locked(task[cur].pi,b,0xffffffffull);
            if(p->tasks>1){task[cur].state=3;p->tasks--;}
            else terminate(task[cur].pi);
            Frame *next=schedule(f);spin_unlock(&sched_lock,fl);return next;
        }
        break;
    case S_THREAD:
        r = newthread(a, b);
        break;
    case S_KILL: {
        Process *q = by_pid(a);
        if (q && q != p && q->app != DESKTOP && (p->app == TERMINAL || desktop)) {
            u32 fl;
            spin_lock(&sched_lock, &fl);
            terminate(q - proc);
            spin_unlock(&sched_lock, fl);
            r = 0;
        }
        break;
    }
    case S_PS:
        if (b <= NPROC && OUT(a, b * sizeof(ProcInfo))) {
            ProcInfo *z = (void *)a;
            u32 fl;
            spin_lock(&sched_lock, &fl);
            r = 0;
            for (int i = 1; i < NPROC && r < (i64)b; i++)
                if (proc[i].pid && !proc[i].dying) {
                    Process *q = proc + i;
                    z[r] = (ProcInfo) {
                        q->pid, q->app, 0, q->tasks, q->switches, {0}, 0
                    };
                    for (int j = 1; j < NTASK; j++)
                        if (task[j].pi == i && task[j].state)
                            z[r].state = task[j].state;
                    z[r].cpu_ticks = q->cpu_ticks;
                    strset(z[r].name, q->app == COMMAND ? q->program : appname(q->app), 24);
                    r++;
                }
            spin_unlock(&sched_lock, fl);
        }
        break;
    case S_INFO:
        if (OUT(a, sizeof(SysInfo))) {
            info((void *)a);
            r = 0;
        }
        break;
    case S_ALLOC: {
        u32 fl;
        spin_lock(&alloc_lock, &fl);
        r = alloc(p, a);
        spin_unlock(&alloc_lock, fl);
        break;
    }
    case S_FREE: {
        u32 fl;
        spin_lock(&alloc_lock, &fl);
        for (int i = 0; i < 32; i++)
            if (p->alloc[i].addr == a && p->alloc[i].n) {
                for (u64 j = 0; j < p->alloc[i].n; j++)
                    vm_unmap(p->root, a + j * PAGE);
                p->alloc[i].n = 0;
                r = 0;
                break;
            }
        spin_unlock(&alloc_lock, fl);
        break;
    }
    case S_WOPEN: {
        u32 fl;
        spin_lock(&sched_lock, &fl);
        if (p->app >= FINDER && p->app <= LASTAPP && a >= 300 && a <= 1280 && b >= 190 && b <= 1024
            && string(c, s, 48)) {
            if (!p->draw)
                p->draw = kmalloc(sizeof(Draw) * NCMDS);
            if (p->draw) {
                p->win = (Win) {
                    p->pid, p->app, a, b, p->win.seq + 1, {0}, 0
                };
                strset(p->win.title, s, 48);
                r = p->pid;
                logstr("[window] open ");
                logstr(s);
                logstr("\n");
            }
        }
        spin_unlock(&sched_lock, fl);
        break;
    }
    case S_WDRAW: {
        u32 fl;
        spin_lock(&sched_lock, &fl);
        if (p->draw && b <= NCMDS && IN(a, b * sizeof(Draw))) {
            memcpy(p->draw, (void *)a, b * sizeof(Draw));
            for (u64 j = 0; j < b; j++)
                p->draw[j].text[99] = 0;
            /* C_RASTER: capture the pixel payload while the caller's
             * address space is the active one (one payload per window). */
            if (p->raster) {
                kfree(p->raster);
                p->raster = 0;
                p->rasterlen = 0;
            }
            for (u64 j = 0; j < b; j++) {
                Draw *dr = &p->draw[j];
                if (dr->type != C_RASTER) continue;
                u64 src = 0;
                i64 rw = dr->w, rh = dr->h;
                memcpy(&src, dr->text, 8);
                if (rw < 1 || rh < 1 || rw > 1280 || rh > 1024 ||
                    rw * rh * 4 > 8 * 1024 * 1024 || !IN(src, (u64)rw * rh * 4)) {
                    dr->type = 0;
                    continue;
                }
                void *buf = kmalloc((size_t)(rw * rh * 4));
                if (!buf) {
                    dr->type = 0;
                    continue;
                }
                memcpy(buf, (void *)src, (size_t)(rw * rh * 4));
                p->raster = buf;
                p->rasterlen = (u64)rw * rh * 4;
            }
            p->ndraw = b;
            p->win.transition = d <= FX_DISSOLVE ? d : 0;
            p->win.seq++;
            if (c && string(c, s, 48))
                strset(p->win.title, s, 48);
            r = 0;
        }
        spin_unlock(&sched_lock, fl);
        break;
    }
    case S_WLIST: {
        u32 fl;
        spin_lock(&sched_lock, &fl);
        if (desktop && b <= NPROC && OUT(a, b * sizeof(Win))) {
            r = 0;
            for (int i = 1; i < NPROC && r < (i64)b; i++)
                if (proc[i].pid && !proc[i].dying && proc[i].win.pid) {
                    ((Win *)a)[r++] = proc[i].win;
                    proc[i].win.transition = 0;   /* one-shot: the desktop consumed it */
                }
        }
        spin_unlock(&sched_lock, fl);
        break;
    }
    case S_WGET: {
        u32 fl;
        Process *q = by_pid(a);
        spin_lock(&sched_lock, &fl);
        if (desktop && q && q->draw && c >= q->ndraw && OUT(b, q->ndraw * sizeof(Draw))) {
            memcpy((void *)b, q->draw, q->ndraw * sizeof(Draw));
            r = q->ndraw;
            if (d && q->raster && q->rasterlen && q->rasterlen <= 8 * 1024 * 1024 &&
                OUT(d, q->rasterlen))
                memcpy((void *)d, q->raster, q->rasterlen);
        }
        spin_unlock(&sched_lock, fl);
        break;
    }
    case S_INPUT:
        if (desktop && OUT(a, sizeof(Event)))
            r = input_pop((void *)a);
        break;
    case S_SEND:
        if (desktop && IN(b, sizeof(Event))) {
            Process *q = by_pid(a);
            if (q) {
                Event e = *(Event *)b;
                e.text[PATHLEN - 1] = 0;
                post(q, &e);
                r = 0;
            }
        }
        break;
    case S_EVENT:
        if (OUT(a, sizeof(Event))) {
            u32 fl;
            spin_lock(&sched_lock, &fl);
            r = 0;
            if (p->head != p->tail) {
                *(Event *)a = p->events[p->tail];
                p->tail = (p->tail + 1) % 32;
                r = 1;
            }
            spin_unlock(&sched_lock, fl);
        }
        break;
    case S_PRESENT: {
        u64 len = (u64)boot.w * boot.h * 4;
        if (desktop && IN(a, len)) {
            int X = b >> 32, Y = (u32)b, ww = c >> 32, hh = (u32)c;
            int sw = d >> 32, sh = (u32)d;   /* logical canvas size (0 = legacy 1:1) */
            if (!sw || !sh) {
                sw = boot.w;
                sh = boot.h;
            }
            if (!c) {
                X = Y = 0;
                ww = sw;
                hh = sh;
            }
            if (X < 0 || Y < 0 || ww <= 0 || hh <= 0 || (u32)X >= sw || (u32)Y >= sh || (u32)ww > sw - X ||
                (u32)hh > sh - Y)break;
            /* Scaled scanout repaints the whole frame: partial tiles would seam. */
            if (sw != boot.w || sh != boot.h) {
                X = Y = 0;
                ww = sw;
                hh = sh;
            }
            boot_console_stop();
            video_present((const u32 *)a, X, Y, ww, hh, sw, sh);
            static int done;
            if (!done) {
                done = 1;
                logstr("[gop] first userspace frame presented\n");
            }
            r = 0;
        }
        break;
    }
    case S_STARTARG:
        if (OUT(a, PATHLEN)) {
            strset((char *)a, p->arg, PATHLEN);
            r = 0;
        }
        break;
    case S_STAT:
        if (string(a, s, sizeof s) && OUT(b, sizeof(Dirent)))
            r = fs_stat(s, (void *)b);
        break;
    case S_LIST:
        if (string(a, s, sizeof s) && c <= NSLOTS && d <= NSLOTS && OUT(b, c * sizeof(Dirent)))
            r = fs_list(s, (void *)b, c, d);
        break;
    case S_CREATE:
        if (string(a, s, sizeof s))
            r = sip_block(s) ? -1 : fs_create(s, !!b);
        break;
    case S_REMOVE:
        if (string(a, s, sizeof s))
            r = sip_block(s) ? -1 : fs_remove(s);
        break;
    case S_MOVE:
        if (string(a, s, sizeof s) && string(b, t, sizeof t))
            r = (sip_block(s) || sip_block(t)) ? -1 : fs_move(s, t);
        break;
    case S_OPEN:
        if (string(a, s, sizeof s))
            r = (b && sip_block(s)) ? -1 : fs_open(p, s, !!b);
        break;
    case S_CLOSE:
        r = fs_close(p, a);
        break;
    case S_READ:
        if (c <= MAXFILE && OUT(b, c))
            r = fs_read(p, a, (void *)b, c);
        break;
    case S_WRITE:
        if (c <= MAXFILE && IN(b, c))
            r = fs_write(p, a, (void *)b, c);
        break;
    case S_POWER:
        if (desktop || p->app == TERMINAL || (p->app == INSTALLER && session_open() && !a && install_finished()))
            r = power(!!a);
        break;
    case S_PREF:
        if (p->app == SETTINGS || desktop || (p->app == GALLERY && a == 1)) {
            u32 old[6] = {theme, wall, dock, reduce_motion, opaque, dark};
            if (a == 0) {
                theme = b % NTHEME;
                if (theme >= 2)wall = theme + 2;
            } else if (a == 1)
                wall = b % NWALL;
            else if (a == 2)
                dock = b & 1;
            else if (a == 3) reduce_motion = b & 1;
            else if (a == 4) opaque = b & 1;
            else if (a == 5) dark = b & 1;
            else
                break;
            if (fs_saveprefs() < 0) {
                theme = old[0];
                wall = old[1];
                dock = old[2];
                reduce_motion = old[3];
                opaque = old[4];
                dark = old[5];
                break;
            }
            for (int i = 1; i < NPROC; i++)if (proc[i].pid && !proc[i].dying && proc[i].win.pid)post(proc + i, &(Event) {
                    .type = EV_THEME
                });
            r = 0;
        }
        break;
    }
    f->rax = r;
    return f;
}

/* All kernel accesses to a process's user pages are pinned for the syscall.
 * Mapping mutations are exclusive, including legacy allocation and stacks.
 * A contended caller yields and REEXECUTES its two-byte SYSCALL instruction:
 * never spin with IRQs off while a preempted reader needs this CPU to finish.
 * No lock is retained across the retry; ordinary network calls may enable IRQs.
 * The current task's CPU claim keeps p alive until this kernel frame returns. */
Frame *syscall_do(Frame *f)
{
    Process *p=current();u64 n=f->rax;
    int writer=(n==S_VM && f->rdi!=TMC_VM_QUERY)||n==S_ALLOC||n==S_FREE||n==S_THREAD;
    int acquired=uaccess_enter(&p->memory_users,writer);
    if(!acquired) {
        f->rip-=2; /* SYSENTER is not implemented: this ABI uses x86-64 SYSCALL. */
        u32 fl;spin_lock(&sched_lock,&fl);
        Frame *next=schedule(f);spin_unlock(&sched_lock,fl);return next;
    }
    Frame *next=syscall_inner(f);
    uaccess_leave(&p->memory_users,writer);
    return next;
}
