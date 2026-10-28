#include "timer.h"
#include "console.h"
#include <stdint.h>

static uint64_t ticks = 0;

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void timer_init(uint32_t frequency) {
    if (frequency == 0) frequency = 100;

    uint32_t divisor = 1193182 / frequency;

    // PIT command: channel 0, lobyte/hibyte, rate generator
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));

    ticks = 0;

    console_set_color(0x55FF55);
    console_print("[OK] Timer (PIT) ready at ");
    // print frequency roughly
    console_print("100 Hz\n");
}

void timer_handler(void) {
    ticks++;
    // Future: call scheduler here for preemption
}

uint64_t timer_get_ticks(void) {
    return ticks;
}
