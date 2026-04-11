// libFuzzer harness: RanPoint::map_to_curve.
//
// Property: map_to_curve(u) on any 32 bytes must return an on-curve
// point. We validate this by re-encoding and re-decoding via from_bytes —
// a point that is not on the curve will fail decode.

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

    uint8_t u[32];
    std::memcpy(u, data, 32);

    auto p = ranshaw::RanPoint::map_to_curve(u);
    auto bytes = p.to_bytes();

    // On-curve invariant: re-decode must succeed.
    auto decoded = ranshaw::RanPoint::from_bytes(bytes.data());
    if (!decoded.has_value())
    {
        __builtin_trap();
    }

    // Determinism: map_to_curve is pure in u.
    auto p2 = ranshaw::RanPoint::map_to_curve(u);
    auto b2 = p2.to_bytes();
    if (std::memcmp(bytes.data(), b2.data(), 32) != 0)
    {
        __builtin_trap();
    }

    return 0;
}
