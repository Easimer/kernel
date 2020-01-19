#include "common.h"
#include "uart.h"
#include "port_io.h"
#include "logging.h"

#define BUFFER_SIZE (8)

struct UART_State {
    u16 addr;
    u8 buffer[BUFFER_SIZE];
    int buffer_count;
};

static UART_State gPorts[4];

static int PortIDToPortAddr(int port) {
    switch(port) {
        case PORT_COM1: return 0x03F8;
        case PORT_COM2: return 0x02F8;
        case PORT_COM3: return 0x03E8;
        case PORT_COM4: return 0x02E8;
    }
    return 0;
}

static bool IsTransmitEmpty(u16 addr) {
    return (inb(addr + 5) & 0x20) != 0;
}

static void PutChar(u16 addr, char ch) {
    while(!IsTransmitEmpty(addr));

    outb(addr, ch);
}

static void Flush(UART_State* state) {
    u8* buf = state->buffer;
    for(int i = 0; i < state->buffer_count; i++) {
        PutChar(state->addr, *buf);
        buf++;
    }
    state->buffer_count = 0;
}

static void WriteChar(UART_State* state, char ch) {
    // Place char into buffer
    state->buffer[state->buffer_count++] = ch;

    // Flush when full or newline
    if(state->buffer_count == BUFFER_SIZE || ch == '\n') {
        Flush(state);
    }
}

static void UART_WriteString(void* user, const char* s) {
    auto state = (UART_State*)user;

    while(*s) {
        WriteChar(state, *s);
        s++;
    }
}

static void UART_WriteChar(void* user, char ch) {
    auto state = (UART_State*)user;

    WriteChar(state, ch);
}

void UART_PutChar(int port, char ch) {
    if(port >= 0 && port < 4) {
        auto user = &gPorts[port];
        UART_WriteChar(user, ch);
    }
}

void UART_Flush(int port) {
    if(port >= 0 && port < 4) {
        auto user = &gPorts[port];
        Flush(user);
    }
}

static Log_Destination gLogDst = {
    .WriteString = UART_WriteString,
    .WriteChar = UART_WriteChar,
};

void UART_Setup(int port) {
    auto addr = PortIDToPortAddr(port);

    outb(addr + 1, 0x00); // Disable all interrupts
    outb(addr + 3, 0x80); // Enable DLAB
    // Set baud rate to 9600
    outb(addr + 0, 12); // Speed low-byte
    outb(addr + 1, 0); // Speed high-byte
    outb(addr + 3, 0x03); // 8N1
    outb(addr + 2, 0xC7); // Enable FIFO, clear FIFO, 14-byte threshold
    outb(addr + 4, 0x0B); // Enable IRQs, RTS/DSR set

    gPorts[port].buffer_count = 0;
    gPorts[port].addr = addr;
    Log_Register(gPorts + port, &gLogDst);
}

void UART_Shutdown(int port) {
    auto addr = PortIDToPortAddr(port);

    outb(addr + 1, 0x00); // Disable all interrupts
}

