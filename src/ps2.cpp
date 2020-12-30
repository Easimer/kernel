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

struct control_reg_t {
public:
    control_reg_t(u8 v) : value(v) {}
    explicit operator u8() const { return value; }
    control_reg_t& operator|=(u8 mask) {
        value |= mask;
        return *this;
    }
    control_reg_t& operator&=(u8 mask) {
        value &= mask;
        return *this;
    }
private:
    u8 value;
};

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

static void SendCommand(u8 b) {
    outb(PORT_COMMAND, b);
}

static void SendData(u8 b) {
    outb(PORT_DATA, b);
}

static u8 ReadStatus() {
    return inb(PORT_STATUS);
}

static u8 ReadData() {
    return inb(PORT_DATA);
}

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

static control_reg_t ReadCfgByte() {
    while(!CanSendData());
    SendCommand(0x20);
    while(!CanReadData());
    auto ret = control_reg_t(ReadData());
    logprintf("ps2: read cfg=%x\n", (u8)ret);
    return ret;
}

static void WriteCfgByte(control_reg_t b) {
    int n = 0;
    do {
        while(!CanSendData());
        SendCommand(0x60);
        while(!CanSendData());
        SendData((u8)b);
        // Make sure cfg byte was really written
        while(!CanSendData());
        SendCommand(0x20);
        while(!CanReadData());
        u8 cfg = ReadData();
        if(cfg == (u8)b) {
            //logprintf("ps2: written cfg=%x\n", (u8)b);
            return;
        }
    } while(n++ < 5);
    logprintf("ps2: failed to write cfg=%x\n", (u8)b);
    
}

static void InitCfgByte() {
    u8 status;

    auto cfg = ReadCfgByte();
    status = ReadStatus();

    gPS2.dual_channel = ((u8)cfg & CFG_CLOCK2);
    cfg &= ~(CFG_IRQ1 | CFG_IRQ2 | CFG_TRANS1);
    cfg |= CFG_CLOCK1 | CFG_CLOCK2;

    // keylock
    if((~status) & 0x10) {
        cfg |= 0x08;
        logprintf("ps2: ignoring keylock\n");
    }

    WriteCfgByte(cfg);
}

static void DetermineDualChannel() {
    if(!gPS2.dual_channel) {
        SendCommand(0xA8);
        auto cfg = ReadCfgByte();
        // TODO: isn't this redundant
        if(!((u8)cfg & CFG_CLOCK2)) {
            gPS2.dual_channel = true;
        }
    }
}

static bool DoSelfTest() {
    bool ret = false;
    u8 res;

    for(int i = 0; i < 5; i++) {
        SendCommand(CMD_SELFTEST);
        if(ReadDataWithTimeout(1000, &res)) {
            if(res == 0x55) {
                ret = true;
            } else {
                logprintf("ps2: self test i=%d res=%x\n", i, res);
            }
        } else {
            logprintf("ps2: self test i=%d timed out\n", i);
        }
    }

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
    auto cfg = ReadCfgByte();
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
    if(((u8)cfg & ((i == 0) ? CFG_IRQ1 : CFG_IRQ2)) == 0) {
        logprintf("ps2: failed to enable interrupts for device %d cfg=%x\n", i, cfg);
        ASSERT(0);
    }
}

#define SendToDevice PS2_SendToDevice
void PS2_SendToDevice(u32 dev, u8 v) {
    if(dev != 0) {
        while(!CanSendData());
        SendCommand(0xD4);
    }
    while(!CanSendData());
    SendData(v);
    ReadStatus();
}

#define ResetDevice PS2_ResetDevice
bool PS2_ResetDevice(u32 dev) {
    SendToDevice(dev, 0xFF);

    while(!CanReadData());
    auto res = ReadData();

    if(res != 0xFA) {
        logprintf("ps2: ResetDevice failed? rc=%x\n", res);
    }

    return res == 0xFA;
}

static bool DisableScanning(u32 dev) {
    u8 res;

    SendToDevice(dev, 0xF5);

    // Recv response
    if(!ReadDataWithTimeout(10000, &res)) {
        logprintf("ps2: disable scanning command timed out on device %d\n", dev);
        return false;
    }

    if(res != 0xFA) {
        logprintf("ps2: disable scanning command failed on device %d with response %x\n", dev, res);
        return false;
    }

    return true;
}

static void FlushDataBuffer() {
    while(CanReadData()) ReadData();
}

static void DetermineDeviceType(u32 dev) {
    u8 res;

    FlushDataBuffer();

    int cnt = 0;
    while(true) {
        logprintf("ps2: attempting to disable scanning for the %dth time\n", cnt);
        if(!DisableScanning(dev)) {
            logprintf("ps2: can't determine device %d type: disabling scanning has failed\n", dev);
            cnt++;
            continue;
        }
        break;
    }

    FlushDataBuffer();

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
            logprintf("ps2: can't determine device %d type: id cmd failed rc=%x\n", dev, res);
        }
    } else {
        logprintf("ps2: can't determine device %d type: id cmd timed out\n", dev);
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
    // TEMP
    auto cfg = ReadCfgByte();
    logprintf("ps2: before init cfg byte was %x\n", (u8)cfg);
    // TEMP
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

