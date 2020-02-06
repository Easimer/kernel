#include "common.h"
#include "process.h"
#include "interrupts.h"
#include "utils.h"
#include "memory.h"
#include "vm.h"
#include "pfalloc.h"

#include "exec.h"
#include "exec_fmt.h"
#include "volumes.h"
#include "logging.h"

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

static void Clear(Registers* regs) {
    regs->ds = 32;
    regs->edi = 0; regs->esi = 0;
    regs->ebp = 0; regs->esp = 0;
    regs->ebx = 0; regs->edx = 0;
    regs->ecx = 0; regs->eax = 0;
    regs->int_no = 0; regs->err_code = 0;
    regs->eip = 0; regs->cs = 24;
    regs->eflags = 0;
    regs->useresp = 0; regs->ss = 32;
}

static void Admit(Process* proc) {
    if(gReadyQueue.last) {
        gReadyQueue.last->ready_queue_next = proc;
        gReadyQueue.last = proc;
    } else {
        gReadyQueue.first = gReadyQueue.last = proc;
        proc->ready_queue_next = NULL;
    }
}

static void SwitchToProcess(Process* proc) {
    ASSERT(proc);
    ASSERT(proc->pid > 0);

    if(gCurrentProcess != 0) {
        // TODO: 
    }

    gCurrentProcess = proc->pid;
    SwitchPageDirectory(proc->page_directory);
}

void Scheduler_Interrupt(Registers* regs) {
    (void)regs;
}

void Scheduler_Init() {
    for(int i = 0; i < MAX_PROCESS_POOL_SIZE; i++) {
        gProcessPool[i].used = false;
        gProcessPool[i].pid = i + 1;
    }
    gReadyQueue.first = gReadyQueue.last = NULL;
    gCurrentProcess = 0;
}

int Execute_Program(Volume_Handle volume, const char* path, int argc, const char** argv) {
    int ret = EXEC_ERR_NOTFOUND;
    int rd, fd, pid = -1;
    Exec_Header hdr;
    Process* proc = NULL;
    u32 page_directory;
    u32 len, mem_len;
    u32 program_memory, stack_memory;
    void *program;
    auto stack = (void*)0x40000000;
    u32 mem_remain;
    void* cur_virt;
    u32 cur_phys;
    bool ok;

    logprintf("exec: loading program '%d:/%s'\n", volume, path);

    // Find out if the executable exists and is valid
    fd = File_Open(volume, path, O_RDONLY);
    if(fd == -1) {
        logprintf("exec: not found\n");
        goto notfound;
    }
    rd = File_Read(&hdr, 1, sizeof(hdr), fd);
    if(rd != sizeof(hdr)) {
        logprintf("exec: truncated\n");
        goto invalid;
    }

    if(hdr.magic != EXEC_MAGIC) {
        logprintf("exec: not an executable\n");
        goto invalid;
    }

    // Allocate new process slot
    for(int i = 0; i < MAX_PROCESS_POOL_SIZE && pid == -1; i++) {
        if(!gProcessPool[i].used) {
            pid = i;
        }
    }
    
    if(pid == -1) {
        logprintf("exec: no free process descriptor\n");
        goto out_of_memory;
    }

    proc = &gProcessPool[pid];
    proc->used = true;

    // Allocate page directory
    if(!AllocatePageDirectory(&page_directory)) {
        logprintf("exec: out of memory (PD)\n");
        goto out_of_memory_pd;
    }

    // Switch to that page directory
    SwitchPageDirectory(page_directory);
    proc->page_directory = page_directory;

    // Load image
    File_Seek(fd, 0, whence_t::END);
    len = (u32)File_Tell(fd);
    mem_len = (len + 4095) & 0xFFFFF000;
    // TODO: PFA_Alloc that signals error (NULL is a valid physical address)
    program_memory = PFA_Alloc(mem_len + 4096);
    stack_memory = PFA_Alloc(4096);
    logprintf("exec: len=%x mem_len=%x program memory: %xp stack: %xp\n", len, mem_len, program_memory, stack_memory);
    program = (void*)EXEC_START;
    // Map program memory
    mem_remain = mem_len;
    cur_virt = (void*)0;
    cur_phys = program_memory;
    ok = true;
    while(mem_remain != 0 && ok) {
        logprintf("exec: remain=%d virt=%x phys=%x\n", mem_remain, cur_virt, cur_phys);
        ok = ok && MM_VirtualMap(cur_virt, cur_phys);
        mem_remain -= 4096;
        cur_virt = (void*)((u8*)cur_virt + 4096);
        cur_phys += 4096;
    }
    if(!ok) {
        goto out_of_memory_vm;
    }
    logprintf("exec: mapped program memory\n");
    if(!MM_VirtualMap(stack, stack_memory)) {
        goto out_of_memory_stack;
    }
    logprintf("exec: mapped stack memory\n");
    memset(0x00000000, 0xCDCDCDCD, mem_len);
    File_Seek(fd, 0, whence_t::SET);
    rd = File_Read(program, 1, len, fd);

    // Setup process descriptor
    proc->pid = pid + 1;
    proc->page_directory = page_directory;
    proc->ready_queue_next = NULL;
    proc->state = Process_State::New;
    Clear(&proc->regs);
    proc->regs.ebp = 0x40000FFF;
    proc->regs.esp = 0x40000FFF;
    proc->regs.eip = EXEC_START;
    proc->regs.useresp = 0x40000FFF;
    proc->regs.eflags = 0x206;
    // Admit process
    Admit(proc);
    (void)argc;
    (void)argv;

    return 0;
notfound:
    return EXEC_ERR_NOTFOUND;
invalid:
    ret = EXEC_ERR_NOTANEXE;
    goto error;
out_of_memory_stack:
    PFA_Free(stack_memory);
out_of_memory_vm:
    PFA_Free(program_memory);
    // TODO: does FreePageDirectory free the allocated page tables?
    FreePageDirectory(page_directory);
out_of_memory_pd:
    proc->used = false;
out_of_memory:
    ret = EXEC_ERR_NOMEM;
error:
    File_Close(fd);
    return ret;
}

static const char* argv_init[] = {"/COMMAND.EXE"};

void Scheduler_LoadInit() {
    u32 max_volumes = Volume_GetCount();
    for(u32 vol = 1; vol < max_volumes; vol++) {
        auto res = Execute_Program(vol, "/COMMAND.EXE", 1, argv_init);
        if(res == 0) {
            ASSERT(gProcessPool[0].used);
            SwitchToProcess(&gProcessPool[0]);
            return;
        } else {
            if(res == EXEC_ERR_NOMEM) {
                logprintf("exec: out of memory!\n");
            }
        }
    }
    logprintf("can't find init\n");
}