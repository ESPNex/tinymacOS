/* SPDX-License-Identifier: MIT
 * Streaming UDIF ADC/zlib/bzip2/LZFSE/XZ positional decoder. No chunk/image-size RAM
 * allocation. zlib verifies Adler32, bzip2 verifies block and stream CRC32.
 * ADC has no checksum: validate every token, back-reference and exact length.
 */
#include "../kernel/k.h"
#include "image_memory.h"
#include "image8.h"
#define XZ_USE_CRC64
#include "../vendor/xz-embedded/xz.h"
#define Z_PREFIX
#define Z_SOLO
#include "../vendor/zlib-1.3.1/zlib.h"
#define NDEBUG
#include "../vendor/lzfse/lzfse_internal.h"
#define BZ_NO_STDIO
#include "../vendor/bzip2-1.0.8/bzlib.h"
static void*za8(void*x,unsigned n,unsigned s){(void)x;if(s&&n>~(size_t)0/s)return NULL;size_t bytes=(size_t)n*s;void*p=image_mem_alloc(bytes);if(p)memset(p,0,bytes);return p;}
static void zf8(void*x,void*p){(void)x;image_mem_free(p);}
static void*ba8(void*x,int n,int s){return n<0||s<0?NULL:za8(x,n,s);}
/* Upstream's non-stdio internal-invariant handler, not an input-validation
 * path. Malformed streams are reported by bzDecompress as BZ_DATA_ERROR. */
void bz_internal_error(int n){(void)n;panic("bzip2 internal invariant",n,0);}
typedef struct {Image8*i;const Image8Run*r;u64 at;u8 in[4096];unsigned used,bytes;} Input8;
static int fill8(Input8*x){
 if(x->at==x->r->length)return 0;x->bytes=MIN((u64)sizeof x->in,x->r->length-x->at);x->used=0;
 if(x->i->source(x->i->context,x->r->offset+x->at,x->in,x->bytes))return -1;x->at+=x->bytes;return 1;
}
static int getc8(Input8*x){if(x->used==x->bytes){int r=fill8(x);if(r<=0)return -1;}return x->in[x->used++];}
static void window_copy8(void*out,u64 off,size_t n,const u8*src,u64 at,size_t bytes){u64 lo=MAX(off,at),hi=MIN(off+n,at+bytes);if(hi>lo)memcpy((u8*)out+(lo-off),src+(lo-at),hi-lo);}
static int adc8(Input8*x,u64 off,void*out,size_t n){
 u8*ring=image_mem_alloc(65536);if(!ring)return -1;u64 produced=0,limit=x->r->count*512;int result=-1;
 while(x->used<x->bytes||x->at<x->r->length){
  int c=getc8(x);if(c<0)goto done;unsigned count,back=0;
  if(c&128)count=(c&127)+1;
  else if(c&64){int a=getc8(x),b=getc8(x);if(a<0||b<0)goto done;count=(c&63)+4;back=(a*256+b)+1;}
  else{int a=getc8(x);if(a<0)goto done;count=((c&63)>>2)+3;back=((c&3)*256+a)+1;}
  if(count>limit-produced||(back&&back>produced))goto done;
  while(count--){int v=back?ring[(produced-back)&65535]:getc8(x);if(v<0)goto done;ring[produced&65535]=v;if(produced>=off&&produced-off<n)((u8*)out)[produced-off]=v;produced++;}
 }
 if(produced==limit)result=0;
done:image_mem_free(ring);return result;
}
static int zlib8(Input8*x,u64 off,void*out,size_t n){
 z_stream s={.zalloc=za8,.zfree=zf8};if(inflateInit(&s)!=Z_OK)return -1;u8*buffer=image_mem_alloc(32768);int result=-1;u64 produced=0,limit=x->r->count*512;
 if(!buffer)goto done;
 for(;;){
  if(!s.avail_in){int r=fill8(x);if(r<0)goto done;s.next_in=x->in;s.avail_in=r?x->bytes:0;}
  unsigned before=s.avail_in;s.next_out=buffer;s.avail_out=32768;int r=inflate(&s,Z_NO_FLUSH);unsigned got=32768-s.avail_out;
  if(got>limit-produced)goto done;window_copy8(out,off,n,buffer,produced,got);produced+=got;
  if(r==Z_STREAM_END){if(produced==limit&&!s.avail_in&&x->at==x->r->length)result=0;break;}
  if(r!=Z_OK||(!got&&s.avail_in==before))break;
 }
done:image_mem_free(buffer);inflateEnd(&s);return result;
}
static int bzip8(Input8*x,u64 off,void*out,size_t n){
 bz_stream s={.bzalloc=ba8,.bzfree=zf8};if(BZ2_bzDecompressInit(&s,0,1)!=BZ_OK)return -1;u8*buffer=image_mem_alloc(32768);int result=-1;u64 produced=0,limit=x->r->count*512;
 if(!buffer)goto done;
 for(;;){
  if(!s.avail_in){int r=fill8(x);if(r<0)goto done;s.next_in=(char*)x->in;s.avail_in=r?x->bytes:0;}
  unsigned before=s.avail_in;s.next_out=(char*)buffer;s.avail_out=32768;int r=BZ2_bzDecompress(&s);unsigned got=32768-s.avail_out;
  if(got>limit-produced)goto done;window_copy8(out,off,n,buffer,produced,got);produced+=got;
  if(r==BZ_STREAM_END){if(produced==limit&&!s.avail_in&&x->at==x->r->length)result=0;break;}
  if(r!=BZ_OK||(!got&&s.avail_in==before))break;
 }
done:image_mem_free(buffer);BZ2_bzDecompressEnd(&s);return result;
}
/* Apple's FSE blocks require the full encoded block to be available, not
 * the full UDIF run. Keep a bounded input window and a sliding 256 KiB output
 * history (maximum LZFSE distance 262139), plus 64 KiB decode space. */
