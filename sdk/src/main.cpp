#include "kernel_sc.h"

static char VKMap[VK_UNKNOWN] = "0123456789abcdefghijklmnopqrstuvwxyz \0\0\0\0\0\0\0\0\0\0\0\0\0`-=[];'\\,./\n\b\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
static char VKMapShift[VK_UNKNOWN] = ")!@#$%^&*(ABCDEFGHIJKLMNOPQRSTUVWXYZ \0\0\0\0\0\0\0\0\0\0\0\0\0`_+{}:\"|<>?\n\b\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

static bool TranslateVK(char* out, const Keyboard_Event& ev) {
    bool ret = false;


    if(!(ev.flags & KBEV_RELEASED)) {
        char c;
        if(ev.flags & KBEV_SHIFT) {
            c = VKMapShift[ev.vk];
        } else {
            c = VKMap[ev.vk];
        }

        if(c != '\0') {
            ret = true;
            *out = c;
        }
    }

    return ret;
}

static void ReadCommand(char* buffer, int buffer_siz) {
    int cur = 0;
    bool over = false;

    buffer[0] = 0;

    while(!over) {
        Keyboard_Event ev;
        if(poll_kbd(0, &ev)) {
            bool released = ev.flags & KBEV_RELEASED;
            if(ev.vk == VK_RETURN) {
                over = true;
            } else if(ev.vk == VK_BACKSPACE && !released) {
                if(cur != 0) {
                    buffer[cur] = 0;
                    cur--;
                }
            } else {
                char ch;
                if(TranslateVK(&ch, ev)) {
                    if(cur < buffer_siz - 1) {
                        buffer[cur + 0] = ch;
                        buffer[cur + 1] = 0;
                        cur++;
                    }
                }
            }
        }
    }

}

int main(int argc, char** argv) {
    int rc = 0;
    bool exit = false;
    char cmdbuf[64];

    while(!exit) {
        print("> ");
        ReadCommand(cmdbuf, 64);

        print("Command: ");
        print(cmdbuf);
        print("\n");
    }

    return rc;
}
