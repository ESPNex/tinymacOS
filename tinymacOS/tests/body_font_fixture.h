#pragma once
#include "../include/mono_wire.h"
static unsigned char font_fixture[65536];static int fixture_loaded,fixture_measures;
int TextWidth(const char*s,int face){
 fixture_measures++;
 if(!fixture_loaded){FILE*f=fopen("assets/type.tfn","rb");assert(f);assert(fread(font_fixture,1,sizeof font_fixture,f)>4096);fclose(f);fixture_loaded=1;}
 unsigned ng;memcpy(&ng,font_fixture+8,4);assert(ng==207);int width=0;
 while(*s){int used;unsigned c=mono_decode(s,strlen(s),&used);unsigned glyph=mono_reference(c)-1;unsigned short advance;memcpy(&advance,font_fixture+12+4*ng+face*(4+8*ng)+4+glyph*8,2);width+=advance;s+=used;}
 return width;
}
