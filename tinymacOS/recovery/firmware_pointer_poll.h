#pragma once
#include "firmware_pointer_health.h"
#include "firmware_pointer_scale.h"
/* Included by the Recovery runtime and its actual-protocol host contract. */
static void poll_pointer_sources(EFI_ABSOLUTE_POINTER_PROTOCOL *absolute,
    EFI_SIMPLE_POINTER_PROTOCOL **pointers,int *pointer_down,UINTN npointers)
{
        static EFI_ABSOLUTE_POINTER_PROTOCOL *last_absolute;
        static int abs_valid,abs_x,abs_y,abs_down;
        static EFI_SIMPLE_POINTER_PROTOCOL *simple_source[16];
        static RecPointerHealth simple_health[16],absolute_health;
        static RecPointerAxis simple_axes[16][2];
        int got_abs = 0;
        if (absolute && !got_abs) {
            EFI_ABSOLUTE_POINTER_STATE p;
            memset(&p,0,sizeof p);
            EFI_STATUS status=absolute->GetState(absolute,&p);
            if(rec_pointer_reset_due(&absolute_health,absolute,status))absolute->Reset(absolute,FALSE);
            if (!EFI_ERROR(status) && absolute->Mode &&
                absolute->Mode->AbsoluteMaxX>absolute->Mode->AbsoluteMinX &&
                absolute->Mode->AbsoluteMaxY>absolute->Mode->AbsoluteMinY) {
                EFI_ABSOLUTE_POINTER_MODE *m=absolute->Mode;
                int x=pointer_pixel(p.CurrentX,m->AbsoluteMinX,m->AbsoluteMaxX,rw);
                int y=pointer_pixel(p.CurrentY,m->AbsoluteMinY,m->AbsoluteMaxY,rh);
                int down=!!(p.ActiveButtons & EFI_ABSP_TouchActive);
                /* A firmware absolute device can return EFI_SUCCESS forever
                 * without fresh activity. It must not hide SimplePointer USB.
                 * Compare against that source, not the shared on-screen cursor. */
                if(last_absolute!=absolute || !abs_valid || x!=abs_x || y!=abs_y || down!=abs_down) {
                    mx=x;my=y;pointer_button(down);got_abs=1;dirty=1;
                }
                last_absolute=absolute;abs_valid=1;abs_x=x;abs_y=y;abs_down=down;
            }
        }
        /* A changing/invalid AbsolutePointer must not starve a real USB mouse.
         * Consume relative sources too; genuine relative activity wins this poll. */
        for(UINTN i=0;i<npointers;i++) {
            EFI_SIMPLE_POINTER_STATE p;EFI_SIMPLE_POINTER_PROTOCOL *pointer=pointers[i];
            memset(&p,0,sizeof p);
            EFI_STATUS status=pointer->GetState(pointer,&p);
            if(rec_pointer_reset_due(&simple_health[i],pointer,status))pointer->Reset(pointer,FALSE);
            if (!EFI_ERROR(status)) {
#ifdef TMC_DEBUG
                pointer_trace(i,pointer,&p);
#endif
                int down=!!p.LeftButton;
                if(i>=COUNT(simple_source))continue;
                if(simple_source[i]!=pointer){simple_source[i]=pointer;pointer_down[i]=0;memset(simple_axes[i],0,sizeof simple_axes[i]);}
                if(!p.RelativeMovementX && !p.RelativeMovementY && down==pointer_down[i])continue;
                pointer_down[i]=down;
                int dx=rec_pointer_pixels(&simple_axes[i][0],p.RelativeMovementX,pointer->Mode&&pointer->Mode->ResolutionX?pointer->Mode->ResolutionX:8);
                int dy=rec_pointer_pixels(&simple_axes[i][1],p.RelativeMovementY,pointer->Mode&&pointer->Mode->ResolutionY?pointer->Mode->ResolutionY:8);
                mx = MAX(0, MIN(rw - 1, (i64)mx + dx));
                my = MAX(0, MIN(rh - 1, (i64)my + dy));
                pointer_button(down);dirty=1;
            }
        }
}
