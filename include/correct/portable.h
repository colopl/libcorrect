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

#endif // CORRECT_PORTABLE_H
