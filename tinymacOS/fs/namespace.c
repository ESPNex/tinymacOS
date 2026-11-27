/* SPDX-License-Identifier: MIT
 * Serialized native TMCFS2 and foreign-volume namespace. No legacy backend.
 */
#include "../kernel/k.h"
#include "fs2_runtime.h"
#include "home_name.h"
#include "volumes.h"
u32 persistent, fsrev, usednodes, theme, wall, dock, reduce_motion, opaque, dark;
RecSpin fs_gate;
void fs_init(void){if(!fs2_mount())panic("TMCFS2 initialization failed; disks untouched",0,0);}
int fs_home_bind(const char*name){
 if(!home_name_valid(name)||boot.recovery)return -3;
 u32 fl;rec_lock(&fs_gate,&fl);int r=fs2_home_bind(name);rec_unlock(&fs_gate,fl);return r;
}
int fs_stat(const char *p, Dirent *d)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r=vol_stat?vol_stat(p,d):-2;
    if(r==-2)r=fs2_stat(p,d);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_list(const char *p, Dirent *d, int cap, int skip)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r=vol_list?vol_list(p,d,cap,skip):-2;
    if(r==-2)r=fs2_list(p,d,cap,skip);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_create(const char *p, int dir)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r=vol_create?vol_create(p,dir):-2;if(r==-2)r=fs2_create(p,dir);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_remove(const char *p)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r=vol_remove?vol_remove(p,0):-2;if(r==-2&&!(vol_reserved&&vol_reserved(p)))r=fs2_remove(p,0);
    rec_unlock(&fs_gate, fl);
    return r;
}
int fs_remove_flags(const char *p,unsigned flags)
{
    u32 fl;rec_lock(&fs_gate,&fl);
    int r=vol_remove?vol_remove(p,flags):-2;if(r==-2&&!(vol_reserved&&vol_reserved(p)))r=fs2_remove(p,flags);
    rec_unlock(&fs_gate,fl);return r;
}

static int cross_copy(Process*files,const char*src,const char*dst,unsigned depth,u8*buf,u8*check){
 Dirent e,exists;if(depth>48||fs_stat(src,&e)||!fs_stat(dst,&exists)||fs_create(dst,e.dir))return -1;
 if(e.dir){int skip=0,r;Dirent child;while((r=fs_list(src,&child,1,skip++))>0){char a[PATHLEN],b[PATHLEN];if(strlen(src)+strlen(child.name)+2>PATHLEN||strlen(dst)+strlen(child.name)+2>PATHLEN)return -1;strset(a,src,PATHLEN);append(a,"/",PATHLEN);append(a,child.name,PATHLEN);strset(b,dst,PATHLEN);append(b,"/",PATHLEN);append(b,child.name,PATHLEN);if(cross_copy(files,a,b,depth+1,buf,check))return -1;}return r<0?-1:0;}
 int in=fs_open(files,src,0),out=fs_open(files,dst,1),result=-1;if(in<0||out<0)goto done;u64 at=0;while(at<e.size){int n=MIN(4096,e.size-at);if(fs_read(files,in,buf,n)!=n||fs_write(files,out,buf,n)!=n||fs_seek(files,out,at)||fs_read(files,out,check,n)!=n||memcmp(buf,check,n))goto done;at+=n;}result=0;
 done:if(in>=0)fs_close(files,in);if(out>=0)fs_close(files,out);return result;
}
static int cross_move(const char*a,const char*b){
 char src[PATHLEN],dst[PATHLEN];Dirent e,exists;if(!vol_normalize||vol_normalize(a,src)||vol_normalize(b,dst)||eq(src,"/")||fs_stat(src,&e)||e.ro||!fs_stat(dst,&exists))return -1;
 size_t n=strlen(src);if(!memcmp(src,dst,MIN(n,strlen(dst)))&&strlen(dst)>=n&&(!dst[n]||dst[n]=='/'))return -1;
 Process*files=kmalloc(sizeof *files);u8*buffer=kmalloc(8192);if(!files||!buffer){kfree(files);kfree(buffer);return -1;}memset(files,0,sizeof *files);
 int r=cross_copy(files,src,dst,0,buffer,buffer+4096);
 if(!r)r=fs_remove_flags(src,e.dir?REMOVE_RECURSIVE:REMOVE_FILE_ONLY);
 /* A failed copy leaves the source untouched; keep any destination as an
  * explicitly failed partial copy, never report that the move succeeded. */
 kfree(buffer);kfree(files);return r;
}
int fs_move(const char *p, const char *q)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r=vol_move?vol_move(p,q):-2;if(r==-2)r=fs2_move(p,q);if(r==-3)r=cross_move(p,q);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_open(Process *p, const char *path, int w)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r=vol_open?vol_open(p,path,w):-2;
    if(r==-2)r=fs2_open(p,path,w);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_close(Process *p, int fd)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r;
    if(fd>=0&&fd<12&&p->fd[fd].id&&(p->fd[fd].backend==4||p->fd[fd].backend==5)){memset(p->fd+fd,0,sizeof(FD));r=0;}
    else r=fs2_close(p,fd);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_read(Process *p, int d, void *b, int n)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int foreign=d>=0&&d<12&&(p->fd[d].backend==4||p->fd[d].backend==5);
    int r=foreign?(vol_read?vol_read(p,d,b,n):-1):fs2_read(p,d,b,n);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_write(Process *p, int d, const void *b, int n)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int foreign=d>=0&&d<12&&(p->fd[d].backend==4||p->fd[d].backend==5);
    int r=foreign?(vol_write?vol_write(p,d,b,n):-1):fs2_write(p,d,b,n);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_saveprefs(void)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs2_prefs();
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_seek(Process *p, int d, u64 off)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int foreign=d>=0&&d<12&&(p->fd[d].backend==4||p->fd[d].backend==5);
    int r=foreign?(vol_seek?vol_seek(p,d,off):-1):fs2_seek(p,d,off);
    rec_unlock(&fs_gate, fl);
    return r;
}

int account_sector(int slot, void *buf, int wr)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs2_account(slot,buf,wr);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_snapshot(void *buf, u32 n)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs2_snapshot(buf,n);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_snap_create(const char *name)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs2_snap_create(name);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_snap_list(void *out, int cap)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs2_snap_list(out,cap);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_snap_page(void*out,int cap,int skip)
{
 if(cap<0||skip<0||(!out&&cap))return -1;u32 fl;rec_lock(&fs_gate,&fl);int r;
 r=fs2_snap_page(out,cap,skip);
 rec_unlock(&fs_gate,fl);return r;
}

int fs_snap_restore(const char *name)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs2_snap_restore(name);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_snap_verify(const char *name)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs2_snap_verify(name);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_snap_delete(const char *name)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs2_snap_delete(name);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_snap_info(FsInfo *f)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs2_info(f);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_enc(int sub, const char *pass, EncInfo *out)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs2_enc(sub,pass,out);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_touch(const char *p)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r;if(vol_reserved&&vol_reserved(p)){Dirent e;r=fs_stat(p,&e);r=!r?(e.ro?-1:0):(vol_create?vol_create(p,0):-1);}else r=fs2_touch(p);
    rec_unlock(&fs_gate, fl);
    return r;
}
