#include "common.h"
#include "port_io.h"
#include "ps2.h"
#include "logging.h"
#include "utils.h"
#include "timer.h"
#include "interrupts.h"

#define PORT_DATA       (0x60)
#define PORT_STATUS     (0x64)
#define PORT_COMMAND    (0x64)

#define SendCommand(b) outb(PORT_COMMAND, b)
#define SendData(b) outb(PORT_DATA, b)
#define ReadStatus() inb(PORT_STATUS)
#define ReadData() inb(PORT_DATA)

#define STM_OUT_STATUS  (0x01)
#define STM_IN_STATUS   (0x02)
#define STM_SYSFLAG     (0x04)
#define STM_CMDDAT      (0x08)
#define STM_UNK0        (0x10)
#define STM_UNK1        (0x20)
#define STM_TIMEOUT     (0x40)
#define STM_PARITY      (0x80)

#define CFG_IRQ1        (0x01)
#define CFG_IRQ2        (0x02)
#define CFG_SYSFLAG     (0x04)
#define CFG_ZERO1       (0x08)
#define CFG_CLOCK1      (0x10)
#define CFG_CLOCK2      (0x20)
#define CFG_TRANS1      (0x40)
#define CFG_ZERO2       (0x80)

#define CMD_DISABLE1        (0xAD)
#define CMD_ENABLE1         (0xAE)
#define CMD_DISABLE2        (0xA7)
#define CMD_ENABLE2         (0xA8)
#define CMD_SELFTEST        (0xAA)

enum PS2_Interface {
    IF_None,
    IF_Keyboard,
    IF_Mouse,
    IF_Max
};

struct PS2_State {
    bool has_ps2;
    bool dual_channel;
    PS2_Interface if_types[2];
};

static PS2_State gPS2;

void PS2_Initialize_MF2_Keyboard(u32 dev);

static bool SystemHasPS2() {
    // TODO: implement this
    return true; // assuming system has PS/2 controller
}

static void PS2_Disable() {
    SendCommand(CMD_DISABLE1);
    SendCommand(CMD_DISABLE2);
}

static void PS2_Enable() {
    SendCommand(CMD_ENABLE1);
    SendCommand(CMD_ENABLE2);
}

static inline bool CanReadData() {
    return (ReadStatus() & STM_OUT_STATUS) != 0;
}

static inline bool CanSendData() {
    return (ReadStatus() & STM_IN_STATUS) == 0;
}

#define ReadDataWithTimeout PS2_ReadDataWithTimeout
bool PS2_ReadDataWithTimeout(u32 ticks, u8* value) {
    bool ret = false;
    u32 start = TicksElapsed();
    u32 end = start + ticks;

    while(TicksElapsed() < end && !CanReadData());

    if(CanReadData()) {
        *value = ReadData();
        ret = true;
    }

    return ret;
}

bool PS2_ReadData(u8* value) {
    bool ret = false;
    u32 c = 0;

    while(c < 8192 && !ret) {
        if(CanReadData()) {
            *value = ReadData();
            ret = true;
        }
        c++;
    }

    return ret;
}

static void FlushOutputBuffer() {
    while(CanReadData()) {
        ReadData();
    }
}

static u8 ReadCfgByte() {
    u8 ret;

    while(!CanSendData());
    SendCommand(0x20);
    while(!CanReadData());
    ret = ReadData();

    return ret;
}

static void WriteCfgByte(u8 b) {
    while(!CanSendData());
    SendCommand(0x60);
    while(!CanSendData());
    SendData(b);
}

static void InitCfgByte() {
    u8 cfg;

    cfg = ReadCfgByte();

    gPS2.dual_channel = (cfg & CFG_CLOCK2);
    cfg = (cfg & ~(CFG_IRQ1 | CFG_IRQ2 | CFG_TRANS1)) | CFG_CLOCK1;

    WriteCfgByte(cfg);
}

static void DetermineDualChannel() {
    u8 cfg;
    if(!gPS2.dual_channel) {
        SendCommand(0xA8);
        cfg = ReadCfgByte();
        if(!(cfg & CFG_CLOCK2)) {
            gPS2.dual_channel = true;
        }
    }
}

static bool DoSelfTest() {
    bool ret = false;
    u8 res;
    SendCommand(CMD_SELFTEST);
    while(!CanReadData());
    res = ReadData();

    ret = res == 0x55;

    return ret;
}

