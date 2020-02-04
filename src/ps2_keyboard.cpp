#include "common.h"
#include "ps2.h"
#include "memory.h"
#include "utils.h"
#include "virtkeys.h"
#include "interrupts.h"
#include "syscalls.h"
#include "ring_buffer.h"
#include "timer.h"
#include "dev_fs.h"

#include "logging.h"

#define MAX_BUFFERED_EVENTS (256)

struct PS2_Keyboard {
    u32 dev;
    u8 flags;
    volatile bool ack, resend, echo;
    Ring_Buffer<Keyboard_Event, MAX_BUFFERED_EVENTS> event_buffer;
    Ring_Buffer<char, 128> char_buffer;
};

static PS2_Keyboard* gpKeyboards[2] = { NULL, NULL };
static bool gbRegisteredSyscall = false;

#define PREFIX_EXTRA (0xE0)
#define PREFIX_RELEASE (0xF0)

#define UNK VK_UNKNOWN

// Virtual key mappings
// Normal scancodes
static Virtual_Key VkMap_1B[256] = {
    UNK, VK_F9, UNK, VK_F5,
    VK_F3, VK_F1, VK_F2, VK_F12,
    UNK, VK_F10, VK_F8, VK_F6,
    VK_F4, VK_TAB, VK_BACKTICK, UNK,
    UNK, VK_META, VK_LSHIFT, UNK,
    VK_LCTRL, VK_q, VK_1, UNK,
    UNK, UNK, VK_z, VK_s,
    VK_a, VK_w, VK_2, UNK,
    UNK, VK_c, VK_x, VK_d,
    VK_e, VK_4, VK_3, UNK,
    UNK, VK_SPACE, VK_v, VK_f,
    VK_t, VK_r, VK_5, UNK,
    UNK, VK_n, VK_b, VK_h,
    VK_g, VK_y, VK_6, UNK,
    UNK, UNK, VK_m, VK_j,
    VK_u, VK_7, VK_8, UNK,
    UNK, VK_COMMA, VK_k, VK_i,
    VK_o, VK_0, VK_9, UNK,
    UNK, VK_PERIOD, VK_SLASH, VK_l,
    VK_SEMICOLON, VK_p, VK_MINUS, UNK,
    UNK, UNK, VK_QUOTE, UNK,
    VK_LBRACKET, VK_EQUALS, UNK, UNK,
    VK_CAPSLOCK, VK_RSHIFT, VK_RETURN, VK_RBRACKET,
    UNK, VK_BACKSLASH, UNK, UNK,
    UNK, UNK, UNK, UNK,
    UNK, UNK, VK_BACKSPACE, UNK,
    UNK, VK_KP1, UNK, VK_KP4,
    VK_KP7, UNK, UNK, UNK,
    VK_KP0, VK_KPPERIOD, VK_KP2, VK_KP5,
    VK_KP6, VK_KP8, VK_ESCAPE, VK_NUMLOCK,
    VK_F11, VK_KPPLUS, VK_KP3, VK_KPMINUS,
    VK_KPASTERISK, VK_KP9, VK_SCROLLLOCK, UNK,
    UNK, UNK, UNK, VK_F7,

    UNK,
};

static bool SendCommandWaitForAck(PS2_Keyboard* kbd, u8 command) {
    bool ret = false;

    PS2_SendToDevice(kbd->dev, command);
    auto end = TicksElapsed() + 1000;
    while(TicksElapsed() < end && !kbd->ack) asm volatile("hlt");

    ret = kbd->ack;
    kbd->ack = false;

    return ret;
}

static bool SendCommandAndDataWaitForAck(PS2_Keyboard* kbd, u8 command, u8 data) {
    bool ret = false;

    PS2_SendToDevice(kbd->dev, command);
    PS2_SendToDevice(kbd->dev, data);
    auto end = TicksElapsed() + 1000;
    while(TicksElapsed() < end && !kbd->ack) asm volatile("hlt");

    ret = kbd->ack;
    kbd->ack = false;

    return ret;
}

static bool SendEcho(PS2_Keyboard* kbd) {
    bool ret = false;

    PS2_SendToDevice(kbd->dev, 0xEE);
    auto end = TicksElapsed() + 1000;
    while(TicksElapsed() < end && !kbd->echo) asm volatile("hlt");

    ret = kbd->echo;
    kbd->echo = false;

    return ret;
}

static bool EnableScanning(PS2_Keyboard* kb) {
    bool ret = false;

    ASSERT(kb);
    
    if(SendCommandWaitForAck(kb, 0xF4)) {
        ret = true;
        logprintf("ps2: enabled scanning on keyboard %d\n", kb->dev);
    } else {
        logprintf("ps2: failed to enable scanning on keyboard %d: no ACK or timed out\n", kb->dev);
    }

    return ret;
}

