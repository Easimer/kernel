#ifndef KERNEL_EXEC_H
#define KERNEL_EXEC_H

#include "common.h"
#include "volumes.h"

#define EXEC_ERR_NOTFOUND (-1)
#define EXEC_ERR_NOTANEXE (-2)
#define EXEC_ERR_NOMEM (-3)

int Execute_Program(Volume_Handle volume, const char* path, int argc, const char** argv);

#endif /* KERNEL_EXEC_H */
