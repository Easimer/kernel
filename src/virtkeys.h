#ifndef KERNEL_VIRTKEYS_H
#define KERNEL_VIRTKEYS_H

enum Virtual_Key {
    VK_0 = 0,
    VK_1,
    VK_2,
    VK_3,
    VK_4,
    VK_5,
    VK_6,
    VK_7,
    VK_8,
    VK_9,

    VK_a,
    VK_b,
    VK_c,
    VK_d,
    VK_e,
    VK_f,
    VK_g,
    VK_h,
    VK_i,
    VK_j,
    VK_k,
    VK_l,
    VK_m,
    VK_n,
    VK_o,
    VK_p,
    VK_q,
    VK_r,
    VK_s,
    VK_t,
    VK_u,
    VK_v,
    VK_w,
    VK_x,
    VK_y,
    VK_z,
    VK_SPACE,

    VK_ESCAPE,
    VK_F1,
    VK_F2,
    VK_F3,
    VK_F4,
    VK_F5,
    VK_F6,
    VK_F7,
    VK_F8,
    VK_F9,
    VK_F10,
    VK_F11,
    VK_F12,

    VK_BACKTICK,
    VK_MINUS,
    VK_EQUALS,
    
    VK_LBRACKET,
    VK_RBRACKET,
    
    VK_SEMICOLON,
    VK_QUOTE,
    VK_BACKSLASH,
    
    VK_COMMA,
    VK_PERIOD,
    VK_SLASH,

    VK_RETURN,
    VK_BACKSPACE,

    VK_HOME,
    VK_END,
    VK_INSERT,
    VK_DELETE,
    VK_PGUP,
    VKPGDOWN,

    VK_LSHIFT,
    VK_CAPSLOCK,
    VK_TAB,

    VK_LCTRL,
    VK_GUI,
    VK_META,

    VK_RCTRL,
    VK_RSHIFT,

    VK_UP,
    VK_LEFT,
    VK_DOWN,
    VK_RIGHT,

    VK_KP0,
    VK_KP1,
    VK_KP2,
    VK_KP3,
    VK_KP4,
    VK_KP5,
    VK_KP6,
    VK_KP7,
    VK_KP8,
    VK_KP9,

    VK_KPSLASH,
    VK_KPASTERISK,
    VK_KPMINUS,
    VK_KPPLUS,
    VK_KPRETURN,
    VK_KPPERIOD,
    VK_NUMLOCK,

    VK_SCROLLLOCK,
    VK_PRINTSCR,
    VK_PAUSE,

    VK_UNKNOWN
};

#define KBEV_RELEASED (0x01)
#define KBEV_SHIFT    (0x02)
#define KBEV_CTRL     (0x04)

struct Keyboard_Event {
    u32 vk;
    u32 flags;
};

#endif /* KERNEL_VIRTKEYS_H */
