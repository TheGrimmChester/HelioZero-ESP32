#pragma once

/*
 * helio_triac_isr.h — Volatile triac timing state shared between ISRs and main loop.
 */

#include <Arduino.h>



/** Last accepted zero-cross time (`esp_timer_get_time()`, µs) for ISR debounce. */

extern volatile uint32_t last_zc_us;

extern volatile int phase_delay_ticks;

extern volatile int triac_delay_percent;

/** zc_sync_state: >0 ZC-synced, <0 internal 10 ms clock. */

extern volatile int16_t zc_sync_state;

extern volatile int IT_half_period;

extern volatile int IT_half_period_in;

extern hw_timer_t *timer;

extern hw_timer_t *timer10ms;



void helio_zc_isr();

void helio_phase_tick_isr();

void helio_half_cycle_tick_isr();



/** Zero-cross + 100 µs + 10 ms timers. */

void helio_triac_hw_init(void);

/** Action 0 regulation mode for ISR (MODE_DECOUPE_ONOFF). */

void helio_triac_set_action0_mode(uint8_t mode);
