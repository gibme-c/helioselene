// libFuzzer harness: RanScalar::reduce_wide.
//
// Property: reduce_wide(x) must produce a canonical in-range scalar, i.e.
// its to_bytes encoding must re-decode via from_bytes without failure.
// Also: reduce_wide is deterministic — calling it twice on the same input
// must yield bit-identical output.

#include "ranshaw.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 64)
    {
        return 0;
    }

    uint8_t wide[64];
    std::memcpy(wide, data, 64);

    auto s1 = ranshaw::RanScalar::reduce_wide(wide);
    auto b1 = s1.to_bytes();

    // Canonical: re-decoding via from_bytes must succeed.
    auto s1r = ranshaw::RanScalar::from_bytes(b1.data());
    if (!s1r.has_value())
    {
        __builtin_trap();
    }

    // Determinism: reduce_wide on the same input must be bit-identical.
    auto s2 = ranshaw::RanScalar::reduce_wide(wide);
    auto b2 = s2.to_bytes();
    if (std::memcmp(b1.data(), b2.data(), 32) != 0)
    {
        __builtin_trap();
    }

    return 0;
}
