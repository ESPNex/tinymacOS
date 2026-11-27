#pragma once
/* Called only before ExitBootServices, after an installed system is verified.
 * Promote a dedicated OpenCore entry; NEVER overwrite/delete Microsoft files
 * or their boot variables. Retain the original BootOrder in our vendor GUID.
 */
#define TMC_BOOT_VENDOR {0x3f9a25c4,0x71b2,0x4e83,{0xa4,0xc1,0x5e,0x9d,0x27,0x8b,0x6c,0x10}}
static void boot_option_name(CHAR16 name[9],u16 id)
{
    const CHAR16 hex[]=L"0123456789ABCDEF";
    memcpy(name,L"Boot0000",18);for(int j=0;j<4;j++)name[4+j]=hex[(id>>(12-j*4))&15];
}
/* A physical device prefix may be expanded by firmware. Treat identical
 * HD()/File() tails as the same target; never match only a description. */
static UINTN boot_hd_tail(const u8*p,UINTN n){
 UINTN at=0,hd=n;while(at+4<=n){u16 z;memcpy(&z,p+at+2,2);if(z<4||z>n-at)return n;
  if(p[at]==4&&p[at+1]==1&&z==42)hd=at;
  if(p[at]==0x7f)return p[at+1]==0xff&&z==4&&at+4==n?hd:n;
  at+=z;
 }return n;
}
static int boot_option_matches(const u8*a,UINTN an,const u8*b,UINTN bn){
 if(an==bn&&!memcmp(a,b,an))return 1;if(an<8||bn<8||memcmp(a,b,4))return 0;
 UINTN ad=6,bd=6;while(ad+2<=an&&(a[ad]||a[ad+1]))ad+=2;while(bd+2<=bn&&(b[bd]||b[bd+1]))bd+=2;
 if(ad+2>an||bd+2>bn||ad!=bd||memcmp(a+6,b+6,ad-4))return 0;ad+=2;bd+=2;
 u16 al,bl;memcpy(&al,a+4,2);memcpy(&bl,b+4,2);if(ad+al!=an||bd+bl!=bn)return 0;
 UINTN x=boot_hd_tail(a+ad,al),y=boot_hd_tail(b+bd,bl);
 return x<al&&y<bl&&al-x==bl-y&&!memcmp(a+ad+x,b+bd+y,al-x);
}
static EFI_STATUS boot_set_default(EFI_RUNTIME_SERVICES*rs,const void*path,UINTN pathbytes)
{
    static u8 option[2048],check[2048],previous[512];
    EFI_GUID global=EFI_GLOBAL_VARIABLE,vendor=TMC_BOOT_VENDOR;
    const CHAR16 description[]=L"tinymacOS OpenCore";
    const UINT32 attrs=EFI_VARIABLE_NON_VOLATILE|EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS;
    u16 order[257],next[257],chosen=0;UINTN bytes=512;UINT32 orderattrs=attrs;
    if(!path||pathbytes<4||pathbytes>1024)return EFI_INVALID_PARAMETER;
    u32 disabled=0;UINTN ds=sizeof disabled;
    EFI_STATUS blocked=rs->GetVariable(L"tinymacOS-default-disabled",&vendor,0,&ds,&disabled);
    if(!EFI_ERROR(blocked)){if(ds!=sizeof disabled||disabled>1)return EFI_ACCESS_DENIED;if(disabled)return EFI_SUCCESS;}
    else if(blocked!=EFI_NOT_FOUND)return blocked;
    EFI_STATUS status=rs->GetVariable(L"BootOrder",&global,&orderattrs,&bytes,order);
    if(status==EFI_NOT_FOUND){bytes=0;orderattrs=attrs;}
    else if(EFI_ERROR(status)||bytes%2||bytes>512)return EFI_INVALID_PARAMETER;
    if(!(orderattrs&EFI_VARIABLE_NON_VOLATILE))return EFI_UNSUPPORTED;
    UINTN original=bytes;memcpy(previous,order,bytes);
    UINTN optionbytes=6+sizeof description+pathbytes;memset(option,0,optionbytes);
    u32 active=1;memcpy(option,&active,4);u16 plen=pathbytes;memcpy(option+4,&plen,2);
    memcpy(option+6,description,sizeof description);memcpy(option+6+sizeof description,path,pathbytes);
    int found=0,created=0,existing=0;
    for(UINTN j=0;j<bytes/2;j++){
        CHAR16 name[9];boot_option_name(name,order[j]);UINTN n=sizeof check;
        if(!EFI_ERROR(rs->GetVariable(name,&global,0,&n,check))&&boot_option_matches(check,n,option,optionbytes)){chosen=order[j];found=existing=1;break;}
    }
    if(found&&bytes&&order[0]==chosen)return EFI_SUCCESS;
    /* Never replace a variable owned by another bootloader, even if it is not
     * currently listed in BootOrder. A full variable namespace fails safely. */
    if(!found){
        for(u32 id=0;id<=65535;id++){
            CHAR16 name[9];boot_option_name(name,id);UINTN n=sizeof check;
            status=rs->GetVariable(name,&global,0,&n,check);
            if(status==EFI_NOT_FOUND){chosen=id;found=1;break;}
            if(!EFI_ERROR(status)&&boot_option_matches(check,n,option,optionbytes)){chosen=id;found=existing=1;break;}
            if(EFI_ERROR(status)&&status!=EFI_BUFFER_TOO_SMALL)return status;
        }
        if(!found)return EFI_OUT_OF_RESOURCES;
    }
    if(!existing&&bytes==512)return EFI_OUT_OF_RESOURCES;
    /* Preserve the very first BootOrder. Existing backups are never replaced;
     * inability to save one prevents ALL boot-order/entry changes. */
    UINTN saved=sizeof check;status=rs->GetVariable(L"tinymacOS-previousBootOrder",&vendor,0,&saved,check);
    if(status==EFI_NOT_FOUND){
        u8 backup[520];memcpy(backup,"TMCBOOT2",8);memcpy(backup+8,previous,original);
        status=rs->SetVariable(L"tinymacOS-previousBootOrder",&vendor,attrs,original+8,backup);
        if(EFI_ERROR(status))return status;
        saved=sizeof check;
        if(EFI_ERROR(rs->GetVariable(L"tinymacOS-previousBootOrder",&vendor,0,&saved,check))||saved!=original+8||memcmp(check,backup,saved))return EFI_DEVICE_ERROR;
    }else if(EFI_ERROR(status)||saved<8||memcmp(check,"TMCBOOT2",8))return EFI_ACCESS_DENIED;
    if(!found)return EFI_NOT_FOUND;
    /* Reuse an identical existing option; otherwise fill only a free number. */
    CHAR16 name[9];boot_option_name(name,chosen);UINTN n=sizeof check;
    status=rs->GetVariable(name,&global,0,&n,check);
    if(status==EFI_NOT_FOUND){status=rs->SetVariable(name,&global,attrs,optionbytes,option);if(EFI_ERROR(status))return status;created=1;}
    else if(EFI_ERROR(status)||!boot_option_matches(check,n,option,optionbytes))return EFI_ACCESS_DENIED;
    n=sizeof check;
    if(EFI_ERROR(rs->GetVariable(name,&global,0,&n,check))||!boot_option_matches(check,n,option,optionbytes)){
        if(created)rs->SetVariable(name,&global,attrs,0,0);return EFI_DEVICE_ERROR;
    }
    UINTN count=1;next[0]=chosen;
    for(UINTN j=0;j<original/2;j++)if(order[j]!=chosen)next[count++]=order[j];
    status=rs->SetVariable(L"BootOrder",&global,orderattrs,count*2,next);
    n=sizeof check;
    if(EFI_ERROR(status)||EFI_ERROR(rs->GetVariable(L"BootOrder",&global,0,&n,check))||n!=count*2||memcmp(check,next,n)){
        /* A firmware write may succeed despite a failing readback (or even
         * an error return). Restore first; never delete a referenced option. */
        EFI_STATUS failure=EFI_ERROR(status)?status:EFI_DEVICE_ERROR;
        rs->SetVariable(L"BootOrder",&global,orderattrs,original,previous);
        UINTN z=sizeof check;EFI_STATUS readback=rs->GetVariable(L"BootOrder",&global,0,&z,check);
        int restored=(!original?readback==EFI_NOT_FOUND:
            !EFI_ERROR(readback)&&z==original&&!memcmp(check,previous,original));
        if(created&&restored)rs->SetVariable(name,&global,attrs,0,0);
        return failure;
    }
    return EFI_SUCCESS;
}

