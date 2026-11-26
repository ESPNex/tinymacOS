#ifndef TMC_PTHREAD_CASES_H
#define TMC_PTHREAD_CASES_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
unsigned pthread_cases_run(void);
void pthread_test_pause(void);
uint64_t pthread_test_ticks(void);
void pthread_cpp_touch(void);
unsigned pthread_cpp_finish(void);
#ifdef __cplusplus
}
#endif
#endif
