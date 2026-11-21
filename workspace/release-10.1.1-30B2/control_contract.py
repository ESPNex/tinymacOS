#!/usr/bin/env python3
"""ASan/UBSan execution of shipping control.c, geometry and input transitions."""
from pathlib import Path
import subprocess,tempfile,json,hashlib
home=Path(__file__).resolve().parents[2];root=home/'tinymacOS'
code=r'''
#include <assert.h>
#include <stdio.h>
#include "desk.h"
#include "control.h"
#undef sc
static int calls,lastkey,lastval,fail,launched,overlay,mission,acted,tiles;
SysInfo sys;int W=1280,H=800,mx,my,dirty,lite;
static long fake(long n,int a,int b,int c,int d){(void)c;(void)d;assert(n==S_PREF);assert(a>=0&&a<=5);calls++;lastkey=a;lastval=b;if(fail)return -1;
 if(a==0)sys.theme=b;if(a==1)sys.wall=b;if(a==2)sys.dock=b;if(a==3)sys.reduce_motion=b;if(a==4)sys.opaque=b;if(a==5)sys.dark=b;return 0;}
#define sc(n,a,b,c,d) fake(n,a,b,c,d)
void togglepref(int a){fake(S_PREF,a,a==1?(sys.wall+1)%NWALL:a==2?!sys.dock:a==3?!sys.reduce_motion:!sys.opaque,0,0);}
void overlay_show(int n){overlay=n;}
void mc_enter(void){mission++;}
void launch(int n){launched=n;}
void action(int n){acted=n;}
void shadow(Rect b,int r,int s){(void)b;(void)r;(void)s;}
void glass(Rect b,int r,int k){assert(b.w>0&&b.h>0);assert(r>0&&r<=60);assert(k==MAT_DOCK);tiles++;}
void roundr(int a,int b,int c,int d,int e,u32 f,int g){(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;}
void stroke(Rect a,int b,u32 c,int d){(void)a;(void)b;(void)c;(void)d;}
void line(int x,int y,int X,int Y,u32 c){(void)x;(void)y;(void)X;(void)Y;(void)c;}
void icon(int x,int y,int s,int id){(void)x;(void)y;assert(s>0&&s<=128);assert(id<0&&id>=-42);}
void text(int x,int y,const char *s,int f,u32 c){(void)x;(void)y;(void)s;(void)f;(void)c;}
void center(int x,int y,const char*s,int f,u32 c){text(x,y,s,f,c);}
int textw(const char*s,int f){(void)f;return (int)strlen(s)*7;}
void photo(Rect b,int w,int r){assert(b.w>0&&b.h>0&&w>=0&&w<NWALL&&r>0);}
#include "control.c"
static void hit(Rect p,int id){Rect b=control_rect(p,id);control_click(p,b.x+b.w/2,b.y+b.h/2);}
int main(void){
 int sizes[][2]={{800,600},{1024,600},{1024,768},{1280,800},{1920,1080}};
 for(unsigned i=0;i<sizeof(sizes)/sizeof(*sizes);i++){
  W=sizes[i][0];H=sizes[i][1];Rect p=control_bounds(W,H);assert(p.x>=0&&p.y>=34&&p.x+p.w<W&&p.y+p.h<H);
  for(int id=0;id<CC_COUNT;id++){Rect b=control_rect(p,id);assert(b.w>0&&b.h>0&&b.x>=p.x&&b.y>=p.y&&b.x+b.w<=p.x+p.w&&b.y+b.h<=p.y+p.h);
   assert(control_hit(p,b.x+b.w/2,b.y+b.h/2,id));assert(!control_hit(p,b.x-1,b.y,id));
   for(int j=0;j<CC_COUNT;j++)if(j!=id)assert(!control_hit(p,b.x+b.w/2,b.y+b.h/2,j));
   if(id==CC_THEME_SLIDER||id==CC_WALL_SLIDER){int n=id==CC_THEME_SLIDER?NTHEME:NWALL;assert(control_slider_value(b,b.x-100,n)==0);assert(control_slider_value(b,b.x+b.w+100,n)==n-1);int lastv=-1;for(int x=b.x;x<=b.x+b.w;x++){int v=control_slider_value(b,x,n);assert(v>=lastv&&v<n);lastv=v;}}
  }
  for(int x=p.x;x<p.x+p.w;x+=3)for(int y=p.y;y<p.y+p.h;y+=3){int n=0;for(int id=0;id<CC_COUNT;id++)n+=control_hit(p,x,y,id);assert(n<=1);}
  tiles=0;control_draw(p);assert(tiles==CC_COUNT);
 }
 Rect p=control_bounds(1280,800);control_cancel();hit(p,CC_APPEAR);assert(lastkey==5&&sys.dark==1);hit(p,CC_APPEAR);assert(!sys.dark);
 hit(p,CC_MATERIAL);assert(lastkey==4&&sys.opaque==1);hit(p,CC_MOTION);assert(lastkey==3&&sys.reduce_motion==1);hit(p,CC_DOCK);assert(lastkey==2&&sys.dock==1);hit(p,CC_WALL);assert(lastkey==1&&sys.wall==1);
 int id=CC_THEME_SLIDER;Rect b=control_rect(p,id);control_click(p,b.x+b.w/2,b.y+b.h/2);mx=b.x-100;control_pointer(p,1);assert(lastkey==0&&sys.theme==0);mx=b.x+b.w+100;control_pointer(p,1);assert(sys.theme==NTHEME-1);int n=calls;control_pointer(p,1);assert(calls==n);control_pointer(p,0);mx=b.x;control_pointer(p,1);assert(calls==n);
 control_key(p,&(Event){.a=K_HOME});assert(sys.theme==0);control_key(p,&(Event){.a=K_RIGHT});assert(sys.theme==1);control_key(p,&(Event){.a=K_END});assert(sys.theme==NTHEME-1);control_key(p,&(Event){.a=K_RIGHT});assert(sys.theme==NTHEME-1);
 control_cancel();control_key(p,&(Event){.a='\t'});control_key(p,&(Event){.a='\t',.c=1});control_key(p,&(Event){.a='\n'});assert(launched==SETTINGS);
 hit(p,CC_CALC);assert(launched==CALC);hit(p,CC_CALENDAR);assert(launched==CALENDAR);hit(p,CC_TERMINAL);assert(launched==TERMINAL);hit(p,CC_SETTINGS);assert(launched==SETTINGS);hit(p,CC_MISSION);assert(mission==1&&overlay==0);hit(p,CC_LOCK);assert(acted==152);
 fail=1;int old=sys.dark;hit(p,CC_APPEAR);assert(sys.dark==old);control_cancel();n=calls;control_pointer(p,1);assert(calls==n);
 puts("PASS actual control.c: 5 resolutions, shared rounded hit geometry, disjoint targets, monotonic slider quantization, real six-preference dispatch, drag/capture/release, keyboard endpoints/tab, actual app actions, failed syscall has no optimistic state; all tiles MAT_DOCK, all symbols procedural");return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='tmc101-contract-',dir='/var/tmp') as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['gcc','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wall','-Wextra','-Wno-sign-compare','-Wno-misleading-indentation','-I'+str(root/'include'),'-I'+str(root/'userspace'),'-I'+str(root/'desktop'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 r=subprocess.run([str(p/'test')],text=True,capture_output=True);print(r.stdout,r.stderr);assert r.returncode==0
 report={'passed':True,'sanitizers':['address','undefined'],'log':r.stdout,'source_sha256':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [root/'desktop/control.h',root/'desktop/control.c']}}
 (Path(__file__).parent/'control-contract-result.json').write_text(json.dumps(report,indent=2)+'\n')
