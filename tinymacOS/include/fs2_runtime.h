#pragma once
/* Included after kernel/k.h. The legacy backend remains available; no format
 * conversion or implicit format is performed when mounting existing media. */
extern int fs2_active;
int fs2_mount(void);
int fs2_home_bind(const char*);
int fs2_stat(const char*,Dirent*);
int fs2_list(const char*,Dirent*,int,int);
int fs2_create(const char*,int);
int fs2_remove(const char*,unsigned);
int fs2_move(const char*,const char*);
int fs2_open(Process*,const char*,int);
int fs2_close(Process*,int);
int fs2_read(Process*,int,void*,int);
int fs2_write(Process*,int,const void*,int);
int fs2_seek(Process*,int,u64);
int fs2_account(int,void*,int);
int fs2_prefs(void);
int fs2_snap_create(const char*);
int fs2_snap_list(void*,int);
int fs2_snap_page(void*,int,int);
int fs2_local_backup(int);
int fs2_snap_restore(const char*);
int fs2_snap_delete(const char*);
int fs2_snap_verify(const char*);
int fs2_info(FsInfo*);
int fs2_touch(const char*);
int fs2_enc(int,const char*,EncInfo*);

int fs2_install_data(int,u64,u64,const void*,size_t,int);
int fs2_disk_verify(int,u64,u64);
int fs2_disk_reset(int,u64,u64);
int fs2_disk_backup(int,u64,u64,int);
int fs2_disk_present(int,u64,u64);

int fs2_install_encrypted(int,u64,u64,const void*,size_t,const char*,const u8[128],char[65]);

int fs2_disk_rekey_reset(int,u64,u64,const char*,const u8[128],char[65]);
int fs2_external_import(int,u64,u64,int,u64,u64,const char*,int,u64*);

/* Kernel export sink only: positional writes without the editor truncation policy. */
i64 fs2_write_at(Process*,int,u64,const void*,size_t);
