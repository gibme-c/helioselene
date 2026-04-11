// libFuzzer harness: ShawPoint::from_bytes — mirror of the Ran version.

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

    auto p = ranshaw::ShawPoint::from_bytes(in);
    if (!p.has_value())
    {
        return 0;
    }

    auto out1 = p->to_bytes();
    auto p2 = ranshaw::ShawPoint::from_bytes(out1.data());
    if (!p2.has_value())
    {
        __builtin_trap();
    }
    auto out2 = p2->to_bytes();
    if (std::memcmp(out1.data(), out2.data(), 32) != 0)
    {
        __builtin_trap();
    }

    auto sum = *p + (-*p);
    if (!sum.is_identity())
    {
        __builtin_trap();
    }

    return 0;
}
