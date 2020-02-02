#include "common.h"
#include "interrupts.h"
#include "port_io.h"
#include "utils.h"
#include "logging.h"
#include "vm.h"

#define GDT_ACCESSED    (0x01)
#define GDT_READWRITE   (0x02)
#define GDT_DIRECTION   (0x04)
#define GDT_EXECUTABLE  (0x08)
#define GDT_TYPE        (0x10)
#define GDT_PRIV_LO     (0x20)
#define GDT_PRIV_HI     (0x40)
#define GDT_PRESENT     (0x80)
#define GDT_PRIV(level) ((level & 0x3) << 5)

#define GDT_KERNEL_DATA (GDT_PRESENT | GDT_PRIV(0) | GDT_TYPE | GDT_READWRITE)
#define GDT_KERNEL_CODE (GDT_KERNEL_DATA | GDT_EXECUTABLE)
#define GDT_USER_DATA (GDT_PRESENT | GDT_PRIV(3) | GDT_TYPE | GDT_READWRITE)
#define GDT_USER_CODE (GDT_USER_DATA | GDT_EXECUTABLE)

struct IDTD {
    u16 limit;
    u32 base;
} PACKED;

using GDTD = IDTD;

struct IDT_Entry {
    u16 off1;
    u16 sel;
    u8 zero;
    u8 attr;
    u16 off2;
} PACKED;

struct GDT_Entry {
    u16 limit;
    u16 base_low;
    u8 base_middle;
    u8 access;
    u8 granuality;
    u8 base_high;
} PACKED;

static IDT_Entry idt[256];
static GDT_Entry gdt[5];
static IDTD idtd;
static GDTD gdtd;
static Interrupt_Handler handlers[256];

extern "C" void isr0();
extern "C" void isr1();
extern "C" void isr2();
extern "C" void isr3();
extern "C" void isr4();
extern "C" void isr5();
extern "C" void isr6();
extern "C" void isr7();
extern "C" void isr8();
extern "C" void isr9();
extern "C" void isr10();
extern "C" void isr11();
extern "C" void isr12();
extern "C" void isr13();
extern "C" void isr14();
extern "C" void isr15();
extern "C" void isr16();
extern "C" void isr17();
extern "C" void isr18();
extern "C" void isr19();
extern "C" void isr20();
extern "C" void isr21();
extern "C" void isr22();
extern "C" void isr23();
extern "C" void isr24();
extern "C" void isr25();
extern "C" void isr26();
extern "C" void isr27();
extern "C" void isr28();
extern "C" void isr29();
extern "C" void isr30();
extern "C" void isr31();
extern "C" void isr127();
extern "C" void isr128();
extern "C" void irq0 ();
extern "C" void irq1 ();
extern "C" void irq2 ();
extern "C" void irq3 ();
extern "C" void irq4 ();
extern "C" void irq5 ();
extern "C" void irq6 ();
extern "C" void irq7 ();
extern "C" void irq8 ();
extern "C" void irq9 ();
extern "C" void irq10();
extern "C" void irq11();
extern "C" void irq12();
extern "C" void irq13();
extern "C" void irq14();
extern "C" void irq15();

extern "C" void IDT_Init(void* idtd);
extern "C" void GDT_Init(void* gdtd);

static void IDT_Set_Gate(u8 i, u32 base, u16 sel, u8 flags) {
    idt[i].off1 = base & 0xFFFF;
    idt[i].off2 = (base >> 16) & 0xFFFF;
    idt[i].sel = sel;
    idt[i].zero = 0;
    idt[i].attr = flags;
}

static void GDT_Set_Gate(u8 i, u32 base, u32 lim, u8 acc, u8 gran) {
    gdt[i].base_low = (base & 0xFFFF);
    gdt[i].base_middle = (base >> 16) & 0xFF;
    gdt[i].base_high = (base >> 24) & 0xFF;

    gdt[i].limit = (lim & 0xFFFF);
    gdt[i].granuality = (lim >> 16) & 0x0F;

    gdt[i].granuality |= gran & 0xF0;
    gdt[i].access = acc;
}

