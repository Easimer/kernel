#include "common.h"
#include "pci.h"
#include "logging.h"

static int IDE_Probe(const PCI_Device* dev) {
    u8 cls, scls;
    
    PCI_Cfg_ReadClass(dev->address, &cls, &scls);
    if(cls != 0x01 || scls != 0x01) {
        // Not a mass storage device
        return PCI_PROBE_ERR;
    }

    return PCI_PROBE_OK;
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