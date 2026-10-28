#pragma once

#include <stdint.h>

// Syscall numbers
#define SYS_YIELD     0
#define SYS_EXIT      1
#define SYS_WRITE     2   // write to console
#define SYS_GETPID    3
#define SYS_CREATE    4   // create process (demo)
#define SYS_PS        5   // list processes

// Initialize syscall interface (registers int 0x80)
void syscall_init(void);

// C handler called from assembly
uint64_t syscall_handler(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3);
