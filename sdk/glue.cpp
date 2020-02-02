extern int main(int argc, char** argv);

struct exeh_t {
    unsigned long magic;
    unsigned long addr_entry;
    unsigned char reserved[120];
};
exeh_t __kernel_exe_header __attribute__((section (".exeh"))) = {
    .magic = 0x7c12f080,
    .addr_entry = (unsigned long)main,
};
