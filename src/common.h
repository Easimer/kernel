#ifndef KERNEL_COMMON_H
#define KERNEL_COMMON_H

#define NULL ((void*)0)

#define CLINK extern "C" __attribute__((__cdecl__))

using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned long;
using u64 = unsigned long long;

using s8 = signed char;
using s16 = signed short;
using s32 = signed long;
using s64 = signed long long;

/*
using va_list = __builtin_va_list;
#define va_start(v,l)   __builtin_va_start(v,l)
#define va_end(v)   __builtin_va_end(v)
#define va_arg(v,l) __builtin_va_arg(v,l)
*/

#endif
