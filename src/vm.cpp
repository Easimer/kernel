#include "common.h"
#include "utils.h"
#include "vm.h"
#include "pfalloc.h"
#include "logging.h"

#define PT_PRESENT	(0x001)
#define PT_READWRITE	(0x002)
#define PT_USER	(0x004)
#define PT_WRITETHRU	(0x008)
#define PT_CACHEDIS	(0x010)
#define PT_ACCESSED	(0x020)
#define PT_DIRTY	(0x040)
#define PT_ZERO	(0x080)
#define PT_GLOBAL	(0x100)
#define PT_CUSTOM1	(0x200)
#define PT_CUSTOM2	(0x400)
#define PT_CUSTOM3	(0x800)

#define PT_ADDR_MASK 0xFFFFFF000
#define PD_ADDR(entry) (entry & PT_ADDR_MASK)
#define PD_IS_PRESENT(entry) ((entry & PT_PRESENT) != 0)

#define PAGE_RESERVED (1021)
#define PAGE_VMTEMP (1022)
#define PAGE_VGA (1023)

extern u32 boot_page_directory; // defined in boot.S
extern u32 boot_page_table; // defined in boot.S
static volatile u32* page_directory;
static u32* kernel_page_table;
static u32* vmtemp;

void MM_Init() {
    page_directory = &boot_page_directory;
    kernel_page_table = &boot_page_table;
    vmtemp = (u32*)(0xC03FE000);
}

static void LoadIntoVMTemp(u32 physical) {
    //logprintf("VMTEMP: loading %x\n", physical);
    kernel_page_table[PAGE_VMTEMP] = physical | (PT_PRESENT | PT_READWRITE);
}

static void UnloadVMTemp() {
    //logprintf("VMTEMP: unloading\n");
    kernel_page_table[PAGE_VMTEMP] = 0;
}

bool MM_VirtualMap(void* vaddr, u32 physical) {
    bool ret = false;

    ASSERT(((u32)vaddr & PT_ADDR_MASK) == (u32)vaddr);
    ASSERT((physical & PT_ADDR_MASK) == physical);

    // Calculate PDT index of this vaddr
    u32 pdi = (u32)vaddr >> 22;
    auto pd_entry = page_directory[pdi];
    // Map the page table
    if(!PD_IS_PRESENT(pd_entry)) {
        // Allocate frame for a new page table
        u32 table_addr = PFA_Alloc(4096);
        ASSERT((table_addr & 0xFFFFF000) == table_addr); // make sure table is 4K-aligned
        pd_entry = page_directory[pdi] = table_addr | PT_PRESENT | PT_READWRITE;
        asm volatile("mov %%cr3, %%eax\nmov %%eax, %%cr3\n":::"eax");
    }
    //logprintf("Mapping page %x to frame %x\n", vaddr, physical);
    //kernel_page_table[PAGE_VMTEMP] = PD_ADDR(pd_entry) | (PT_PRESENT | PT_READWRITE);
    LoadIntoVMTemp(PD_ADDR(pd_entry));
    //logprintf("\tpdi=%x entry=%x\n", pdi, kernel_page_table[PAGE_VMTEMP]);
    // Insert entry
    u32 pti = ((u32)vaddr >> 12) & 0x3FF;
    vmtemp[pti] = physical | PT_PRESENT | PT_READWRITE;
    //logprintf("\tpti=%x entry=%x\n", pti, vmtemp[pti]);
    // Unmap the page table
    //kernel_page_table[PAGE_VMTEMP] = 0;
    UnloadVMTemp();
    ret = true;

    return ret;
}

bool MM_VirtualUnmap(void* vaddr) {
	bool ret = false;

    ASSERT(((u32)vaddr & PT_ADDR_MASK) == (u32)vaddr);

	// Calculate PDT index of this vaddr
    u32 pdi = (u32)vaddr >> 22;
    auto pd_entry = page_directory[pdi];
    // Is the page table present
    if(PD_IS_PRESENT(pd_entry)) {
        // Map page table
        //kernel_page_table[PAGE_VMTEMP] = PD_ADDR(pd_entry) | (PT_PRESENT | PT_READWRITE);
        LoadIntoVMTemp(PD_ADDR(pd_entry));
        
        // Clear table entry
        u32 pti = ((u32)vaddr >> 12) & 0x3FF;
        vmtemp[pti] = 0;

        // Unmap the page table
        //kernel_page_table[PAGE_VMTEMP] = 0;
        UnloadVMTemp();

        ret = true;
    }
  
	return ret;
}