static void TestInterfaces() {
    u8 res;
    SendCommand(0xAB);
    while(!CanReadData());
    res = ReadData();

    if(res != 0x00) {
        gPS2.if_types[0] = IF_None;
    }

    if(gPS2.dual_channel) {
        SendCommand(0xA9);
        while(!CanReadData());
        res = ReadData();

        if(res != 0x00) {
            gPS2.if_types[1] = IF_None;
        }
    }
}

static void EnableInterrupts(u32 i) {
    u8 cfg;
    cfg = ReadCfgByte();
    if(i == 0) {
        cfg |= CFG_IRQ1;
        cfg &= ~CFG_CLOCK1;
    } else if(i == 1) {
        cfg |= CFG_IRQ2;
        cfg &= ~CFG_CLOCK2;
    } else {
        ASSERT(!"EnableInterrupts i > 1");
    }
    WriteCfgByte(cfg);
    cfg = ReadCfgByte();
    if((cfg & ((i == 0) ? CFG_IRQ1 : CFG_IRQ2)) == 0) {
        logprintf("ps2: failed to enable interrupts for device %d cfg=%x\n", i, cfg);
        ASSERT(0);
    }
    logprintf("ps2: cfg byte is %x\n", cfg);
}

#define SendToDevice PS2_SendToDevice
void PS2_SendToDevice(u32 dev, u8 v) {
    if(dev != 0) {
        while(!CanSendData());
        SendCommand(0xD4);
    }
    while(!CanSendData());
    SendData(v);
}

#define ResetDevice PS2_ResetDevice
bool PS2_ResetDevice(u32 dev) {
    SendToDevice(dev, 0xFF);

    while(!CanReadData());
    return ReadData() == 0xFA;
}

static void DetermineDeviceType(u32 dev) {
    u8 res;
    SendToDevice(dev, 0xF5); // disable scanning
    if(ReadDataWithTimeout(1000, &res)) {
        if(res == 0xFA) {
            SendToDevice(dev, 0xF2); // identify
            if(ReadDataWithTimeout(1000, &res)) {
                if(res == 0xFA) {
                    u8 ID[2] = {0, 0};
                    bool timed_out = false;

                    for(u32 i = 0; i < 2 && !timed_out; i++) {
                        if(!ReadDataWithTimeout(1000, &ID[i])) {
                            timed_out = true;
                        }
                    }
                    logprintf("ps2: device %d identifies itself: {%x, %x}\n", dev, ID[0], ID[1]);

                    if(ID[0] == 0x00) {
                        gPS2.if_types[dev] = IF_Mouse;
                    } else if(ID[0] == 0xAB && ID[1] == 0x83) {
                        gPS2.if_types[dev] = IF_Keyboard;
                    } else {
                        gPS2.if_types[dev] = IF_None;
                    }
                } else {
                    logprintf("ps2: can't determine device %d type: id cmd failed\n", dev);
                }
            } else {
                logprintf("ps2: can't determine device %d type: id cmd timed out\n", dev);
            }
        } else {
            logprintf("ps2: can't determine device %d type: cmd failed\n", dev);
        }
    } else {
        logprintf("ps2: can't determine device %d type: cmd timed out\n", dev);
    }
}

static void InitializeDevice(u32 dev) {
    switch(gPS2.if_types[dev]) {
        case IF_Keyboard:
            logprintf("ps2: initializing keyboard %x\n", dev);
            PS2_Initialize_MF2_Keyboard(dev);
            break;
        default:
            logprintf("ps2: don't know how to init dev %x\n", dev);
            break;
    }
}

void PS2_Setup() {
    if(SystemHasPS2()) {
        gPS2.has_ps2 = true;
        PS2_Disable();
        FlushOutputBuffer();
        InitCfgByte();
        if(DoSelfTest()) {
            DetermineDualChannel();
            TestInterfaces();

            PS2_Enable();
            EnableInterrupts(0);
            if(gPS2.dual_channel) {
                EnableInterrupts(1);
            }

            if(ResetDevice(0)) {
                DetermineDeviceType(0);
                InitializeDevice(0);
            } else {
                logprintf("ps2: couldn't reset device 0\n");
            }
            if(gPS2.dual_channel) {
                if(ResetDevice(1)) {
                    DetermineDeviceType(1);
                    InitializeDevice(1);
                } else {
                    logprintf("ps2: couldn't reset device 1\n");
                }
            }
        } else {
            logprintf("ps2: self-test failed\n");
        }
    } else {
        gPS2.has_ps2 = false;
        logprintf("ps2: not supported\n");
    }
}

