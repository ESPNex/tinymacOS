/* Reproduce the pinned upstream signed-length regression independently of
 * the large consumer. The unfixed build is EXPECTED to return Infinity for
 * 0.1; that is evidence of a bug, not a successful numeric conversion. */
#include <wtf/dtoa/double-conversion.h>
#include <bit>
#include <cstdint>
#include <cstdio>
int main()
{
 size_t consumed=0;double value=WTF::double_conversion::StringToDoubleConverter::StringToDouble("0.1",3,&consumed);
 uint64_t bits=std::bit_cast<uint64_t>(value);
#ifdef EXPECT_UPSTREAM_SIGNED_BUG
 if(bits!=UINT64_C(0x7ff0000000000000)||consumed!=3)return 1;
 puts("REPRODUCED negative upstream case: original signed/unsigned bound parses 0.1 as Infinity; NOT numeric acceptance");
#else
 if(bits!=UINT64_C(0x3fb999999999999a)||consumed!=3)return 1;
 puts("PASS patched numeric regression: 0.1 exactly matches independent binary64 oracle");
#endif
}
