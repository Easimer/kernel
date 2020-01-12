#include "common.h"
#include "pc_vga.h"
#include "logging.h"
#include "simd.h"

#define VGA_WIDTH (80)
#define VGA_HEIGHT (25)

#define MAKE_COL(bg, fg) (fg | (bg << 4))
#define MAKE_CHAR(bg, fg, ch) (ch | (MAKE_COL(bg, fg) << 8))

#define COL_BLACK         (0)
#define COL_BLUE          (1)
#define COL_GREEN         (2)
#define COL_CYAN          (3)
#define COL_RED           (4)
#define COL_MAGENTA       (5)
#define COL_BROWN         (6)
#define COL_LIGHT_GREY    (7)
#define COL_DARK_GREY     (8)
#define COL_LIGHT_BLUE    (9)
#define COL_LIGHT_GREEN   (10)
#define COL_LIGHT_CYAN    (11)
#define COL_LIGHT_RED     (12)
#define COL_LIGHT_MAGENTA (13)
#define COL_LIGHT_BROWN   (14)
#define COL_WHITE         (15)

struct PCVGA_State {
    u32 row, column;
    u16* buffer;
};

static PCVGA_State vga;

static void PCVGA_ScrollBack() {
    // Copy nth line into the n-1th line
    for(u32 y = 1; y < VGA_HEIGHT; y++) {
        __m128i chk0, chk1, chk2, chk3, chk4;
        __m128i chk5, chk6, chk7, chk8, chk9;
        u32 base_prev = (y - 1) * VGA_WIDTH;
        u32 base_cur = y * VGA_WIDTH;

        u8* ptr_cur = (u8*)(vga.buffer + base_cur);
        u8* ptr_prv = (u8*)(vga.buffer + base_prev);

        chk0 = _mm_load_si128((__m128i*)(ptr_cur +   0));
        chk1 = _mm_load_si128((__m128i*)(ptr_cur +  16));
        chk2 = _mm_load_si128((__m128i*)(ptr_cur +  32));
        chk3 = _mm_load_si128((__m128i*)(ptr_cur +  48));
        chk4 = _mm_load_si128((__m128i*)(ptr_cur +  64));
        chk5 = _mm_load_si128((__m128i*)(ptr_cur +  80));
        chk6 = _mm_load_si128((__m128i*)(ptr_cur +  96));
        chk7 = _mm_load_si128((__m128i*)(ptr_cur + 112));
        chk8 = _mm_load_si128((__m128i*)(ptr_cur + 128));
        chk9 = _mm_load_si128((__m128i*)(ptr_cur + 144));

        _mm_store_si128((__m128i*)(ptr_prv +   0), chk0);
        _mm_store_si128((__m128i*)(ptr_prv +  16), chk1);
        _mm_store_si128((__m128i*)(ptr_prv +  32), chk2);
        _mm_store_si128((__m128i*)(ptr_prv +  48), chk3);
        _mm_store_si128((__m128i*)(ptr_prv +  64), chk4);
        _mm_store_si128((__m128i*)(ptr_prv +  80), chk5);
        _mm_store_si128((__m128i*)(ptr_prv +  96), chk6);
        _mm_store_si128((__m128i*)(ptr_prv + 112), chk7);
        _mm_store_si128((__m128i*)(ptr_prv + 128), chk8);
        _mm_store_si128((__m128i*)(ptr_prv + 144), chk9);
    }

    // Clear last line
    u32 base_last = (VGA_HEIGHT - 1) * VGA_WIDTH;
    for(u32 x = 0; x < VGA_WIDTH; x++) {
        vga.buffer[base_last + x] = MAKE_CHAR(COL_BLACK, COL_LIGHT_GREY, ' ');
    }
}

static void PCVGA_PutChar(char c) {
    if(c != '\n') {
        vga.buffer[vga.row * VGA_WIDTH + vga.column] = MAKE_CHAR(COL_BLACK, COL_LIGHT_GREY, c);
        vga.column++;
        if(vga.column == VGA_WIDTH) {
            vga.column = 0;
            vga.row++;
            // See: Check row
        }
    } else {
        vga.column = 0;
        vga.row += 1;
        // See: Check row
    }

    // Check row
    if(vga.row == VGA_HEIGHT) {
        PCVGA_ScrollBack();
        vga.row -= 1;
    }
}

static void PCVGA_WriteString(void* user, const char* string) {
    while(*string) {
        PCVGA_PutChar(*string);
        string++;
    }
}

static void PCVGA_WriteChar(void* user, char c) {
    PCVGA_PutChar(c);
}

static Log_Destination gPCVGA_LogDst = {
    .WriteString = PCVGA_WriteString,
    .WriteChar = PCVGA_WriteChar,
};

void PCVGA_Init() {
    vga.row = vga.column = 0;
    vga.buffer = (u16*)0xB8000;

    // Clear screen
    for(u32 y = 0; y < VGA_HEIGHT; y++) {
        for(u32 x = 0; x < VGA_WIDTH; x++) {
            vga.buffer[y * VGA_WIDTH + x] = MAKE_CHAR(COL_BLACK, COL_LIGHT_GREY, ' ');
        }
    }

    vga.buffer[0] = MAKE_CHAR(COL_BLACK, COL_LIGHT_GREY, 'V');
    vga.buffer[1] = MAKE_CHAR(COL_BLACK, COL_LIGHT_GREY, 'G');
    vga.buffer[2] = MAKE_CHAR(COL_BLACK, COL_LIGHT_GREY, 'A');
    vga.buffer[3] = MAKE_CHAR(COL_BLACK, COL_LIGHT_GREY, ' ');
    vga.buffer[4] = MAKE_CHAR(COL_BLACK, COL_LIGHT_GREY, 'O');
    vga.buffer[5] = MAKE_CHAR(COL_BLACK, COL_LIGHT_GREY, 'K');

    Log_Register(NULL, &gPCVGA_LogDst);
}
