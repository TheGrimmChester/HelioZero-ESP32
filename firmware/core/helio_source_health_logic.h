#pragma once

#include <cstdint>

struct SourceHealthScoreInput {
  int last_poll_ms_ago = -1;
  uint32_t poll_period_ms = 500;
  bool last_poll_ok = false;
  uint8_t error_streak = 0;
};

struct SourceHealthScoreResult {
  int health_score = 0;
  int freshness_pts = 0;
  int poll_ok_pts = 0;
  int streak_pts = 0;
};

SourceHealthScoreResult helio_source_health_logic_compute(const SourceHealthScoreInput &in);

/** Matches MQTT `source_stale` when health_score is below 50. */
bool helio_source_health_logic_is_stale(int health_score);
