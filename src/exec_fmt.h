#ifndef KERNEL_EXEC_FMT_H
#define KERNEL_EXEC_FMT_H

#include "common.h"

#define EXEC_MAGIC (0x7c12f080)
#define EXEC_START  (0x00500)
#define EXEC_END    (0x7FFFF)
#define EXEC_MAX_SIZE (EXEC_END - EXEC_START)

using Entry_Point = int (*)(int argc, const char** argv);

struct Exec_Header {
    u32 magic;
    u32 addr_entry;
    u8 reserved[120];
} PACKED;

#endif /* KERNEL_EXEC_FMT_H */
