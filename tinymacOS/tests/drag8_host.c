/* Actual drawcursor save/restore and actual icon atlas, including edge clips. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../desktop/main.c"
const u8 settings_svg_masks[24576]={0};
static u32 scene[128*96],expected[128*96];
int main(void){
 W=128;H=96;resetclip();
 for(int kind=0;kind<2;kind++)for(int badge=0;badge<2;badge++)for(int pos=0;pos<4;pos++){
  for(int y=0;y<H;y++)for(int x=0;x<W;x++)canvas[y*W+x]=scene[y*W+x]=((x*3)<<16)|((y*2)<<8)|37;
  mx=pos&1?W-2:pos==2?0:40;my=pos&2?H-1:20;pointer_kind=-1;cursorValid=0;
  filedrag.started=filedrag.ready=1;filedrag.copy=badge;filedrag.icon=kind?7:6;
  /* Independent expected composition of the real source image + badge. */
  icon(mx+10,my+10,48,filedrag.icon);
  if(badge){rect(mx+48,my+48,14,14,0x2eaa65);rect(mx+54,my+51,2,8,0xffffff);rect(mx+51,my+54,8,2,0xffffff);}
  icon(mx-3,my-2,26,15);memcpy(expected,canvas,W*H*4);memcpy(canvas,scene,W*H*4);
  drawcursor();assert(!memcmp(canvas,expected,W*H*4));assert(curw*curh<=96*96);undocursor();assert(!memcmp(canvas,scene,W*H*4));
 }
 puts("PASS actual drag preview: original folder/file icon atlas, copy badge, all clipped edges, exact composition and complete damage restoration; no generic square");
}
