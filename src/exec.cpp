#include "common.h"
#include "exec.h"
#include "exec_fmt.h"
#include "volumes.h"

int Execute_Program(Volume_Handle volume, const char* path, int argc, const char** argv) {
    int ret = EXEC_ERR_NOTFOUND, rd;

    int fd = File_Open(volume, path, O_RDONLY);
    if(fd != -1) {
        Exec_Header hdr;

        rd = File_Read(&hdr, 1, sizeof(hdr), fd);
        if(rd == sizeof(hdr)) {
            if(hdr.magic == EXEC_MAGIC) {
                auto program = (void*)EXEC_START;
                File_Seek(fd, 0, whence_t::SET);
                rd = File_Read(program, 1, EXEC_MAX_SIZE, fd);
                ret = ((Entry_Point)hdr.addr_entry)(argc, argv);
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
