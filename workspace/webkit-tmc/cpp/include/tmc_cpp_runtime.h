#ifndef TMC_CPP_RUNTIME_H
#define TMC_CPP_RUNTIME_H
#ifdef __cplusplus
extern "C" {
#endif
/* Opt-in no-exceptions process lifecycle. Attach the native TCB FIRST.
 * Initializer boundaries must describe one immutable array, alive forever.
 * Repeated concurrent calls with the SAME range initialize exactly once.
 * Recursive startup returns EDEADLK; a different range returns EINVAL.
 * Initializers/destructors may allocate and register/reenter finalizers.
 * No callback executes under the registry mutex. Finalizers serialize across
 * threads; same-thread nesting is allowed. Callbacks must return normally.
 * __cxa_finalize(dso) consumes that DSO's registrations in reverse order;
 * NULL consumes all. Records are removed BEFORE invocation, hence recursive
 * finalization cannot call one twice. Registrations made by callbacks are
 * included. Termination is not guaranteed if callbacks endlessly register.
 * Quiesce/join other application threads BEFORE final process teardown: a
 * concurrent late registration after the last scan belongs to a later pass.
 * No automatic loader/start.S integration, dlopen/dlclose, exceptions, compiler
 * TLS destructors, quick_exit, signals or complete C exit() is claimed. */
typedef void (*tmc_cpp_initializer)(void);
int tmc_cpp_run_initializers(tmc_cpp_initializer const *first,tmc_cpp_initializer const *last);
int __cxa_atexit(void (*destructor)(void *),void *argument,void *dso);
void __cxa_finalize(void *dso);
/* Debug/accounting only, not a safe shutdown predicate with active producers. */
unsigned long tmc_cpp_pending_destructors(void);
#ifdef __cplusplus
}
#endif
#endif