/* Explicit user-confirmed rollback. Only BootOrder and our vendor flag change;
 * every Boot#### option, Microsoft EFI file and backup remain untouched. */
static EFI_STATUS boot_restore_default(EFI_RUNTIME_SERVICES*rs)
{
 EFI_GUID global=EFI_GLOBAL_VARIABLE,vendor=TMC_BOOT_VENDOR;
 const u32 attrs=EFI_VARIABLE_NON_VOLATILE|EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS;
 u8 backup[520],check[520];UINTN n=sizeof backup;
 EFI_STATUS status=rs->GetVariable(L"tinymacOS-previousBootOrder",&vendor,0,&n,backup);
 if(EFI_ERROR(status))return status;
 if(n<8||n>sizeof backup||(n-8)%2||memcmp(backup,"TMCBOOT2",8))return EFI_ACCESS_DENIED;
 UINTN size=n-8,kept=0;u16 ids[256];memcpy(ids,backup+8,size);
 for(UINTN j=0;j<size/2;j++){
  CHAR16 name[9];boot_option_name(name,ids[j]);UINTN z=0;
  status=rs->GetVariable(name,&global,0,&z,0);
  /* Removed installer USB/CD options are routinely garbage-collected by
   * firmware after the first ISO-detached boot. Do not recreate phantom
   * devices or refuse restoration of the still-present Windows entry. */
  if(status==EFI_NOT_FOUND)continue;
  if(status!=EFI_BUFFER_TOO_SMALL&&EFI_ERROR(status))return status;
  ids[kept++]=ids[j];
 }
 if(size&&!kept)return EFI_NOT_FOUND;
 size=kept*2;
 u32 old=0;UINTN z=sizeof old;int had=0;
 status=rs->GetVariable(L"tinymacOS-default-disabled",&vendor,0,&z,&old);
 if(!EFI_ERROR(status)){if(z!=4||old>1)return EFI_ACCESS_DENIED;had=1;}
 else if(status!=EFI_NOT_FOUND)return status;
 u32 one=1;status=rs->SetVariable(L"tinymacOS-default-disabled",&vendor,attrs,4,&one);if(EFI_ERROR(status))return status;
 z=sizeof one;one=0;
 if(EFI_ERROR(rs->GetVariable(L"tinymacOS-default-disabled",&vendor,0,&z,&one))||z!=4||one!=1)return EFI_DEVICE_ERROR;
 status=rs->SetVariable(L"BootOrder",&global,attrs,size,ids);
 if(EFI_ERROR(status)){rs->SetVariable(L"tinymacOS-default-disabled",&vendor,attrs,had?4:0,had?&old:0);return status;}
 z=sizeof check;status=rs->GetVariable(L"BootOrder",&global,0,&z,check);
 if(!size)return status==EFI_NOT_FOUND?EFI_SUCCESS:EFI_DEVICE_ERROR;
 return !EFI_ERROR(status)&&z==size&&!memcmp(check,ids,size)?EFI_SUCCESS:EFI_DEVICE_ERROR;
}

