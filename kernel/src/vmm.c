#include "vmm.h"
#include "pmm.h"
#include "console.h"
#include <stdint.h>

// We use 4-level paging (PML4 -> PDPT -> PD -> PT)

typedef uint64_t pte_t;

static pte_t *kernel_pml4 = NULL;
static uint64_t kernel_pml4_phys = 0;
static uint64_t hhdm = 0;

// Convert physical address to virtual using HHDM
static inline void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + hhdm);
}

static inline uint64_t virt_to_phys_direct(void *virt) {
    return (uint64_t)virt - hhdm;
}

// Extract indices from virtual address
static inline uint64_t pml4_index(uint64_t addr) { return (addr >> 39) & 0x1FF; }
static inline uint64_t pdpt_index(uint64_t addr) { return (addr >> 30) & 0x1FF; }
static inline uint64_t pd_index(uint64_t addr)   { return (addr >> 21) & 0x1FF; }
static inline uint64_t pt_index(uint64_t addr)   { return (addr >> 12) & 0x1FF; }

// Allocate a zeroed page table page
static pte_t *alloc_table(void) {
    uint64_t phys = pmm_alloc_page();
    if (phys == 0) return NULL;

    pte_t *table = (pte_t *)phys_to_virt(phys);

    // Zero the page
    for (int i = 0; i < 512; i++) {
        table[i] = 0;
    }

    return table;
}

static uint64_t table_to_phys(pte_t *table) {
    return virt_to_phys_direct(table);
}

bool vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    if (!kernel_pml4) return false;

    // Align addresses
    virt &= ~(PAGE_SIZE - 1);
    phys &= ~(PAGE_SIZE - 1);

    uint64_t i4 = pml4_index(virt);
    uint64_t i3 = pdpt_index(virt);
    uint64_t i2 = pd_index(virt);
    uint64_t i1 = pt_index(virt);

    // PML4
    if (!(kernel_pml4[i4] & PTE_PRESENT)) {
        pte_t *pdpt = alloc_table();
        if (!pdpt) return false;
        kernel_pml4[i4] = table_to_phys(pdpt) | PTE_PRESENT | PTE_WRITABLE;
    }
    pte_t *pdpt = (pte_t *)phys_to_virt(kernel_pml4[i4] & ~0xFFFULL);

    // PDPT
    if (!(pdpt[i3] & PTE_PRESENT)) {
        pte_t *pd = alloc_table();
        if (!pd) return false;
        pdpt[i3] = table_to_phys(pd) | PTE_PRESENT | PTE_WRITABLE;
    }
    pte_t *pd = (pte_t *)phys_to_virt(pdpt[i3] & ~0xFFFULL);

    // PD
    if (!(pd[i2] & PTE_PRESENT)) {
        pte_t *pt = alloc_table();
        if (!pt) return false;
        pd[i2] = table_to_phys(pt) | PTE_PRESENT | PTE_WRITABLE;
    }
    pte_t *pt = (pte_t *)phys_to_virt(pd[i2] & ~0xFFFULL);

    // PT - map the page
    pt[i1] = phys | flags | PTE_PRESENT;

    // Invalidate TLB for this page
    asm volatile ("invlpg (%0)" : : "r"(virt) : "memory");

    return true;
}

