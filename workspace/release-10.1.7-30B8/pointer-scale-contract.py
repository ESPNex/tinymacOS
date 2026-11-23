#!/usr/bin/env python3
"""Execute real firmware pointer count/mm conversion with ASan and UBSan."""
from pathlib import Path
import hashlib,json,subprocess,tempfile
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS';w=Path(tempfile.mkdtemp(prefix='tmc1017-scale-',dir='/var/tmp'))
s=r'''
#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include "firmware_pointer_scale.h"
int main(void){
 RecPointerAxis x={0},y={0};
 assert(rec_pointer_pixels(&x,-8192,65536)==-1);
 assert(rec_pointer_pixels(&y,8192,65536)==1);
 assert(rec_pointer_pixels(&x,-60*8192,65536)==-60);
 assert(rec_pointer_pixels(&y,-37*8192,65536)==-37);
 x=(RecPointerAxis){0};int sum=0;for(int i=0;i<12;i++)sum+=rec_pointer_pixels(&x,1,12);assert(sum==8&&x.carry==0);
 for(int i=0;i<12;i++)sum+=rec_pointer_pixels(&x,-1,12);assert(!sum&&!x.carry);
 x=(RecPointerAxis){0};assert(rec_pointer_pixels(&x,1,16)==0&&x.carry==8);assert(rec_pointer_pixels(&y,0,65536)==0);assert(rec_pointer_pixels(&x,1,16)==1);
 assert(rec_pointer_pixels(&x,1,8)==1);assert(rec_pointer_pixels(&x,INT_MAX,0)==0);assert(!x.carry);
 assert(rec_pointer_pixels(&x,INT_MIN,1)==INT_MIN);assert(rec_pointer_pixels(&x,INT_MAX,1)==INT_MAX);
 assert(rec_pointer_pixels(&x,1,~(u64)0)==0);assert(x.carry==8);
 assert(rec_pointer_pixels(&x,-1,~(u64)0)==0);assert(!x.carry);
 x=(RecPointerAxis){.carry=~(u64)0-4,.resolution=~(u64)0,.sign=1};assert(rec_pointer_pixels(&x,1,~(u64)0)==1&&x.carry==4);
 unsigned long long seed=1234567;
 for(int k=0;k<100;k++){
  seed=seed*6364136223846793005ULL+1;u64 res=seed?seed:1;x=(RecPointerAxis){0};__int128 residual=0;
  for(int j=0;j<1000;j++){
   seed=seed*6364136223846793005ULL+1;int32_t d=(int32_t)(seed>>32);residual+=(__int128)d*8;
   __int128 expected=residual/res;residual%=res;
   int want=expected>INT_MAX?INT_MAX:expected<INT_MIN?INT_MIN:(int)expected;
   assert(rec_pointer_pixels(&x,d,res)==want);
   assert((__int128)x.carry*x.sign==residual);
  }
 }
 puts("PASS UEFI counts/mm: scaled 65536 and raw 8 resolutions, exact independent X/Y, signed fractional accumulation, reversal, mode changes, unsupported axes and overflow boundaries");
}
'''
p=w/'test.c';p.write_text(s);exe=w/'test';subprocess.run(['gcc','-O1','-g','-fsanitize=address,undefined','-no-pie','-I'+str(root/'include'),'-I'+str(root/'recovery'),str(p),'-o',str(exe)],check=True);r=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(r.stdout)
(q/'pointer-scale-contract-result.json').write_text(json.dumps({'passed':True,'source_sha256':hashlib.sha256((root/'recovery/firmware_pointer_scale.h').read_bytes()).hexdigest(),'stdout':r.stdout,'sanitizers':['address','undefined'],'specification':'https://uefi.org/specs/UEFI/2.11/12_Protocols_Console_Support.html#simple-pointer-protocol','physical_hardware_tested':False},indent=2)+'\n')
