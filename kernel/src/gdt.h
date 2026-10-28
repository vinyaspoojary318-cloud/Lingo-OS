#pragma once

#include <stdint.h>

// Initialize GDT with kernel and user segments
void gdt_init(void);

// Load TSS (needed for Ring 3 stack switching)
void tss_init(uint64_t kernel_stack);
