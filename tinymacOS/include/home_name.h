#pragma once
#include "text_input.h"
#include "sha256.h"
/* Account display names are UTF-8 path components, never paths. Keep exact
 * spelling, spaces and accents; do not create a silently sanitized identity. */
static inline int home_name_valid(const char *s)
{
 int n=text_field_length(s,32);if(n<1||eq(s,".")||eq(s,".."))return 0;
 for(int i=0;i<n;i++)if(s[i]=='/'||s[i]=='\\')return 0;
 return 1;
}
static inline int home_account_name(const u8 a[512],char out[48])
{
 u32 version,rounds;u8 hash[32];memcpy(&version,a+8,4);memcpy(&rounds,a+12,4);
 if(memcmp(a,"TMCUSER2",8)||version!=1||rounds!=120000||!home_name_valid((const char *)a+16))return 0;
 sha_bytes(a,480,hash);int ok=!memcmp(hash,a+480,32);if(ok)strset(out,(const char *)a+16,48);erase(hash,32);return ok;
}
