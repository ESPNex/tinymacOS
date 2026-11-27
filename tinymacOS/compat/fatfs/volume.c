/* SPDX-License-Identifier: MIT
 * Bounded FatFs adapter. Not a FUSE process and no hosted filesystem calls.
 * Serialized by fs_gate; each operation closes/syncs/unmounts its view so
 * caches cannot be mistaken for another partition or survive a disk edit.
 */
#include "foreign_volume.h"
#include "ff.h"
#include "diskio.h"
typedef struct {ForeignIo io;FATFS fs;FIL file;DIR dir;FILINFO entry;u32 sector;int mounted,opened,isdir,failed;} Context;
static Context*active;
static int code(FRESULT r){switch(r){case FR_OK:return 0;case FR_EXIST:return FOREIGN_EXISTS;case FR_NO_FILE:case FR_NO_PATH:return FOREIGN_MISSING;case FR_WRITE_PROTECTED:return FOREIGN_READ_ONLY;case FR_DENIED:return FOREIGN_FULL;case FR_INVALID_NAME:case FR_INVALID_PARAMETER:case FR_NO_FILESYSTEM:return FOREIGN_INVALID;default:return FOREIGN_IO;}}
static u16 le16(const u8*p){return p[0]|(u16)p[1]<<8;}
static u32 le32(const u8*p){return le16(p)|(u32)le16(p+2)<<16;}
DSTATUS disk_status(BYTE drive){return drive||!active?STA_NOINIT:!active->io.write?STA_PROTECT:0;}
DSTATUS disk_initialize(BYTE drive){return disk_status(drive);}
DRESULT disk_read(BYTE drive,BYTE*p,LBA_t at,UINT n){
 Context*c=active;if(drive||!c||!n||!p)return RES_PARERR;u64 factor=c->sector/512;
 if(at>c->io.sectors/factor||n>c->io.sectors/factor-at){c->failed=1;return RES_PARERR;}
 while(n){u32 blocks=MIN((u64)n,128/factor);if(c->io.read(c->io.context,at*factor,blocks*factor,p)){c->failed=1;return RES_ERROR;}p+=blocks*c->sector;at+=blocks;n-=blocks;}return RES_OK;
}
DRESULT disk_write(BYTE drive,const BYTE*p,LBA_t at,UINT n){
 Context*c=active;if(drive||!c||!n||!p)return RES_PARERR;if(!c->io.write)return RES_WRPRT;u64 factor=c->sector/512;
 if(at>c->io.sectors/factor||n>c->io.sectors/factor-at){c->failed=1;return RES_PARERR;}
 while(n){u32 blocks=MIN((u64)n,128/factor);if(c->io.write(c->io.context,at*factor,blocks*factor,p)){c->failed=1;return RES_ERROR;}p+=blocks*c->sector;at+=blocks;n-=blocks;}return RES_OK;
}
DRESULT disk_ioctl(BYTE drive,BYTE op,void*p){
 Context*c=active;if(drive||!c)return RES_PARERR;
 switch(op){case CTRL_SYNC:if(c->io.flush(c->io.context)){c->failed=1;return RES_ERROR;}return RES_OK;
 case GET_SECTOR_COUNT:if(!p)return RES_PARERR;*(LBA_t*)p=c->io.sectors/(c->sector/512);return RES_OK;
 case GET_SECTOR_SIZE:if(!p)return RES_PARERR;*(WORD*)p=c->sector;return RES_OK;
 case GET_BLOCK_SIZE:if(!p)return RES_PARERR;*(DWORD*)p=1;return RES_OK;default:return RES_PARERR;}
}
DWORD get_fattime(void){return active&&active->io.timestamp?active->io.timestamp(active->io.context):0x00210000u;}
void*ff_memalloc(UINT n){return active?active->io.alloc(active->io.context,n):0;}
void ff_memfree(void*p){if(active&&p)active->io.free(active->io.context,p);}
static Context*begin(ForeignIo*io,int write,int format,int*error){
 *error=FOREIGN_INVALID;
 if(active||!io||!io->sectors||!io->read||!io->flush||!io->alloc||!io->free||(io->logical_bytes!=512&&io->logical_bytes!=4096))return 0;
 if(write&&!io->write){*error=FOREIGN_READ_ONLY;return 0;}Context*c=io->alloc(io->context,sizeof *c);if(!c){*error=FOREIGN_IO;return 0;}memset(c,0,sizeof *c);c->io=*io;c->sector=io->logical_bytes;active=c;
 u8 boot[512];if(!format){
  if(io->read(io->context,0,1,boot)){*error=FOREIGN_IO;goto bad;}
  if(!memcmp(boot+3,"EXFAT   ",8)){
   if(boot[108]<9||boot[108]>12){*error=FOREIGN_INVALID;goto bad;}c->sector=1u<<boot[108];
   if(write&&(le16(boot+106)&0x0006)){*error=FOREIGN_READ_ONLY;goto bad;}
  }else{c->sector=le16(boot+11);if(c->sector!=512&&c->sector!=4096)goto bad;
   /* Refuse unclean/error-marked FAT32; do not "repair" Windows media. */
   if(write&&!le16(boot+17)&&!le16(boot+22)){
    u64 at=(u64)le16(boot+14)*(c->sector/512);u32 flags=le16(boot+40);if(flags&0x80)at+=(u64)(flags&15)*le32(boot+36)*(c->sector/512);
    if(at>=io->sectors||io->read(io->context,at,1,boot)){*error=FOREIGN_IO;goto bad;}
    if((le32(boot+4)&0x0c000000u)!=0x0c000000u){*error=FOREIGN_READ_ONLY;goto bad;}
   }
  }
 }
 *error=code(f_mount(&c->fs,"0:",!format));if(*error)goto bad;c->mounted=1;return c;
 bad:f_mount(0,"0:",0);active=0;io->free(io->context,c);return 0;
}
static int end(Context*c,int result){
 int r=0;if(c->opened)r=code(c->isdir?f_closedir(&c->dir):f_close(&c->file));
 if(c->mounted&&f_mount(0,"0:",0)!=FR_OK)r=FOREIGN_IO;if(c->failed)r=FOREIGN_IO;
 ForeignIo io=c->io;active=0;io.free(io.context,c);return result<0?result:r?r:result;
}
static int valid_path(const char*p){
 if(!p||*p!='/'||strlen(p)>1020)return 0;
 for(const char*s=p;*s;s++)if(*s==':'||*s=='\\')return 0;
 while(*p){while(*p=='/')p++;const char*s=p;while(*p&&*p!='/')p++;size_t n=p-s;if((n==1&&*s=='.')||(n==2&&s[0]=='.'&&s[1]=='.'))return 0;}return 1;
}
/* FatFs requires its caller to reject moving a directory into itself. Compare
 * actual directory cluster IDs, not case-sensitive text: alternate DOS names
 * and case-insensitive Unicode paths must not bypass the ancestry check. */
