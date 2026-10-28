#include "vmm.h"
#include "pmm.h"
#include "console.h"
#include <stdint.h>

typedef uint64_t pte_t;

static pte_t *kernel_pml4 = NULL;
static uint64_t kernel_pml4_phys = 0;
static uint64_t hhdm = 0;

static inline void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + hhdm);
}
static inline uint64_t virt_to_phys_direct(void *virt) {
    return (uint64_t)virt - hhdm;
}
static inline uint64_t pml4_index(uint64_t a) { return (a >> 39) & 0x1FF; }
static inline uint64_t pdpt_index(uint64_t a) { return (a >> 30) & 0x1FF; }
static inline uint64_t pd_index(uint64_t a)   { return (a >> 21) & 0x1FF; }
static inline uint64_t pt_index(uint64_t a)   { return (a >> 12) & 0x1FF; }

static pte_t *alloc_table(void) {
    uint64_t phys = pmm_alloc_page();
    if (!phys) return NULL;
    pte_t *t = (pte_t *)phys_to_virt(phys);
    for (int i = 0; i < 512; i++) t[i] = 0;
    return t;
}
static uint64_t table_phys(pte_t *t) { return virt_to_phys_direct(t); }

static bool map_in_pml4(pte_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
    virt &= ~(PAGE_SIZE - 1);
    phys &= ~(PAGE_SIZE - 1);

    uint64_t i4 = pml4_index(virt);
    uint64_t i3 = pdpt_index(virt);
    uint64_t i2 = pd_index(virt);
    uint64_t i1 = pt_index(virt);

    if (!(pml4[i4] & PTE_PRESENT)) {
        pte_t *pdpt = alloc_table();
        if (!pdpt) return false;
        pml4[i4] = table_phys(pdpt) | PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER);
    }
    pte_t *pdpt = (pte_t *)phys_to_virt(pml4[i4] & ~0xFFFULL);

    if (!(pdpt[i3] & PTE_PRESENT)) {
        pte_t *pd = alloc_table();
        if (!pd) return false;
        pdpt[i3] = table_phys(pd) | PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER);
    }
    pte_t *pd = (pte_t *)phys_to_virt(pdpt[i3] & ~0xFFFULL);

    if (!(pd[i2] & PTE_PRESENT)) {
        pte_t *pt = alloc_table();
        if (!pt) return false;
        pd[i2] = table_phys(pt) | PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER);
    }
    pte_t *pt = (pte_t *)phys_to_virt(pd[i2] & ~0xFFFULL);

    pt[i1] = phys | flags | PTE_PRESENT;
    asm volatile ("invlpg (%0)" : : "r"(virt) : "memory");
    return true;
}

bool vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    if (!kernel_pml4) return false;
    return map_in_pml4(kernel_pml4, virt, phys, flags);
}

bool vmm_map_page_in(uint64_t pml4_phys, uint64_t virt, uint64_t phys, uint64_t flags) {
    pte_t *pml4 = (pte_t *)phys_to_virt(pml4_phys);
    return map_in_pml4(pml4, virt, phys, flags);
}

