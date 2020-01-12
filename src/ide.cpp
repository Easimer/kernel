#include "common.h"
#include "pci.h"
#include "disk.h"
#include "memory.h"
#include "logging.h"
#include "port_io.h"
#include "timer.h"

#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
#define ATA_REG_FEATURES   0x01
#define ATA_REG_SECCOUNT0  0x02
#define ATA_REG_LBA0       0x03
#define ATA_REG_LBA1       0x04
#define ATA_REG_LBA2       0x05
#define ATA_REG_HDDEVSEL   0x06
#define ATA_REG_COMMAND    0x07
#define ATA_REG_STATUS     0x07
#define ATA_REG_SECCOUNT1  0x08
#define ATA_REG_LBA3       0x09
#define ATA_REG_LBA4       0x0A
#define ATA_REG_LBA5       0x0B
#define ATA_REG_CONTROL    0x0C
#define ATA_REG_ALTSTATUS  0x0C
#define ATA_REG_DEVADDRESS 0x0D

#define ATA_SR_BSY     0x80    // Busy
#define ATA_SR_DRDY    0x40    // Drive ready
#define ATA_SR_DF      0x20    // Drive write fault
#define ATA_SR_DSC     0x10    // Drive seek complete
#define ATA_SR_DRQ     0x08    // Data request ready
#define ATA_SR_CORR    0x04    // Corrected data
#define ATA_SR_IDX     0x02    // Index
#define ATA_SR_ERR     0x01    // Error

#define ATA_ER_BBK      0x80    // Bad block
#define ATA_ER_UNC      0x40    // Uncorrectable data
#define ATA_ER_MC       0x20    // Media changed
#define ATA_ER_IDNF     0x10    // ID mark not found
#define ATA_ER_MCR      0x08    // Media change request
#define ATA_ER_ABRT     0x04    // Command aborted
#define ATA_ER_TK0NF    0x02    // Track 0 not found
#define ATA_ER_AMNF     0x01    // No address mark

#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_READ_DMA          0xC8
#define ATA_CMD_READ_DMA_EXT      0x25
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_WRITE_DMA         0xCA
#define ATA_CMD_WRITE_DMA_EXT     0x35
#define ATA_CMD_CACHE_FLUSH       0xE7
#define ATA_CMD_CACHE_FLUSH_EXT   0xEA
#define ATA_CMD_PACKET            0xA0
#define ATA_CMD_IDENTIFY_PACKET   0xA1
#define ATA_CMD_IDENTIFY          0xEC

#define IDE_ATA        0x00
#define IDE_ATAPI      0x01
 
#define ATA_MASTER     0x00
#define ATA_SLAVE      0x01

#define ATA_IDENT_DEVICETYPE   0
#define ATA_IDENT_CYLINDERS    2
#define ATA_IDENT_HEADS        6
#define ATA_IDENT_SECTORS      12
#define ATA_IDENT_SERIAL       20
#define ATA_IDENT_MODEL        54
#define ATA_IDENT_CAPABILITIES 98
#define ATA_IDENT_FIELDVALID   106
#define ATA_IDENT_MAX_LBA      120
#define ATA_IDENT_COMMANDSETS  164
#define ATA_IDENT_MAX_LBA_EXT  200

// Channels:
#define      ATA_PRIMARY      0x00
#define      ATA_SECONDARY    0x01
 
// Directions:
#define      ATA_READ      0x00
#define      ATA_WRITE     0x01

struct Channel {
    u16 base, ctrl, bmide;
    u8 nIEN; // no interrupt
};

struct IDE_Controller;

struct Drive {
    bool present;
    u32 channel; // primary(0) or secondary(1)
    u32 drive; // master(0) or slave(1)
    u32 size; // size in sectors
    u16 type;
    u16 caps; // capabilities
    u16 signature;
    u16 cmdsets; // commandsets
    char model[64];

    IDE_Controller* ctrl;
};

struct IDE_Controller {
    Channel channels[2];
    Drive drives[4];

