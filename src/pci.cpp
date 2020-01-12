#include "common.h"
#include "pci.h"
#include "port_io.h"
#include "logging.h"
#include "memory.h"
#include "utils.h"

#define PORT_CFG_ADDR (0xCF8)
#define PORT_CFG_DATA (0xCFC)

struct PCI_Device_Descriptor {
    PCI_Device device;
    PCI_Driver* driver;
};

struct PCI_Device_List {
    PCI_Device_Descriptor dev;
    PCI_Device_List* next;
};

#define MAX_DRIVERS (128)

static PCI_Driver* gaDrivers[MAX_DRIVERS];
static u32 giDriversLastIndex = 0;

static PCI_Device_List *gpDevices = NULL, *gpDevicesLast = NULL;

u32 PCI_ReadCfgReg(PCI_Address devaddr, u8 offset) {
    u32 ret;
    u32 addr = (devaddr | (offset & 0xFC) | 0x80000000);
    outd(PORT_CFG_ADDR, addr);
    ret = ind(PORT_CFG_DATA);
    return ret;
}

u32 PCI_ReadCfgReg(u8 bus, u8 slot, u8 func, u8 offset) {
    //u32 ret;

    //u32 addr = (((u32)bus << 16) | ((u32)slot << 11) | ((u32)func << 8) | (offset & 0xFC) | 0x80000000);
    //outd(PORT_CFG_ADDR, addr);
    //ret = ind(PORT_CFG_DATA);

    //return ret;

    return PCI_ReadCfgReg(PCI_MAKE_ADDRESS(bus, slot, func), offset);
}

void PCI_Enumerate() {
    for(u16 bus = 0; bus < 256; bus++) {
        for(u8 slot = 0; slot < 32; slot++) {
            for(u8 func = 0; func < 8; func++) {
                u16 vendor, device;
                auto addr = PCI_MAKE_ADDRESS(bus, slot, func);
                PCI_Cfg_ReadID(addr, &vendor, &device);

                if(vendor != 0xFFFF) {
                    logprintf("%x:%x:%d Found device: %x:%x\n", bus, slot, func, vendor, device);
                    PCI_Device_List* elem = (PCI_Device_List*)kmalloc(sizeof(PCI_Device_List));
                    elem->next = NULL;
                    elem->dev.device.vendor = vendor;
                    elem->dev.device.device = device;
                    elem->dev.device.dev_private = NULL;
                    elem->dev.device.address = addr;

                    if(gpDevicesLast) {
                        gpDevicesLast->next = elem;
                        gpDevicesLast = elem;
                    } else {
                        gpDevicesLast = gpDevices = elem;
                    }

                    int res = PCI_PROBE_ERR;
                    for(u32 iDriver = 0; iDriver < giDriversLastIndex && res != PCI_PROBE_OK; iDriver++) {
                        res = gaDrivers[iDriver]->Probe(&elem->dev.device);
                        if(res == PCI_PROBE_OK) {
                            logprintf("Found driver '%s' for device %x:%x.%d\n", gaDrivers[iDriver]->Name, bus, slot, func);
                        }
                    }
                    if(res != PCI_PROBE_OK) {
                        logprintf("No driver for PCI device %x:%x.%d!\n", bus, slot, func);
                    }
                }
            }
        }
    }
}

void PCI_Register_Module(PCI_Driver_Init init) {
    auto driver = init();

    if(driver) {
        ASSERT(giDriversLastIndex < MAX_DRIVERS);

        gaDrivers[giDriversLastIndex++] = driver;

        logprintf("Initialized driver '%s'!\n", driver->Name);
    }
}

PCI_Driver* test_init() {
    return NULL;
}

static PCI_Driver_Register_Proxy proxy(test_init);