#ifndef TMC_LIFECYCLE_CASES_H
#define TMC_LIFECYCLE_CASES_H
#include "tmc_cpp_runtime.h"
#ifdef __cplusplus
extern "C" {
#endif
unsigned lifecycle_setup(tmc_cpp_initializer const *,tmc_cpp_initializer const *);
void *lifecycle_worker(void *);
void *lifecycle_finalizer(void *);
unsigned lifecycle_finish(void);
void lifecycle_pause(void);
#ifdef __cplusplus
}
#endif
#endif
