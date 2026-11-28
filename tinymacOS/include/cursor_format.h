#pragma once
/* Bounded CUR/DIB and RIFF ACON decoder. No allocation, packed casts or host
 * dependencies. Unsupported compression/PNG entries fail closed. The original
 * 32px representation is preferred; larger DIBs are sampled to 32px. */
#define CUR_SIDE 32
#define CUR_FRAMES 64
#define CUR_STEPS 256
typedef struct {u32 rgba[CUR_SIDE*CUR_SIDE];u8 invert[CUR_SIDE*CUR_SIDE];int hx,hy;} CursorFrame;
typedef struct {CursorFrame frame[CUR_FRAMES];u16 seq[CUR_STEPS];u32 rate[CUR_STEPS];unsigned frames,steps;u64 duration;} CursorImage;
static unsigned cur16(const u8*p){return p[0]|(unsigned)p[1]<<8;}
static u32 cur32(const u8*p){return cur16(p)|(u32)cur16(p+2)<<16;}
static int cur_range(u64 at,u64 n,u64 len){return at<=len&&n<=len-at;}
static int cur_dib(const u8*b,unsigned n,CursorFrame*out)
{
 if(n<22||cur16(b)||cur16(b+2)!=2)return 0;
 unsigned count=cur16(b+4);if(!count||count>256||!cur_range(6,(u64)count*16,n))return 0;
 const u8*best=0;unsigned score=~0u;
 for(unsigned i=0;i<count;i++){
  const u8*e=b+6+i*16;unsigned w=e[0]?e[0]:256,h=e[1]?e[1]:256,z=cur32(e+8),at=cur32(e+12);
  if(!cur_range(at,z,n)||at<6+count*16||z<40||w>128||h>128)continue;
  const u8*d=b+at;unsigned bits=cur16(d+14);
  if(cur32(d)!=40||cur32(d+4)!=w||cur32(d+8)!=h*2||cur16(d+12)!=1||cur32(d+16)||
     !(bits==1||bits==4||bits==8||bits==24||bits==32)||cur16(e+4)>=w||cur16(e+6)>=h)continue;
  unsigned s=(w>32?w-32:32-w)+(h>32?h-32:32-h);if(s<score){score=s;best=e;}
 }
 if(!best)return 0;
 unsigned w=best[0]?best[0]:256,h=best[1]?best[1]:256,z=cur32(best+8);
 const u8*d=b+cur32(best+12);unsigned bits=cur16(d+14),colors=bits<=8?(cur32(d+32)?cur32(d+32):1u<<bits):0;
 if(colors>(1u<<8)||(bits<=8&&colors>(1u<<bits)))return 0;
 unsigned row=((w*bits+31)/32)*4,maskrow=((w+31)/32)*4,off=40+colors*4;
 if(!cur_range(off,(u64)row*h+(u64)maskrow*h,z))return 0;
 const u8*p=d+off,*mask=p+row*h;int alpha=0;
 if(bits==32)for(unsigned y=0;y<h&&!alpha;y++)for(unsigned x=0;x<w;x++)if(p[y*row+x*4+3]){alpha=1;break;}
 out->hx=cur16(best+4)*CUR_SIDE/w;out->hy=cur16(best+6)*CUR_SIDE/h;
 for(unsigned y=0;y<CUR_SIDE;y++)for(unsigned x=0;x<CUR_SIDE;x++){
  unsigned sx=x*w/CUR_SIDE,sy=h-1-y*h/CUR_SIDE,i=y*CUR_SIDE+x;const u8*q=p+sy*row;
  unsigned andbit=(mask[sy*maskrow+sx/8]>>(7-sx%8))&1,a=255;u32 rgb;
  if(bits>=24){q+=sx*(bits/8);rgb=(u32)q[2]<<16|(u32)q[1]<<8|q[0];if(alpha)a=q[3];}
  else{unsigned v=bits==8?q[sx]:bits==4?(q[sx/2]>>(4*(1-sx%2)))&15:(q[sx/8]>>(7-sx%8))&1;
   if(v>=colors)return 0;q=d+40+v*4;rgb=(u32)q[2]<<16|(u32)q[1]<<8|q[0];}
  out->invert[i]=!alpha&&andbit&&rgb==0xffffff;
  if(!alpha&&andbit)a=0;out->rgba[i]=(u32)a<<24|rgb;
 }
 return 1;
}
static int cursor_decode(const u8*b,unsigned n,CursorImage*out)
{
 out->frames=out->steps=0;out->duration=0;
 if(n>=6&&cur16(b+2)==2){if(!cur_dib(b,n,out->frame))return 0;out->frames=out->steps=1;out->seq[0]=0;out->rate[0]=1;out->duration=1;return 1;}
 if(n<12||memcmp(b,"RIFF",4)||memcmp(b+8,"ACON",4)||cur32(b+4)!=(u64)n-8)return 0;
 unsigned nf=0,ns=0,def=0,flags=0,actual=0;const u8*seq=0,*rate=0;unsigned seqn=0,raten=0;int header=0;
 for(u64 pos=12;pos<n;){
  if(!cur_range(pos,8,n))return 0;const u8*q=b+pos;unsigned z=cur32(q+4);u64 end=pos+8+(u64)z;
  if(!cur_range(pos+8,z,n)||!cur_range(end,z&1,n))return 0;
  if(!memcmp(q,"anih",4)){
   if(header++||z!=36||cur32(q+8)!=36)return 0;nf=cur32(q+12);ns=cur32(q+16);def=cur32(q+36);flags=cur32(q+40);
   if(!nf||nf>CUR_FRAMES||!ns||ns>CUR_STEPS||!def||def>3600||!(flags&1)||(flags&~3u))return 0;
  }else if(!memcmp(q,"seq ",4)){if(seq)return 0;seq=q+8;seqn=z;}
  else if(!memcmp(q,"rate",4)){if(rate)return 0;rate=q+8;raten=z;}
  else if(!memcmp(q,"LIST",4)){
   if(z<4)return 0;if(!memcmp(q+8,"fram",4))for(u64 p=pos+12;p<end;){
    if(!cur_range(p,8,end))return 0;unsigned len=cur32(b+p+4);u64 next=p+8+(u64)len+(len&1);
    if(!cur_range(p+8,len,end)||next>end)return 0;
    if(!memcmp(b+p,"icon",4)){if(actual>=CUR_FRAMES||!cur_dib(b+p+8,len,out->frame+actual++))return 0;}p=next;
   }
  }
  pos=end+(z&1);
 }
 if(!header||actual!=nf||((flags&2)&&!seq)||(!(flags&2)&&ns!=nf)||(seq&&seqn!=ns*4)||(rate&&raten!=ns*4))return 0;
 for(unsigned i=0;i<ns;i++){unsigned s=(flags&2)?cur32(seq+i*4):i,r=rate?cur32(rate+i*4):def;if(s>=nf||!r||r>3600)return 0;out->seq[i]=s;out->rate[i]=r;out->duration+=r;}
 out->frames=nf;out->steps=ns;return 1;
}
static unsigned cursor_frame(const CursorImage*c,u64 ticks)
{
 if(!c->steps||!c->duration)return 0;
 /* 100Hz OS ticks, 60Hz ANI jiffies; no per-frame rounding drift. */
 u64 t=((ticks/100)*60+(ticks%100)*60/100)%c->duration;
 for(unsigned i=0;i<c->steps;i++){if(t<c->rate[i])return c->seq[i];t-=c->rate[i];}return 0;
}
