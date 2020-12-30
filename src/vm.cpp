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

#define ADDR_PDI(vaddr) ((u32)vaddr >> 22)
#define ADDR_PTI(vaddr) (((u32)vaddr >> 12) & 0x3FF)
#define ADDR_VIRT(pdi, pti) ((void*)(((u32)pdi) * 4096 * 1024 + ((u32)pti) * 4096))

#define PAGE_PD         (1020)
#define PAGE_RESERVED   (1021)
#define PAGE_VMTEMP     (1022)
#define PAGE_VGA        (1023)

#define PAGE_RESERVED_START PAGE_PD

#define SAVE_VMTEMP() auto __vmtemp_entry = kernel_page_table[PAGE_VMTEMP]
#define RESTORE_VMTEMP() kernel_page_table[PAGE_VMTEMP] = __vmtemp_entry

#define PAGE_DIRECTORY_MAX (128)

struct Page_Directory {
    bool used;
    u32 addr;
};

static Page_Directory gPageDirs[PAGE_DIRECTORY_MAX];

extern u32 boot_page_directory; // defined in boot.S
extern u32 boot_page_table; // defined in boot.S
static volatile u32* page_directory;
static u32* kernel_page_table;
static volatile u32* vmtemp;

void MM_Init() {
    kernel_page_table = &boot_page_table;
    vmtemp = (u32*)(0xC03FE000);
    
    for(int i = 0; i < PAGE_DIRECTORY_MAX; i++) {
        gPageDirs[i].used = false;
    }

    // Store boot page directory
    gPageDirs[0].addr = ((u32)&boot_page_directory) - 0xC0000000;
    gPageDirs[0].used = true;
    logprintf("vm: boot page directory %xv %xp\n", &boot_page_directory, gPageDirs[0].addr);

    // Map page directory
    kernel_page_table[PAGE_PD] = (gPageDirs[0].addr) | PT_PRESENT | PT_READWRITE;
    page_directory = (u32*)ADDR_VIRT(768, PAGE_PD);
}

static void LoadIntoVMTemp(u32 physical) {
    logprintf("VMTEMP: loading %x\n", physical);
    kernel_page_table[PAGE_VMTEMP] = physical | (PT_PRESENT | PT_READWRITE);
}

static void UnloadVMTemp() {
    //logprintf("VMTEMP: unloading\n");
    kernel_page_table[PAGE_VMTEMP] = 0;
}

