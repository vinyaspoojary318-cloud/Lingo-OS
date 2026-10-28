#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Page size
#define PAGE_SIZE 4096ULL

// Page table entry flags
#define PTE_PRESENT     (1ULL << 0)
#define PTE_WRITABLE    (1ULL << 1)
#define PTE_USER        (1ULL << 2)   // Ring 3 accessible
#define PTE_WRITE_THROUGH (1ULL << 3)
#define PTE_CACHE_DISABLE (1ULL << 4)
#define PTE_ACCESSED    (1ULL << 5)
#define PTE_DIRTY       (1ULL << 6)
#define PTE_HUGE        (1ULL << 7)   // 2MB page
#define PTE_GLOBAL      (1ULL << 8)
#define PTE_NX          (1ULL << 63)  // No execute

// Initialize virtual memory (sets up kernel page tables)
void vmm_init(uint64_t hhdm_offset);

// Map a virtual page to a physical page with flags
bool vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);

// Unmap a virtual page
void vmm_unmap_page(uint64_t virt);

// Translate virtual address to physical (returns 0 if not mapped)
uint64_t vmm_virt_to_phys(uint64_t virt);

// Switch to the kernel address space (loads CR3)
void vmm_switch_to_kernel(void);

// Get current PML4 physical address
uint64_t vmm_get_pml4_phys(void);
