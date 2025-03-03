#ifndef CORRECT_PORTABLE_H
#define CORRECT_PORTABLE_H

#ifdef __GNUC__
#define HAVE_BUILTINS
#endif

#ifdef HAVE_BUILTINS
static inline int safe_popcount(unsigned int x) {
    return __builtin_popcount(x) & 0xFFFF;
}
#define popcount safe_popcount
#else
static inline int popcount(unsigned int x) {
    /* taken from the helpful http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel */
    x = x - ((x >> 1) & 0x55555555);
    x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
    x = ((x + (x >> 4) & 0x0f0f0f0f) * 0x01010101) >> 24;
    return x & 0xFFFF;
}
#endif

#ifdef HAVE_BUILTINS
#define prefetch __builtin_prefetch
#else
static inline void prefetch(const void *x) 
{
    (void)x;
}
#endif

#ifdef _MSC_VER
#define ALIGNED_MALLOC(size, alignment) _aligned_malloc(size, alignment)
#define ALIGNED_FREE _aligned_free
#else
#include <stdlib.h>
static inline void* _aligned_malloc(size_t size, size_t alignment) {
    void *ptr;

    if (posix_memalign(&ptr, alignment, size) == 0) {
        return ptr;
    }

    return NULL;
}
#define ALIGNED_MALLOC(size, alignment) _aligned_malloc(size, alignment)
#define ALIGNED_FREE free
#endif

#endif  /* CORRECT_PORTABLE_H */
