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
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip, cs, rflags, rsp, ss;
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
    bool is_user;
    uint64_t user_entry;
    uint64_t user_stack;
};

void process_init(void);
int process_create(const char *name);
int process_create_user_demo(const char *name);
struct process *process_current(void);
struct process *process_get(int pid);
void process_yield(void);
void process_exit(int code);
int process_kill(int pid);
void process_list(void);
void process_switch(struct process *next);
void process_schedule(void);  // called from timer for preemption
void process_enter_user_demo(void); // run current user demo if any
