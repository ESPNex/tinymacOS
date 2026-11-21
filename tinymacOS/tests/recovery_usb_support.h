#pragma once
#include "base.h"
#include <assert.h>
#include <stdio.h>
#define EFIAPI
#define EFI_SUCCESS 0
#define EFI_ERROR(x) ((x)!=0)
#define TRUE 1
#define FALSE 0
#define TPL_NOTIFY 16
#define ByProtocol 0
#define EFI_OPEN_PROTOCOL_BY_DRIVER 16
#define EFI_OPEN_PROTOCOL_EXCLUSIVE 32
typedef u64 EFI_STATUS;
typedef u64 UINTN;
typedef u8 UINT8;
typedef u32 UINT32;
typedef u8 BOOLEAN;
typedef void VOID;
typedef void *EFI_HANDLE;
typedef u64 EFI_TPL;
typedef struct {u32 a;u16 b,c;u8 d[8];} EFI_GUID;
typedef struct {UINT32 Attributes;EFI_HANDLE AgentHandle;} EFI_OPEN_PROTOCOL_INFORMATION_ENTRY;
typedef struct {
 EFI_STATUS (*LocateHandleBuffer)(int,EFI_GUID*,void*,UINTN*,EFI_HANDLE**);
 EFI_STATUS (*OpenProtocolInformation)(EFI_HANDLE,EFI_GUID*,EFI_OPEN_PROTOCOL_INFORMATION_ENTRY**,UINTN*);
 EFI_STATUS (*FreePool)(void*);
 EFI_STATUS (*HandleProtocol)(EFI_HANDLE,EFI_GUID*,void**);
 EFI_TPL (*RaiseTPL)(EFI_TPL);
 void (*RestoreTPL)(EFI_TPL);
 EFI_STATUS (*DisconnectController)(EFI_HANDLE,EFI_HANDLE,EFI_HANDLE);
 EFI_STATUS (*ConnectController)(EFI_HANDLE,EFI_HANDLE*,void*,BOOLEAN);
} EFI_BOOT_SERVICES;
extern int rw,rh;