static bool SetLEDs(PS2_Keyboard* kb, u32 mask) {
    bool ret = false;

    ASSERT(kb);
    
    if(SendCommandAndDataWaitForAck(kb, 0xED, mask)) {
        ret = true;
        logprintf("ps2: set LEDs on keyboard %d\n", kb->dev);
    } else {
        logprintf("ps2: failed to set LEDs on keyboard %d: no ACK or timed out\n", kb->dev);
    }

    return ret;
}

static bool SetScancodeSet(PS2_Keyboard* kb, u32 set) {
    bool ret = false;

    ASSERT(kb);
    
    if(SendCommandAndDataWaitForAck(kb, 0xF0, set)) {
        logprintf("ps2: set scancode set on keyboard %d to %d\n", kb->dev, set);
        // TODO: check if SCS was actually set
    } else {
        logprintf("ps2: failed to reset LEDs on keyboard %d: no ACK or timed out\n", kb->dev);
    }

    return ret;
}

bool MapSequenceToVK(int len, u8* sequence, Virtual_Key* vk, bool* released) {
    bool ret = false;
    *vk = VK_UNKNOWN;
    *released = false;

    if(len == 1) {
        *vk = VkMap_1B[sequence[0]];
        *released = false;
        ret = true;
    } else if(len == 2) {
        if(sequence[0] == PREFIX_EXTRA) {
            // TODO: extra page is unimplemented
        } else if(sequence[0] == PREFIX_RELEASE) {
            *released = true;
            *vk = VkMap_1B[sequence[1]];
            ret = true;
        } else {
            ASSERT(!"Unexpected prefix");
        }
    } else if(len == 3) {
        if(sequence[0] == PREFIX_EXTRA) {
            if(sequence[1] == PREFIX_RELEASE) {
                // TODO: extra page is unsupported
            } else {
                ASSERT(!"Unexpected prefix");
            }
        } else {
            ASSERT(!"Unexpected prefix");
        }
    }

    return ret;
}

static void IRQHandler(Registers* regs) {
    Virtual_Key vk;
    bool released;
    u8 sequence[16];
    u32 len = 0;
    u32 remains = 16;
    u8 buf;
    PS2_Keyboard* kbd = NULL;

    if(regs->int_no == IRQ1) {
        kbd = gpKeyboards[0];
    } else if(regs->int_no == IRQ12) {
        kbd = gpKeyboards[1];
    } else {
        ASSERT(!"Shouldn't have received this IRQ!");
    }

    memset(sequence, 0, 16);

    while(remains > 0) {
        ASSERT(remains <= 16);
        if(PS2_ReadData(&buf)) {
            sequence[len++] = buf;
            if(buf == PREFIX_EXTRA) {
                remains = 2;
            } else if(buf == PREFIX_RELEASE) {
                remains = 1;
            } else {
                remains = 0;
            }
        } else {
            len = 0;
            break;
        }
    }

    if(len > 0) {
        switch(sequence[0]) {
            case 0xEE:
            kbd->echo = true;
            break;
            case 0xFA:
            kbd->ack = true;
            break;
            case 0xFE:
            kbd->resend = true;
            break;
            default:
            if(MapSequenceToVK(len, sequence, &vk, &released)) {
                // TODO: store this info somewhere
                // TODO: keypress packet
                Keyboard_Event ev;
                ev.vk = (u32)vk;
                ev.flags = kbd->flags;
                if(released) ev.flags |= KBEV_RELEASED;

                if(vk == VK_LSHIFT || vk == VK_RSHIFT) {
                    if(released) {
                        kbd->flags &= ~KBEV_SHIFT;
                    } else {
                        kbd->flags |= KBEV_SHIFT;
                    }
                } else if(vk == VK_LCTRL || vk == VK_RCTRL) {
                    if(released) {
                        kbd->flags &= ~KBEV_CTRL;
                    } else {
                        kbd->flags |= KBEV_CTRL;
                    }
                }

                //logprintf("kbd: sc=%d flags=%x\n", ev.vk, ev.flags);
                kbd->event_buffer.push(ev);
            } else {
                //logprintf("kbd: cant map sequence\n");
            }
            break;
        }
    }
}

static void SC_PollKbd(Registers* regs) {
    ASSERT(regs->eax == SYSCALL_POLLKBD);
    u32 kbd = regs->ecx;
    Keyboard_Event* dst = (Keyboard_Event*)regs->edx;
    regs->eax = 0;

    if(kbd < 2 && gpKeyboards[kbd]) {
        auto state = gpKeyboards[kbd];

        if(state->event_buffer.pop(dst)) {
            regs->eax = 1;
        }
    } else {
    }
}