static int cycle(Context*c,const char*from,const char*to){
 int r=code(f_stat(from,&c->entry));if(r||!(c->entry.fattrib&AM_DIR))return r;
 if((r=code(f_opendir(&c->dir,from))))return r;DWORD source=c->dir.obj.sclust;if(f_closedir(&c->dir))return FOREIGN_IO;
 char*parent=c->io.alloc(c->io.context,strlen(to)+2);if(!parent)return FOREIGN_IO;strset(parent,to,strlen(to)+2);size_t end=strlen(parent);while(end&&parent[end-1]!='/')end--;if(end>1)parent[end-1]=0;else parent[1]=0;
 r=0;
 for(size_t i=1;;i++)if(!parent[i]||parent[i]=='/'){
  char saved=parent[i];parent[i]=0;r=code(f_opendir(&c->dir,parent));parent[i]=saved;
  if(r)break;int same=c->dir.obj.sclust==source;if(f_closedir(&c->dir)){r=FOREIGN_IO;break;}if(same){r=FOREIGN_INVALID;break;}if(!saved)break;
 }
 c->io.free(c->io.context,parent);return r;
}
static void entry(Context*c,ForeignEntry*out){
 out->size=c->entry.fsize;out->dir=!!(c->entry.fattrib&AM_DIR);out->ro=!c->io.write||!!(c->entry.fattrib&AM_RDO);strset(out->name,c->entry.fname,sizeof out->name);
}
int fat_volume_stat(ForeignIo*io,const char*path,ForeignEntry*out){
 if(!valid_path(path)||!out)return FOREIGN_INVALID;int error;Context*c=begin(io,0,0,&error);if(!c)return error;
 if(eq(path,"/")){memset(out,0,sizeof *out);out->dir=1;out->ro=!io->write;strset(out->name,"/",sizeof out->name);return end(c,0);}
 int r=code(f_stat(path,&c->entry));if(!r)entry(c,out);return end(c,r);
}
int fat_volume_list(ForeignIo*io,const char*path,ForeignEntry*out,int cap,int skip){
 if(!valid_path(path)||cap<0||skip<0||(!out&&cap))return FOREIGN_INVALID;int error;Context*c=begin(io,0,0,&error);if(!c)return error;
 int r=code(f_opendir(&c->dir,path));if(r)return end(c,r);c->opened=c->isdir=1;int n=0;
 while(n<cap){r=code(f_readdir(&c->dir,&c->entry));if(r||!c->entry.fname[0])break;if(skip){skip--;continue;}entry(c,out+n++);}return end(c,r?r:n);
}
int fat_volume_create(ForeignIo*io,const char*path,int directory){
 if(!valid_path(path)||eq(path,"/"))return FOREIGN_INVALID;int error;Context*c=begin(io,1,0,&error);if(!c)return error;int r;
 if(directory)r=code(f_mkdir(path));else{r=code(f_open(&c->file,path,FA_WRITE|FA_CREATE_NEW));if(!r)c->opened=1;}return end(c,r);
}
int fat_volume_remove(ForeignIo*io,const char*path){
 if(!valid_path(path)||eq(path,"/"))return FOREIGN_INVALID;int error;Context*c=begin(io,1,0,&error);return c?end(c,code(f_unlink(path))):error;
}
int fat_volume_move(ForeignIo*io,const char*from,const char*to){
 if(!valid_path(from)||!valid_path(to)||eq(from,"/")||eq(to,"/"))return FOREIGN_INVALID;int error;Context*c=begin(io,1,0,&error);if(!c)return error;
 int r=cycle(c,from,to);return end(c,r?r:code(f_rename(from,to)));
}
i64 fat_volume_read(ForeignIo*io,const char*path,u64 offset,void*p,size_t bytes){
 if(!valid_path(path)||(!p&&bytes)||bytes>0x7fffffffu)return FOREIGN_INVALID;int error;Context*c=begin(io,0,0,&error);if(!c)return error;
 int r=code(f_open(&c->file,path,FA_READ));if(r)return end(c,r);c->opened=1;u64 size=f_size(&c->file);if(offset>=size)return end(c,0);bytes=MIN(bytes,size-offset);
 if((r=code(f_lseek(&c->file,offset))))return end(c,r);UINT n=0;r=code(f_read(&c->file,p,bytes,&n));return end(c,r?r:n);
}
i64 fat_volume_write(ForeignIo*io,const char*path,u64 offset,const void*p,size_t bytes){
 if(!valid_path(path)||(!p&&bytes)||bytes>0x7fffffffu||offset>0x7fffffffffffffffull-bytes)return FOREIGN_INVALID;int error;Context*c=begin(io,1,0,&error);if(!c)return error;
 if(c->fs.fs_type!=FS_EXFAT&&offset+bytes>0xffffffffull)return end(c,FOREIGN_TOO_LARGE);
 int r=code(f_open(&c->file,path,FA_WRITE));if(r)return end(c,r);c->opened=1;
 if((r=code(f_lseek(&c->file,offset)))||f_tell(&c->file)!=offset)return end(c,r?r:FOREIGN_FULL);
 UINT n=0;r=code(f_write(&c->file,p,bytes,&n));if(r||n!=bytes)return end(c,r?r:FOREIGN_FULL);
 if((r=code(f_truncate(&c->file))))return end(c,r);return end(c,n);
}
int fat_volume_format_exfat(ForeignIo*io){
 int error;Context*c=begin(io,1,1,&error);if(!c)return error;void*work=io->alloc(io->context,65536);if(!work)return end(c,FOREIGN_IO);
 MKFS_PARM options={.fmt=FM_EXFAT|FM_SFD,.n_fat=1,.align=1,.n_root=0,.au_size=0};int r=code(f_mkfs("0:",&options,work,65536));io->free(io->context,work);return end(c,r);
}
