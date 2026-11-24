#pragma once
/* Only explicit firmware errors trigger a soft Reset of that pointer protocol.
 * EFI_NOT_READY is normal idle, never evidence of a dead mouse. No controller,
 * hub, keyboard or claimed UsbIo interface is disconnected by this policy. */
typedef struct { const void *source; unsigned errors, cooldown; } RecPointerHealth;
static inline int rec_pointer_reset_due(RecPointerHealth *h,const void *source,EFI_STATUS status)
{
    if(h->source!=source){h->source=source;h->errors=h->cooldown=0;}
    if(h->cooldown)h->cooldown--;
    if(!EFI_ERROR(status)){h->errors=0;return 0;}
    if(status==EFI_NOT_READY)return 0;
    if(h->errors<3)h->errors++;
    if(h->errors<3||h->cooldown)return 0;
    h->errors=0;h->cooldown=170;return 1;
}
