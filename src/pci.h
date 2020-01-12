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

using PCI_Driver_Poll = void (*)(void* user);

struct PCI_Driver {
    const char* Name;
    PCI_Driver_Probe Probe;
    PCI_Driver_Poll Poll;
};

void PCI_Enumerate();
u32 PCI_ReadCfgReg(PCI_Address devaddr, u8 offset);
u32 PCI_ReadCfgReg(u8 bus, u8 slot, u8 func, u8 offset);

inline void PCI_Cfg_ReadID(PCI_Address addr, u16* vendor, u16* device) {
    u32 reg = PCI_ReadCfgReg(addr, 0x00);
    *vendor = (reg & 0xFFFF);
    *device = (reg >> 16);
}

#include "logging.h"
struct PCI_Driver_Register_Proxy {
    PCI_Driver_Register_Proxy(void(*module_init)()) {
        logprintf("Module init: %x\n", module_init);
    }
};

#endif /* KERNEL_PCI_H */