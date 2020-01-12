#include "common.h"
#include "pci.h"
#include "disk.h"
#include "memory.h"
#include "logging.h"

struct IDE_Disk {

};

bool IDE_Disk_Read(void* user, u32* bytes_read, void* buf, u32 siz, u32 off) {
    auto disk = (IDE_Disk*)user;
    return false;
}

bool IDE_Disk_Write(void* user, u32* bytes_written, const void* buf, u32 siz, u32 off) {
    auto disk = (IDE_Disk*)user;
    return false;
}

bool IDE_Disk_Flush(void* user) {
    auto disk = (IDE_Disk*)user;
    return true;
}

static Disk_Device_Descriptor gDiskDesc = {
    .Read = IDE_Disk_Read,
    .Write = IDE_Disk_Write,
    .Flush = IDE_Disk_Flush,
};

static int IDE_Probe(const PCI_Device* dev) {
    int ret;
    u8 cls, scls;
    
    PCI_Cfg_ReadClass(dev->address, &cls, &scls);
    if(cls != 0x01 || scls != 0x01) {
        // Not a mass storage device
        return PCI_PROBE_ERR;
    }

    IDE_Disk* user = (IDE_Disk*)kmalloc(sizeof(IDE_Disk));

    if(user) {
        if(Disk_Register_Device(user, &gDiskDesc)) {
            ret = PCI_PROBE_OK;
        } else {
            logprintf("PCI IDE: couldn't register disk\n");
            ret = PCI_PROBE_ERR;
        }
    } else {
        logprintf("PCI IDE: couldn't allocate memory for disk state, out of memory?\n");
        ret = PCI_PROBE_ERR;
    }

    return ret;
}

static PCI_Driver IDE_Driver = {
    .Name = "PCI IDE Controller",
    .Probe = IDE_Probe,
    .Poll = NULL,
};

static PCI_Driver* IDE_Init() {
    return &IDE_Driver;
}

REGISTER_PCI_DRIVER(IDE_Init);