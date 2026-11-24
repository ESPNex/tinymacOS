#include "tmc_xhci_compat.h"
#define TP_PROTO(...) (__VA_ARGS__)
#define TP_ARGS(...)
#define DECLARE_EVENT_CLASS(...)
#define DEFINE_EVENT(template,name,proto,...) static inline void trace_##name proto {}
#define TRACE_EVENT(name,proto,...) static inline void trace_##name proto {}
