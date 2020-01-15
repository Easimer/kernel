#include "common.h"
#include "logging.h"
#include "interrupts.h"
#include "syscalls.h"
#include "utils.h"

#include <stdarg.h>

struct Log_Destination_Entry {
    void* user;
    bool occupied;
    const Log_Destination* desc;
};

#define LOG_DEST_ENTRY_MAXCOUNT (4)

static Log_Destination_Entry gEntries[LOG_DEST_ENTRY_MAXCOUNT];

static void Syscall_Print(Registers* regs) {
    switch(regs->eax) {
        case SYSCALL_PRINT:
            Log_LogString((const char*)regs->esi);
            break;
        case SYSCALL_PRINTCH:
            Log_LogChar((char)((regs->edx) & 0xFF));
            break;
        default: ASSERT(0); break;
    }
}

void Log_Init() {
    for(int i = 0; i < LOG_DEST_ENTRY_MAXCOUNT; i++) {
        gEntries[i].occupied = false;
    }

    RegisterSyscallHandler(SYSCALL_PRINT, Syscall_Print);
}

void Log_Register(void* user, const Log_Destination* dest) {
    int index = -1;
    for(int i = 0; i < LOG_DEST_ENTRY_MAXCOUNT; i++) {
        if(!gEntries[i].occupied) {
            index = i;
            break;
        }
    }

    if(index != -1) {
        gEntries[index].occupied = true;
        gEntries[index].user = user;
        gEntries[index].desc = dest;
    } else {
        // Out of log destination slots!!!
    }
}

void Log_LogString(const char* str) {
    for(int i = 0; i < LOG_DEST_ENTRY_MAXCOUNT; i++) {
        auto& dst = gEntries[i];

        if(dst.occupied) {
            // Does this destination support the WriteString operation
            if(dst.desc->WriteString) {
                dst.desc->WriteString(dst.user, str);
            } else {
                // It does not, try WriteChar
                if(dst.desc->WriteChar) {
                    while(*str) {
                        dst.desc->WriteChar(dst.user, *str);
                        str++;
                    }
                }
            }
        }
    }
}

void Log_LogChar(char ch) {
    for(int i = 0; i < LOG_DEST_ENTRY_MAXCOUNT; i++) {
        auto& dst = gEntries[i];

        if(dst.occupied) {
            // Does this destination support the WriteChar operation
            if(dst.desc->WriteChar) {
                dst.desc->WriteChar(dst.user, ch);
            } else {
                // It does not, try WriteString
                if(dst.desc->WriteString) {
                    char buf[2] = {ch, 0};
                    dst.desc->WriteString(dst.user, buf);
                }
            }
        }
    }
}

static char val2digit(int v, int base) {
    if(v > 0 && v < 10)
        return (char)(v + 0x30);
    if (v >= 10) {
        return (char)(v - 10 + 0x41);
    }
    return '0';
}

static u32 itoa_noflip(char* buf, u32 bufsiz, s32 v, int base) {
    u32 i = 0;
    bool neg = v < 0;
    u32 reallimit = bufsiz - 1;
    if (neg) {
        reallimit -= 1;
        v *= -1;
    }
    do {
        buf[i++] = val2digit(v % base, base);
        v /= base;
    } while(v && i != reallimit);
    if (neg) {
        buf[i] = '-';
        buf[i + 1] = '\0';
        i++;
    } else {
        buf[i] = '\0';
    }
    return i;
}

static u32 utoa_noflip(char* buf, u32 bufsiz, u32 v, int base) {
	u32 i = 0;
	do {
		buf[i++] = val2digit(v % base, base);
		v /= base;
	} while (v && i != bufsiz - 1);
	buf[i] = '\0';
	return i;
}

static char* utoa(char* buf, u32 bufsiz, u32 v, int base) {
	u32 j = utoa_noflip(buf, bufsiz, v, base) - 1;
	u32 i = 0;
	char t;
	while (i < j) {
		t = buf[i];
		buf[i] = buf[j];
		buf[j] = t;
		i++; j--;
	}
    
	return buf;
}

static char* itoa(char* buf, u32 bufsiz, s32 v, int base) {
    u32 j = itoa_noflip(buf, bufsiz, v, base) - 1;
    u32 i = 0;
    char t;
    while (i < j) {
        t = buf[i];
        buf[i] = buf[j];
        buf[j] = t;
        i++; j--;
    }

    return buf;
}

void Log_LogInt(s32 x) {
    char buffer[32];
    itoa(buffer, 32, x, 10);
    Log_LogString(buffer);
}

void Log_LogHex(u32 x) {
    char buffer[34];
    buffer[0] = '0';
    buffer[1] = 'x';
    utoa(buffer + 2, 32, x, 16);
    Log_LogString(buffer);
}

// Supported: %s %d %x %c %%
void logprintf(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int step = 0;

    while(*format) {
        char cur = *format;
        if(cur == '%') {
            char fmt = format[1];
            step = 2;
            switch(fmt) {
                case '%':
                    Log_LogChar('%');
                    break;
                case 'c':
                    Log_LogChar(va_arg(ap, int));
                    break;
                case 's':
                    Log_LogString(va_arg(ap, const char*));
                    break;
                case 'd':
                    Log_LogInt(va_arg(ap, s32));
                    break;
                case 'x':
                    Log_LogHex(va_arg(ap, u32));
                    break;
                case '\0':
                    // truncated
                    step = 1;
                    break;
                default:
                    Log_LogChar('%');
                    Log_LogChar(fmt);
                    break;
            }
        } else {
            Log_LogChar(cur);
            step = 1;
        }
        format += step;
    }

    va_end(ap);
}
