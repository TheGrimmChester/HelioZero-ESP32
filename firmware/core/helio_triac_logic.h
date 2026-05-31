#pragma once

/* helio_triac_logic.h — Triac triac_delay_percent threshold math (IRAM on ESP32; native unit tests). */

#include <cstdint>

#if defined(ESP_PLATFORM)
#include <esp_attr.h>
#define TRIAC_LOGIC_IRAM IRAM_ATTR
#else
#define TRIAC_LOGIC_IRAM
#endif

/** Triac gate delay threshold in 100 µs ticks (ISR semantics). */
int TRIAC_LOGIC_IRAM triac_delay_threshold_ticks(int triac_delay_percent, int max_delay_ticks);

/** Compute max triac_delay_percent ticks from half-period microseconds (matches helio_mains_profile). */
uint8_t triac_max_delay_ticks_from_half_period_us(uint32_t half_period_us);
