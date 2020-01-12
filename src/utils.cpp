#include "common.h"
#include "utils.h"
#include "logging.h"

void do_assert(const char* expr, s32 line, const char* file, const char* function) {
    logprintf("========================\nAssertion failed: %s\nFile: %s\nFunction: %s:%d\n", expr, file, function, line);
    
    while(1);
}

void memset(void* dst, int value, u32 len) {
	if (!(dst && len)) {
		return;
	}
	int* tgti = (int*)dst;
	while (len > 4) {
		*tgti = value;
		len -= 4;
		tgti++;
	}
	char* tgtc = (char*)dst;
	while (len) {
		*tgtc = (char)(value & 0xFF);
		len--;
		tgtc++;
	}
}