#define PIC1		0x20		/* IO base address for master PIC */
#define PIC2		0xA0		/* IO base address for slave PIC */
#define PIC1_COMMAND	PIC1
#define PIC1_DATA	(PIC1+1)
#define PIC2_COMMAND	PIC2
#define PIC2_DATA	(PIC2+1)

#define PIC_EOI		0x20		/* End-of-interrupt command code */

#define ICW1_ICW4	0x01		/* ICW4 (not) needed */
#define ICW1_SINGLE	0x02		/* Single (cascade) mode */
#define ICW1_INTERVAL4	0x04		/* Call address interval 4 (8) */
#define ICW1_LEVEL	0x08		/* Level triggered (edge) mode */
#define ICW1_INIT	0x10		/* Initialization - required! */
 
#define ICW4_8086	0x01		/* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO	0x02		/* Auto (normal) EOI */
#define ICW4_BUF_SLAVE	0x08		/* Buffered mode/slave */
#define ICW4_BUF_MASTER	0x0C		/* Buffered mode/master */
#define ICW4_SFNM	0x10		/* Special fully nested (not) */

static void PIC_Setup() {
    //u8 a1, a2;

    //a1 = inb(PIC1_DATA);
    //a2 = inb(PIC2_DATA);

    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    outb(PIC1_DATA, 0x20);
    outb(PIC1_DATA, 0x28);
    outb(PIC1_DATA, 4);
    outb(PIC2_DATA, 2);

    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    //outb(PIC1_DATA, a1);
    //outb(PIC2_DATA, a2);
    outb(PIC1_DATA, 0);
    outb(PIC2_DATA, 0);
}

void PIC_Mask(int IRQ) {
    ASSERT(IRQ >= IRQ0 && IRQ <= IRQ15);
    int off = IRQ - IRQ0;
    u8 mask;
    if(IRQ < IRQ8) {
        mask = inb(PIC1_DATA);
        outb(PIC1_DATA, mask | (1 << off));
    } else {
        off = off - 8;
        mask = inb(PIC2_DATA);
        outb(PIC2_DATA, mask | (1 << off));
    }
    logprintf("PIC masks are now %x %x\n", inb(PIC2_DATA), inb(PIC1_DATA));
}

void PIC_Unmask(int IRQ) {
    ASSERT(IRQ >= IRQ0 && IRQ <= IRQ15);
    int off = IRQ - IRQ0;
    u8 mask;
    if(IRQ < IRQ8) {
        mask = inb(PIC1_DATA);
        outb(PIC1_DATA, mask & (~(1 << off)));
    } else {
        off = off - 8;
        mask = inb(PIC2_DATA);
        outb(PIC2_DATA, mask & (~(1 << off)));
    }
    logprintf("PIC masks are now %x %x\n", inb(PIC2_DATA), inb(PIC1_DATA));
}

