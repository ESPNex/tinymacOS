/* Explicit no-exceptions failure policy for the exercised libstdc++ subset.
 * These are fatal paths, NOT successful stubs or exception ABI emulation.
 * No streams, locale, std::thread, shared_ptr/glibc TLS, RTTI or unwinding.
 * Only symbols required by the selected, linked consumers are provided. */
#include <bits/functexcept.h>
#include <cstdlib>
namespace std {
void __throw_bad_alloc(){__builtin_trap();}
void __throw_bad_array_new_length(){__builtin_trap();}
void __throw_length_error(const char*){__builtin_trap();}
void __throw_logic_error(const char*){__builtin_trap();}
void __throw_out_of_range(const char*){__builtin_trap();}
void __throw_out_of_range_fmt(const char*,...){__builtin_trap();}
void __throw_bad_function_call(){__builtin_trap();}
void __glibcxx_assert_fail(const char*,int,const char*,const char*) noexcept {__builtin_trap();}
}
extern "C" void abort() noexcept {__builtin_trap();}
