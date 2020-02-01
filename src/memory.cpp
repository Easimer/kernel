#include "common.h"
#include "utils.h"
#include "memory.h"
#include "logging.h"

#define KERNEL_RESERVED (8 * 1024 * 1024)

struct Mem_State {
    void* base;
    u32 length;
};

struct Block_Header {
    u32 size;
    bool free;
};

struct Pool_State {
    u32 marker;
    char* heap_bot;
    char* heap_top;
    u32 size;
    // Points to the last block's head
    char* sp;
};

static Pool_State* gpPools[2];

void Mem_Init(void* base, u32 length) {
    ASSERT(length >= KERNEL_RESERVED);

    Pool_State* pool_kernel = (Pool_State*)base;
    Pool_State* pool_user = (Pool_State*)((u8*)base + KERNEL_RESERVED);
    // Kernel pool
    pool_kernel->marker = 0xDEADBEEF;
    pool_kernel->heap_bot = (char*)(pool_kernel + 1);
    pool_kernel->size = KERNEL_RESERVED;
    pool_kernel->heap_top = (char*)((u8*)base + KERNEL_RESERVED);
    pool_kernel->sp = pool_kernel->heap_top;
    // User pool
    pool_user->marker = 0xDEADBEEF;
    pool_user->heap_bot = (char*)(pool_user + 1);
    pool_user->size = length - KERNEL_RESERVED;
    pool_user->heap_top = (char*)pool_user + (pool_user->size);
    pool_user->sp = pool_user->heap_top;

	logprintf("[MEM] Kernel pool: %x length=%x bytes\n", pool_kernel, pool_kernel->size);
	logprintf("[MEM] Program pool: %x length=%x bytes\n", pool_user, pool_user->size);

    gpPools[0] = pool_kernel;
    gpPools[1] = pool_user;
}

void* pmalloc(int pool, u32 size) {
    Pool_State* state = gpPools[pool];

    auto newhead = state->sp - size;
	// align the block data on 16 bytes boundary
	u32 rem = (u32)newhead & 0xF;
	newhead -= rem;
	ASSERT(((u32)newhead & 0xF) == 0);
	newhead -= sizeof(Block_Header);
	u32 fullsize = size + rem + sizeof(Block_Header);
	if (newhead < state->heap_bot) {
		ASSERT(!"pmalloc failed: stack is full");
		return nullptr;
	}
	auto block = (Block_Header*)newhead;
	block->size = size + rem;
	block->free = false;
	state->sp -= fullsize;
    
	// Catch block outside of the range and
	// raise an assertion
	if (!((void*)block >= (void*)state->heap_bot && (void*)block < (void*)state->heap_top)) {
		ASSERT(0);
		return nullptr;
	}
    
	return ((char*)block + sizeof(Block_Header));
}

static void __heappop(Pool_State* state) {
	auto block = (Block_Header*)state->sp;
	if (state->sp == state->heap_top)
		return;
	if (block->free) {
		state->sp += sizeof(Block_Header) + block->size;
		__heappop(state);
	} else {
		return;
	}
}

void pfree(int pool, void* addr) {
    if (!addr) return;
	Pool_State* state = gpPools[pool];
	// find original block address
	auto block = (Block_Header*)((char*)addr - sizeof(Block_Header));
    
	// Reject block outside of the range and
	// raise an assertion
	if (!((void*)block >= (void*)state->heap_bot && (void*)block < (void*)state->heap_top)) {
		ASSERT(!"Tried deallocating block from outside of this heap");
		return;
	}
        
	block->free = true;
	__heappop(state);
}

void* AllocatePage() {
	return NULL;
}

void DeallocatePage(void* addr) {
	(void)addr;
}
