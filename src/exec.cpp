#include "common.h"
#include "exec.h"
#include "exec_fmt.h"
#include "volumes.h"
#include "logging.h"

#include "pfalloc.h"
#include "vm.h"
#include "utils.h"

int Execute_Program(Volume_Handle volume, const char* path, int argc, const char** argv) {
    int ret = EXEC_ERR_NOTFOUND;
    int rd, fd;
    Exec_Header hdr;
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

    // Allocate page directory
    if(!AllocatePageDirectory(&page_directory)) {
        logprintf("exec: out of memory (PD)\n");
        goto out_of_memory_pd;
    }

    // Switch to that page directory
    SwitchPageDirectory(page_directory);

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

    ((Entry_Point)hdr.addr_entry)(argc, argv);

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
    ret = EXEC_ERR_NOMEM;
error:
    File_Close(fd);
    return ret;
}

static const char* argv_init[] = {"/COMMAND.EXE"};

void Spawn_Init() {
    u32 max_volumes = Volume_GetCount();
    for(u32 vol = 1; vol < max_volumes; vol++) {
        auto res = Execute_Program(vol, "/COMMAND.EXE", 1, argv_init);
        if(res == 0) {
            return;
        } else {
            if(res == EXEC_ERR_NOMEM) {
                logprintf("exec: out of memory!\n");
            }
        }
    }
    ASSERT(!"couldn't spawn init\n");
}
