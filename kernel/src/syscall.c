#include "syscall.h"
#include "console.h"
#include "process.h"
#include "user.h"
#include "vmm.h"
#include "idt.h"
#include <stdint.h>

#define SYSCALL_EXIT_TO_SHELL 0xDEADBEEFULL

extern void shell_run(void);

uint64_t syscall_handler(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    (void)arg3;

    switch (num) {
        case SYS_YIELD:
            process_yield();
            return 0;

        case SYS_EXIT: {
            struct process *p = process_current();
            if (p && p->is_user) {
                console_set_color(0x55FF55);
                console_print("\n[USER] Process exited (code ");
                console_print_dec((int)arg1);
                console_print(") — returning to shell\n\n");
                process_exit((int)arg1);
                return SYSCALL_EXIT_TO_SHELL;
            }
            process_exit((int)arg1);
            return 0;
        }

        case SYS_WRITE: {
            if (arg1 == 0) return (uint64_t)-1;
            // Kernel or identity-mapped string for demo
            console_set_color(0xFFFFFF);
            console_print((const char *)arg1);
            return 0;
        }

        case SYS_GETPID: {
            struct process *p = process_current();
            return p ? (uint64_t)p->pid : 0;
        }

        case SYS_CREATE:
            return (uint64_t)process_create(arg1 ? (const char *)arg1 : "syscall-proc");

        case SYS_PS:
            process_list();
            return 0;

        default:
            console_set_color(0xFF5555);
            console_print("[SYSCALL] Unknown ");
            console_print_dec(num);
            console_print("\n");
            return (uint64_t)-1;
    }
}

void syscall_init(void) {
    // Gate registered in idt_init (int 0x80, DPL=3)
    console_set_color(0x55FF55);
    console_print("[OK] Syscall int 0x80 (Ring3 callable)\n");
}

// Called from assembly after user SYS_EXIT
void user_exit_return(void) {
    vmm_switch_to_kernel();
    enable_interrupts();
    shell_run();
    for (;;) asm volatile ("hlt");
}