void vmm_unmap_page(uint64_t virt) {
    if (!kernel_pml4) return;
    virt &= ~(PAGE_SIZE - 1);
    uint64_t i4 = pml4_index(virt);
    if (!(kernel_pml4[i4] & PTE_PRESENT)) return;
    pte_t *pdpt = (pte_t *)phys_to_virt(kernel_pml4[i4] & ~0xFFFULL);
    uint64_t i3 = pdpt_index(virt);
    if (!(pdpt[i3] & PTE_PRESENT)) return;
    pte_t *pd = (pte_t *)phys_to_virt(pdpt[i3] & ~0xFFFULL);
    uint64_t i2 = pd_index(virt);
    if (!(pd[i2] & PTE_PRESENT)) return;
    pte_t *pt = (pte_t *)phys_to_virt(pd[i2] & ~0xFFFULL);
    pt[pt_index(virt)] = 0;
    asm volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

uint64_t vmm_virt_to_phys(uint64_t virt) {
    if (!kernel_pml4) return 0;
    uint64_t off = virt & (PAGE_SIZE - 1);
    virt &= ~(PAGE_SIZE - 1);
    uint64_t i4 = pml4_index(virt);
    if (!(kernel_pml4[i4] & PTE_PRESENT)) return 0;
    pte_t *pdpt = (pte_t *)phys_to_virt(kernel_pml4[i4] & ~0xFFFULL);
    uint64_t i3 = pdpt_index(virt);
    if (!(pdpt[i3] & PTE_PRESENT)) return 0;
    pte_t *pd = (pte_t *)phys_to_virt(pdpt[i3] & ~0xFFFULL);
    uint64_t i2 = pd_index(virt);
    if (!(pd[i2] & PTE_PRESENT)) return 0;
    if (pd[i2] & PTE_HUGE) return (pd[i2] & ~0x1FFFFFULL) + (virt & 0x1FFFFF);
    pte_t *pt = (pte_t *)phys_to_virt(pd[i2] & ~0xFFFULL);
    if (!(pt[pt_index(virt)] & PTE_PRESENT)) return 0;
    return (pt[pt_index(virt)] & ~0xFFFULL) + off;
}

void vmm_switch_to_kernel(void) {
    if (kernel_pml4_phys)
        asm volatile ("mov %0, %%cr3" : : "r"(kernel_pml4_phys) : "memory");
}

void vmm_switch_address_space(uint64_t pml4_phys) {
    if (pml4_phys)
        asm volatile ("mov %0, %%cr3" : : "r"(pml4_phys) : "memory");
}

uint64_t vmm_get_pml4_phys(void) { return kernel_pml4_phys; }

uint64_t vmm_create_address_space(void) {
    pte_t *pml4 = alloc_table();
    if (!pml4) return 0;

    // Copy kernel half (entries 256-511) so kernel stays mapped
    for (int i = 256; i < 512; i++) {
        pml4[i] = kernel_pml4[i];
    }
    // Also keep lower identity for now (helps early hardware)
    for (int i = 0; i < 256; i++) {
        pml4[i] = kernel_pml4[i];
    }

    return table_phys(pml4);
}

void vmm_init(uint64_t hhdm_offset) {
    hhdm = hhdm_offset;
    kernel_pml4 = alloc_table();
    if (!kernel_pml4) {
        console_set_color(0xFF5555);
        console_print("[FAIL] PML4\n");
        return;
    }
    kernel_pml4_phys = table_phys(kernel_pml4);

    // Map first 4GiB identity + HHDM with 2MB pages
    for (uint64_t addr = 0; addr < 0x100000000ULL; addr += 0x200000ULL) {
        for (int pass = 0; pass < 2; pass++) {
            uint64_t virt = pass == 0 ? addr : addr + hhdm;
            uint64_t i4 = pml4_index(virt);
            uint64_t i3 = pdpt_index(virt);
            uint64_t i2 = pd_index(virt);

            if (!(kernel_pml4[i4] & PTE_PRESENT)) {
                pte_t *pdpt = alloc_table();
                if (!pdpt) return;
                kernel_pml4[i4] = table_phys(pdpt) | PTE_PRESENT | PTE_WRITABLE;
            }
            pte_t *pdpt = (pte_t *)phys_to_virt(kernel_pml4[i4] & ~0xFFFULL);
            if (!(pdpt[i3] & PTE_PRESENT)) {
                pte_t *pd = alloc_table();
                if (!pd) return;
                pdpt[i3] = table_phys(pd) | PTE_PRESENT | PTE_WRITABLE;
            }
            pte_t *pd = (pte_t *)phys_to_virt(pdpt[i3] & ~0xFFFULL);
            pd[i2] = addr | PTE_PRESENT | PTE_WRITABLE | PTE_HUGE;
        }
    }

    vmm_switch_to_kernel();
    console_set_color(0x55FF55);
    console_print("[OK] VMM (per-process AS support)\n");
}
