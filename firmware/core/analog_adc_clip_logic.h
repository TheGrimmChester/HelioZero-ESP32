#pragma once

#include <cstdint>

struct AnalogAdcClipState {
  bool clipping = false;
  uint8_t clear_streak = 0;
};

/** Update clip state from peak raw ADC magnitudes (12-bit, 0..4095). */
void analog_adc_clip_logic_update(AnalogAdcClipState &st, int peak_volt_raw, int peak_amp_raw);

bool analog_adc_clip_logic_is_clipping(const AnalogAdcClipState &st);