void vmm_unmap_page(uint64_t virt) {
    if (!kernel_pml4) return;

    virt &= ~(PAGE_SIZE - 1);

    uint64_t i4 = pml4_index(virt);
    uint64_t i3 = pdpt_index(virt);
    uint64_t i2 = pd_index(virt);
    uint64_t i1 = pt_index(virt);

    if (!(kernel_pml4[i4] & PTE_PRESENT)) return;
    pte_t *pdpt = (pte_t *)phys_to_virt(kernel_pml4[i4] & ~0xFFFULL);

    if (!(pdpt[i3] & PTE_PRESENT)) return;
    pte_t *pd = (pte_t *)phys_to_virt(pdpt[i3] & ~0xFFFULL);

    if (!(pd[i2] & PTE_PRESENT)) return;
    pte_t *pt = (pte_t *)phys_to_virt(pd[i2] & ~0xFFFULL);

    pt[i1] = 0;
    asm volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

uint64_t vmm_virt_to_phys(uint64_t virt) {
    if (!kernel_pml4) return 0;

    uint64_t offset = virt & (PAGE_SIZE - 1);
    virt &= ~(PAGE_SIZE - 1);

    uint64_t i4 = pml4_index(virt);
    uint64_t i3 = pdpt_index(virt);
    uint64_t i2 = pd_index(virt);
    uint64_t i1 = pt_index(virt);

    if (!(kernel_pml4[i4] & PTE_PRESENT)) return 0;
    pte_t *pdpt = (pte_t *)phys_to_virt(kernel_pml4[i4] & ~0xFFFULL);

    if (!(pdpt[i3] & PTE_PRESENT)) return 0;
    pte_t *pd = (pte_t *)phys_to_virt(pdpt[i3] & ~0xFFFULL);

    if (!(pd[i2] & PTE_PRESENT)) return 0;

    // Support huge pages (2MB)
    if (pd[i2] & PTE_HUGE) {
        return (pd[i2] & ~0x1FFFFFULL) + (virt & 0x1FFFFF);
    }

    pte_t *pt = (pte_t *)phys_to_virt(pd[i2] & ~0xFFFULL);
    if (!(pt[i1] & PTE_PRESENT)) return 0;

    return (pt[i1] & ~0xFFFULL) + offset;
}

void vmm_switch_to_kernel(void) {
    if (kernel_pml4_phys) {
        asm volatile ("mov %0, %%cr3" : : "r"(kernel_pml4_phys) : "memory");
    }
}

uint64_t vmm_get_pml4_phys(void) {
    return kernel_pml4_phys;
}

void vmm_init(uint64_t hhdm_offset) {
    hhdm = hhdm_offset;

    // Allocate PML4
    kernel_pml4 = alloc_table();
    if (!kernel_pml4) {
        console_set_color(0xFF5555);
        console_print("[FAIL] Could not allocate PML4\n");
        return;
    }
    kernel_pml4_phys = table_to_phys(kernel_pml4);

    // Map first 4 GiB as identity (useful for early hardware access)
    // Using 2MB huge pages for efficiency
    for (uint64_t addr = 0; addr < 0x100000000ULL; addr += 0x200000ULL) {
        uint64_t i4 = pml4_index(addr);
        uint64_t i3 = pdpt_index(addr);
        uint64_t i2 = pd_index(addr);

        if (!(kernel_pml4[i4] & PTE_PRESENT)) {
            pte_t *pdpt = alloc_table();
            if (!pdpt) break;
            kernel_pml4[i4] = table_to_phys(pdpt) | PTE_PRESENT | PTE_WRITABLE;
        }
        pte_t *pdpt = (pte_t *)phys_to_virt(kernel_pml4[i4] & ~0xFFFULL);

        if (!(pdpt[i3] & PTE_PRESENT)) {
            pte_t *pd = alloc_table();
            if (!pd) break;
            pdpt[i3] = table_to_phys(pd) | PTE_PRESENT | PTE_WRITABLE;
        }
        pte_t *pd = (pte_t *)phys_to_virt(pdpt[i3] & ~0xFFFULL);

        // 2MB huge page
        pd[i2] = addr | PTE_PRESENT | PTE_WRITABLE | PTE_HUGE;
    }

    // Also map the same region into higher half (HHDM style)
    // This keeps the HHDM working
    for (uint64_t addr = 0; addr < 0x100000000ULL; addr += 0x200000ULL) {
        uint64_t virt = addr + hhdm;
        uint64_t i4 = pml4_index(virt);
        uint64_t i3 = pdpt_index(virt);
        uint64_t i2 = pd_index(virt);

        if (!(kernel_pml4[i4] & PTE_PRESENT)) {
            pte_t *pdpt = alloc_table();
            if (!pdpt) break;
            kernel_pml4[i4] = table_to_phys(pdpt) | PTE_PRESENT | PTE_WRITABLE;
        }
        pte_t *pdpt = (pte_t *)phys_to_virt(kernel_pml4[i4] & ~0xFFFULL);

        if (!(pdpt[i3] & PTE_PRESENT)) {
            pte_t *pd = alloc_table();
            if (!pd) break;
            pdpt[i3] = table_to_phys(pd) | PTE_PRESENT | PTE_WRITABLE;
        }
        pte_t *pd = (pte_t *)phys_to_virt(pdpt[i3] & ~0xFFFULL);

        pd[i2] = addr | PTE_PRESENT | PTE_WRITABLE | PTE_HUGE;
    }

    // Load our page tables
    vmm_switch_to_kernel();

    console_set_color(0x55FF55);
    console_print("[OK] Virtual Memory Manager ready\n");
    console_set_color(0xCCCCCC);
    console_print("     PML4 at ");
    console_print_hex(kernel_pml4_phys);
    console_print("\n");
    console_print("     Identity + HHDM mapped (first 4 GiB)\n\n");
}
