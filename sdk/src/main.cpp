#include "kernel_sc.h"

extern int fd_stdout;
extern int fd_stderr;
extern int fd_stdin;

#define STDOUT (fd_stdout)
#define STDIN (fd_stdin)
#define STDERR (fd_stderr)

static int strlen(const char* s) {
    auto c = s;
    while(*c++);
    return c - s - 1;
}

static void print_stdout(const char* string) {
    write(STDOUT, string, 1, strlen(string));
}

int main(int argc, char** argv) {
    int rc = 0;
    Keyboard_Event ev;
    char ch;
    bool over = false;

    int keyboard = open(0, "tty1", O_RDONLY);
    if(keyboard != -1) {
        print_stdout("Opened keyboard, echoing input:\n");
        for(;;) {
            char ch;
            if(read(keyboard, &ch, 1, 1) == 1) {
                if(ch == '\x04') {
                    break;
                } else {
                    char buf[2] = {ch, '\0'};
                    print_stdout(buf);
                }
            }
        }
    } else {
        print_stdout("Failed to open keyboard for reading!\n");
    }

    return rc;
}
