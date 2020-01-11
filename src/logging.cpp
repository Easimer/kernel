#include "common.h"
#include "logging.h"

struct Log_Destination_Entry {
    void* user;
    bool occupied;
    const Log_Destination* desc;
};

#define LOG_DEST_ENTRY_MAXCOUNT (4)

static Log_Destination_Entry gEntries[LOG_DEST_ENTRY_MAXCOUNT];

void Log_Init() {
    for(int i = 0; i < LOG_DEST_ENTRY_MAXCOUNT; i++) {
        gEntries[i].occupied = false;
    }
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

