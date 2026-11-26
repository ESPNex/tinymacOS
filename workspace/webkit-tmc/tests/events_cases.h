#ifndef TMC_EVENTS_CASES_H
#define TMC_EVENTS_CASES_H
#include "tmc_runloop.h"
#ifdef __cplusplus
extern "C" {
#endif
unsigned events_cases_run(void);
void events_pause(void);
int events_cpp_post(struct tmc_runloop*);
unsigned events_cpp_completed(void);
unsigned events_cpp_finish(void);
#ifdef __cplusplus
}
#endif
#endif
