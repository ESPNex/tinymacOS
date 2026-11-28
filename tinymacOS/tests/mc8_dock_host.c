/* Exercise the actual shell chrome helper with actual glass/shadow pixels. */
#include <assert.h>
#include <stdio.h>
#include "../desktop/desk.h"
#include "../desktop/shell.c"
SysInfo sys;int mx=630,my=20;
u64 backdrop_key(Rect q){(void)q;return 123;} /* even a reused/colliding scene key cannot replay raw pixels */
static u32 expected[640*480];
static void fill(u32 c){for(int i=0;i<W*H;i++)canvas[i]=c;}
int main(void){
 W=640;H=480;resetclip();
 Rect docks[]={{150,390,340,69},{10,60,68,350}};
 for(int dark=0;dark<2;dark++)for(int reduced=0;reduced<2;reduced++)for(unsigned i=0;i<2;i++){
  night=dark;lite=reduced;Rect q=docks[i];
  fill(0xddeeff);chrome(1,q,23,MAT_DOCK);
  /* An earlier bright window lies in the padded capture. Next scene is dark. */
  fill(0x192536);shadow(q,23,52);glass(q,23,MAT_DOCK);
  for(int k=0;k<W*H;k++)expected[k]=canvas[k];
  fill(0x192536);chrome(1,q,23,MAT_DOCK);
  for(int k=0;k<W*H;k++)assert(canvas[k]==expected[k]);
  for(int y=0;y<q.y-16;y++)for(int x=0;x<W;x++)assert(canvas[y*W+x]==0x192536);
 }
 puts("PASS actual Dock chrome: changed scene with identical tile key equals fresh shadow+glass pixel-for-pixel; no rectangular old-window padding replay; bottom/side Dock, light/dark/reduce-transparency");
}
