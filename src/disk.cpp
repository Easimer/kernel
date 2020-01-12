#include "common.h"
#include "disk.h"
#include "logging.h"
#include "utils.h"
#include "memory.h"

struct Disk_Device {
    void* user;
    const Disk_Device_Descriptor* desc;
};

#define MAX_DISKS (64)

static Disk_Device gaDisks[MAX_DISKS];
static u32 giDisksLastIndex = 0;

bool Disk_Register_Device(void* user, const Disk_Device_Descriptor* desc) {
    bool ret = false;

    if(desc) {
        if(giDisksLastIndex < MAX_DISKS) {
            gaDisks[giDisksLastIndex].user = user;
            gaDisks[giDisksLastIndex].desc = desc;

            logprintf("Registered disk #%d (%x, %x)\n", giDisksLastIndex, user, desc);

            giDisksLastIndex++;
            ret = true;
        }
    }

    return ret;
}