#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_PROCESSES 32
#define KERNEL_STACK_SIZE 8192

enum process_state {
    PROC_UNUSED = 0,
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_ZOMBIE
};

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
    uint64_t pml4_phys;
    uint64_t kernel_stack;
    uint64_t kernel_stack_phys;
    struct cpu_context ctx;
    int exit_code;
    bool is_user;            // true if Ring 3 process
};

void process_init(void);
int process_create(const char *name);
struct process *process_current(void);
struct process *process_get(int pid);
void process_set_state(int pid, enum process_state state);
void process_yield(void);
void process_exit(int code);
void process_list(void);

// Switch from current process to another
void process_switch(struct process *next);

// Test: create a demo user process (Ring 3 entry prepared)
int process_create_user_demo(const char *name);
