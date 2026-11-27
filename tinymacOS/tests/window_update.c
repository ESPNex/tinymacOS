#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "window_update.h"
int main(void){
 Draw old[32]={0},next[32]={0};old[0].type=next[0].type=C_TEXT;
 assert(window_commands_equal(old,32,next,32));assert(!window_commands_equal(old,31,next,32));
 next[1].x=1;assert(!window_commands_equal(old,32,next,32));next[1].x=0;
 strcpy(next[0].text,"changed");assert(!window_commands_equal(old,32,next,32));next[0].text[0]=0;memset(next[0].text,0,sizeof next[0].text);
 old[0].type=next[0].type=C_RASTER;assert(!window_commands_equal(old,32,next,32));
 old[0].type=next[0].type=C_TEXT;
 unsigned publications=0,reused=0;clock_t start=clock();
 for(unsigned frame=0;frame<1000;frame++)for(unsigned win=0;win<20;win++){
  publications++;reused+=window_commands_equal(old,32,next,32);
 }
 assert(publications==20000&&reused==publications);
 printf("PASS actual window dedup predicate: 20 windows x 1000 identical frames = %u avoided invalidations; changed geometry/text/count and raster always invalidate; CPU %.3f s\n",reused,(double)(clock()-start)/CLOCKS_PER_SEC);
}
