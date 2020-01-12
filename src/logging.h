#ifndef KERNEL_LOGGING_H
#define KERNEL_LOGGING_H

using Log_Destination_WriteString = void (*)(void* user, const char* string);
using Log_Destination_WriteChar = void (*)(void* user, char c);

struct Log_Destination {
    Log_Destination_WriteString WriteString;
    Log_Destination_WriteChar WriteChar;
};

void Log_Init();
void Log_Register(void* user, const Log_Destination* dest);
void Log_LogString(const char* str);
void Log_LogChar(char ch);
void Log_LogInt(s32 x);
void Log_LogHex(u32 x);

void logprintf(const char* format, ...);

#endif /* KERNEL_LOGGING_H */
