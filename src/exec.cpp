#include "common.h"
#include "exec.h"
#include "exec_fmt.h"
#include "volumes.h"
#include "logging.h"

#include "pfalloc.h"
#include "vm.h"
#include "utils.h"

int Execute_Program_Old(Volume_Handle volume, const char* path, int argc, const char** argv) {
    int ret = EXEC_ERR_NOTFOUND, rd;

    logprintf("Execute_Program(%s);\n", path);
    int fd = File_Open(volume, path, O_RDONLY);
    if(fd != -1) {
        Exec_Header hdr;

        rd = File_Read(&hdr, 1, sizeof(hdr), fd);
        if(rd == sizeof(hdr)) {
            if(hdr.magic == EXEC_MAGIC) {
                File_Seek(fd, 0, whence_t::END);
                auto len = (u32)File_Tell(fd);
                auto mem_len = (len + 4095) & 0xFFFFF000;
                u32 program_memory = PFA_Alloc(mem_len + 4096);
                logprintf("Program physical memory: %x\n", program_memory);
                auto program = (void*)EXEC_START;
                if(MM_VirtualMap(0, program_memory) && MM_VirtualMap((void*)0x1000, program_memory + 0x1000)) {
                    memset(program, 0xCDCDCDCD, mem_len);
                    File_Seek(fd, 0, whence_t::SET);
                    rd = File_Read(program, 1, len, fd);
                    logprintf("Loaded COMMAND.EXE (size = %x bytes), entering\n", len);
                    ret = ((Entry_Point)hdr.addr_entry)(argc, argv);
                } else {
                    logprintf("Failed to allocate map memory for PID 1\n");
                }
                PFA_Free(program_memory);
            } else {
                ret = EXEC_ERR_NOTANEXE;
            }
        } else {
            ret = EXEC_ERR_NOTANEXE;
        }

        File_Close(fd);
    }

    return ret;
}
