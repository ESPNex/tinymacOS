/* Actual GCC standard-library templates + pinned WTF dtoa, linked on the
 * native heap/runtime, not a replacement vector/string or JS engine. */
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <atomic>
#include <functional>
#include <span>
#include <bit>
#include <array>
#include <limits>
#include <cmath>
#include <wtf/dtoa/double-conversion.h>
#include "tmc_threads.h"
#include "tmc_heap.h"
#include "tmc_runloop.h"
#include "tmc_time.h"
#include "tmc_cpp_runtime.h"
#include "stdlib_cases.h"
#include "dtoa_vectors.h"
using namespace WTF::double_conversion;
static std::atomic<unsigned> failure{0},ready{0},go{0},constructed{0},tasks{0},cleaned{0},roundtrips{0},fx_checks{0};
static tmc_runloop loop=TMC_RUNLOOP_INITIALIZER;
#define SC(x) do{if(!(x)){unsigned z=0;failure.compare_exchange_strong(z,__LINE__);}}while(0)
static const std::vector<unsigned>& shared()
{
 static const std::vector<unsigned> value=[](){constructed++;stdlib_pause();return std::vector<unsigned>(128,0xc011); }();return value;
}
static void numeric_basics()
{
 double(*volatile ceil_call)(double)=::ceil;
 struct CeilCase {double input,output;};
 const CeilCase c[]={{0,0},{-0.0,-0.0},{0.1,1},{-0.1,-0.0},{1.25,2},{-1.25,-1},{4503599627370496.,4503599627370496.},{std::numeric_limits<double>::infinity(),std::numeric_limits<double>::infinity()}};
 for(const auto&v:c)SC(std::bit_cast<uint64_t>(ceil_call(v.input))==std::bit_cast<uint64_t>(v.output));
 uint64_t nan=UINT64_C(0x7ff8000000001234);SC(std::bit_cast<uint64_t>(ceil_call(std::bit_cast<double>(nan)))==nan);
 const char bytes[]={'a',0,'b','a'};SC(strnlen(bytes,4)==1&&strnlen(bytes,0)==0&&std::memchr(bytes,'b',4)==bytes+2&&!std::memchr(bytes,'z',4));

 for(const auto&v:dtoa_vectors){size_t processed=0;double d=StringToDoubleConverter::StringToDouble(v.text,std::strlen(v.text),&processed);SC(std::bit_cast<uint64_t>(d)==v.bits&&processed==std::strlen(v.text));}
 struct Test {double value;const char*text;};
 const Test fixed[]={{0,"0"},{-0.0,"0"},{0.1,"0.1"},{0.000001,"0.000001"},{0.0000001,"1e-7"},{1e20,"100000000000000000000"},{1e21,"1e+21"},{-123.5,"-123.5"},{std::numeric_limits<double>::infinity(),"Infinity"},{std::numeric_limits<double>::quiet_NaN(),"NaN"}};
 for(auto&v:fixed){std::array<char,128>b;StringBuilder builder{std::span<char>(b)};SC(DoubleToStringConverter::EcmaScriptConverter().ToShortest(v.value,&builder));auto text=builder.Finalize();SC(std::string(text.data())==v.text);}
 std::array<char,128>b;StringBuilder builder{std::span<char>(b)};SC(DoubleToStringConverter::EcmaScriptConverter().ToFixed(1.25,1,&builder));SC(std::string(builder.Finalize().data())=="1.3");
 const uint16_t wide[]={ '1','2','.','5' };size_t processed=0;SC(StringToDoubleConverter::StringToDouble(wide,4,&processed)==12.5&&processed==4);
 SC(StringToDoubleConverter::StringToFloat("0.1",3,&processed)==0.1f&&processed==3);
 SC(StringToDoubleConverter::StringToDouble("12.5x",5,&processed)==12.5&&processed==4);
 SC(StringToDoubleConverter::StringToDouble("abc",3,&processed)==0&&processed==0);
}
struct Task {std::function<void()> call;};
static void invoke(void*p){static_cast<Task*>(p)->call();}
static void dispose(void*p){delete static_cast<Task*>(p);cleaned.fetch_add(1,std::memory_order_release);}
static uint64_t random64(uint64_t&x){x^=x<<13;x^=x>>7;x^=x<<17;return x;}
extern "C" unsigned stdlib_setup(stdlib_job*jobs)
{
 SC(!tmc_heap_set_cache_limit(256u<<10));numeric_basics();
 for(unsigned w=0;w<4;w++){
  auto*handoff=new std::vector<std::string>(32,std::string(100,char('A'+w)));jobs[w]={w,handoff};
 }
 return failure.load();
}
extern "C" void stdlib_start(void){while(ready.load(std::memory_order_acquire)!=4)stdlib_pause();go.store(1,std::memory_order_release);}
extern "C" void*stdlib_dispatcher(void*p){SC(!tmc_runloop_run(&loop));return p;}
extern "C" void*stdlib_worker(void*p)
{
 auto&job=*static_cast<stdlib_job*>(p);ready.fetch_add(1,std::memory_order_release);while(!go.load(std::memory_order_acquire))stdlib_pause();
 auto*handoff=static_cast<std::vector<std::string>*>(job.handoff);SC(handoff->size()==32);for(auto&s:*handoff)SC(s==std::string(100,char('A'+job.seed)));delete handoff;job.handoff=nullptr;
 SC(shared().size()==128&&shared()[127]==0xc011);
 // The kernel explicitly saves x87/SSE2 (not AVX). Verify independent thread
 // rounding-control words across real sleep/context switches, then restore RN
 // before dtoa: changing its rounding mode would invalidate the algorithm.
 for(unsigned i=0;i<32;i++){
  uint32_t mxcsr=(0x1f80u|(job.seed<<13)),actual;uint16_t cw=(uint16_t)(0x37f|(job.seed<<10)),acw;
  asm volatile("ldmxcsr %0;fldcw %1"::"m"(mxcsr),"m"(cw):"memory");stdlib_pause();
  asm volatile("stmxcsr %0;fnstcw %1":"=m"(actual),"=m"(acw)::"memory");
  uint32_t normal=0x1f80;uint16_t normalcw=0x37f;asm volatile("ldmxcsr %0;fldcw %1"::"m"(normal),"m"(normalcw):"memory");
  SC((actual&0xffc0)==mxcsr&&acw==cw);fx_checks++;
 }
 uint64_t state=UINT64_C(0x103c011123456789)+job.seed;
 for(unsigned n=0;n<2000;n++){
  uint64_t bits=random64(state);if((bits&UINT64_C(0x7ff0000000000000))==UINT64_C(0x7ff0000000000000))bits^=UINT64_C(0x0010000000000000);
  double d=std::bit_cast<double>(bits);std::array<char,128>b;StringBuilder builder{std::span<char>(b)};
  SC(DoubleToStringConverter::EcmaScriptConverter().ToShortest(d,&builder));auto result=builder.Finalize();size_t processed=0;
  double parsed=StringToDoubleConverter::StringToDouble(result.data(),std::strlen(result.data()),&processed);SC(std::bit_cast<uint64_t>(parsed)==bits&&processed==std::strlen(result.data()));roundtrips++;
  if(!(n%16)){
   std::vector<std::string> values;for(unsigned k=0;k<16;k++)values.emplace_back(std::string(80,char('p'-k))+std::string("\0tail",5));
   std::sort(values.begin(),values.end());SC(std::is_sorted(values.begin(),values.end())&&values.front().size()==85&&values.front()[80]==0&&values.front().find("tail")==81);
   auto owned=std::make_unique<std::vector<std::string>>(std::move(values));SC(values.empty()&&owned->size()==16);std::vector<std::string> copy=*owned;SC(copy==*owned);copy.erase(copy.begin()+2,copy.begin()+7);SC(copy.size()==11);
  }
  if(n<100){
   std::string message(100,char('A'+job.seed));auto*t=new Task{[message=std::move(message)](){SC(message.size()==100&&message.front()==message.back());tasks++;}};
   uint64_t id=0,now=0;SC(!tmc_time_now_ticks(&now));int r=tmc_runloop_post(&loop,n%2?now+1:0,invoke,t,dispose,&id);SC(!r);if(r)delete t;
  }
  if(!(n%32))stdlib_pause();
 }
 return p;
}
extern "C" unsigned stdlib_finish(void)
{
 while(cleaned.load(std::memory_order_acquire)!=400)stdlib_pause();
 SC(ready==4&&constructed==1&&tasks==400&&roundtrips==8000&&fx_checks==128);
 SC(!tmc_runloop_stop(&loop));return failure.load();
}
extern "C" unsigned stdlib_teardown(void)
{
 SC(!tmc_runloop_destroy(&loop));SC(tmc_cpp_pending_destructors()==1);__cxa_finalize(nullptr);SC(!tmc_cpp_pending_destructors());
 SC(!tmc_heap_set_cache_limit(0)&&!tmc_heap_trim());auto s=tmc_heap_statistics();SC(!s.spans&&!s.live&&!s.reserved);return failure.load();
}
extern "C" void stdlib_length_trap(void){std::vector<unsigned> impossible;impossible.reserve(SIZE_MAX);}
extern "C" void stdlib_support_trap(void){WTF::toASCIIHexValue('!');}
