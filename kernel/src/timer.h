#pragma once

#include <stdint.h>

// Initialize PIT timer (IRQ0) at given frequency (Hz)
void timer_init(uint32_t frequency);

// Called from IRQ0 handler
void timer_handler(void);

// Get tick count since boot
uint64_t timer_get_ticks(void);
