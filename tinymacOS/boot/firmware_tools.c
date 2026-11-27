/* User-invoked pre-OS maintenance. Never writes system/Windows EFI files. */
#include <efi.h>
#include <efilib.h>
#include "base.h"
#include "gopmode.h"
#include "bootdefault.h"
static EFI_SYSTEM_TABLE *table;
static void say(const CHAR16*s){table->ConOut->OutputString(table->ConOut,(CHAR16*)s);}
static void num(UINT64 n){CHAR16 b[24];int k=0;do{b[k++]=L'0'+n%10;n/=10;}while(n&&k<23);for(int i=0;i<k/2;i++){CHAR16 q=b[i];b[i]=b[k-i-1];b[k-i-1]=q;}b[k]=0;say(b);}
static EFI_INPUT_KEY key(void){EFI_INPUT_KEY k;for(;;){if(!EFI_ERROR(table->ConIn->ReadKeyStroke(table->ConIn,&k)))return k;table->BootServices->Stall(20000);}}
EFI_STATUS _entry(EFI_HANDLE image,EFI_SYSTEM_TABLE*st)
{
 (void)image;table=st;st->BootServices->SetWatchdogTimer(0,0,0,0);st->ConOut->ClearScreen(st->ConOut);
#ifdef TMC_RESTORE_TOOL
 say(L"Ripristino ordine di avvio\r\n\r\nR: ripristina l'ordine precedente a tinymacOS.\r\nE: riabilita la preferenza OpenCore al prossimo avvio nativo.\r\nEsc: annulla senza modifiche.\r\n\r\nI file Microsoft e le voci Boot#### non verranno eliminati.\r\n");
 EFI_INPUT_KEY k=key();EFI_STATUS result=EFI_ABORTED;
 if(k.UnicodeChar==L'r'||k.UnicodeChar==L'R')result=boot_restore_default(st->RuntimeServices);
 else if(k.UnicodeChar==L'e'||k.UnicodeChar==L'E'){
  EFI_GUID vendor=TMC_BOOT_VENDOR;UINTN n=4;u32 v=0;
  result=st->RuntimeServices->GetVariable(L"tinymacOS-default-disabled",&vendor,0,&n,&v);
  if(!EFI_ERROR(result))result=st->RuntimeServices->SetVariable(L"tinymacOS-default-disabled",&vendor,7,0,0);
  else if(result==EFI_NOT_FOUND)result=EFI_SUCCESS;
 }
 if(result==EFI_ABORTED)return EFI_SUCCESS;
 say(EFI_ERROR(result)?L"\r\nOperazione NON verificata. Usa le impostazioni del firmware.\r\n":L"\r\nOperazione verificata. Nessun file Microsoft modificato.\r\nLe voci di dispositivi non piu presenti vengono ignorate.\r\n");
#else
 say(L"Diagnostica video UEFI (nessuna modifica)\r\nFirmware: ");say(st->FirmwareVendor);say(L"\r\n");
 EFI_GUID gg=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;EFI_GRAPHICS_OUTPUT_PROTOCOL*g=0;
 EFI_STATUS r=st->BootServices->HandleProtocol(st->ConsoleOutHandle,&gg,(void**)&g);
 if(EFI_ERROR(r))r=st->BootServices->LocateProtocol(&gg,0,(void**)&g);
 if(EFI_ERROR(r)){say(L"GOP assente.\r\n");goto end;}
 say(L"Modalita offerte dal firmware: ");num(g->Mode->MaxMode);say(L"  Corrente: ");num(g->Mode->Mode);say(L"\r\n");
 for(UINT32 i=0;i<g->Mode->MaxMode;i++){
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION*m=0;UINTN n=0;if(EFI_ERROR(g->QueryMode(g,i,&n,&m)))continue;
  if(m&&n>=sizeof *m){num(i);say(L": ");num(m->HorizontalResolution);say(L"x");num(m->VerticalResolution);say(L"  stride=");num(m->PixelsPerScanLine);say(L"  formato=");num(m->PixelFormat);say(gop_usable(m)?L"  compatibile\r\n":L"  non compatibile\r\n");}
  if(m)st->BootServices->FreePool(m);
  if((i+1)%14==0){say(L"Premi un tasto per proseguire, Esc per uscire.\r\n");if(key().ScanCode==SCAN_ESC)return EFI_SUCCESS;st->ConOut->ClearScreen(st->ConOut);}
 }
 /* LocateProtocol fallback is diagnostic only: with multiple GPUs EDID may
  * describe another output. Do not use it to invent or force a GOP mode. */
 {EFI_GUID edid={0xbd8c1056,0x9f36,0x44ec,{0x92,0xa8,0xa6,0x33,0x7f,0x81,0x79,0x86}};
  struct {UINT32 size;UINT8*bytes;}*e=0;
  if(!EFI_ERROR(st->BootServices->LocateProtocol(&edid,0,(void**)&e))&&e&&e->size>=128&&e->bytes){
   unsigned sum=0;for(int j=0;j<128;j++)sum+=e->bytes[j];
   if(!(sum&255)&&!memcmp(e->bytes,"\0\xff\xff\xff\xff\xff\xff\0",8)&&(e->bytes[54]||e->bytes[55])){
    say(L"EDID disponibile (verificare uscita): preferita ");num(e->bytes[56]|(e->bytes[58]&0xf0)<<4);say(L"x");num(e->bytes[59]|(e->bytes[61]&0xf0)<<4);say(L"\r\n");
   }else say(L"EDID presente, descrittore preferito non valido/disponibile.\r\n");
  }else say(L"EDID non esposto dal firmware.\r\n");}
 say(L"Non e' un test GPU: una modalita non offerta richiede diagnosi firmware/driver.\r\n");
end:
#endif
 say(L"\r\nPremi un tasto per tornare a OpenCore.\r\n");key();return EFI_SUCCESS;
}
