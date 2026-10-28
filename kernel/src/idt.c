#include "idt.h"
#include "console.h"
#include "timer.h"
#include "keyboard.h"

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr idtp;

extern void isr_stub_0(void); extern void isr_stub_1(void);
extern void isr_stub_2(void); extern void isr_stub_3(void);
extern void isr_stub_4(void); extern void isr_stub_5(void);
extern void isr_stub_6(void); extern void isr_stub_7(void);
extern void isr_stub_8(void); extern void isr_stub_9(void);
extern void isr_stub_10(void); extern void isr_stub_11(void);
extern void isr_stub_12(void); extern void isr_stub_13(void);
extern void isr_stub_14(void); extern void isr_stub_15(void);
extern void isr_stub_16(void); extern void isr_stub_17(void);
extern void isr_stub_18(void); extern void isr_stub_19(void);
extern void isr_stub_20(void); extern void isr_stub_21(void);
extern void isr_stub_22(void); extern void isr_stub_23(void);
extern void isr_stub_24(void); extern void isr_stub_25(void);
extern void isr_stub_26(void); extern void isr_stub_27(void);
extern void isr_stub_28(void); extern void isr_stub_29(void);
extern void isr_stub_30(void); extern void isr_stub_31(void);
extern void irq_stub_0(void);
extern void irq_stub_1(void);

static void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, uint8_t flags) {
    idt[num].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[num].offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[num].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[num].selector    = selector;
    idt[num].ist         = 0;
    idt[num].type_attr   = flags;
    idt[num].zero        = 0;
}

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void pic_remap(void) {
    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);
    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);
    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint64_t)&idt;

    for (int i = 0; i < 256; i++) idt_set_gate(i, 0, 0, 0);

    idt_set_gate(0,  (uint64_t)isr_stub_0,  0x08, 0x8E);
    idt_set_gate(1,  (uint64_t)isr_stub_1,  0x08, 0x8E);
    idt_set_gate(2,  (uint64_t)isr_stub_2,  0x08, 0x8E);
    idt_set_gate(3,  (uint64_t)isr_stub_3,  0x08, 0x8E);
    idt_set_gate(4,  (uint64_t)isr_stub_4,  0x08, 0x8E);
    idt_set_gate(5,  (uint64_t)isr_stub_5,  0x08, 0x8E);
    idt_set_gate(6,  (uint64_t)isr_stub_6,  0x08, 0x8E);
    idt_set_gate(7,  (uint64_t)isr_stub_7,  0x08, 0x8E);
    idt_set_gate(8,  (uint64_t)isr_stub_8,  0x08, 0x8E);
    idt_set_gate(9,  (uint64_t)isr_stub_9,  0x08, 0x8E);
    idt_set_gate(10, (uint64_t)isr_stub_10, 0x08, 0x8E);
    idt_set_gate(11, (uint64_t)isr_stub_11, 0x08, 0x8E);
    idt_set_gate(12, (uint64_t)isr_stub_12, 0x08, 0x8E);
    idt_set_gate(13, (uint64_t)isr_stub_13, 0x08, 0x8E);
    idt_set_gate(14, (uint64_t)isr_stub_14, 0x08, 0x8E);
    idt_set_gate(15, (uint64_t)isr_stub_15, 0x08, 0x8E);
    idt_set_gate(16, (uint64_t)isr_stub_16, 0x08, 0x8E);
    idt_set_gate(17, (uint64_t)isr_stub_17, 0x08, 0x8E);
    idt_set_gate(18, (uint64_t)isr_stub_18, 0x08, 0x8E);
    idt_set_gate(19, (uint64_t)isr_stub_19, 0x08, 0x8E);
    idt_set_gate(20, (uint64_t)isr_stub_20, 0x08, 0x8E);
    idt_set_gate(21, (uint64_t)isr_stub_21, 0x08, 0x8E);
    idt_set_gate(22, (uint64_t)isr_stub_22, 0x08, 0x8E);
    idt_set_gate(23, (uint64_t)isr_stub_23, 0x08, 0x8E);
    idt_set_gate(24, (uint64_t)isr_stub_24, 0x08, 0x8E);
    idt_set_gate(25, (uint64_t)isr_stub_25, 0x08, 0x8E);
    idt_set_gate(26, (uint64_t)isr_stub_26, 0x08, 0x8E);
    idt_set_gate(27, (uint64_t)isr_stub_27, 0x08, 0x8E);
    idt_set_gate(28, (uint64_t)isr_stub_28, 0x08, 0x8E);
    idt_set_gate(29, (uint64_t)isr_stub_29, 0x08, 0x8E);
    idt_set_gate(30, (uint64_t)isr_stub_30, 0x08, 0x8E);
    idt_set_gate(31, (uint64_t)isr_stub_31, 0x08, 0x8E);

    idt_set_gate(32, (uint64_t)irq_stub_0, 0x08, 0x8E); // Timer
    idt_set_gate(33, (uint64_t)irq_stub_1, 0x08, 0x8E); // Keyboard

    pic_remap();

    // Unmask timer (IRQ0) and keyboard (IRQ1)
    outb(PIC1_DATA, 0xFC); // 11111100
    outb(PIC2_DATA, 0xFF);

    asm volatile ("lidt %0" : : "m"(idtp));
}

void exception_handler(uint64_t int_no, uint64_t err_code) {
    console_set_color(0xFF5555);
    console_print("\n[EXCEPTION] Int ");
    console_print_dec(int_no);
    console_print(" err ");
    console_print_hex(err_code);
    console_print("\n");
    for (;;) asm volatile ("hlt");
}

void pic_eoi(uint8_t irq) {
    if (irq >= 8) outb(PIC2_COMMAND, 0x20);
    outb(PIC1_COMMAND, 0x20);
}

// Called from irq_common for IRQ0
void irq0_handler(void) {
    timer_handler();
}
