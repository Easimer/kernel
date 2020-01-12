#ifndef KERNEL_SIMD_H
#define KERNEL_SIMD_H

typedef long long __m128i __attribute__ ((__vector_size__ (16), __may_alias__));

extern __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
_mm_store_si128 (__m128i *__P, __m128i __B)
{
  *__P = __B;
}


extern __inline __m128i __attribute__((__gnu_inline__, __always_inline__, __artificial__))
_mm_load_si128 (__m128i const *__P)
{
  return *__P;
}

#endif /* KERNEL_SIMD_H */