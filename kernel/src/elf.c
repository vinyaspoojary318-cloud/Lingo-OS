#include "elf.h"
#include "console.h"
#include "pmm.h"
#include "vmm.h"
#include <stddef.h>

static void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = dest;
    const uint8_t *s = src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

static void *memset(void *s, int c, size_t n) {
    uint8_t *p = s;
    for (size_t i = 0; i < n; i++) p[i] = (uint8_t)c;
    return s;
}

bool elf_check(const void *data, size_t size) {
    if (size < sizeof(struct elf64_header)) return false;

    const struct elf64_header *eh = data;

    if (eh->magic != ELF_MAGIC) return false;
    if (eh->cls != ELF_CLASS64) return false;
    if (eh->endian != ELF_DATA_LSB) return false;
    if (eh->type != ELF_TYPE_EXEC) return false;
    if (eh->machine != ELF_MACHINE_X86_64) return false;
    if (eh->phoff == 0 || eh->phnum == 0) return false;

    // Basic bounds check
    uint64_t ph_end = eh->phoff + (uint64_t)eh->phnum * eh->phentsize;
    if (ph_end > size) return false;

    return true;
}

bool elf_load(const void *data, size_t size, struct elf_load_info *info) {
    if (!info) return false;
    info->success = false;
    info->entry = 0;
    info->user_stack_top = 0;

    if (!elf_check(data, size)) {
        console_set_color(0xFF5555);
        console_print("[ELF] Invalid ELF file\n");
        return false;
    }

    const struct elf64_header *eh = data;
    const uint8_t *base = data;

    console_set_color(0xAAAAFF);
    console_print("[ELF] Loading executable\n");
    console_set_color(0xCCCCCC);
    console_print("      Entry point: ");
    console_print_hex(eh->entry);
    console_print("\n");
    console_print("      Program headers: ");
    console_print_dec(eh->phnum);
    console_print("\n");

    // Load each PT_LOAD segment
    for (uint16_t i = 0; i < eh->phnum; i++) {
        const struct elf64_phdr *ph = (const struct elf64_phdr *)(base + eh->phoff + i * eh->phentsize);

        if (ph->type != PT_LOAD) continue;

        if (ph->offset + ph->filesz > size) {
            console_set_color(0xFF5555);
            console_print("[ELF] Segment out of bounds\n");
            return false;
        }

        console_print("      Segment vaddr=");
        console_print_hex(ph->vaddr);
        console_print(" filesz=");
        console_print_hex(ph->filesz);
        console_print(" memsz=");
        console_print_hex(ph->memsz);
        console_print("\n");

        // Map pages for this segment
        uint64_t seg_start = ph->vaddr & ~(PAGE_SIZE - 1);
        uint64_t seg_end   = (ph->vaddr + ph->memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        for (uint64_t va = seg_start; va < seg_end; va += PAGE_SIZE) {
            uint64_t phys = pmm_alloc_page();
            if (phys == 0) {
                console_set_color(0xFF5555);
                console_print("[ELF] Out of memory mapping segment\n");
                return false;
            }

            uint64_t flags = PTE_PRESENT | PTE_WRITABLE | PTE_USER;
            if (!vmm_map_page(va, phys, flags)) {
                console_set_color(0xFF5555);
                console_print("[ELF] Failed to map page\n");
                return false;
            }

            // Zero the page
            memset((void *)va, 0, PAGE_SIZE);
        }

        // Copy file data into the mapped region
        if (ph->filesz > 0) {
            memcpy((void *)ph->vaddr, base + ph->offset, ph->filesz);
        }
    }

    // Set up a simple user stack at a fixed high address (for demo)
    // 0x7FFFFFF000 - a few pages
    uint64_t stack_top = 0x7FFFFFF000ULL;
    uint64_t stack_size = 16 * PAGE_SIZE; // 64 KiB stack

    for (uint64_t off = 0; off < stack_size; off += PAGE_SIZE) {
        uint64_t va = stack_top - stack_size + off;
        uint64_t phys = pmm_alloc_page();
        if (phys == 0) {
            console_set_color(0xFF5555);
            console_print("[ELF] Out of memory for user stack\n");
            return false;
        }
        vmm_map_page(va, phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
        memset((void *)va, 0, PAGE_SIZE);
    }

    info->entry = eh->entry;
    info->user_stack_top = stack_top;
    info->success = true;

    console_set_color(0x55FF55);
    console_print("[ELF] Load successful\n");
    console_set_color(0xCCCCCC);
    console_print("      User stack top: ");
    console_print_hex(stack_top);
    console_print("\n\n");

    return true;
}