static int lzfse8(Input8*x,u64 off,void*out,size_t n){
 size_t input_bytes=65536;const unsigned history=262144,step=65536;
 lzfse_decoder_state*s=image_mem_alloc(sizeof *s);u8*in=image_mem_alloc(input_bytes),*buffer=image_mem_alloc(history+step);
 int result=-1;u64 produced=0,limit=x->r->count*512;unsigned keep=0;
 if(!s||!in||!buffer)goto done;memset(s,0,sizeof *s);s->src=s->src_begin=s->src_end=in;
 s->dst=s->dst_begin=buffer;s->dst_end=buffer+step;
 for(;;){
  size_t left=s->src_end-s->src;
  if(left<input_bytes&&x->at<x->r->length){
   memmove(in,s->src,left);size_t take=MIN((u64)(input_bytes-left),x->r->length-x->at);
   if(x->i->source(x->i->context,x->r->offset+x->at,in+left,take))goto done;
   x->at+=take;s->src=s->src_begin=in;s->src_end=in+left+take;
  }
  const u8*before_src=s->src;u8*before_dst=s->dst;int r=lzfse_decode(s);size_t got=s->dst-before_dst;
  if(got>limit-produced)goto done;window_copy8(out,off,n,before_dst,produced,got);produced+=got;
  if(r==LZFSE_STATUS_OK){if(s->end_of_stream&&produced==limit&&s->src==s->src_end&&x->at==x->r->length)result=0;break;}
  if(r!=LZFSE_STATUS_DST_FULL&&r!=LZFSE_STATUS_SRC_EMPTY)break;
  if(!got&&before_src==s->src){
   if(x->at==x->r->length)break;
   if(s->src_end-s->src==input_bytes){
    /* Need the complete FSE encoded block. Grow for that block, never for
     * the image/run's decoded length; no fixed 1 MiB refusal. */
    u64 remaining=x->r->length-x->at;size_t old=input_bytes,next=input_bytes;
    if(remaining>~(size_t)0-next)break;
    next+=MIN((u64)next,remaining);if(next<=input_bytes)break;
    u8*grown=image_mem_alloc(next);if(!grown)break;memcpy(grown,s->src,input_bytes);
    image_mem_free(in);in=grown;input_bytes=next;s->src=s->src_begin=in;s->src_end=in+old;
   }
  }
  keep=MIN((u64)history,produced);memmove(buffer,s->dst-keep,keep);
  s->dst_begin=buffer;s->dst=buffer+keep;s->dst_end=s->dst+step;
 }
done:image_mem_free(s);image_mem_free(in);image_mem_free(buffer);return result;
}
/* ULMO uses XZ/LZMA2. Accept the complete on-disk uint32 dictionary range;
 * the allocator, rather than a provisional 4 MiB policy, decides availability. */
static int lzma8(Input8*x,u64 off,void*out,size_t n){
 static int initialized;if(!initialized){xz_crc32_init();xz_crc64_init();initialized=1;}
 struct xz_dec*s=xz_dec_init(XZ_DYNALLOC,0xffffffffu);if(!s)return -1;
 u8*buffer=image_mem_alloc(32768);struct xz_buf b={0};int result=-1;u64 produced=0,limit=x->r->count*512;
 if(!buffer)goto done;
 for(;;){
  if(b.in_pos==b.in_size){int r=fill8(x);if(r<0)goto done;b.in=x->in;b.in_pos=0;b.in_size=r?x->bytes:0;}
  size_t before=b.in_pos;b.out=buffer;b.out_pos=0;b.out_size=32768;enum xz_ret r=xz_dec_run(s,&b);
  if(b.out_pos>limit-produced)goto done;window_copy8(out,off,n,buffer,produced,b.out_pos);produced+=b.out_pos;
  if(r==XZ_STREAM_END){if(produced==limit&&b.in_pos==b.in_size&&x->at==x->r->length)result=0;break;}
  if(r!=XZ_OK||(!b.out_pos&&b.in_pos==before))break;
 }
done:image_mem_free(buffer);xz_dec_end(s);return result;
}
int image8_codec_supported(u32 type){return type==IMAGE8_ADC||type==IMAGE8_ZLIB||type==IMAGE8_BZIP2||type==IMAGE8_LZFSE||type==IMAGE8_LZMA;}
int image8_codec_read(Image8*i,const Image8Run*r,u64 off,void*out,size_t n){
 if(!i||!r||!out||!image8_codec_supported(r->type)||off>r->count*512||n>r->count*512-off)return -1;
 Input8*x=image_mem_alloc(sizeof *x);if(!x)return -1;memset(x,0,sizeof *x);x->i=i;x->r=r;
 int result=r->type==IMAGE8_ADC?adc8(x,off,out,n):r->type==IMAGE8_ZLIB?zlib8(x,off,out,n):r->type==IMAGE8_BZIP2?bzip8(x,off,out,n):r->type==IMAGE8_LZFSE?lzfse8(x,off,out,n):lzma8(x,off,out,n);image_mem_free(x);return result;
}
