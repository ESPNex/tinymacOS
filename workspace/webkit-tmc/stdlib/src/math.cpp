/* IEEE binary64 ceil needed by the pinned dtoa component, without libm.
 * Integer bit decomposition preserves signed zero, infinities and NaN payloads.
 * Floating exception flags/signaling-NaN behavior are not a libm ABI promise. */
#include <bit>
#include <cstdint>
extern "C" double ceil(double value) noexcept
{
 uint64_t bits=std::bit_cast<uint64_t>(value),magnitude=bits&UINT64_C(0x7fffffffffffffff);
 unsigned raw=(unsigned)(magnitude>>52);
 if(raw>=1075||!magnitude)return value;
 if(raw<1023)return bits>>63?std::bit_cast<double>(UINT64_C(0x8000000000000000)):1.0;
 unsigned fractional=1075-raw;uint64_t mask=(UINT64_C(1)<<fractional)-1;
 if(!(bits&mask))return value;
 bits&=~mask;if(!(bits>>63))bits+=UINT64_C(1)<<fractional;
 return std::bit_cast<double>(bits);
}
