#include "analog_adc_clip_logic.h"

#include <algorithm>

namespace {
constexpr int kClipThreshold = 4090;
constexpr int kClearThreshold = 4080;
constexpr uint8_t kClearPollsRequired = 2;
}  // namespace

void analog_adc_clip_logic_update(AnalogAdcClipState &st, int peak_volt_raw, int peak_amp_raw) {
  const int pv = std::max(0, peak_volt_raw);
  const int pa = std::max(0, peak_amp_raw);
  if (pv >= kClipThreshold || pa >= kClipThreshold) {
    st.clipping = true;
    st.clear_streak = 0;
    return;
  }
  if (st.clipping && pv < kClearThreshold && pa < kClearThreshold) {
    st.clear_streak++;
    if (st.clear_streak >= kClearPollsRequired) {
      st.clipping = false;
      st.clear_streak = 0;
    }
  } else if (!st.clipping) {
    st.clear_streak = 0;
  }
}

bool analog_adc_clip_logic_is_clipping(const AnalogAdcClipState &st) { return st.clipping; }
