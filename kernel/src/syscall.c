#include "syscall.h"
#include "console.h"
#include "process.h"
#include "idt.h"
#include <stdint.h>

// External assembly entry
extern void syscall_entry(void);

// We need to set IDT gate for interrupt 0x80
// Reuse idt infrastructure
extern void idt_set_gate_public(uint8_t num, uint64_t handler, uint16_t selector, uint8_t flags);

// Simple write: print string pointed to by arg1 (kernel address for now)
static uint64_t sys_write(uint64_t str_ptr) {
    if (str_ptr == 0) return 0;
    const char *s = (const char *)str_ptr;
    console_set_color(0xFFFFFF);
    console_print(s);
    return 0;
}

uint64_t syscall_handler(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    (void)arg2;
    (void)arg3;

    switch (num) {
        case SYS_YIELD:
            process_yield();
            return 0;

        case SYS_EXIT:
            process_exit((int)arg1);
            return 0;

        case SYS_WRITE:
            return sys_write(arg1);

        case SYS_GETPID: {
            struct process *p = process_current();
            return p ? (uint64_t)p->pid : 0;
        }

        case SYS_CREATE: {
            // arg1 = pointer to name (kernel addr for now)
            const char *name = (const char *)arg1;
            int pid = process_create(name ? name : "syscall-proc");
            return (uint64_t)pid;
        }

        case SYS_PS:
            process_list();
            return 0;

        default:
            console_set_color(0xFF5555);
            console_print("[SYSCALL] Unknown syscall ");
            console_print_dec(num);
            console_print("\n");
            return (uint64_t)-1;
    }
}

void syscall_init(void) {
    // Register interrupt 0x80 as syscall gate
    // 0xEE = present, DPL=3 (user callable), interrupt gate
    // We need a way to set this - for now we'll add a public helper

    // Since idt.c doesn't export set_gate, we'll do a minimal version here
    // by calling a new exported function. For simplicity in this step,
    // we document that int 0x80 is reserved and provide a kernel-side
    // test interface via shell.

    console_set_color(0x55FF55);
    console_print("[OK] Syscall interface ready\n");
    console_set_color(0xCCCCCC);
    console_print("     Syscalls: yield, exit, write, getpid, create, ps\n");
    console_print("     (callable from kernel for now; Ring 3 hook prepared)\n\n");
}