void* MM_VirtualMapKernel(u32 physical, u32 page_count) {
    void* ret = NULL;
    ASSERT(page_count > 0);

    //logprintf("Mapping %d frames starting at %x\n", page_count, physical);

    for(u32 pdi = 768; pdi < 1024 && ret == NULL; pdi++) {
        //auto d_entry = kernel_page_subdirectory[pdi];
        auto d_entry = page_directory[pdi];
        if(d_entry & PT_PRESENT) {
            auto addr = PD_ADDR(d_entry);
            LoadIntoVMTemp(addr);

            for(u32 pti = 0; pti < 1024; pti++) {
                //logprintf("\tPDI=%d PTI=%d\n", pdi, pti);
                if((vmtemp[pti] & PT_PRESENT) == 0) {
                    u32 i = 0;
                    while(i < page_count && pti + i < 1024 && (vmtemp[pti + i] & PT_PRESENT) == 0) {
                        i++;
                    }
                    if(i == page_count) {
                        for(u32 p = 0; p < page_count; p++) {
                            vmtemp[pti + p] = (physical + p * 4096) | PT_PRESENT | PT_READWRITE;
                            //logprintf("\tPDI=%d PTI=%d entry=%x\n", pdi, pti + p, vmtemp[pti + p]);
                        }
                        ret = (void*)(pdi * 4096 * 1024 + pti * 4096);
                        //logprintf("\tRET=%x\n", ret);
                        
                        break;
                    }
                }
            }

            UnloadVMTemp();
        } else {
            logprintf("PDI=%d is not present entry=%x\n", pdi, PD_ADDR(d_entry));
        }
    }

    return ret;
}

bool MM_MapToPhysical(u32* out_phys, void* addr) {
    bool ret = false;

    if(addr) {
        auto vaddr = ((u32)addr & 0xFFFFF000);
        auto off = (u32)addr - vaddr;
        u32 pdi = (u32)vaddr >> 22;
        u32 pti = ((u32)vaddr >> 12) & 0x3FF;
        u32 pd_entry = page_directory[pdi];
        if(pd_entry & PT_PRESENT) {
            LoadIntoVMTemp(PD_ADDR(pd_entry));
            u32* pt = vmtemp;
            auto pt_entry = pt[pti];
            if(pt_entry & PT_PRESENT) {
                if(out_phys) {
                    *out_phys = PD_ADDR(pt_entry) + off;
                }
                ret = true;
            }
        }
    }

    return ret;
}

void MM_PrintDiagnostic(void* addr) {
    auto vaddr = ((u32)addr & 0xFFFFF000);
    auto off = (u32)addr - vaddr;
    u32 pdi = (u32)vaddr >> 22;
    u32 pti = ((u32)vaddr >> 12) & 0x3FF;
    u32 pd_entry = page_directory[pdi];
    logprintf("VM Diagnostic\n\tMemory access was %d bytes into page %x\n\tPage directory entry #%d for this was: %x\n\tPage table was %s\n",
        off, vaddr, pdi, pd_entry, (pd_entry & PT_PRESENT) ? "PRESENT" : "NOT PRESENT");
    if(pd_entry & PT_PRESENT) {
        LoadIntoVMTemp(PD_ADDR(pd_entry));
        u32* pt = vmtemp;
        auto pt_entry = pt[pti];
        logprintf("\n\tPage table entry #%d was: %x\n\tThe page was %s\n", pti, pt_entry, (pt_entry & PT_PRESENT) ? "PRESENT" : "NOT PRESENT");
        if(pt_entry & PT_PRESENT) {
            logprintf("\tPhysical address: %x\n", PD_ADDR(pt_entry));
        }
    }
}