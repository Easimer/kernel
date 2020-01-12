#ifndef KERNEL_UTILS_H
#define KERNEL_UTILS_H

#include "common.h"

void do_assert(const char* expr, s32 line, const char* file, const char* function);

#define _ASSERT(expr) #expr
#define ASSERT(expr) if(!(expr)) {do_assert((const char*)_ASSERT(expr),(s32) __LINE__, (const char*)__FILE__, (const char*)__FUNCTION__);}

#endif /* KERNEL_UTILS_H */
