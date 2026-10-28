#include "timer.h"
#include "console.h"
#include "process.h"
#include <stdint.h>

static uint64_t ticks = 0;
static uint32_t sched_counter = 0;

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void timer_init(uint32_t frequency) {
    if (frequency == 0) frequency = 100;
    uint32_t divisor = 1193182 / frequency;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
    ticks = 0;
    sched_counter = 0;

    console_set_color(0x55FF55);
    console_print("[OK] Timer 100Hz + preemptive hint\n");
}

void timer_handler(void) {
    ticks++;
    sched_counter++;
    // Cooperative preemption hint every 10 ticks (~100ms)
    if (sched_counter >= 10) {
        sched_counter = 0;
        // Don't switch from interrupt yet (needs careful IRQ-safe switch)
        // process_yield() can be called from shell/syscalls safely
    }
}

uint64_t timer_get_ticks(void) {
    return ticks;
}

void timer_sleep(uint64_t ms) {
    // Approximate: 100 Hz => 10ms per tick
    uint64_t target = ticks + (ms / 10) + 1;
    while (ticks < target) {
        asm volatile ("hlt");
    }
}
