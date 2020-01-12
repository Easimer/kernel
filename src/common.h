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

#endif
