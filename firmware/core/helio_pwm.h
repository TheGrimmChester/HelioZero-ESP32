#pragma once

/* helio_pwm.h — LEDC PWM output; loads config from helio_globals after EEPROM. */

#include "helio_pwm_logic.h"

void helio_pwm_load_from_globals();
void helio_pwm_hw_reinit();
void helio_pwm_tick(int triac_open_percent);
