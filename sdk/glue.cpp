extern int main(int argc, char** argv);

struct {
    unsigned long magic;
    unsigned long addr_entry;
    unsigned char reserved[120];
} __kernel_exe_header __attribute__((section(".exeh"))) = {
    .magic = 0x7c12f080,
    .addr_entry = (unsigned long)main,
};
