#ifndef HELIOSELENE_CT_BARRIER_H
#define HELIOSELENE_CT_BARRIER_H

#include <cstdint>

#if defined(__GNUC__) || defined(__clang__)

static inline uint32_t ct_barrier_u32(uint32_t x)
{
    __asm__ __volatile__("" : "+r"(x));
    return x;
}

static inline uint64_t ct_barrier_u64(uint64_t x)
{
    __asm__ __volatile__("" : "+r"(x));
    return x;
}

#else

static inline uint32_t ct_barrier_u32(uint32_t x)
{
    volatile uint32_t v = x;
    return v;
}

static inline uint64_t ct_barrier_u64(uint64_t x)
{
    volatile uint64_t v = x;
    return v;
}

#endif

#endif // HELIOSELENE_CT_BARRIER_H