/* A new explicitly confirmed installation may supersede a prior user rollback.
 * Do not use this for ordinary native boots or a Recovery system reinstall. */
static EFI_STATUS boot_install_default(EFI_RUNTIME_SERVICES*rs,const void*path,UINTN bytes){
 EFI_GUID vendor=TMC_BOOT_VENDOR;u32 value=0;UINTN n=4;
 EFI_STATUS status=rs->GetVariable(L"tinymacOS-default-disabled",&vendor,0,&n,&value);
 int disabled=!EFI_ERROR(status)&&n==4&&value==1;
 if(!EFI_ERROR(status)&&(n!=4||value>1))return EFI_ACCESS_DENIED;
 if(EFI_ERROR(status)&&status!=EFI_NOT_FOUND)return status;
 if(disabled){status=rs->SetVariable(L"tinymacOS-default-disabled",&vendor,7,0,0);if(EFI_ERROR(status))return status;
  n=4;status=rs->GetVariable(L"tinymacOS-default-disabled",&vendor,0,&n,&value);
  if(status!=EFI_NOT_FOUND)return EFI_DEVICE_ERROR;
 }
 status=boot_set_default(rs,path,bytes);
 if(EFI_ERROR(status)&&disabled){value=1;rs->SetVariable(L"tinymacOS-default-disabled",&vendor,7,4,&value);}
 return status;
}
