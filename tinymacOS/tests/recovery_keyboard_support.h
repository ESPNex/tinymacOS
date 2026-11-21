#pragma once
#include "recovery_usb_support.h"
#include <stdlib.h>
#include <string.h>
#define DEVICE_PATH_PROTOCOL {0}
typedef struct {UINT32 Attributes;EFI_HANDLE AgentHandle;} KTestOpenInfo;
/* The fixture substitutes only firmware/PS2 imports, not Recovery logic. */
#define EFI_OPEN_PROTOCOL_INFORMATION_ENTRY KTestOpenInfo
typedef struct {
 EFI_STATUS (*LocateHandleBuffer)(int,EFI_GUID*,void*,UINTN*,EFI_HANDLE**);
 EFI_STATUS (*OpenProtocolInformation)(EFI_HANDLE,EFI_GUID*,KTestOpenInfo**,UINTN*);
 EFI_STATUS (*FreePool)(void*);
 EFI_STATUS (*HandleProtocol)(EFI_HANDLE,EFI_GUID*,void**);
 EFI_STATUS (*DisconnectController)(EFI_HANDLE,EFI_HANDLE,EFI_HANDLE*);
 EFI_TPL (*RaiseTPL)(EFI_TPL);
 void (*RestoreTPL)(EFI_TPL);
} KTestServices;
#define EFI_BOOT_SERVICES KTestServices
static u64 ticks;
static int keyboard_on;
static u8 inb(u16 p){(void)p;return 255;}
static int cmd(int c){(void)c;return 0;}
static int data(int c){(void)c;return 0;}
static int receive(int c){(void)c;return -1;}
static int device(int a,int b){(void)a;(void)b;return 0;}
static void ps2_setleds(int c){(void)c;}
static void keyboard_byte(u8 b){(void)b;}
static void keyboard_led_poll(void){}
