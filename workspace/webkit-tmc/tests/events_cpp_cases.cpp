#include <pthread.h>
#include <new>
#include "tmc_cpp_runtime.h"
#include "events_cases.h"
#include <errno.h>
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static unsigned created,called,destroyed,completed,updates,local_dtors,failure;
struct State {unsigned *value;State():value(new unsigned(17)){}~State(){if(*value!=17)failure=1;delete value;local_dtors++;}};
static State& state(){static State s;return s;}
struct Lock {Lock(){if(pthread_mutex_lock(&mutex))__builtin_trap();}~Lock(){if(pthread_mutex_unlock(&mutex))__builtin_trap();}};
struct Task {
 unsigned value=42;
 Task(){__atomic_fetch_add(&created,1,__ATOMIC_RELAXED);}
 ~Task(){if(value!=43)failure=1;__atomic_fetch_add(&destroyed,1,__ATOMIC_RELAXED);}
 void run(){Lock lock;if(value!=42||*state().value!=17)failure=1;value++;updates++;__atomic_fetch_add(&called,1,__ATOMIC_RELAXED);}
};
static void call(void*p){static_cast<Task*>(p)->run();}
static void dispose(void*p){delete static_cast<Task*>(p);__atomic_fetch_add(&completed,1,__ATOMIC_RELEASE);}
extern "C" int events_cpp_post(struct tmc_runloop*loop)
{
 Task*t=new(std::nothrow)Task;if(!t)return ENOMEM;
 uint64_t id;int r=tmc_runloop_post(loop,0,call,t,dispose,&id);
 if(r){t->value=43;delete t;}return r;
}
extern "C" unsigned events_cpp_completed(void){return __atomic_load_n(&completed,__ATOMIC_ACQUIRE);}
extern "C" unsigned events_cpp_finish(void)
{
 if(created!=400||called!=400||destroyed!=400||completed!=400||updates!=400||local_dtors||tmc_cpp_pending_destructors()!=1)failure=1;
 __cxa_finalize(nullptr);if(local_dtors!=1||tmc_cpp_pending_destructors())failure=1;
 if(pthread_mutex_destroy(&mutex))failure=1;
 return failure;
}
