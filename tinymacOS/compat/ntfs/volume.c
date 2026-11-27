/* SPDX-License-Identifier: GPL-2.0-or-later
 * Real pinned libntfs-3g filesystem API. No FUSE/host syscalls. Every write
 * mount is preceded by a separate forensic clean/hibernation preflight.
 */
#include "config.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "volume.h"
#include "device.h"
#include "inode.h"
#include "attrib.h"
#include "dir.h"
#include "unistr.h"
#include "logging.h"
#include "volume_bridge.h"
extern struct ntfs_device_operations ntfs_device_unix_io_ops;
extern int tmc_ntfs_check_clean(ntfs_volume*);
static int err(void){switch(errno){case ENOENT:return -5;case EEXIST:return -4;case ENOTEMPTY:return -6;case ENOSPC:return -7;case EROFS:case EACCES:case EPERM:return -3;case EINVAL:case ENAMETOOLONG:return -2;default:return -1;}}
static int directory(ntfs_inode*n){return !!(n->mrec->flags&MFT_RECORD_IS_DIRECTORY);}
static int protected(ntfs_inode*n){return n->mft_no<16||!!(n->flags&(FILE_ATTR_READONLY|FILE_ATTR_REPARSE_POINT|FILE_ATTR_ENCRYPTED|FILE_ATTR_COMPRESSED));}
static int info(ntfs_inode*n,NtVolumeEntry*e,int ro){
 memset(e,0,sizeof *e);e->dir=directory(n);e->ro=ro||(n->mft_no!=FILE_root&&protected(n));if(!e->dir){ntfs_attr*a=ntfs_attr_open(n,AT_DATA,AT_UNNAMED,0);if(!a)return err();e->size=a->data_size;ntfs_attr_close(a);}return 0;
}
static int split(ntfs_volume*v,const char*path,ntfs_inode**parent,ntfschar**name,int*length){
 char*p=strdup(path);if(!p){errno=ENOMEM;return -1;}char*s=strrchr(p,'/');if(!s||!s[1]){free(p);errno=EINVAL;return -1;}
 *length=ntfs_mbstoucs(s+1,name);if(*length<=0||*length>255){free(*name);*name=0;free(p);errno=EINVAL;return -1;}
 if(s==p)s[1]=0;else *s=0;*parent=ntfs_pathname_to_inode(v,0,p);free(p);if(!*parent){free(*name);*name=0;return -1;}return 0;
}
typedef struct {ntfs_volume*v;NtVolumeRequest*q;int count,skip,error,full;} Listing;
static int fill(void*ptr,const ntfschar*name,int len,int kind,s64 pos,MFT_REF ref,unsigned type){
 (void)pos;(void)type;Listing*l=ptr;if(kind==FILE_NAME_DOS||MREF(ref)<16||(len==1&&name[0]==const_cpu_to_le16('.'))||(len==2&&name[0]==const_cpu_to_le16('.')&&name[1]==const_cpu_to_le16('.')))return 0;
 if(l->skip){l->skip--;return 0;}if(l->count==l->q->cap){l->full=1;return 1;}
 ntfs_inode*n=ntfs_inode_open(l->v,ref);if(!n){l->error=err();return -1;}
 NtVolumeEntry*e=l->q->entries+l->count;int r=info(n,e,l->q->readonly);if(ntfs_inode_close(n)&&!r)r=-1;if(r){l->error=r;return -1;}
 char*dst=e->name;int bytes=ntfs_ucstombs(name,len,&dst,sizeof e->name);if(bytes<0){l->error=-2;return -1;}e->name[bytes]=0;l->count++;return 0;
}
static long long operation(ntfs_volume*v,NtVolumeRequest*q,int write){
 ntfs_inode*n=0,*parent=0,*targetparent=0,*exists=0;ntfs_attr*a=0;ntfschar*name=0,*targetname=0;int length=0,targetlength=0,r=-1,dirty=0;long long result=-1;
 n=ntfs_pathname_to_inode(v,0,q->path);
 if(q->op==NT_VCREATE){
  if(n){result=-4;goto out;}if(errno!=ENOENT){result=err();goto out;}
  if(split(v,q->path,&parent,&name,&length)){result=err();goto out;}if(!directory(parent)||(parent->mft_no!=FILE_root&&protected(parent))){result=-3;goto out;}
 }else if(!n){ntfs_log_error("native path lookup error %d: %s\n",errno,q->path);result=err();goto out;}
 if(n&&q->op!=NT_VSTAT&&q->op!=NT_VLIST&&(n->flags&(FILE_ATTR_REPARSE_POINT|FILE_ATTR_ENCRYPTED))){result=-3;goto out;}
 if(write&&n&&protected(n)){result=-3;goto out;}
 if(q->op==NT_VSTAT){result=info(n,q->entries,q->readonly||!!(v->flags&VOLUME_IS_DIRTY));goto out;}
 if(q->op==NT_VLIST){Listing l={.v=v,.q=q,.skip=q->skip};s64 pos=0;if(!directory(n)){result=-2;goto out;}r=ntfs_readdir(n,&pos,&l,fill);result=l.error?l.error:r&&!l.full?err():l.count;goto out;}
 if(q->op==NT_VREAD||q->op==NT_VWRITE){if(directory(n)){result=-2;goto out;}a=ntfs_attr_open(n,AT_DATA,AT_UNNAMED,0);if(!a){result=err();goto out;}}
 if(q->op==NT_VREMOVE||q->op==NT_VMOVE){
  if(split(v,q->path,&parent,&name,&length)){result=err();goto out;}
  if(!directory(parent)||(parent->mft_no!=FILE_root&&protected(parent))){result=-3;goto out;}
  if(q->op==NT_VREMOVE&&directory(n)&&ntfs_check_empty_dir(n)){result=err();goto out;}
 }
 if(q->op==NT_VMOVE){
  exists=ntfs_pathname_to_inode(v,0,q->target);if(exists){result=-4;goto out;}if(errno!=ENOENT){result=err();goto out;}
  if(split(v,q->target,&targetparent,&targetname,&targetlength)){result=err();goto out;}
  if(!directory(targetparent)||(targetparent->mft_no!=FILE_root&&protected(targetparent))){result=-3;goto out;}
  if(directory(n)){
   ntfs_inode*walk=ntfs_inode_open(v,targetparent->mft_no);unsigned depth=0;
   while(walk){u64 id=walk->mft_no;if(id==n->mft_no||depth++>=256){ntfs_inode_close(walk);result=-2;goto out;}if(id==FILE_root){ntfs_inode_close(walk);break;}ntfs_inode*up=ntfs_dir_parent_inode(walk);ntfs_inode_close(walk);walk=up;if(!walk){result=-1;goto out;}}
  }
 }
 if(write){if(ntfs_volume_write_flags(v,v->flags|VOLUME_IS_DIRTY)){result=err();goto out;}dirty=1;}
 switch(q->op){
 case NT_VCREATE:n=ntfs_create(parent,0,name,length,q->directory?S_IFDIR:S_IFREG);result=n?0:err();break;
 case NT_VREAD:result=ntfs_attr_pread(a,q->offset,q->bytes,q->data);if(result<0)result=err();break;
 case NT_VWRITE:result=ntfs_attr_pwrite(a,q->offset,q->bytes,q->data);if(result<0)result=err();else if(result!=(long long)q->bytes)result=-7;else if(ntfs_attr_truncate(a,q->offset+result))result=err();break;
 case NT_VREMOVE:r=ntfs_delete(v,q->path,n,parent,name,length);n=parent=0;result=r?err():0;break;
 case NT_VMOVE:
  if(ntfs_link(n,targetparent,targetname,targetlength)){result=err();break;}
  if(ntfs_inode_sync(n)||ntfs_inode_sync(targetparent)){result=err();break;}
  if(targetparent->mft_no==parent->mft_no){
   ntfs_inode_close(parent);parent=targetparent;targetparent=0;
  }
  /* Keep both names, not neither, if unlink fails; report failure honestly. */
  r=ntfs_delete(v,q->path,n,parent,name,length);n=parent=0;result=r?err():0;break;
 default:result=-2;break;
 }
 out:
 if(a)ntfs_attr_close(a);if(n&&ntfs_inode_close(n))result=-1;if(parent&&ntfs_inode_close(parent))result=-1;if(targetparent&&ntfs_inode_close(targetparent))result=-1;if(exists&&ntfs_inode_close(exists))result=-1;free(name);free(targetname);
 if(dirty&&result>=0&&ntfs_volume_write_flags(v,v->flags&~VOLUME_IS_DIRTY))result=-1;
 return result;
}
long long tmc_ntfs_volume_run(NtVolumeRequest*q,int write,int preflight){
 struct ntfs_device*d=ntfs_device_alloc("tmc-partition",0,&ntfs_device_unix_io_ops,0);if(!d)return err();
 ntfs_volume*v=ntfs_device_mount(d,write?NTFS_MNT_NONE:NTFS_MNT_RDONLY|NTFS_MNT_FORENSIC);if(!v){ntfs_log_error("native volume mount error %d\n",errno);ntfs_device_free(d);return err();}
 long long result;
 if(preflight)result=(v->flags&VOLUME_IS_DIRTY)||tmc_ntfs_check_clean(v)?-3:0;
 else result=operation(v,q,write);

 if(ntfs_umount(v,0))result=-1;return result;
}
