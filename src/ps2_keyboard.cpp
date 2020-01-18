#include "common.h"
#include "ps2.h"
#include "memory.h"
#include "utils.h"
#include "virtkeys.h"
#include "interrupts.h"
#include "ring_buffer.h"

#include "logging.h"

#define MAX_BUFFERED_EVENTS (64)

struct PS2_Keyboard {
    u32 dev;
    u8 flags;
    Ring_Buffer<Keyboard_Event, MAX_BUFFERED_EVENTS> event_buffer;
};

static PS2_Keyboard* gpKeyboards[2] = { NULL, NULL };

#define PREFIX_EXTRA (0xE0)
#define PREFIX_RELEASE (0xF0)

#define UNK VK_UNKNOWN

// Virtual key mappings
// Normal scancodes
static Virtual_Key VkMap_1B[256] = {
    UNK, VK_F9, UNK, VK_F5,
    VK_F3, VK_F1, VK_F2, VK_F12,
    UNK, VK_F10, VK_F8, UNK,
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

static bool EnableScanning(PS2_Keyboard* kb) {
    bool ret = false;
    u8 res;

    ASSERT(kb);
    PS2_SendToDevice(kb->dev, 0xF4);

    if(PS2_ReadDataWithTimeout(1000, &res)) {
        if(res == 0xFA) {
            ret = true;
        }
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
        if(PS2_ReadDataWithTimeout(1000, &buf)) {
            sequence[len++] = buf;
            if(buf == PREFIX_EXTRA) {
                remains = 2;
            } else if(buf == PREFIX_RELEASE) {
                remains = 1;
            } else {
                remains = 0;
            }
        }
    }

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

        kbd->event_buffer.push(ev);
    } else {
    }
}

void PS2_Initialize_MF2_Keyboard(u32 dev) {
    ASSERT(dev < 2);
    ASSERT(gpKeyboards[dev] == NULL);
    PS2_Keyboard* state = (PS2_Keyboard*)kmalloc(sizeof(PS2_Keyboard));
    gpKeyboards[dev] = state;

    state->dev = dev;
    state->flags = 0;

    Interrupts_Register_Handler(dev == 0 ? IRQ1 : IRQ12, IRQHandler);

    EnableScanning(state);
}