static void IDT_Setup() {
    idtd.limit = sizeof(IDT_Entry) * 256 - 1;
    idtd.base = (u32)&idt;

    memset(idt, 0, sizeof(IDT_Entry) * 256);

    /*
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
	outb(0x21, 0x20);
	outb(0xA1, 0x28);
	outb(0x21, 0x04);
	outb(0xA1, 0x02);
	outb(0x21, 0x01);
	outb(0xA1, 0x01);
	outb(0x21, 0x0);
	outb(0xA1, 0x0);
    */

    IDT_Set_Gate( 0, (u32)isr0 , 0x08, 0x8E);
	IDT_Set_Gate( 1, (u32)isr1 , 0x08, 0x8E);
	IDT_Set_Gate( 2, (u32)isr2 , 0x08, 0x8E);
	IDT_Set_Gate( 3, (u32)isr3 , 0x08, 0x8E);
	IDT_Set_Gate( 4, (u32)isr4 , 0x08, 0x8E);
	IDT_Set_Gate( 5, (u32)isr5 , 0x08, 0x8E);
	IDT_Set_Gate( 6, (u32)isr6 , 0x08, 0x8E);
	IDT_Set_Gate( 7, (u32)isr7 , 0x08, 0x8E);
	IDT_Set_Gate( 8, (u32)isr8 , 0x08, 0x8E);
	IDT_Set_Gate( 9, (u32)isr9 , 0x08, 0x8E);
	IDT_Set_Gate( 10, (u32)isr10 , 0x08, 0x8E);
	IDT_Set_Gate( 11, (u32)isr11 , 0x08, 0x8E);
	IDT_Set_Gate( 12, (u32)isr12 , 0x08, 0x8E);
	IDT_Set_Gate( 13, (u32)isr13 , 0x08, 0x8E);
	IDT_Set_Gate( 14, (u32)isr14 , 0x08, 0x8E);
	IDT_Set_Gate( 15, (u32)isr15 , 0x08, 0x8E);
	IDT_Set_Gate( 16, (u32)isr16 , 0x08, 0x8E);
	IDT_Set_Gate( 17, (u32)isr17 , 0x08, 0x8E);
	IDT_Set_Gate( 18, (u32)isr18 , 0x08, 0x8E);
	IDT_Set_Gate( 19, (u32)isr19 , 0x08, 0x8E);
	IDT_Set_Gate( 20, (u32)isr20 , 0x08, 0x8E);
	IDT_Set_Gate( 21, (u32)isr21 , 0x08, 0x8E);
	IDT_Set_Gate( 22, (u32)isr22 , 0x08, 0x8E);
	IDT_Set_Gate( 23, (u32)isr23 , 0x08, 0x8E);
	IDT_Set_Gate( 24, (u32)isr24 , 0x08, 0x8E);
	IDT_Set_Gate( 25, (u32)isr25 , 0x08, 0x8E);
	IDT_Set_Gate( 26, (u32)isr26 , 0x08, 0x8E);
	IDT_Set_Gate( 27, (u32)isr27 , 0x08, 0x8E);
	IDT_Set_Gate( 28, (u32)isr28 , 0x08, 0x8E);
	IDT_Set_Gate( 29, (u32)isr29 , 0x08, 0x8E);
	IDT_Set_Gate( 30, (u32)isr30 , 0x08, 0x8E);
	IDT_Set_Gate(31, (u32)isr31, 0x08, 0x8E);
	IDT_Set_Gate(32, (u32)irq0, 0x08, 0x8E);
	IDT_Set_Gate(33, (u32)irq1, 0x08, 0x8E);
	IDT_Set_Gate(34, (u32)irq2, 0x08, 0x8E);
	IDT_Set_Gate(35, (u32)irq3, 0x08, 0x8E);
	IDT_Set_Gate(36, (u32)irq4, 0x08, 0x8E);
	IDT_Set_Gate(37, (u32)irq5, 0x08, 0x8E);
	IDT_Set_Gate(38, (u32)irq6, 0x08, 0x8E);
	IDT_Set_Gate(39, (u32)irq7, 0x08, 0x8E);
	IDT_Set_Gate(40, (u32)irq8, 0x08, 0x8E);
	IDT_Set_Gate(41, (u32)irq9, 0x08, 0x8E);
	IDT_Set_Gate(42, (u32)irq10, 0x08, 0x8E);
	IDT_Set_Gate(43, (u32)irq11, 0x08, 0x8E);
	IDT_Set_Gate(44, (u32)irq12, 0x08, 0x8E);
	IDT_Set_Gate(45, (u32)irq13, 0x08, 0x8E);
	IDT_Set_Gate(46, (u32)irq14, 0x08, 0x8E);
	IDT_Set_Gate(47, (u32)irq15, 0x08, 0x8E);
	IDT_Set_Gate(127, (u32)isr127, 0x08, 0x8E);
    IDT_Set_Gate(128, (u32)isr128, 0x08, 0x8E);

    IDT_Init(&idtd);
}

static void GDT_Setup() {
    gdtd.limit = sizeof(GDT_Entry) * 5 - 1;
    gdtd.base = (u32)&gdt;

    GDT_Set_Gate(0, 0, 0, 0, 0);                // Null
    GDT_Set_Gate(1, 0, 0xFFFFFFFF, GDT_KERNEL_CODE, 0xCF); // Kernel code
    GDT_Set_Gate(2, 0, 0xFFFFFFFF, GDT_KERNEL_DATA, 0xCF); // Kernel data
    GDT_Set_Gate(3, 0, 0xFFFFFFFF, GDT_USER_CODE, 0xCF); // User code
    GDT_Set_Gate(4, 0, 0xFFFFFFFF, GDT_USER_DATA, 0xCF); // User data

    GDT_Init(&gdtd);
}

