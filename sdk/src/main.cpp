#include "kernel_sc.h"

int main(int argc, char** argv) {
    int rc = -1, rd = 0, fd;
    char buf[32];

    fd = open(0, "/LOREM.TXT", O_RDONLY);
    if(fd != -1) {
        do {
            rd = read(fd, buf, 1, 31);
            if(rd > 0) {
                buf[rd] = 0;
                print(buf);
            }
        } while(rd > 0);
        rc = 0;

        close(fd);
    }

    return rc;
}
