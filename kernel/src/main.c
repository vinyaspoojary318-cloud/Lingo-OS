#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include "console.h"
#include "pmm.h"
#include "vmm.h"
#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "fs.h"
#include "process.h"
#include "syscall.h"
#include "elf.h"
#include "shell.h"

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

static void hcf(void) {
    for (;;) asm volatile ("hlt");
}

void kmain(void) {
    if (LIMINE_BASE_REVISION_SUPPORTED == false) hcf();

    if (framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1 ||
        memmap_request.response == NULL ||
        hhdm_request.response == NULL) {
        hcf();
    }

    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    uint64_t hhdm = hhdm_request.response->offset;

    console_init(fb);
    console_clear(0x0A0A1A);

    console_set_color(0x00FFCC);
    console_print("========================================\n");
    console_print("         New-OS Kernel v1.1\n");
    console_print("========================================\n\n");

    console_set_color(0xFFCC55); console_print("[..] PMM...\n");
    pmm_init(memmap_request.response, hhdm);
    console_set_color(0x55FF55); console_print("[OK] Physical Memory Manager\n");

    console_set_color(0xFFCC55); console_print("[..] VMM...\n");
    vmm_init(hhdm);

    console_set_color(0xFFCC55); console_print("[..] GDT + TSS...\n");
    gdt_init();

    console_set_color(0xFFCC55); console_print("[..] IDT + PIC...\n");
    idt_init();
    console_set_color(0x55FF55); console_print("[OK] Interrupts ready\n");

    console_set_color(0xFFCC55); console_print("[..] Keyboard...\n");
    keyboard_init();
    console_set_color(0x55FF55); console_print("[OK] Keyboard ready\n");

    console_set_color(0xFFCC55); console_print("[..] Filesystem...\n");
    fs_init();
    console_set_color(0x55FF55); console_print("[OK] Filesystem ready\n");

    console_set_color(0xFFCC55); console_print("[..] Processes...\n");
    process_init();

    console_set_color(0xFFCC55); console_print("[..] Syscalls...\n");
    syscall_init();

    console_set_color(0x55FF55);
    console_print("[OK] ELF64 loader ready\n\n");

    enable_interrupts();

    console_set_color(0x00FFAA);
    console_print("Dual-mode stack:\n");
    console_print("  Paging | GDT/Ring3 | Processes | Context switch\n");
    console_print("  Syscalls | ELF loader\n\n");

    shell_run();
    hcf();
}
