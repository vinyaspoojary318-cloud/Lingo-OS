#include "process.h"
#include "pmm.h"
#include "vmm.h"
#include "console.h"
#include "gdt.h"
#include <stddef.h>

extern void context_switch(struct cpu_context *old, struct cpu_context *new);
extern void enter_user_mode(uint64_t entry, uint64_t user_stack);

static struct process processes[MAX_PROCESSES];
static int current_pid = 0;
static int next_pid = 1;

static void str_copy(char *dest, const char *src, size_t max) {
    size_t i = 0;
    while (src && src[i] && i < max - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = 0;
}

void process_init(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        processes[i].state = PROC_UNUSED;
        processes[i].pid = -1;
        processes[i].pml4_phys = 0;
        processes[i].kernel_stack = 0;
        processes[i].kernel_stack_phys = 0;
        processes[i].exit_code = 0;
        processes[i].name[0] = 0;
        processes[i].is_user = false;
    }

    struct process *kproc = &processes[0];
    kproc->pid = 0;
    kproc->state = PROC_RUNNING;
    str_copy(kproc->name, "kernel", 32);
    kproc->pml4_phys = vmm_get_pml4_phys();
    kproc->is_user = false;

    current_pid = 0;
    next_pid = 1;

    console_set_color(0x55FF55);
    console_print("[OK] Process subsystem ready\n");
    console_set_color(0xCCCCCC);
    console_print("     Kernel process (PID 0) running\n\n");
}

int process_create(const char *name) {
    int slot = -1;
    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_UNUSED) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return -1;

    struct process *p = &processes[slot];
    p->pid = next_pid++;
    p->state = PROC_READY;
    str_copy(p->name, name ? name : "unnamed", 32);
    p->exit_code = 0;
    p->is_user = false;

    uint64_t stack_phys = pmm_alloc_pages(2);
    if (stack_phys == 0) {
        p->state = PROC_UNUSED;
        return -1;
    }

    p->kernel_stack_phys = stack_phys;
    p->kernel_stack = (uint64_t)pmm_phys_to_virt(stack_phys) + KERNEL_STACK_SIZE;
    p->pml4_phys = vmm_get_pml4_phys();

    for (size_t i = 0; i < sizeof(struct cpu_context); i++)
        ((uint8_t *)&p->ctx)[i] = 0;

    p->ctx.rsp = p->kernel_stack;
    p->ctx.rflags = 0x202;

    return p->pid;
}

struct process *process_current(void) {
    if (current_pid < 0 || current_pid >= MAX_PROCESSES) return NULL;
    if (processes[current_pid].state == PROC_UNUSED) return NULL;
    return &processes[current_pid];
}

struct process *process_get(int pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state != PROC_UNUSED && processes[i].pid == pid)
            return &processes[i];
    }
    return NULL;
}

void process_set_state(int pid, enum process_state state) {
    struct process *p = process_get(pid);
    if (p) p->state = state;
}

void process_switch(struct process *next) {
    if (!next || next->state == PROC_UNUSED) return;

    struct process *cur = process_current();
    if (!cur) return;

    if (cur == next) return;

    cur->state = PROC_READY;
    next->state = PROC_RUNNING;
    current_pid = next->pid;

    // Update TSS with new kernel stack
    tss_init(next->kernel_stack);

    // Perform context switch
    context_switch(&cur->ctx, &next->ctx);
}

void process_yield(void) {
    // Find next READY process (simple round-robin)
    int start = current_pid;
    for (int i = 1; i <= MAX_PROCESSES; i++) {
        int idx = (start + i) % MAX_PROCESSES;
        if (processes[idx].state == PROC_READY) {
            process_switch(&processes[idx]);
            return;
        }
    }
    // No other process - just pause
    asm volatile ("pause");
}

void process_exit(int code) {
    struct process *p = process_current();
    if (!p || p->pid == 0) return;

    p->exit_code = code;
    p->state = PROC_ZOMBIE;

    if (p->kernel_stack_phys) {
        pmm_free_pages(p->kernel_stack_phys, 2);
        p->kernel_stack_phys = 0;
        p->kernel_stack = 0;
    }

    // Switch back to kernel process
    current_pid = 0;
    processes[0].state = PROC_RUNNING;
}

void process_list(void) {
    console_set_color(0xAAAAFF);
    console_print("PID  STATE     USER  NAME\n");
    console_set_color(0xCCCCCC);

    for (int i = 0; i < MAX_PROCESSES; i++) {
        struct process *p = &processes[i];
        if (p->state == PROC_UNUSED) continue;

        console_print_dec(p->pid);
        if (p->pid < 10) console_print("    ");
        else console_print("   ");

        switch (p->state) {
            case PROC_READY:   console_print("READY    "); break;
            case PROC_RUNNING: console_print("RUNNING  "); break;
            case PROC_BLOCKED: console_print("BLOCKED  "); break;
            case PROC_ZOMBIE:  console_print("ZOMBIE   "); break;
            default:           console_print("UNKNOWN  "); break;
        }

        console_print(p->is_user ? "Yes   " : "No    ");
        console_print(p->name);
        console_print("\n");
    }
    console_print("\n");
}

int process_create_user_demo(const char *name) {
    int pid = process_create(name);
    if (pid < 0) return -1;

    struct process *p = process_get(pid);
    if (!p) return -1;

    p->is_user = true;
    // Note: actual enter_user_mode requires a mapped user page + entry point.
    // This marks the process as user-mode ready for future ELF loading.

    return pid;
}
