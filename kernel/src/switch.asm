; Context switch, Ring 3 entry, syscall entry — New-OS
bits 64

global context_switch
global enter_user_mode
global syscall_entry
global return_to_shell_asm

extern syscall_handler
extern user_exit_return

; void context_switch(struct cpu_context *old, struct cpu_context *new)
context_switch:
    mov [rdi + 0*8], r15
    mov [rdi + 1*8], r14
    mov [rdi + 2*8], r13
    mov [rdi + 3*8], r12
    mov [rdi + 4*8], r11
    mov [rdi + 5*8], r10
    mov [rdi + 6*8], r9
    mov [rdi + 7*8], r8
    mov [rdi + 8*8], rbp
    mov [rdi + 9*8], rdi
    mov [rdi + 10*8], rsi
    mov [rdi + 11*8], rdx
    mov [rdi + 12*8], rcx
    mov [rdi + 13*8], rbx
    mov [rdi + 14*8], rax
    mov rax, [rsp]
    mov [rdi + 15*8], rax
    pushfq
    pop rax
    mov [rdi + 17*8], rax
    mov [rdi + 18*8], rsp

    mov r15, [rsi + 0*8]
    mov r14, [rsi + 1*8]
    mov r13, [rsi + 2*8]
    mov r12, [rsi + 3*8]
    mov r11, [rsi + 4*8]
    mov r10, [rsi + 5*8]
    mov r9,  [rsi + 6*8]
    mov r8,  [rsi + 7*8]
    mov rbp, [rsi + 8*8]
    mov rdx, [rsi + 11*8]
    mov rcx, [rsi + 12*8]
    mov rbx, [rsi + 13*8]
    mov rax, [rsi + 14*8]
    mov rsp, [rsi + 18*8]
    mov rax, [rsi + 17*8]
    push rax
    popfq
    mov rax, [rsi + 15*8]
    push rax
    mov rdi, [rsi + 9*8]
    mov rsi, [rsi + 10*8]
    ret

; void enter_user_mode(uint64_t entry, uint64_t user_stack)
enter_user_mode:
    mov ax, 0x23
    mov ds, ax
    mov es, ax

    push 0x23              ; SS
    push rsi               ; RSP
    pushfq
    pop rax
    or rax, 0x200
    push rax               ; RFLAGS
    push 0x1B              ; CS (user code)
    push rdi               ; RIP

    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rbp, rbp
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15
    iretq

; int 0x80 entry — user/kernel
syscall_entry:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Linux-ish: rax=num, rdi=arg1, rsi=arg2, rdx=arg3
    ; After pushes, original rax is at [rsp+14*8]
    mov rdi, [rsp + 14*8]   ; syscall number (rax)
    mov rsi, [rsp + 9*8]    ; arg1 (original rdi)
    mov rdx, [rsp + 8*8]    ; arg2 (original rsi)
    mov rcx, [rsp + 11*8]   ; arg3 (original rdx)

    call syscall_handler

    ; If handler returned special value 0xDEADBEEF, jump to shell return
    mov rbx, 0xDEADBEEF
    cmp rax, rbx
    je .exit_to_shell

    mov [rsp + 14*8], rax   ; return value -> user rax

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    iretq

.exit_to_shell:
    ; Discard interrupt frame and go to C return path
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    call user_exit_return
    ; never returns; if it does, halt
    jmp $
