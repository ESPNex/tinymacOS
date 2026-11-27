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
        if(!EFI_ERROR(rs->GetVariable(name,&global,0,&n,check))&&n==optionbytes&&!memcmp(check,option,n)){chosen=order[j];found=existing=1;break;}
    }
    if(found&&bytes&&order[0]==chosen)return EFI_SUCCESS;
    /* Never replace a variable owned by another bootloader, even if it is not
     * currently listed in BootOrder. A full variable namespace fails safely. */
    if(!found){
        for(u32 id=0;id<=65535;id++){
            CHAR16 name[9];boot_option_name(name,id);UINTN n=sizeof check;
            status=rs->GetVariable(name,&global,0,&n,check);
            if(status==EFI_NOT_FOUND){chosen=id;found=1;break;}
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
    else if(EFI_ERROR(status)||n!=optionbytes||memcmp(check,option,n))return EFI_ACCESS_DENIED;
    n=sizeof check;
    if(EFI_ERROR(rs->GetVariable(name,&global,0,&n,check))||n!=optionbytes||memcmp(check,option,n)){
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
 UINTN size=n-8;u16 ids[256];memcpy(ids,backup+8,size);
 for(UINTN j=0;j<size/2;j++){
  CHAR16 name[9];boot_option_name(name,ids[j]);UINTN z=0;
  status=rs->GetVariable(name,&global,0,&z,0);
  if(status!=EFI_BUFFER_TOO_SMALL&&EFI_ERROR(status))return EFI_NOT_FOUND;
 }
 u32 old=0;UINTN z=sizeof old;int had=0;
 status=rs->GetVariable(L"tinymacOS-default-disabled",&vendor,0,&z,&old);
 if(!EFI_ERROR(status)){if(z!=4||old>1)return EFI_ACCESS_DENIED;had=1;}
 else if(status!=EFI_NOT_FOUND)return status;
 u32 one=1;status=rs->SetVariable(L"tinymacOS-default-disabled",&vendor,attrs,4,&one);if(EFI_ERROR(status))return status;
 z=sizeof one;one=0;
 if(EFI_ERROR(rs->GetVariable(L"tinymacOS-default-disabled",&vendor,0,&z,&one))||z!=4||one!=1)return EFI_DEVICE_ERROR;
 status=rs->SetVariable(L"BootOrder",&global,attrs,size,backup+8);
 if(EFI_ERROR(status)){rs->SetVariable(L"tinymacOS-default-disabled",&vendor,attrs,had?4:0,had?&old:0);return status;}
 z=sizeof check;status=rs->GetVariable(L"BootOrder",&global,0,&z,check);
 if(!size)return status==EFI_NOT_FOUND?EFI_SUCCESS:EFI_DEVICE_ERROR;
 return !EFI_ERROR(status)&&z==size&&!memcmp(check,backup+8,size)?EFI_SUCCESS:EFI_DEVICE_ERROR;
}