static void BroadcastTableMapping(u32 idx, u32 entry) {
    SAVE_VMTEMP();
    for(int i = 0; i < PAGE_DIRECTORY_MAX; i++) {
        if(gPageDirs[i].used) {
            LoadIntoVMTemp(gPageDirs[i].addr);
            vmtemp[idx] = entry;
        }
    }
    RESTORE_VMTEMP();
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
        u32 table_addr;
        PFA_Alloc(&table_addr, 4096);
        ASSERT((table_addr & 0xFFFFF000) == table_addr); // make sure table is 4K-aligned
        pd_entry = page_directory[pdi] = table_addr | PT_PRESENT | PT_READWRITE;
        asm volatile("mov %%cr3, %%eax\nmov %%eax, %%cr3\n":::"eax");
        if(pdi >= 768) {
            BroadcastTableMapping(pdi, pd_entry);
        }
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

// Broadcasts a page mapping into all page directories.
// Used when the kernel allocates memory.
/*
static void BroadcastMapping(void* virt, u32 entry) {
    auto pd_idx = ADDR_PDI(virt);
    auto pt_idx = ADDR_PTI(virt);
    BroadcastMapping(pd_idx, pt_idx, entry);
}
*/

static void* MM_VirtualMap_Interval(u32 physical, u32 page_count, u32 first, u32 last) {
    void* ret = NULL;
    ASSERT(page_count > 0);

    //logprintf("Mapping %d frames starting at %x\n", page_count, physical);

    for(u32 pdi = first; pdi < last && ret == NULL; pdi++) {
        //auto d_entry = kernel_page_subdirectory[pdi];
        auto d_entry = page_directory[pdi];
        if(d_entry & PT_PRESENT) {
            auto addr = PD_ADDR(d_entry);
            LoadIntoVMTemp(addr);

            for(u32 pti = 0; pti < 1024; pti++) {
                //logprintf("\tPDI=%d PTI=%d\n", pdi, pti);
                if((vmtemp[pti] & PT_PRESENT) == 0) {
                    u32 i = 0;
                    // Can we fit `page_count` pages here?
                    while(i < page_count && pti + i < last && (vmtemp[pti + i] & PT_PRESENT) == 0) {
                        i++;
                    }
                    if(i == page_count) {
                        // We can.
                        ret = (void*)(pdi * 4096 * 1024 + pti * 4096);
                        for(u32 p = 0; p < page_count; p++) {
                            u32 entry = (physical + p * 4096) | PT_PRESENT | PT_READWRITE;
                            vmtemp[pti + p] = entry;
                            //logprintf("\tPDI=%d PTI=%d entry=%x\n", pdi, pti + p, vmtemp[pti + p]);
                        }
                        
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

void* MM_VirtualMapProgram(u32 physical, u32 page_count) {
    return MM_VirtualMap_Interval(physical, page_count, 0, 768);
}

void* MM_VirtualMapKernel(u32 physical, u32 page_count) {
    return MM_VirtualMap_Interval(physical, page_count, 768, 1024);
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
            auto pt = vmtemp;
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
        auto pt = vmtemp;
        auto pt_entry = pt[pti];
        logprintf("\n\tPage table entry #%d was: %x\n\tThe page was %s\n", pti, pt_entry, (pt_entry & PT_PRESENT) ? "PRESENT" : "NOT PRESENT");
        if(pt_entry & PT_PRESENT) {
            logprintf("\tPhysical address: %x\n", PD_ADDR(pt_entry));
        }
    }
}


bool AllocatePageDirectory(u32* res) {
    ASSERT(res);

    if(!PFA_Alloc(res, 4096)) {
        return false;
    }

    bool ret = false;

    auto pd = (u32*)MM_VirtualMapKernel(*res);
    if(pd) {
        memset(pd, 0, 4096);
        // Copy kernel entries
        for(int i = 768; i < 1024; i++) {
            pd[i] = page_directory[i];
        }
        ret = true;
    }

    MM_VirtualUnmap(pd);

    return ret;
}

bool FreePageDirectory(u32 pd_phys) {
    PFA_Free(pd_phys);
    return true;
}

void SwitchPageDirectory(u32 pd_phys) {
    asm volatile("mov %0, %%cr3\r\n" : : "r"(pd_phys));
    kernel_page_table[PAGE_PD] = pd_phys | PT_PRESENT | PT_READWRITE;
}

void* AllocateProgramMemory(u32 program_id, u32 pd, u32 size) {
    void* ret = NULL;

    if(kernel_page_table[PAGE_PD] != pd) {
        SwitchPageDirectory(pd);
    }

    u32 phys;
    if(PFA_Alloc(&phys, program_id, size)) {
        ret = MM_VirtualMapProgram(phys, (size + 4095) / 4096);
    }

    return ret;
}

void FreeProgramMemory(u32 pd, void* addr) {
    u32 phys;

    if(kernel_page_table[PAGE_PD] != pd) {
        SwitchPageDirectory(pd);
    }
    
    if(MM_MapToPhysical(&phys, addr)) {
        MM_VirtualUnmap(addr);
        PFA_Free(phys);
    }
}

void* kmalloc(u32 size) {
    void* ret = NULL;

    ASSERT(size > 0);

    if(size > 0) {
        auto round_size = (size + 4096) & 0xFFFFF000;
        //logprintf("Allocating memory for %d bytes (real size will be %d)\n", size, round_size);

        u32 phys;
        PFA_Alloc(&phys, round_size);
        ret = MM_VirtualMapKernel(phys);
        if(ret == NULL) {
            PFA_Free(phys);
        }
    }

    return ret;
}

void kfree(void* addr) {
    if(addr) {
        u32 phys;
        if(MM_MapToPhysical(&phys, addr)) {
            MM_VirtualUnmap(addr);
            PFA_Free(phys);
        }
    }
}
