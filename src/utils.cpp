#include "common.h"
#include "utils.h"
#include "logging.h"

void do_assert(const char* expr, s32 line, const char* file, const char* function) {
    logprintf("========================\nAssertion failed: %s\nFile: %s\nFunction: %s:%d\n", expr, file, function, line);
    
    while(1);
}
