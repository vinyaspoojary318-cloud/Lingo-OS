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
#include "timer.h"
#include "kheap.h"
#include "fs.h"
#include "process.h"
#include "syscall.h"
#include "shell.h"

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0
};
__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST, .revision = 0
};
__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST, .revision = 0
};

static void hcf(void) { for (;;) asm volatile ("hlt"); }

void kmain(void) {
    if (LIMINE_BASE_REVISION_SUPPORTED == false) hcf();
    if (!framebuffer_request.response || framebuffer_request.response->framebuffer_count < 1 ||
        !memmap_request.response || !hhdm_request.response) hcf();

    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    uint64_t hhdm = hhdm_request.response->offset;

    console_init(fb);
    console_clear(0x0A0A1A);

    console_set_color(0x00FFCC);
    console_print("========================================\n");
    console_print("        Natunux Kernel v1.5\n");
    console_print("========================================\n\n");

    pmm_init(memmap_request.response, hhdm);
    console_set_color(0x55FF55); console_print("[OK] PMM\n");
    vmm_init(hhdm);
    gdt_init();
    idt_init();
    console_set_color(0x55FF55); console_print("[OK] GDT/IDT\n");
    timer_init(100);
    keyboard_init();
    console_set_color(0x55FF55); console_print("[OK] Keyboard\n");
    kheap_init();
    fs_init();
    process_init();
    syscall_init();

    enable_interrupts();

    console_set_color(0x00FFAA);
    console_print("\nNatunux — natural-language-first OS\n");
    console_print("  userdemo  then  runuser\n\n");

    shell_run();
    hcf();
}
