#pragma once

/*
 * Multi-sinus / train / demi-sinus tables and 10 ms half-wave stepping.
 */

#include <Arduino.h>
#include "helio_board.h"
#include "helio_regulation_modes.h"

#include <cstdint>

/** Build tabPulseSinusOn/Total (call once at boot). */
void helio_pulse_modes_init_tables(void);

/** After regulation: set PulseOn/PulseTotal for multi/train/demi modes. */
void helio_pulse_modes_apply_regulation_output(int action_index, uint8_t regulation_mode, int triac_delay_percent_percent);

/** 10 ms / ZC-sync tick — GPIO pulse patterns (ISR-safe). */
#if defined(FLEET_BUNDLE_NATIVE_STUB)
void helio_pulse_modes_tick_10ms(void);
#else
void IRAM_ATTR helio_pulse_modes_tick_10ms(void);
#endif

uint8_t helio_pulse_sinus_on(uint8_t open_percent);
uint8_t helio_pulse_sinus_total(uint8_t open_percent);
