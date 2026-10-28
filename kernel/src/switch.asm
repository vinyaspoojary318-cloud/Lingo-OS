; Context switch and Ring 3 entry for New-OS
; NASM syntax, 64-bit

global context_switch
global enter_user_mode
global syscall_entry

extern syscall_handler

; void context_switch(struct cpu_context *old, struct cpu_context *new)
; rdi = old context
; rsi = new context
context_switch:
    ; Save old context
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

    ; Save RIP (return address)
    mov rax, [rsp]
    mov [rdi + 15*8], rax

    ; Save RFLAGS
    pushfq
    pop rax
    mov [rdi + 17*8], rax

    ; Save RSP
    mov [rdi + 18*8], rsp

    ; Load new context
    mov r15, [rsi + 0*8]
    mov r14, [rsi + 1*8]
    mov r13, [rsi + 2*8]
    mov r12, [rsi + 3*8]
    mov r11, [rsi + 4*8]
    mov r10, [rsi + 5*8]
    mov r9,  [rsi + 6*8]
    mov r8,  [rsi + 7*8]
    mov rbp, [rsi + 8*8]
    ; skip rdi/rsi for now
    mov rdx, [rsi + 11*8]
    mov rcx, [rsi + 12*8]
    mov rbx, [rsi + 13*8]
    mov rax, [rsi + 14*8]

    ; Load new RSP
    mov rsp, [rsi + 18*8]

    ; Load RFLAGS
    mov rax, [rsi + 17*8]
    push rax
    popfq

    ; Jump to new RIP
    mov rax, [rsi + 15*8]
    push rax

    mov rdi, [rsi + 9*8]
    mov rsi, [rsi + 10*8]

    ret

; void enter_user_mode(uint64_t entry, uint64_t user_stack)
; rdi = user entry point
; rsi = user stack pointer
enter_user_mode:
    ; Build iretq frame for Ring 3
    ; SS
    mov ax, 0x23          ; User data segment (GDT index 4, RPL=3)
    push rax

    ; RSP (user stack)
    push rsi

    ; RFLAGS
    pushfq
    pop rax
    or rax, 0x200         ; Enable interrupts
    push rax

    ; CS
    mov ax, 0x1B          ; User code segment (GDT index 3, RPL=3)
    push rax

    ; RIP
    push rdi

    ; Clear general registers for safety
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

; Syscall entry point (called via interrupt 0x80 or syscall later)
syscall_entry:
    ; Save user registers
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

    ; Arguments: rdi already has syscall number if using int 0x80 convention
    ; For int 0x80: eax = syscall number, args in ebx, ecx, edx...
    ; We'll use: rax = number, rdi, rsi, rdx, r10, r8, r9 = args (Linux style-ish)

    mov rdi, rax          ; syscall number
    mov rsi, rdi          ; arg1 - wait, need to fix
    ; Actually after pushes, restore properly

    ; Call C handler
    ; syscall_handler(uint64_t num, uint64_t arg1, ...)
    ; For simplicity: num in rax (saved), we pass from stack

    mov rdi, [rsp + 14*8] ; rax (syscall number)
    mov rsi, [rsp + 9*8]  ; rdi (arg1)
    mov rdx, [rsp + 8*8]  ; rsi (arg2)
    mov rcx, [rsp + 11*8] ; rdx (arg3)

    call syscall_handler

    ; Return value in rax - put it back
    mov [rsp + 14*8], rax

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