static void GeneralProtectionFault(Registers* regs) {
    logprintf("======================\n");
    logprintf("GENERAL PROTECTION FAULT\n");
    logprintf("EAX: %x EBX: %x ECX: %x EDX: %x\n", regs->eax, regs->ebx, regs->ecx, regs->edx);
    logprintf("ESI: %x EDI: %x EBP: %x ESP: %x\n", regs->esi, regs->edi, regs->ebp, regs->esp);
    logprintf("EIP: %x CS: %x EFLAGS: %x SS: %x\n", regs->eip, regs->cs, regs->eflags, regs->ss);
    logprintf("======================\n");

    while(1) {
        asm volatile("hlt");
    }
}

static void PageFault(Registers* regs) {
    u32 addr;
    asm volatile("movl %%cr2, %0" : "=r"(addr) :); // fetch the address that we tried to access
    bool P = regs->err_code & 1;
    bool W = regs->err_code & 2;
    bool U = regs->err_code & 4;
    bool R = regs->err_code & 8;
    bool I = regs->err_code & 16;

    //if(!MM_MapToPhysical(NULL, (void*)addr) || P) {
    if(1) {
        logprintf("======================\n");
        logprintf("PAGE FAULT\n");
        logprintf("EAX: %x EBX: %x ECX: %x EDX: %x\n", regs->eax, regs->ebx, regs->ecx, regs->edx);
        logprintf("ESI: %x EDI: %x EBP: %x ESP: %x\n", regs->esi, regs->edi, regs->ebp, regs->esp);
        logprintf("EIP: %x CS: %x EFLAGS: %x SS: %x\n", regs->eip, regs->cs, regs->eflags, regs->ss);
        logprintf("ADDRESS: %x ERR: %c%c%c%c%c\n", addr, I ? 'I' : '-', R ? 'R' : '-', U ? 'U' : '-', W ? 'W' : 'R', P ? 'P' : '-');
        MM_PrintDiagnostic((void*)addr);
        logprintf("======================\n");

        while(1) {
            asm volatile("hlt");
        }
    }
    // TODO: check if we were in kernel or user mode when we implement user mode
}

struct Syscall_Handler {
    u32 id;
    void(*func)(Registers* regs);
};

#define MAX_SYSCALLS (128)

static Syscall_Handler gaSyscallHandlers[MAX_SYSCALLS];
static u32 giSyscallHandlersLastIndex = 0;

void RegisterSyscallHandler(u32 id, void(*func)(Registers* regs)) {
    if(giSyscallHandlersLastIndex < MAX_SYSCALLS) {
        auto& SCH = gaSyscallHandlers[giSyscallHandlersLastIndex];
        SCH.id = id;
        SCH.func = func;
        giSyscallHandlersLastIndex++;
    }
}

static void SyscallHandler(Registers* regs) {
    auto id = regs->eax;
    for(u32 i = 0; i < giSyscallHandlersLastIndex; i++) {
        if(gaSyscallHandlers[i].id == id) {
            gaSyscallHandlers[i].func(regs);
            return;
        }
    }

    logprintf("PROGRAM ERROR: invalid syscall or no handler: %x\n", id);
}

void Interrupts_Setup() {
    GDT_Setup();
    PIC_Setup();
    IDT_Setup();

    handlers[13] = GeneralProtectionFault;
    handlers[14] = PageFault;
    handlers[0x80] = SyscallHandler;

    asm volatile("sti");
}

void Interrupts_Register_Handler(u32 i, Interrupt_Handler handler) {
    handlers[i] = handler;
}

extern "C" void ISRHandler(Registers regs) {
    if(regs.int_no < 256 && handlers[regs.int_no]) {
        handlers[regs.int_no](&regs);
    }
}

extern "C" void IRQHandler(Registers regs) {
    if(handlers[regs.int_no]) {
        handlers[regs.int_no](&regs);
    }
    if(regs.int_no >= 40) {
        outb(0xA0, 0x20);
    }

    outb(0x20, 0x20); // send EOI
}
