#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_PROCESSES 32
#define KERNEL_STACK_SIZE 8192   // 8 KiB kernel stack per process

enum process_state {
    PROC_UNUSED = 0,
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_ZOMBIE
};

// Saved CPU registers for context switch
struct cpu_context {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

struct process {
    int pid;
    enum process_state state;
    char name[32];

    // Address space
    uint64_t pml4_phys;          // Physical address of process PML4

    // Kernel stack (used when process enters kernel via syscall/interrupt)
    uint64_t kernel_stack;       // Top of kernel stack (virtual)
    uint64_t kernel_stack_phys;

    // Saved context
    struct cpu_context ctx;

    // Simple exit code
    int exit_code;
};

// Initialize process subsystem (creates kernel process 0)
void process_init(void);

// Create a new process (returns pid, or -1 on failure)
int process_create(const char *name);

// Get current running process
struct process *process_current(void);

// Get process by PID
struct process *process_get(int pid);

// Set process state
void process_set_state(int pid, enum process_state state);

// Simple cooperative yield (placeholder for real scheduler)
void process_yield(void);

// Exit current process
void process_exit(int code);

// Debug: list all processes
void process_list(void);
