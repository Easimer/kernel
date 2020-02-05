#include "common.h"
#include "process.h"
#include "interrupts.h"
#include "utils.h"
#include "memory.h"

enum class Process_State {
    New, Ready, Running, Blocked, Exit
};

struct Process {
    bool used;
    int pid;

    Process_State state;
    Process* ready_queue_next;

    u32 page_directory;

    Registers regs;
    // TODO: SSE registers
};

struct Ready_Queue {
    Process* first;
    Process* last;
};

#define MAX_PROCESS_POOL_SIZE (128)

// TODO: maybe SoAize this?
static Process gProcessPool[MAX_PROCESS_POOL_SIZE];
static Ready_Queue gReadyQueue;
static int gCurrentProcess;

/*
static void Admit(Process* proc) {
    if(gReadyQueue.last) {
        gReadyQueue.last->ready_queue_next = proc;
        gReadyQueue.last = proc;
    } else {
        gReadyQueue.first = gReadyQueue.last = proc;
        proc->ready_queue_next = NULL;
    }
}
*/

/*
static void SwitchToProcess(Process* proc) {
    ASSERT(proc);
    ASSERT(proc->pid > 0);

    if(gCurrentProcess != 0) {
        // TODO: 
    }

    gCurrentProcess = proc->pid;
    SwitchPageDirectory(proc->page_directory);
}
*/

void Scheduler_Interrupt(Registers* regs) {
    (void)regs;
}

void Scheduler_LoadInit() {
}

void Scheduler_Init() {
    for(int i = 0; i < MAX_PROCESS_POOL_SIZE; i++) {
        gProcessPool[i].used = false;
        gProcessPool[i].pid = i + 1;
    }
    gReadyQueue.first = gReadyQueue.last = NULL;
    gCurrentProcess = 0;
}
