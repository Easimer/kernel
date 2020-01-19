#include "common.h"
#include "utils.h"
#include "logging.h"
#include "simd.h"
#include "pc_vga.h"

struct Stack_Frame {
	Stack_Frame* ebp;
	u32 eip;
} PACKED;

static void DumpStackTrace(u32 max_frames) {
	Stack_Frame* stack;

	asm("movl %%ebp, %0" : "=r"(stack) ::);
	for(u32 frame = 0; stack && frame < max_frames; frame++) {
		logprintf("%x\n", stack->eip);
		stack = stack->ebp;
	}
}

void do_assert(const char* expr, s32 line, const char* file, const char* function) {
    logprintf("========================\nAssertion failed: %s\nFile: %s\nFunction: %s:%d\n", expr, file, function, line);
    
	logprintf("Stack trace:\n");
	DumpStackTrace(32);
	logprintf("=====================");
	PCVGA_PrintNotice("!!! CRASH !!!");

    while(1) {
		asm volatile("hlt");
	}
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

static void memcpyu(void* dst, const void* src, u32 len) {
	u64* d64 = (u64*)dst;
	const u64* s64 = (const u64*)src;
	while (len >= 16) {
        _mm_storeu_si128((__m128i*)d64, _mm_loadu_si128((__m128i const*)s64));
        d64++; s64++;
        d64++; s64++;
        len -= 16;
	}
	u8* d8 = (u8*)d64;
	const u8* s8 = (const u8*)s64;
	while (len) {
		*d8 = *s8;
		d8++; s8++;
		len--;
	}
}

void memcpy(void* dst, const void* src, u32 len) {
	if (!(dst && src && len)) {
		return;
	}
	// if the addresses are unaligned, use unaligned memcpy
	if (!(
		(((u32)dst & 0xF) == 0) &&
		(((u32)src & 0xF) == 0)
		)) {
		memcpyu(dst, src, len);
		return;
	}
    
	u64* d64 = (u64*)dst;
	const u64* s64 = (const u64*)src;
	while (len >= 16) {
        _mm_store_si128((__m128i*)d64, _mm_load_si128((__m128i const*)s64));
        d64++; s64++;
        d64++; s64++;
        len -= 16;
	}
	u8* d8 = (u8*)d64;
	const u8* s8 = (const u8*)s64;
	while(len) {
		*d8 = *s8;
		d8++; s8++;
		len--;
	}
}

u32 strlen(const char* s) {
	u32 ret = 0;
	while(*s++) ret++;
	return ret;
}

bool strncmp(const char* lhs, const char* rhs, u32 n) {
	for (u32 i = 0; i < n; i++) {
		char l = lhs[i];
		char r = rhs[i];
		if (l != r)
			return false;
		if (!l)
			break;
	}
	return true;
}

bool strcmp(const char* lhs, const char* rhs) {
	while (*lhs && (*lhs == *rhs))
		lhs++, rhs++;
	return *lhs == *rhs;
}