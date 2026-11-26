#ifndef TMC_CPP_CASES_H
#define TMC_CPP_CASES_H
#ifdef __cplusplus
extern "C" {
#endif
struct cpp_job {unsigned seed,rounds;void *handoff;};
/* Drivers supply an actual host sleep/native scheduler yield plus CPU sample. */
void cpp_test_pause(void);
void cpp_test_progress(const char*);
unsigned cpp_test_basics(void);
void *cpp_test_handoff(unsigned seed);
void cpp_test_start(void);
void *cpp_test_worker(void*);
unsigned cpp_test_finish(void);
unsigned cpp_test_snapshot(void);
void cpp_test_oom_trap(void);
void cpp_test_recursion_trap(void);
#ifdef __cplusplus
}
#endif
#endif
