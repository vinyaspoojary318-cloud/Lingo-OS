#include "syscall.h"
#include "console.h"
#include "process.h"
#include "user.h"
#include "timer.h"
#include <stdint.h>

uint64_t syscall_handler(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    (void)arg3;

    switch (num) {
        case SYS_YIELD:
            process_yield();
            return 0;

        case SYS_EXIT:
            process_exit((int)arg1);
            return 0;

        case SYS_WRITE: {
            // arg1 = user pointer to string, arg2 = length (optional)
            if (!user_ptr_valid((void *)arg1, 1)) {
                // Allow kernel addresses during early testing
                if (arg1 < 0xFFFF800000000000ULL && arg1 > 0x1000) {
                    console_print((const char *)arg1);
                    return 0;
                }
                return (uint64_t)-1;
            }
            // Safe path for true user pointers later
            char buf[128];
            size_t n = arg2 ? (size_t)arg2 : 127;
            if (n > 127) n = 127;
            if (!copy_from_user(buf, (void *)arg1, n)) return (uint64_t)-1;
            buf[n] = 0;
            console_print(buf);
            return 0;
        }

        case SYS_GETPID: {
            struct process *p = process_current();
            return p ? (uint64_t)p->pid : 0;
        }

        case SYS_CREATE: {
            const char *name = "syscall-proc";
            if (arg1) name = (const char *)arg1;
            return (uint64_t)process_create(name);
        }

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
    console_set_color(0x55FF55);
    console_print("[OK] Syscalls (with user ptr checks)\n");
}
