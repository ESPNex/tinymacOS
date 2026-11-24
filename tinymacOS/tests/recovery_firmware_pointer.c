/* Compile the actual runtime polling routine with EFI protocol fixtures. */
#include "../recovery/screen.h"
#include "hidtablet.h"
#include <assert.h>
#include <stdio.h>
int rw=1280,rh=800;
static int mx=100,my=100,dirty,button,rel_reads,resets,abs_changes,abs_calls;
static EFI_STATUS relative_status=EFI_SUCCESS;
static EFI_SIMPLE_POINTER_STATE rel={.RelativeMovementX=1};
static EFI_SIMPLE_POINTER_MODE mode={.ResolutionX=8,.ResolutionY=8};
static EFI_ABSOLUTE_POINTER_MODE amode={.AbsoluteMaxX=1000,.AbsoluteMaxY=1000};
static void pointer_button(int down){button=down;}
static EFI_STATUS EFIAPI rstate(EFI_SIMPLE_POINTER_PROTOCOL *p,EFI_SIMPLE_POINTER_STATE *s){(void)p;rel_reads++;*s=rel;return relative_status;}
static EFI_STATUS EFIAPI reset(EFI_SIMPLE_POINTER_PROTOCOL *p,BOOLEAN extended){(void)p;assert(!extended);resets++;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI astate(EFI_ABSOLUTE_POINTER_PROTOCOL *p,EFI_ABSOLUTE_POINTER_STATE *s){(void)p;*s=(EFI_ABSOLUTE_POINTER_STATE){.CurrentX=400+(abs_changes?abs_calls++:0),.CurrentY=300};return EFI_SUCCESS;}
static EFI_STATUS EFIAPI areset(EFI_ABSOLUTE_POINTER_PROTOCOL *p,BOOLEAN extended){(void)p;assert(!extended);return EFI_SUCCESS;}
#include "../recovery/firmware_pointer_poll.h"
int main(void)
{
    EFI_SIMPLE_POINTER_PROTOCOL rp={.GetState=rstate,.Reset=reset,.Mode=&mode};
    EFI_SIMPLE_POINTER_PROTOCOL *ptrs[]={&rp};int down[1]={0};
    EFI_ABSOLUTE_POINTER_PROTOCOL ap={.GetState=astate,.Reset=areset,.Mode=0};
    poll_pointer_sources(&ap,ptrs,down,1);assert(mx==101&&my==100&&rel_reads==1);
    puts("PASS invalid AbsolutePointer does not starve the firmware USB mouse");
    ap.Mode=&amode;abs_changes=1;int before=rel_reads;
    for(int i=0;i<100;i++)poll_pointer_sources(&ap,ptrs,down,1);
    assert(rel_reads==before+100);assert(mx==pointer_pixel(499,0,1000,1280)+1);
    puts("PASS changing absolute source cannot hide independent relative input");
    mx=100;my=100;for(int i=0;i<200;i++)poll_pointer_sources(0,ptrs,down,1);assert(mx==300&&my==100);
    mode.ResolutionX=mode.ResolutionY=0;rel.RelativeMovementX=-1;rel.RelativeMovementY=1;
    for(int i=0;i<200;i++)poll_pointer_sources(0,ptrs,down,1);assert(mx==100&&my==300);
    mode.ResolutionX=mode.ResolutionY=65536;rel.RelativeMovementX=8192;rel.RelativeMovementY=0;
    for(int i=0;i<100;i++)poll_pointer_sources(0,ptrs,down,1);assert(mx==200&&my==300);
    puts("PASS zero-resolution compatibility and OVMF 8192-count steps normalize without edge pinning");
    rel.RelativeMovementX=rel.RelativeMovementY=0;relative_status=EFI_NOT_READY;before=resets;
    for(int i=0;i<1000;i++)poll_pointer_sources(0,ptrs,down,1);assert(resets==before);
    relative_status=EFI_DEVICE_ERROR;
    for(int i=0;i<3;i++)poll_pointer_sources(0,ptrs,down,1);assert(resets==before+1);
    for(int i=0;i<100;i++)poll_pointer_sources(0,ptrs,down,1);assert(resets==before+1);
    relative_status=EFI_SUCCESS;mode.ResolutionX=8;rel.RelativeMovementX=4;rel.LeftButton=TRUE;
    poll_pointer_sources(0,ptrs,down,1);assert(mx==204&&button);
    rel.LeftButton=FALSE;poll_pointer_sources(0,ptrs,down,1);assert(!button);
    puts("PASS explicit firmware errors soft-reset only the failed pointer with backoff; idle never resets; resumed movement/click works");
}
