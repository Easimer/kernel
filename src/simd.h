#ifndef KERNEL_SIMD_H
#define KERNEL_SIMD_H

typedef float __m128 __attribute__ ((__vector_size__ (16), __may_alias__));
typedef float __m128_u __attribute__ ((__vector_size__ (16), __may_alias__, __aligned__ (1)));
typedef long long __m128i __attribute__ ((__vector_size__ (16), __may_alias__));
typedef long long __m128i_u __attribute__ ((__vector_size__ (16), __may_alias__, __aligned__ (1)));

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

extern __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
_mm_storeu_si128 (__m128i_u *__P, __m128i __B)
{
  *__P = __B;
}

extern __inline __m128i __attribute__((__gnu_inline__, __always_inline__, __artificial__))
_mm_loadu_si128 (__m128i_u const *__P)
{
  return *__P;
}

extern __inline __m128 __attribute__((__gnu_inline__, __always_inline__, __artificial__))
_mm_load_ps (float const *__P)
{
  return *(__m128 *)__P;
}

extern __inline __m128 __attribute__((__gnu_inline__, __always_inline__, __artificial__))
_mm_loadu_ps (float const *__P)
{
  return *(__m128_u *)__P;
}

extern __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
_mm_store_ps (float *__P, __m128 __A)
{
  *(__m128 *)__P = __A;
}

extern __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
_mm_storeu_ps (float *__P, __m128 __A)
{
  *(__m128_u *)__P = __A;
}

#endif /* KERNEL_SIMD_H */