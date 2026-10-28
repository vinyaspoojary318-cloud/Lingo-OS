#include "gdt.h"
#include "console.h"
#include <stdint.h>

// GDT entry
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// TSS for 64-bit
struct tss {
    uint32_t reserved0;
    uint64_t rsp0;       // Kernel stack for Ring 0 entry
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

static struct gdt_entry gdt[7];
static struct gdt_ptr gdtr;
static struct tss tss;

static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low    = base & 0xFFFF;
    gdt[num].base_mid    = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;
    gdt[num].limit_low   = limit & 0xFFFF;
    gdt[num].granularity = (limit >> 16) & 0x0F;
    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access      = access;
}

void gdt_init(void) {
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base  = (uint64_t)&gdt;

    // Null descriptor
    gdt_set_gate(0, 0, 0, 0, 0);

    // Kernel code (0x08) - 64-bit
    gdt_set_gate(1, 0, 0xFFFFF, 0x9A, 0xA0);

    // Kernel data (0x10)
    gdt_set_gate(2, 0, 0xFFFFF, 0x92, 0xA0);

    // User code (0x18) - 64-bit, DPL=3  --> selector 0x1B with RPL
    gdt_set_gate(3, 0, 0xFFFFF, 0xFA, 0xA0);

    // User data (0x20) - DPL=3 --> selector 0x23 with RPL
    gdt_set_gate(4, 0, 0xFFFFF, 0xF2, 0xA0);

    // TSS descriptor (takes 2 entries in long mode)
    uint64_t tss_base = (uint64_t)&tss;
    uint32_t tss_limit = sizeof(tss) - 1;

    gdt[5].limit_low   = tss_limit & 0xFFFF;
    gdt[5].base_low    = tss_base & 0xFFFF;
    gdt[5].base_mid    = (tss_base >> 16) & 0xFF;
    gdt[5].access      = 0x89; // present, TSS
    gdt[5].granularity = (tss_limit >> 16) & 0x0F;
    gdt[5].base_high   = (tss_base >> 24) & 0xFF;

    // High part of TSS base
    gdt[6].limit_low   = (tss_base >> 32) & 0xFFFF;
    gdt[6].base_low    = (tss_base >> 48) & 0xFFFF;
    gdt[6].base_mid    = 0;
    gdt[6].access      = 0;
    gdt[6].granularity = 0;
    gdt[6].base_high   = 0;

    // Load GDT
    asm volatile ("lgdt %0" : : "m"(gdtr));

    // Reload segments
    asm volatile (
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        : : : "rax", "memory"
    );

    // Load TSS
    asm volatile ("ltr %%ax" : : "a"(0x28)); // TSS selector

    console_set_color(0x55FF55);
    console_print("[OK] GDT + TSS ready (Ring 0 + Ring 3 segments)\n");
}

void tss_init(uint64_t kernel_stack) {
    // Clear TSS
    for (size_t i = 0; i < sizeof(tss); i++) {
        ((uint8_t *)&tss)[i] = 0;
    }
    tss.rsp0 = kernel_stack;
    tss.iomap_base = sizeof(tss);
}
