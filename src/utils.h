#ifndef KERNEL_UTILS_H
#define KERNEL_UTILS_H

#include "common.h"

void do_assert(const char* expr, s32 line, const char* file, const char* function);

#define _ASSERT(expr) #expr
#define ASSERT(expr) if(!(expr)) {do_assert((const char*)_ASSERT(expr),(s32) __LINE__, (const char*)__FILE__, (const char*)__FUNCTION__);}

void memset(void* dst, int value, u32 len);
void memcpy(void* dst, const void* src, u32 len);

u32 strlen(const char* s);
bool strncmp(const char* lhs, const char* rhs, u32 n);
bool strcmp(const char* lhs, const char* rhs);

extern "C" void SSE_Setup();

#endif /* KERNEL_UTILS_H */
