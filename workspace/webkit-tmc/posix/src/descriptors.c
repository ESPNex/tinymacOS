#include "tmc_descriptors.h"
#include "abi.h"
#include <errno.h>
#include <limits.h>

static struct tmc_open_description *lookup(struct tmc_fd_table *t,int fd)
{
    if(!t || fd<TMC_FD_FIRST || fd>=TMC_FD_MAX || !t->slots[fd]) {
        errno=EBADF;return 0;
    }
    return &t->objects[t->slots[fd]-1];
}
static int vacant(struct tmc_fd_table *t,int minimum)
{
    if(!t || minimum<0 || minimum>=TMC_FD_MAX){errno=EINVAL;return -1;}
    for(int i=minimum<TMC_FD_FIRST?TMC_FD_FIRST:minimum;i<TMC_FD_MAX;i++)
        if(!t->slots[i])return i;
    errno=EMFILE;return -1;
}
void tmc_fd_init(struct tmc_fd_table *t)
{
    if(!t)return;
    for(unsigned i=0;i<TMC_FD_MAX;i++) {
        t->slots[i]=0;
        t->objects[i]=(struct tmc_open_description){0,0,0};
    }
}
int tmc_fd_adopt(struct tmc_fd_table *t,int kind,int handle)
{
    if(!t || handle<0 || (kind!=TMC_FD_FILE && kind!=TMC_FD_TCP)) {errno=EINVAL;return -1;}
    int object=-1;
    for(unsigned i=0;i<TMC_FD_MAX;i++) {
        struct tmc_open_description *o=&t->objects[i];
        if(o->references && o->kind==kind && o->native_handle==handle){errno=EEXIST;return -1;}
        if(!o->references && object<0)object=(int)i;
    }
    int fd=vacant(t,TMC_FD_FIRST);
    if(fd<0)return -1;
    if(object<0){errno=EMFILE;return -1;}
    t->objects[object]=(struct tmc_open_description){kind,handle,1};
    t->slots[fd]=(unsigned)object+1;
    return fd;
}
int tmc_fd_open_readonly(struct tmc_fd_table *t,const char *path)
{
    if(vacant(t,TMC_FD_FIRST)<0)return -1;
    if(!path || path[0]!='/'){errno=EINVAL;return -1;}
    unsigned n=0;while(n<PATHLEN && path[n])n++;
    if(n==PATHLEN){errno=ENAMETOOLONG;return -1;}
    long h=tmc_syscall(S_OPEN,(uintptr_t)path,0,0,0);
    if(h<0){errno=EIO;return -1;} /* Native ABI cannot distinguish ENOENT/EACCES. */
    if(h>INT_MAX){errno=EOVERFLOW;return -1;}
    int fd=tmc_fd_adopt(t,TMC_FD_FILE,(int)h);
    if(fd<0) {
        int saved=errno;
        /* EEXIST indicates a contract violation: don't close an owned handle. */
        if(saved!=EEXIST)tmc_syscall(S_CLOSE,(unsigned)h,0,0,0);
        errno=saved;
    }
    return fd;
}
int tmc_fd_connect(struct tmc_fd_table *t,const char *host,uint16_t port)
{
    if(vacant(t,TMC_FD_FIRST)<0)return -1;
    int h=tmc_tcp_open(host,port);
    if(h<0)return -1;
    int fd=tmc_fd_adopt(t,TMC_FD_TCP,h);
    if(fd<0) {
        int saved=errno;
        if(saved!=EEXIST)tmc_tcp_close(h);
        errno=saved;
    }
    return fd;
}
int tmc_fd_dup(struct tmc_fd_table *t,int fd,int minimum)
{
    struct tmc_open_description *o=lookup(t,fd);
    if(!o)return -1;
    int dest=vacant(t,minimum);
    if(dest<0)return -1;
    o->references++;t->slots[dest]=t->slots[fd];
    return dest;
}
int tmc_fd_close(struct tmc_fd_table *t,int fd)
{
    struct tmc_open_description *o=lookup(t,fd);
    if(!o)return -1;
    t->slots[fd]=0;
    if(--o->references)return 0;
    int kind=o->kind,h=o->native_handle;
    *o=(struct tmc_open_description){0,0,0};
    if(kind==TMC_FD_TCP)return tmc_tcp_close(h);
    if(tmc_syscall(S_CLOSE,(unsigned)h,0,0,0)<0){errno=EIO;return -1;}
    return 0;
}
long tmc_fd_read(struct tmc_fd_table *t,int fd,void *buf,size_t n)
{
    struct tmc_open_description *o=lookup(t,fd);
    if(!o)return -1;
    if(o->kind==TMC_FD_TCP)return tmc_tcp_read(o->native_handle,buf,n);
    if(!n)return 0;
    if(!buf){errno=EFAULT;return -1;}
    if(n>MAXFILE)n=MAXFILE;
    long r=tmc_syscall(S_READ,(unsigned)o->native_handle,(uintptr_t)buf,n,0);
    if(r<0 || (unsigned long)r>n){errno=EIO;return -1;}
    return r;
}
long tmc_fd_write(struct tmc_fd_table *t,int fd,const void *buf,size_t n)
{
    struct tmc_open_description *o=lookup(t,fd);
    if(!o)return -1;
    if(o->kind==TMC_FD_FILE){errno=EBADF;return -1;} /* read-only adoption */
    return tmc_tcp_write(o->native_handle,buf,n);
}
int tmc_fd_seek_absolute(struct tmc_fd_table *t,int fd,uint64_t at)
{
    struct tmc_open_description *o=lookup(t,fd);
    if(!o)return -1;
    if(o->kind!=TMC_FD_FILE){errno=ESPIPE;return -1;}
    if(tmc_syscall(S_SEEK,(unsigned)o->native_handle,at,0,0)<0){errno=EINVAL;return -1;}
    return 0;
}
int tmc_fd_poll(struct tmc_fd_table *t,struct tmc_fd_poll *p,size_t count,int timeout)
{
    if(!t || (count&&!p) || count>TMC_FD_MAX || timeout< -1){errno=EINVAL;return -1;}
    uint64_t start=tmc_monotonic_ns();
    for(;;) {
        int ready=0;
        for(size_t i=0;i<count;i++)p[i].revents=0;
        for(size_t i=0;i<count;i++) {
            if(p[i].fd<0)continue;
            struct tmc_open_description *o=lookup(t,p[i].fd);
            unsigned status;
            if(!o)status=TMC_POLL_INVALID;
            else if(o->kind==TMC_FD_FILE)status=TMC_NET_READABLE;
            else {
                int r=tmc_tcp_status(o->native_handle);
                if(r<0) {
                    if(errno!=EBADF)return -1;
                    status=TMC_POLL_INVALID;
                } else status=(unsigned)r;
            }
            p[i].revents=status & ((p[i].events&(TMC_NET_READABLE|TMC_NET_WRITABLE)) |
                                   TMC_NET_HANGUP|TMC_NET_ERROR|TMC_POLL_INVALID);
            ready+=p[i].revents!=0;
        }
        if(ready)return ready;
        if(timeout>=0 && tmc_monotonic_ns()-start>=(uint64_t)timeout*UINT64_C(1000000))return 0;
        if(tmc_syscall(S_SLEEP,1,0,0,0)<0){errno=EIO;return -1;}
    }
}
