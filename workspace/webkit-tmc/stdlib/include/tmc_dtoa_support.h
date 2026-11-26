#pragma once
/* Explicit bounded port surface for the pinned WTF dtoa component ONLY.
 * Not replacement WTF headers, allocator, StringCommon, or a complete WTF build.
 * Assertions remain checked traps, including upstream security assertions. */
#include <algorithm>
#include <array>
#include <span>
#include <ranges>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <type_traits>
#define ASSERT_ENABLED 1
#define ASSERT(x) do {if(!(x))__builtin_trap();} while(0)
#define ASSERT_WITH_SECURITY_IMPLICATION(x) ASSERT(x)
#define ASSERT_UNDER_CONSTEXPR_CONTEXT(x) ASSERT(x)
#define ASSERT_NOT_REACHED() __builtin_trap()
#define UNUSED_PARAM(x) (void)(x)
#define WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
#define WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
#define WTF_EXPORT_PRIVATE
#define NODELETE
namespace WTF {
template<class T> constexpr std::span<T> unsafeMakeSpan(T*p,size_t n){return {p,n};}
template<class D,size_t N,class S,size_t M> inline void memmoveSpan(std::span<D,N>d,std::span<S,M>s)
{static_assert(sizeof(D)==sizeof(S));ASSERT(d.size()>=s.size());std::memmove(d.data(),s.data(),s.size_bytes());}
template<class T> inline std::span<const std::byte> asByteSpan(const T&v)
{return {reinterpret_cast<const std::byte*>(&v),sizeof(T)};}
template<class T> inline std::span<std::byte> asMutableByteSpan(T&v)
{return {reinterpret_cast<std::byte*>(&v),sizeof(T)};}
template<size_t N> inline size_t strlenSpan(std::span<char,N>s)
{size_t n=0;while(n<s.size()&&s[n])n++;ASSERT(n<s.size());return n;}
template<class T> constexpr bool isASCIIDigit(T c){return c>='0'&&c<='9';}
template<class T> constexpr unsigned toASCIIHexValue(T c)
{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;__builtin_trap();}
}
