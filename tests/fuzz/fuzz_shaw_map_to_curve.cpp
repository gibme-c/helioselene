// libFuzzer harness: ShawPoint::map_to_curve — mirror of the Ran version.

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

    auto p = ranshaw::ShawPoint::map_to_curve(u);
    auto bytes = p.to_bytes();

    auto decoded = ranshaw::ShawPoint::from_bytes(bytes.data());
    if (!decoded.has_value())
    {
        __builtin_trap();
    }

    auto p2 = ranshaw::ShawPoint::map_to_curve(u);
    auto b2 = p2.to_bytes();
    if (std::memcmp(bytes.data(), b2.data(), 32) != 0)
    {
        __builtin_trap();
    }

    return 0;
}
