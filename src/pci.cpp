#include "common.h"
#include "pci.h"
#include "port_io.h"
#include "logging.h"

#define PORT_CFG_ADDR (0xCF8)
#define PORT_CFG_DATA (0xCFC)

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
                }
            }
        }
    }
}

void test_init() {

}

static PCI_Driver_Register_Proxy proxy(test_init);