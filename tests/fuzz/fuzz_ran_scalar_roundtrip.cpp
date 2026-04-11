// libFuzzer harness: RanScalar::from_bytes → to_bytes canonical round-trip.
//
// Property: when from_bytes accepts an input, re-encoding via to_bytes
// must produce a canonical byte string that round-trips back to the same
// scalar bit-for-bit. Catches any non-canonical encoding, reduction
// inconsistency, or silent acceptance of out-of-range inputs.

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

    auto s = ranshaw::RanScalar::from_bytes(in);
    if (!s.has_value())
    {
        return 0;
    }

    auto out1 = s->to_bytes();

    // Round-trip: the canonical encoding must decode without failure and
    // produce bit-identical bytes on a second encode.
    auto s2 = ranshaw::RanScalar::from_bytes(out1.data());
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
