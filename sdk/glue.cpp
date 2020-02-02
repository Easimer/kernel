#include "kernel_sc.h"

extern int main(int argc, char** argv);

int fd_stdout;
int fd_stderr;
int fd_stdin;

int _start(int argc, char** argv) {
    int rc;

    fd_stdout = open(0, "vga", O_RDWR);
    fd_stderr = fd_stdout;
    fd_stdin = -1;

    rc = main(argc, argv);

    close(fd_stdout);

    return rc;
}

struct exeh_t {
    unsigned long magic;
    unsigned long addr_entry;
    unsigned char reserved[120];
};
exeh_t __kernel_exe_header __attribute__((section (".exeh"))) = {
    .magic = 0x7c12f080,
    .addr_entry = (unsigned long)_start,
};