static void RegisterSyscall() {
    if(!gbRegisteredSyscall) {
        gbRegisteredSyscall = true;

        RegisterSyscallHandler(SYSCALL_POLLKBD, &SC_PollKbd);
    }
}

static char bt_bp_map[] = {
    '`', '-', '=', '[', ']', ';', '\'', '\\', ',', '.', '/', '\n', '\b'
};

static char bt_bp_shift_map[] = {
    '~', '_', '+', '{', '}', ':', '"', '|', '<', '>', '?', '\n', '\b'
};

static const char* vt_seq_map[] = {
    "\033[11~", "\033[12~", "\033[13~", "\033[14~", "\033[15~", "\033[17~",
    "\033[18~", "\033[19~", "\033[20~", "\033[21~", "\033[23~", "\033[24~",
    "", "", "", "", "", "", "", "", "", "", "", "", "", 
    "\033[7~", "\033[8~", "\033[2~", "\033[3~", "\033[5~", "\033[6~", 
    "", "", "", "", "", "", "", "",
    "\033[A", "\033[D", "\033[B", "\033[C",
};

// TODO: This doesn't belong here
template<u32 Size>
static bool TranslateControlSequence(Ring_Buffer<char, Size>* dst, const Keyboard_Event& ev) {
    char ch;

    if(ev.vk >= VK_0 && ev.vk <= VK_9) {
        ch = '0' + (ev.vk - VK_0);
        goto single_char;
    } else if(ev.vk >= VK_a && ev.vk <= VK_z) {
        if(ev.flags & KBEV_CTRL) {
            switch(ev.vk) {
                case VK_d:
                    ch = '\x04'; // EOT
                    break;
                default:
                    dst->push('^');
                    dst->push('A' + (ev.vk - VK_a));
                    goto mapped;
            }
        } else {
            ch = ((ev.flags & KBEV_SHIFT) ? 'A' : 'a') + (ev.vk - VK_a);
        }
        goto single_char;
    } else if(ev.vk == VK_SPACE) {
        ch = ' ';
        goto single_char;
    } else if(ev.vk == VK_ESCAPE) {
        ch = '\033';
        goto single_char;
    } else if(ev.vk >= VK_BACKTICK && ev.vk <= VK_BACKSPACE) {
        ch = ((ev.flags & KBEV_SHIFT) ? bt_bp_shift_map : bt_bp_map)[ev.vk - VK_BACKTICK];
        goto single_char;
    } else if(ev.vk >= VK_F1 && ev.vk <= VK_RIGHT) {
        auto seq = vt_seq_map[ev.vk - VK_F1];
        while(*seq) {
            dst->push(*seq);
            seq++;
        }
        goto mapped;
    } else {
        goto cant_map;
    }

single_char:
    dst->push(ch);
mapped:
    return true;
cant_map:
    return false;
}

static bool CHDEV_Send(void* user, char ch) {
    (void)user;
    (void)ch;
    return false;
}
static bool CHDEV_Recv(void* user, char* ch) {
    bool ret = false;
    ASSERT(user != NULL);
    auto kbd = (PS2_Keyboard*)user;
    Keyboard_Event ev;

    if(kbd->char_buffer.pop(ch)) {
        ret = true;
    } else {
        if(kbd->event_buffer.pop(&ev)) {
            if((ev.flags & KBEV_RELEASED) && TranslateControlSequence(&kbd->char_buffer, ev)) {
                if(kbd->char_buffer.pop(ch)) {
                    ret = true;
                }
            }
        }
    }

    return ret;
}

static struct Character_Device_Descriptor gTTY = {
    .Name = "PS/2 Keyboard",
    .Send = CHDEV_Send,
    .Recv = CHDEV_Recv,
};

void PS2_Initialize_MF2_Keyboard(u32 dev) {
    ASSERT(dev < 2);
    ASSERT(gpKeyboards[dev] == NULL);
    PS2_Keyboard* state = (PS2_Keyboard*)kmalloc(sizeof(PS2_Keyboard));
    memset(state, 0, sizeof(PS2_Keyboard));
    gpKeyboards[dev] = state;

    state->dev = dev;
    state->flags = 0;

    RegisterSyscall();
    //Interrupts_Register_Handler(dev == 0 ? IRQ1 : IRQ12, IRQHandler);
    auto irq = dev == 0 ? IRQ1 : IRQ12;
    PIC_Unmask(irq);
    Interrupts_Register_Handler(irq, IRQHandler);
    
    SetLEDs(state, 7);
    SetScancodeSet(state, 2);

    EnableScanning(state);

    if(!SendEcho(state)) {
        logprintf("ps2: keyboard echo failed\n");
        ASSERT(0);
    }
    SetLEDs(state, 0);

    CharDev_Register(state, &gTTY);
}
