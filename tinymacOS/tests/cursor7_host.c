#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/abi.h"
#include "../include/cursor_format.h"
#include "../include/window_interaction.h"
static CursorImage image;
static unsigned char bytes[1024*1024],copy[1024*1024];
static unsigned readasset(const char*name){char p[256];snprintf(p,sizeof p,"assets/cursors/%s",name);FILE*f=fopen(p,"rb");assert(f);unsigned n=fread(bytes,1,sizeof bytes,f);fclose(f);return n;}
static void put32(u8*p,u32 n){for(int i=0;i<4;i++)p[i]=n>>(8*i);}
int main(void)
{
 const char*names[]={"Normal.cur","Text.cur","Select.cur","Move.cur","Horizontal Resize.cur","Vertical Resize.cur","Diagonal Resize 1.cur","Diagonal Resize 2.cur","Unavailable.cur","Busy.ani","Working.ani","Help.cur","Precision.cur","Handwriting.cur","Alternate.cur","Location.cur","Person.cur"};
 for(unsigned i=0;i<COUNT(names);i++){
  unsigned n=readasset(names[i]);assert(cursor_decode(bytes,n,&image));assert(image.frames==(i==9||i==10?30:1));
  for(unsigned f=0;f<image.frames;f++){CursorFrame*q=image.frame+f;assert(q->hx>=0&&q->hx<32&&q->hy>=0&&q->hy<32);int visible=0;for(int k=0;k<1024;k++)visible+=!!(q->rgba[k]>>24);assert(visible>5&&visible<1024);}
  if(image.frames>1){assert(image.steps==30&&image.duration==30);assert(cursor_frame(&image,0)==0&&cursor_frame(&image,2)==1&&cursor_frame(&image,50)==0);assert(memcmp(image.frame[0].rgba,image.frame[1].rgba,sizeof image.frame[0].rgba));}
  for(unsigned cut=0;cut<n;cut+=37)assert(!cursor_decode(bytes,cut,&image));
  memcpy(copy,bytes,n);for(unsigned j=0;j<250;j++){unsigned at=(j*7919u)%n;u8 old=bytes[at];bytes[at]^=0xff;cursor_decode(bytes,n,&image);bytes[at]=old;}assert(!memcmp(copy,bytes,n));
 }
 unsigned n=readasset("Normal.cur");put32(bytes+18,0xfffffff0);assert(!cursor_decode(bytes,n,&image));
 n=readasset("Busy.ani");put32(bytes+24,65);assert(!cursor_decode(bytes,n,&image));
 /* Synthetic ANI checks optional reordered seq/rate and mixed frame duration,
  * not just the supplied default-rate animation. */
 n=readasset("Normal.cur");unsigned curbytes=n;memcpy(copy,bytes,n);memset(bytes,0,256);memcpy(bytes,"RIFF",4);memcpy(bytes+8,"ACONanih",8);put32(bytes+16,36);put32(bytes+20,36);put32(bytes+24,2);put32(bytes+28,3);put32(bytes+48,6);put32(bytes+52,3);
 unsigned p=56;memcpy(bytes+p,"seq ",4);put32(bytes+p+4,12);put32(bytes+p+8,1);put32(bytes+p+12,0);put32(bytes+p+16,1);p+=20;
 memcpy(bytes+p,"rate",4);put32(bytes+p+4,12);put32(bytes+p+8,6);put32(bytes+p+12,12);put32(bytes+p+16,18);p+=20;
 memcpy(bytes+p,"LIST",4);put32(bytes+p+4,4+2*(8+curbytes));memcpy(bytes+p+8,"fram",4);p+=12;
 for(int f=0;f<2;f++){memcpy(bytes+p,"icon",4);put32(bytes+p+4,curbytes);memcpy(bytes+p+8,copy,curbytes);p+=8+curbytes;}
 put32(bytes+4,p-8);assert(cursor_decode(bytes,p,&image)&&image.duration==36);assert(cursor_frame(&image,0)==1&&cursor_frame(&image,10)==0&&cursor_frame(&image,30)==1&&cursor_frame(&image,60)==1);
 put32(bytes+56+8,2);assert(!cursor_decode(bytes,p,&image));
 assert(window_edge(0,0,600,400)==(EDGE_LEFT|EDGE_TOP));assert(window_edge(599,399,600,400)==(EDGE_RIGHT|EDGE_BOTTOM));assert(!window_edge(100,100,600,400));assert(!window_edge(-1,100,600,400));
 for(int e=1;e<16;e++){if((e&3)==3||(e&12)==12)continue;
  for(int dx=-2000;dx<=2000;dx+=50)for(int dy=-2000;dy<=2000;dy+=50){int x,y,w,h;window_resize_geometry(&x,&y,&w,&h,100,100,600,400,e,dx,dy,460,286,1280,800,34);assert(x>=0&&y>=34&&w>=460&&h>=286&&x+w<=1280&&y+h<=800);if(e&EDGE_LEFT)assert(x+w==700);if(e&EDGE_TOP)assert(y+h==500);}
 }
 puts("PASS M7 actual 17 CUR/ANI decodes/hotspots/alpha/frame changes; truncation/mutations/offset overflow rejected, seq/rate timing and 8-direction resize anchors/clamps");
}