    u8 irq;
    u8 atapi_packet[12] = {0xA8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
};

static void WriteRegister(IDE_Controller* ctrl, u8 channel, u8 reg, u8 dat) {
    if(reg > 0x07 && reg < 0x0C) {
        WriteRegister(ctrl, channel, ATA_REG_CONTROL, 0x80 | ctrl->channels[channel].nIEN);
    }

    if(reg < 0x08) {
        outb(ctrl->channels[channel].base + reg - 0x00, dat);
    } else if(reg < 0x0C) {
        outb(ctrl->channels[channel].base + reg - 0x06, dat);
    } else if(reg < 0x0E) {
        outb(ctrl->channels[channel].base + reg - 0x0A, dat);
    } else if(reg < 0x16) {
        outb(ctrl->channels[channel].base + reg - 0x0E, dat);
    }

    if(reg > 0x07 && reg < 0x0C) {
        WriteRegister(ctrl, channel, ATA_REG_CONTROL, ctrl->channels[channel].nIEN);
    }
}

static u8 ReadRegister(IDE_Controller* ctrl, u8 channel, u8 reg) {
    u8 ret;

    if(reg > 0x07 && reg < 0x0C) {
        WriteRegister(ctrl, channel, ATA_REG_CONTROL, 0x80 | ctrl->channels[channel].nIEN);
    }

    if(reg < 0x08) {
        ret = inb(ctrl->channels[channel].base + reg - 0x00);
    } else if(reg < 0x0C) {
        ret = inb(ctrl->channels[channel].base + reg - 0x06);
    } else if(reg < 0x0E) {
        ret = inb(ctrl->channels[channel].ctrl + reg - 0x0A);
    } else if(reg < 0x16) {
        ret = inb(ctrl->channels[channel].bmide + reg - 0x0E);
    }

    if(reg > 0x07 && reg < 0x0C) {
        WriteRegister(ctrl, channel, ATA_REG_CONTROL, ctrl->channels[channel].nIEN);
    }

    return ret;
}

static void ReadBuffer(IDE_Controller* ctrl, u8 channel, u8 reg, u32* buffer, u32 quads) {
    if(reg > 0x07 && reg < 0x0C) {
        WriteRegister(ctrl, channel, ATA_REG_CONTROL, 0x80 | ctrl->channels[channel].nIEN);
    }

    // NOTE: removed these inline ASMs, because we set ES to DS in boot.S before kmain
    //asm ("pushw %%es; movw %%ds, %%ax; movw %%ax, %%es;" : : : "eax");

    if(reg < 0x08) {
        insd(ctrl->channels[channel].base + reg - 0x00, buffer, quads);
    } else if(reg < 0x0C) {
        insd(ctrl->channels[channel].base + reg - 0x06, buffer, quads);
    } else if(reg < 0x0E) {
        insd(ctrl->channels[channel].ctrl + reg - 0x0A, buffer, quads);
    } else if(reg < 0x16) {
        insd(ctrl->channels[channel].bmide + reg - 0x0E, buffer, quads);
    }

    //asm volatile("popw %es");

    if(reg > 0x07 && reg < 0x0C) {
        WriteRegister(ctrl, channel, ATA_REG_CONTROL, ctrl->channels[channel].nIEN);
    }
}

static u8 Poll(IDE_Controller* ctrl, u8 ch, u32 advanced_check) {
    for(int i = 0; i < 4; i++) {
        ReadRegister(ctrl, ch, ATA_REG_ALTSTATUS);
    }

    while(ReadRegister(ctrl, ch, ATA_REG_STATUS) & ATA_SR_BSY);

    if(advanced_check) {
        u8 state = ReadRegister(ctrl, ch, ATA_REG_STATUS);

        if(state & ATA_SR_ERR) {
            return 2;
        }

        if(state & ATA_SR_DF) {
            return 1;
        }

        if((state & ATA_SR_DRQ) == 0) {
            return 3;
        }
    }

    return 0;
}

static u8 PrintError(IDE_Controller* ctrl, u32 drive, u8 err) {
    if(err != 0) {
        logprintf("IDE error[ctrl=%x drive=%d]: ", ctrl, drive);
        switch(err) {
            case 1:
            {
                logprintf("Device Fault\n");
                break;
            }
            case 2:
            {
                auto st = ReadRegister(ctrl, ctrl->drives[drive].channel, ATA_REG_ERROR);
                if(st & ATA_ER_AMNF) logprintf("no address mark found, ");
                if(st & ATA_ER_TK0NF) logprintf("no media or media error, ");
                if(st & ATA_ER_ABRT) logprintf("command aborted, ");
                if(st & ATA_ER_IDNF) logprintf("ID mark not found, ");
                if(st & ATA_ER_MC) logprintf("no media or media error, ");
                if(st & ATA_ER_UNC) logprintf("uncorrectable data error, ");
                if(st & ATA_ER_BBK) logprintf("bad sectors, ");
                logprintf("\n");
                break;
            }
            case 3:
            {
                logprintf("nothing was read\n");
                break;
            }
            case 4:
            {
                logprintf("write protected!\n");
                break;
            }
        }
    } else {
        return 0;
    }
}

static bool IDE_Disk_Read(void* user, u32* bytes_read, void* buf, u32 siz, u32 off) {
    auto drive = (Drive*)user;
    auto ctrl = drive->ctrl;
    return false;
}

static bool IDE_Disk_Write(void* user, u32* bytes_written, const void* buf, u32 siz, u32 off) {
    auto drive = (Drive*)user;
    auto ctrl = drive->ctrl;
    return false;
}

static bool IDE_Disk_Flush(void* user) {
    auto drive = (Drive*)user;
    auto ctrl = drive->ctrl;
    return true;
}

static Disk_Device_Descriptor gDiskDesc = {
    .Read = IDE_Disk_Read,
    .Write = IDE_Disk_Write,
    .Flush = IDE_Disk_Flush,
};

static bool IDE_Initialize_Controller(const PCI_Device* dev, IDE_Controller* ctrl) {
    bool ret = true;
    u32 bar[5];
    u32 count = 0;
    u8 buffer[2048];

    for(int i = 0; i < 5; i++) {
        bar[i] = PCI_Cfg_ReadBAR(dev->address, i);
        logprintf("IDE BAR%d=%x\n", i, bar[i]);
    }

    // Determine IO ports
    ctrl->channels[ATA_PRIMARY].base = (bar[0] & 0xFFFFFFFC)    + 0x1F0 * (!bar[0]);
    ctrl->channels[ATA_PRIMARY].ctrl = (bar[1] & 0xFFFFFFFC)    + 0x3F6 * (!bar[1]);
    ctrl->channels[ATA_SECONDARY].base = (bar[2] & 0xFFFFFFFC)  + 0x170 * (!bar[2]);
    ctrl->channels[ATA_SECONDARY].ctrl = (bar[3] & 0xFFFFFFFC)  + 0x376 * (!bar[3]);
    ctrl->channels[ATA_PRIMARY].bmide = (bar[4] & 0xFFFFFFFC) + 0;
    ctrl->channels[ATA_SECONDARY].bmide = (bar[4] & 0xFFFFFFFC) + 8;

    logprintf("IDE PRIM Base=%x Ctrl=%x\n", ctrl->channels[ATA_PRIMARY].base, ctrl->channels[ATA_PRIMARY].ctrl);

    // Disable IRQs
    WriteRegister(ctrl, ATA_PRIMARY, ATA_REG_CONTROL, 2);
    WriteRegister(ctrl, ATA_SECONDARY, ATA_REG_CONTROL, 2);

    for(u32 channel = 0; channel < 2; channel++) {
        for(u32 drive = 0; drive < 2; drive++) {
            u8 err = 0, type = IDE_ATA, status;

            ctrl->drives[count].present = false;

            // Select drive
            WriteRegister(ctrl, channel, ATA_REG_HDDEVSEL, 0xA0 | (drive << 4));
            Sleep(1);

            // Identify
            WriteRegister(ctrl, channel, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
            Sleep(1);

            if(ReadRegister(ctrl, channel, ATA_REG_STATUS) == 0) {
                logprintf("Channel %d Drive %d: disk is not present\n", channel, drive);
                continue; // No disk present in this drive
            }

            while(1) {
                status = ReadRegister(ctrl, channel, ATA_REG_STATUS);
                if((status & ATA_SR_ERR)) {
                    // Device is not ATA
                    err = 1;
                    break;
                }
                if(!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) {
                    // OK
                    break;
                }
            }

            if(err != 0) {
                u8 cl = ReadRegister(ctrl, channel, ATA_REG_LBA1);
                u8 ch = ReadRegister(ctrl, channel, ATA_REG_LBA2);

                if(cl == 0x14 & ch == 0xEB) {
                    type = IDE_ATAPI;
                } else if(cl == 0x69 && ch == 0x96) {
                    type = IDE_ATAPI;
                } else {
                    logprintf("Channel %d Drive %d: disk is of unknown type\n", channel, drive);
                    continue;
                }

                WriteRegister(ctrl, channel, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
                SleepTicks(64);
            }

            ReadBuffer(ctrl, channel, ATA_REG_DATA, (u32*)buffer, 128);

            auto& D = ctrl->drives[count];
            D.present = true;
            D.type = type;
            D.channel = channel;
            D.drive = drive;
            D.signature = *((u16*)(buffer + ATA_IDENT_DEVICETYPE));
            D.caps = *((u16*)(buffer + ATA_IDENT_CAPABILITIES));
            D.cmdsets = *((u16*)(buffer + ATA_IDENT_COMMANDSETS));
            D.ctrl = ctrl;

            if(D.cmdsets & (1 << 26)) {
                // 48-bit LBA
                D.size = *((u32*)(buffer + ATA_IDENT_MAX_LBA_EXT));
            } else {
                // 24-bit LBA
                D.size = *((u32*)(buffer + ATA_IDENT_MAX_LBA));
            }

            // Copy model identifier
            for(u32 i = 0; i < 40; i++) {
                D.model[i] = buffer[ATA_IDENT_MODEL + i];
            }
            D.model[40] = 0;

            if(!Disk_Register_Device(&D, &gDiskDesc)) {
                logprintf("PCI IDE: couldn't register disk\n");
                ret = false;
            }

            count++;
        }
    }

    logprintf("    Discovered disks:\n");
    for(int i = 0; i < 4; i++) {
        auto& drive = ctrl->drives[i];
        if(drive.present) {
            logprintf("    - /dev/disk%d:\n        - Model: %s\n        - Size: %d sectors\n", i, drive.model, drive.size);
        }
    }

    return ret;
}

static int IDE_Probe(const PCI_Device* dev) {
    int ret;
    u8 cls, scls;
    
    PCI_Cfg_ReadClass(dev->address, &cls, &scls);
    if(cls != 0x01 || scls != 0x01) {
        // Not a mass storage device
        return PCI_PROBE_ERR;
    }

    IDE_Controller* user = (IDE_Controller*)kmalloc(sizeof(IDE_Controller));

    if(user) {    
        if(IDE_Initialize_Controller(dev, user)) {
            ret = PCI_PROBE_OK;
        } else {
            logprintf("PCI IDE: couldn't initialize controller!\n");
            kfree(user);
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