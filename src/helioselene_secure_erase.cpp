#include "helioselene_secure_erase.h"

#ifdef _MSC_VER
#include <windows.h>
#elif defined(__STDC_LIB_EXT1__)
#define HELIOSELENE_HAS_MEMSET_S 1
#elif (defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 25))) || defined(__OpenBSD__) \
    || defined(__FreeBSD__)
#define HELIOSELENE_HAS_EXPLICIT_BZERO 1
#include <strings.h>
#else
static void *(*const volatile memset_func)(void *, int, size_t) = std::memset;
#endif

void helioselene_secure_erase(void *pointer, size_t length)
{
#ifdef _MSC_VER
    SecureZeroMemory(pointer, length);
#elif defined(HELIOSELENE_HAS_MEMSET_S)
    memset_s(pointer, length, 0, length);
#elif defined(HELIOSELENE_HAS_EXPLICIT_BZERO)
    explicit_bzero(pointer, length);
#else
    memset_func(pointer, 0, length);
#endif
}
