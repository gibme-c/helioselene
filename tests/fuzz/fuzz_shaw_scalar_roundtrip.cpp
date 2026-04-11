// libFuzzer harness: ShawScalar::from_bytes → to_bytes canonical round-trip.
// Mirror of fuzz_ran_scalar_roundtrip for the cycle's second curve.

#include "ranshaw.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 32)
    {
        return 0;
    }

    uint8_t in[32];
    std::memcpy(in, data, 32);

    auto s = ranshaw::ShawScalar::from_bytes(in);
    if (!s.has_value())
    {
        return 0;
    }

    auto out1 = s->to_bytes();
    auto s2 = ranshaw::ShawScalar::from_bytes(out1.data());
    if (!s2.has_value())
    {
        __builtin_trap();
    }
    auto out2 = s2->to_bytes();
    if (std::memcmp(out1.data(), out2.data(), 32) != 0)
    {
        __builtin_trap();
    }

    return 0;
}
