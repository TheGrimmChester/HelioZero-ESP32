/*
 * helio_triac_isr.cpp — Triac ISRs: ZC + 100 µs phase-cut + 10 ms pulse modes.
 */
#include "helio_triac_isr.h"

#include "helio_board.h"
#include "helio_mains_profile.h"
#include "helio_pulse_modes.h"
#include "helio_regulation_modes.h"
#include "helio_triac_logic.h"

#include <esp_arduino_version.h>
#include <esp_timer.h>

#if defined(CONFIG_IDF_TARGET_ESP32)
#include "soc/gpio_struct.h"
#endif

volatile uint32_t last_zc_us = 0;
volatile int phase_delay_ticks = 0;
volatile int triac_delay_percent = 100;
volatile int16_t zc_sync_state = 0;
volatile int IT_half_period = 0;
volatile int IT_half_period_in = 0;
hw_timer_t *timer = nullptr;
hw_timer_t *timer10ms = nullptr;

static volatile uint8_t g_action0_mode = kModeDecoupeOnoff;

void helio_triac_set_action0_mode(uint8_t mode) { g_action0_mode = mode; }

#if defined(CONFIG_IDF_TARGET_ESP32)
static inline void IRAM_ATTR triac_gpio_set_level_isr(int level) {
  const uint32_t mask = (1UL << (unsigned)kTriacDimGpio);
  if (level) {
    GPIO.out_w1ts = mask;
  } else {
    GPIO.out_w1tc = mask;
  }
}
#else
static inline void IRAM_ATTR triac_gpio_set_level_isr(int level) { digitalWrite(kTriacDimGpio, level); }
#endif

static inline int IRAM_ATTR triac_delay_threshold_ticks_isr(void) {
  return triac_delay_threshold_ticks(triac_delay_percent, (int)g_triac_max_delay_ticks_isr);
}

void IRAM_ATTR helio_zc_isr() {
  IT_half_period += 1;
  const uint32_t now = (uint32_t)esp_timer_get_time();
  if ((uint32_t)(now - last_zc_us) > 2000u) {
    phase_delay_ticks = 0;
    last_zc_us = now;
    triac_gpio_set_level_isr(0);
    IT_half_period_in += 1;
    zc_sync_state += 3;
    if (zc_sync_state > 5) zc_sync_state = 5;
    if (zc_sync_state > 0) {
      helio_pulse_modes_tick_10ms();
    }
  }
}

void IRAM_ATTR helio_phase_tick_isr() {
  if (g_action0_mode != kModeDecoupeOnoff) {
    triac_gpio_set_level_isr(0);
    return;
  }
  phase_delay_ticks += 1;
  const int threshold = triac_delay_threshold_ticks_isr();
  const int maxTick = (int)g_triac_max_delay_ticks_isr;
  if (phase_delay_ticks > threshold && triac_delay_percent < 98 && zc_sync_state > 0 && threshold <= maxTick) {
    triac_gpio_set_level_isr(1);
  } else {
    triac_gpio_set_level_isr(0);
  }
}

void IRAM_ATTR helio_half_cycle_tick_isr() {
  zc_sync_state -= 1;
  if (zc_sync_state < -5) zc_sync_state = -5;
  if (zc_sync_state < 0) {
    helio_pulse_modes_tick_10ms();
  }
}

void helio_triac_hw_init(void) {
#ifndef METER_ONLY_BUILD
  attachInterrupt(kZeroCrossGpio, helio_zc_isr, RISING);

  auto end_timer = [](hw_timer_t *&t, void (*isr)()) {
    if (t == nullptr) return;
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    timerDetachInterrupt(t);
    timerStop(t);
    timerEnd(t);
#else
    timerAlarmDisable(t);
    timerDetachInterrupt(t);
    timerEnd(t);
#endif
    t = nullptr;
    (void)isr;
  };

  end_timer(timer, helio_phase_tick_isr);
  end_timer(timer10ms, helio_half_cycle_tick_isr);

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  timer = timerBegin(1000000u);
  if (timer != nullptr) {
    timerAttachInterrupt(timer, helio_phase_tick_isr);
    timerAlarm(timer, 100, true, 0);
    timerStart(timer);
  }
  timer10ms = timerBegin(1000000u);
  if (timer10ms != nullptr) {
    timerAttachInterrupt(timer10ms, helio_half_cycle_tick_isr);
    timerAlarm(timer10ms, 10000, true, 0);
    timerStart(timer10ms);
  }
#else
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &helio_phase_tick_isr, true);
  timerAlarmWrite(timer, 100, true);
  timerAlarmEnable(timer);

  timer10ms = timerBegin(1, 80, true);
  timerAttachInterrupt(timer10ms, &helio_half_cycle_tick_isr, true);
  timerAlarmWrite(timer10ms, 10000, true);
  timerAlarmEnable(timer10ms);
#endif
#else
  (void)timer;
  (void)timer10ms;
#endif
}
