#pragma once
#include "mono_wire.h"
/* Exactly the compositor's repertoire and F_BODY advances, not byte counts,
 * estimated average widths or hardcoded font-family metrics. */
int BodyWidth(const char *s,int bytes)
{
 /* The sealed interface font cannot change during a process lifetime. Cache
  * each real repertoire advance once: wrapping a large document stays O(n),
  * without reopening files or searching the font table for every character. */
 static u16 advances[208];int width=0;
 while(bytes>0&&*s){int used;unsigned code=mono_decode(s,bytes,&used);int tab=code=='\t';
  unsigned ref=mono_reference(tab?' ':code);int adv=advances[ref];
  if(!adv){char glyph[5];code=mono_code(ref-1);int z=text_encode(code,glyph);glyph[z]=0;
   adv=TextWidth(glyph,F_BODY);if(adv>0&&adv<65536)advances[ref]=adv;else adv=8;
  }
  width+=adv*(tab?4:1);s+=used;bytes-=used;
 }
 return width;
}
int BodyOffset(const char*s,int start,int end,int pixel)
{
 int x=0;while(start<end){int next=text_next(s,start);int adv=BodyWidth(s+start,next-start);
  if(pixel<x+(adv+1)/2)break;x+=adv;start=next;
 }return start;
}
