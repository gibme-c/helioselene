// libFuzzer harness: ShawScalar::reduce_wide — mirror of the Ran version.

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

    auto s1 = ranshaw::ShawScalar::reduce_wide(wide);
    auto b1 = s1.to_bytes();

    auto s1r = ranshaw::ShawScalar::from_bytes(b1.data());
    if (!s1r.has_value())
    {
        __builtin_trap();
    }

    auto s2 = ranshaw::ShawScalar::reduce_wide(wide);
    auto b2 = s2.to_bytes();
    if (std::memcmp(b1.data(), b2.data(), 32) != 0)
    {
        __builtin_trap();
    }

    return 0;
}
