#!/usr/bin/env python3
"""Execute actual Recovery pointer arbitration; stale direct registration is a negative control."""
from pathlib import Path
import subprocess,tempfile,json,hashlib
q=Path(__file__).parent;root=q.parents[1]/'tinymacOS';source=(root/'recovery/main.c').read_text()
a=source.index('static void poll_pointer_sources(');b=source.index('EFI_STATUS _entry',a);actual=source[a:b]
base=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define EFI_ERROR(a) ((a)!=0)
#define EFI_ABSP_TouchActive 1
#define FALSE 0
#define TPL_NOTIFY 16
typedef unsigned long UINTN;
typedef struct{int x,y,buttons,wheel;}TabletState;
typedef struct{int RelativeMovementX,RelativeMovementY,LeftButton;}EFI_SIMPLE_POINTER_STATE;
typedef struct Ptr Ptr;struct Ptr{int (*GetState)(Ptr*,EFI_SIMPLE_POINTER_STATE*);int which;};
typedef Ptr EFI_SIMPLE_POINTER_PROTOCOL;
typedef struct{uint64_t AbsoluteMinX,AbsoluteMaxX,AbsoluteMinY,AbsoluteMaxY;}EFI_ABSOLUTE_POINTER_MODE;
typedef struct{uint64_t CurrentX,CurrentY;int ActiveButtons;}EFI_ABSOLUTE_POINTER_STATE;
typedef struct Abs Abs;struct Abs{int(*GetState)(Abs*,EFI_ABSOLUTE_POINTER_STATE*);EFI_ABSOLUTE_POINTER_MODE *Mode;};
typedef Abs EFI_ABSOLUTE_POINTER_PROTOCOL;
static int mx,my,rw=1280,rh=800,bs,direct_ready,absolute_ready,fw_ready[2],calls,button;
static TabletState direct_state;
static EFI_SIMPLE_POINTER_STATE fwstate[2];
static int rec_pointer_next(int unused,TabletState*s){(void)unused;if(!direct_ready)return 0;direct_ready=0;*s=direct_state;return 1;}
static int pointer_pixel(uint64_t v,uint64_t lo,uint64_t hi,int n){return (v-lo)*(n-1)/(hi-lo);}
static void pointer_button(int down){button=down;calls++;}
static int simple(Ptr*p,EFI_SIMPLE_POINTER_STATE*s){if(!fw_ready[p->which])return 1;fw_ready[p->which]=0;*s=fwstate[p->which];return 0;}
static int absolute(Abs*p,EFI_ABSOLUTE_POINTER_STATE*s){(void)p;if(!absolute_ready)return 1;absolute_ready=0;*s=(EFI_ABSOLUTE_POINTER_STATE){100,100,1};return 0;}
'''
tail=r'''
int main(void){Ptr idle={simple,0},usb={simple,1};Ptr *ps[]={&idle,&usb};int down[2]={0};
 mx=my=100;fwstate[1]=(EFI_SIMPLE_POINTER_STATE){40,0,1};fw_ready[1]=1;
 poll_pointer_sources(1,0,ps,down,2);assert(mx==140&&my==100&&button==1&&calls==1);
 fwstate[1]=(EFI_SIMPLE_POINTER_STATE){0,-17,1};fw_ready[1]=1;
 poll_pointer_sources(1,0,ps,down,2);assert(mx==140&&my==83&&calls==2);
 fwstate[1]=(EFI_SIMPLE_POINTER_STATE){0,0,0};fw_ready[1]=1;poll_pointer_sources(1,0,ps,down,2);assert(!button&&calls==3);
 EFI_ABSOLUTE_POINTER_MODE mode={0,100,0,100};Abs abs={absolute,&mode};absolute_ready=1;
 poll_pointer_sources(1,&abs,ps,down,2);assert(mx==1279&&my==799&&button==1);
 absolute_ready=1;fwstate[1]=(EFI_SIMPLE_POINTER_STATE){-37,0,1};fw_ready[1]=1;
 poll_pointer_sources(1,&abs,ps,down,2);assert(mx==1242&&my==799&&button==1);
 direct_ready=1;direct_state=(TabletState){500,400,0,0};poll_pointer_sources(1,&abs,ps,down,2);assert(mx==500&&my==400&&!button);
 int n=calls;poll_pointer_sources(1,&abs,ps,down,2);assert(calls==n);
 puts("PASS actual Recovery arbitration: idle direct + idle first firmware pointer cannot hide active second mouse; separate X/Y, press/release, AbsolutePointer fallback, fresh direct priority and no phantom idle activity; repeated EFI_SUCCESS AbsolutePointer cannot hide SimplePointer");
}
'''
with tempfile.TemporaryDirectory(prefix='tmc1011-pointer-contract-',dir='/var/tmp') as d:
 p=Path(d)
 def run(name,fn):
  c=p/(name+'.c');c.write_text(base+fn+tail)
  subprocess.run(['gcc','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wall','-Wextra','-Werror',str(c),'-o',str(p/name)],check=True)
  return subprocess.run([str(p/name)],capture_output=True,text=True)
 fixed=run('fixed',actual);print(fixed.stdout,fixed.stderr);assert fixed.returncode==0
 old=actual.replace('absolute && !got_abs','absolute && !direct').replace('if (!got_abs) for','if (!direct && !got_abs) for')
 assert old!=actual;negative=run('old-exclusive-gates',old);assert negative.returncode!=0
 print('PASS negative control: previous exclusive gates fail the same idle-direct/firmware-mouse assertion')
 prior=subprocess.check_output(['git','--git-dir=/var/tmp/tmc-github-git/tinymacOS.git','show','6c60166:tinymacOS/recovery/main.c'],text=True)
 a=prior.index('static void poll_pointer_sources(');b=prior.index('EFI_STATUS _entry',a)
 previous=run('previous-10-1-1-idle-absolute',prior[a:b]);assert previous.returncode!=0
 print('PASS negative control: actual 10.1.1 arbitration fails the repeated-idle-absolute assertion')
 report={'passed':True,'negative_control_failed_as_expected':True,'negative_returncode':negative.returncode,'previous_10_1_1_idle_absolute_failed':previous.returncode!=0,'log':fixed.stdout,'sanitizers':['address','undefined'],'sha256':{str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in [root/'recovery/main.c',root/'recovery/usbhid.c',root/'drivers/xhci.c',root/'drivers/usbroute.c']}}
 (q/'pointer-contract-result.json').write_text(json.dumps(report,indent=2)+'\n')
