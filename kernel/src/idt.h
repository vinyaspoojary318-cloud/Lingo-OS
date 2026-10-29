#pragma once

#include <stdint.h>

void idt_init(void);
void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, uint8_t flags);

static inline void enable_interrupts(void) { asm volatile ("sti"); }
static inline void disable_interrupts(void) { asm volatile ("cli"); }
