#include "helio_self_test.h"

#include "storage_eeprom.h"
#include "helio_globals.h"
#include "helio_mains_profile.h"
#include "helio_triac_isr.h"
#include "triac_api_shim.h"

SelfTestPersisted g_self_test;

namespace {
enum class Phase : uint8_t { Idle, ZcSample, TriacDry, SourceCheck, Done };
Phase g_phase = Phase::Idle;
unsigned long g_phase_start_ms = 0;
int g_zc_total = 0;
}  // namespace

void helio_self_test_set_pending(bool pending) { g_self_test.pending = pending; }

void helio_self_test_skip() {
  g_self_test.skipped = true;
  g_self_test.pending = false;
  g_phase = Phase::Idle;
}

void helio_self_test_start_run() {
  g_self_test.skipped = false;
  g_self_test.pending = false;
  g_phase = Phase::ZcSample;
  g_phase_start_ms = millis();
  g_zc_total = 0;
}

void helio_self_test_tick(unsigned long now_ms) {
  if (g_phase == Phase::Idle) return;

  if (g_phase == Phase::ZcSample) {
    int in_d = 0, raw = 0;
    TriacReadAndResetCounters(in_d, raw);
    g_zc_total += in_d;
    if ((now_ms - g_phase_start_ms) >= 1000UL) {
      g_self_test.zc_edges_per_sec = (uint16_t)g_zc_total;
      const uint32_t hz = helio_mains_effective_frequency_hz();
      const int expect = (hz >= 55) ? 110 : 90;
      const int tol = (hz >= 55) ? 25 : 20;
      g_self_test.zc_ok =
          g_self_test.zc_edges_per_sec >= (uint16_t)(expect - tol) &&
          g_self_test.zc_edges_per_sec <= (uint16_t)(expect + tol + 40);
      g_phase = Phase::TriacDry;
      g_phase_start_ms = now_ms;
    }
    return;
  }

  if (g_phase == Phase::TriacDry) {
    g_self_test.triac_ok = zc_sync_state > 0;
    g_phase = Phase::SourceCheck;
    g_phase_start_ms = now_ms;
    return;
  }

  if (g_phase == Phase::SourceCheck) {
    g_self_test.source_ok = (last_metering_task_ms > 0) && (time_sync_valid || meter_reading_valid);
    g_self_test.run_epoch = (uint32_t)(now_ms / 1000UL);
    g_phase = Phase::Done;
    persistConfigToEeprom();
    return;
  }
}

void helio_self_test_append_health_json(JsonObject obj) {
  obj["pending"] = g_self_test.pending;
  obj["skipped"] = g_self_test.skipped;
  obj["last_run_epoch"] = g_self_test.run_epoch;
  JsonObject results = obj.createNestedObject("results");
  results["zc_ok"] = g_self_test.zc_ok;
  results["triac_ok"] = g_self_test.triac_ok;
  results["source_ok"] = g_self_test.source_ok;
  results["zc_edges_per_sec"] = g_self_test.zc_edges_per_sec;
}
