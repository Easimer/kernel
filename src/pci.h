#ifndef KERNEL_PCI_H
#define KERNEL_PCI_H

#include "common.h"

using PCI_Address = u32;

#define PCI_MAKE_ADDRESS(bus, slot, func) (((u32)bus << 16) | ((u32)slot << 11) | ((u32)func << 8))

struct PCI_Device {
    PCI_Address address;
    u16 vendor, device;

    void* dev_private;
};

using PCI_Driver_Probe = int (*)(const PCI_Device* device);

#define PCI_PROBE_OK  (0)
#define PCI_PROBE_ERR (1)

using PCI_Driver_Poll = void (*)(const PCI_Device* device);

struct PCI_Driver {
    const char* Name;
    PCI_Driver_Probe Probe;
    PCI_Driver_Poll Poll;
};

using PCI_Driver_Init = PCI_Driver* (*)();

void PCI_Enumerate();
u32 PCI_ReadCfgReg(PCI_Address devaddr, u8 offset);
u32 PCI_ReadCfgReg(u8 bus, u8 slot, u8 func, u8 offset);

inline void PCI_Cfg_ReadID(PCI_Address addr, u16* vendor, u16* device) {
    u32 reg = PCI_ReadCfgReg(addr, 0x00);
    *vendor = (reg & 0xFFFF);
    *device = (reg >> 16);
}

inline void PCI_Cfg_ReadClass(PCI_Address addr, u8* dev_class, u8* dev_subclass) {
    u32 reg = PCI_ReadCfgReg(addr, 0x08);
    *dev_class = (reg >> 24) & 0xFF;
    *dev_subclass = (reg >> 16) & 0xFF;
}

inline u32 PCI_Cfg_ReadBAR(PCI_Address addr, u32 id) {
    u32 ret = 0xFFFFFFFF;

    if(id < 6) {
        ret = PCI_ReadCfgReg(addr, 0x10 + id);
    }

    return ret;
}

void PCI_Register_Module(PCI_Driver_Init init);

struct PCI_Driver_Register_Proxy {
    PCI_Driver_Register_Proxy(PCI_Driver_Init init) {
        PCI_Register_Module(init);
    }
};

#define REGISTER_PCI_DRIVER(init) static PCI_Driver_Register_Proxy __devproxy(init)

#endif /* KERNEL_PCI_H */