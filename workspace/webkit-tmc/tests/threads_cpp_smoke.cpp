/* Real freestanding C++ consumer of the native C ABI. No libstdc++, exceptions,
 * RTTI, allocation, std::thread or compiler thread_local claim. */
#include "tmc_threads.h"
#include "tmc_sync.h"
struct Context { tmc_mutex mutex {}; unsigned value {}; };
class Guard {
    tmc_mutex& m;
public:
    bool okay;
    explicit Guard(tmc_mutex& mutex):m(mutex),okay(!tmc_mutex_lock(&m)) { }
    ~Guard() { if(okay)tmc_mutex_unlock(&m); }
    Guard(const Guard&)=delete;
    Guard& operator=(const Guard&)=delete;
};
static void *cpp_worker(void *argument)
{
    auto *context=static_cast<Context *>(argument);
    if(!tmc_thread_current() || !tmc_thread_id())return nullptr;
    int *error=tmc_tls_errno_location();if(!error)return nullptr;
    *error=321;
    { Guard guard(context->mutex);if(!guard.okay)return nullptr;context->value++; }
    return *error==321?context:nullptr;
}
extern "C" int tmc_cpp_thread_smoke(void)
{
    Context context;
    tmc_thread thread {};
    int *error=tmc_tls_errno_location();if(!error)return 1;
    int saved=*error;
    if(tmc_thread_create(&thread,cpp_worker,&context))return 2;
    void *result=nullptr;
    if(tmc_thread_join(&thread,&result))return 3;
    if(result!=&context || context.value!=1 || *error!=saved)return 4;
    { Guard guard(context.mutex);if(!guard.okay)return 5; }
    return 0;
}
