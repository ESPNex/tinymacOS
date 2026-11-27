#pragma once
/* Kernel-only scalable block envelope dispatch; caller holds fs_gate. No
 * implicit encryption/format/conversion. Plain volumes keep their raw layout. */
int fs2_volume_encrypted(int,u64,u64);
u64 fs2_volume_blocks(int,u64,u64);
int fs2_volume_read(int,u64,u64,u64,void*);
int fs2_volume_write(int,u64,u64,u64,const void*);
int fs2_volume_locked(int);
int fs2_volume_control(int,int,VaultRequest*,VaultInfo*,int*);
int fs2_volume_sector_guard(int,u64,u32);
void fs2_volume_forget(int);

/* Installer only: already preflighted NEW extent, persisted blank headers. */
int fs2_volume_create_fresh(int,u64,u64,const char*,const u8[128],char[65]);

/* Explicit irreversible offline Recovery operation, never normal format. */
int fs2_volume_rekey_fresh(int,u64,u64,const char*,const u8[128],char[65]);
