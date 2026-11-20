#!/usr/bin/env python3
"""Compile real video.c and pointer scaling with host sanitizers; inject stale scanout."""
import subprocess,tempfile,json
from pathlib import Path
r=Path('/home/user/tinymacOS');w=Path(__file__).resolve().parents[1];tmp=Path(tempfile.mkdtemp(prefix='tmc-243-host-',dir='/var/tmp'))
shim='''#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "handoff.h"
typedef uint32_t u32;typedef uint64_t u64;typedef uint16_t u16;
#define MIN(a,b) ((a)<(b)?(a):(b))
struct Boot boot;u64 ticks;
static void outw(int a,int b){(void)a;(void)b;}
static int inw(int a){(void)a;return 0;}
'''
main='''
#include "pointer_scale.h"
static u32 src[1920*1080],fb[1936*1080];
static u32 scan(u32 v){return boot.rgb?((v&255)<<16)|(v&0xff00)|((v>>16)&255):v;}
static void check(void){for(unsigned y=0;y<boot.h;y++)for(unsigned x=0;x<boot.w;x++){
 unsigned sx=(u64)x*1280/boot.w,sy=(u64)y*800/boot.h;
 assert(fb[y*boot.stride+x]==scan(src[sy*1280+sx]));}
 for(unsigned y=0;y<boot.h;y++)for(unsigned x=boot.w;x<boot.stride;x++)assert(fb[y*boot.stride+x]==0x12345678);
}
int main(void){
 for(int i=0;i<1280*800;i++)src[i]=(i*137u)&0xffffff;
 for(int scaled=0;scaled<2;scaled++)for(int rgb=0;rgb<2;rgb++){
  boot=(struct Boot){.fb=(u64)fb,.w=scaled?1920:1280,.h=scaled?1080:800,.stride=scaled?1936:1296,.rgb=rgb};
  for(int i=0;i<1936*1080;i++)fb[i]=0x12345678;
  first=1;ticks=0;last_refresh=0;video_present(src,0,0,1280,800,1280,800);check();
  /* Same content: the old cache never notices externally stale pixels. */
  for(int y=490;y<494;y++)for(int x=510;x<750;x++)fb[y*boot.stride+x]=0xffffff;
  ticks=99;video_present(src,0,0,1280,800,1280,800);
  assert(fb[490*boot.stride+510]==0xffffff);
  ticks=100;video_present(src,4,4,26,26,1280,800);check();
  /* Full repair must promote even a cursor-only rect; next frame cached. */
  ticks=101;video_present(src,0,0,1280,800,1280,800);check();
 }
 int dims[][2]={{1280,1920},{720,1080},{1280,1280},{800,1280},{1280,1600}};
 for(unsigned k=0;k<sizeof dims/sizeof*dims;k++)for(int sign=-1;sign<=1;sign+=2){
  int rem=0,total=0;long long raw=0;
  for(int i=0;i<10000;i++){int delta=sign*(1+i%9);raw+=delta;total+=pointer_scale_axis(delta,dims[k][0],dims[k][1],&rem);
   assert(total==raw*dims[k][0]/dims[k][1]);assert(abs(rem)<dims[k][1]);}
  for(int i=9999;i>=0;i--){int delta=-sign*(1+i%9);raw+=delta;total+=pointer_scale_axis(delta,dims[k][0],dims[k][1],&rem);}
  assert(total==0&&rem==0);
 }
 int rem=0,total=0;for(int i=0;i<30;i++)total+=pointer_scale_axis(1,1280,1920,&rem);assert(total==20);
 puts("PASS: scanout corruption repaired in 100 ticks, partial promotion, RGB/BGR, padded stride, scaled/unscaled; signed subpixel conservation");
}
'''
video=(r/'drivers/video.c').read_text();assert video.startswith('#include "../kernel/k.h"')
source=shim+video.split('\n',1)[1]+main
c=tmp/'test.c';c.write_text(source);exe=tmp/'test'
subprocess.run(['gcc','-std=gnu11','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-I'+str(r/'include'),str(c),'-o',str(exe)],check=True)
p=subprocess.run([str(exe)],text=True,capture_output=True);print(p.stdout,p.stderr);assert p.returncode==0
# Negative comparison: the released 2.4.2 video source is saved before patching.
baseline=w/'qa/video-242-baseline.c'
if baseline.exists():
 c.write_text(shim+baseline.read_text().split('\n',1)[1]+main.replace('last_refresh=0;',''))
 subprocess.run(['gcc','-std=gnu11','-O1','-no-pie','-I'+str(r/'include'),str(c),'-o',str(exe)],check=True)
 old=subprocess.run([str(exe)],capture_output=True,text=True);assert old.returncode!=0
else:old=None
(w/'evidence/host-regressions.json').write_text(json.dumps({'pass':True,'sanitizers':['ASan','UBSan'],'scanout_injection':['BGR unscaled padded stride','RGB unscaled padded stride','BGR scaled padded stride','RGB scaled padded stride'],'repair_without_mouse':True,'repair_on_partial_promoted_full':True,'signed_subpixel_reports_per_direction_per_geometry':10000,'geometries':5,'baseline_242_fails_injected_corruption':old is not None,'note':'Host RAM scanout simulation, not physical GPU validation.'},indent=2))
