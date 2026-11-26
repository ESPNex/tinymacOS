/* A real C++ consumer of the source-level pthread header + native new/guard
 * and __cxa_atexit lifecycle, linked without a hosted C++ standard library. */
#include <pthread.h>
#include <new>
#include "tmc_cpp_runtime.h"
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static unsigned updates,constructed,destroyed,failure;
struct Lock {
 Lock(){if(pthread_mutex_lock(&mutex))__builtin_trap();}
 ~Lock(){if(pthread_mutex_unlock(&mutex))__builtin_trap();}
};
struct Local {
 unsigned *value;
 Local():value(new unsigned(17)){constructed++;}
 ~Local(){if(*value!=17)failure=1;destroyed++;delete value;}
};
extern "C" void pthread_cpp_touch(void)
{
 static Local local;
 Lock lock;if(*local.value!=17)failure=1;updates++;
}
extern "C" unsigned pthread_cpp_finish(void)
{
 if(constructed!=1||updates!=2000||destroyed||tmc_cpp_pending_destructors()!=1)failure=1;
 __cxa_finalize(nullptr);
 if(destroyed!=1||tmc_cpp_pending_destructors())failure=1;
 if(pthread_mutex_destroy(&mutex))failure=1;
 return failure;
}
