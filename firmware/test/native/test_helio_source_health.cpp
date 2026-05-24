#include <gtest/gtest.h>

#include "helio_source_health_logic.h"

TEST(HelioSourceHealth, FullScoreWhenFreshAndOk) {
  SourceHealthScoreInput in;
  in.last_poll_ms_ago = 100;
  in.poll_period_ms = 400;
  in.last_poll_ok = true;
  in.error_streak = 0;
  const auto r = helio_source_health_logic_compute(in);
  EXPECT_EQ(r.health_score, 100);
}

TEST(HelioSourceHealth, DegradesWhenStale) {
  SourceHealthScoreInput in;
  in.last_poll_ms_ago = 5000;
  in.poll_period_ms = 400;
  in.last_poll_ok = false;
  in.error_streak = 3;
  const auto r = helio_source_health_logic_compute(in);
  EXPECT_LT(r.health_score, 50);
}

TEST(HelioSourceHealth, MidFreshnessAndSingleError) {
  SourceHealthScoreInput in;
  in.last_poll_ms_ago = -1;
  in.poll_period_ms = 400;
  in.last_poll_ok = false;
  in.error_streak = 1;
  const auto missing = helio_source_health_logic_compute(in);
  EXPECT_EQ(missing.freshness_pts, 0);
  EXPECT_EQ(missing.streak_pts, 10);

  in.last_poll_ms_ago = 800;
  in.last_poll_ok = true;
  in.error_streak = 0;
  const auto mid = helio_source_health_logic_compute(in);
  EXPECT_GT(mid.freshness_pts, 0);
  EXPECT_LT(mid.freshness_pts, 40);
  EXPECT_FALSE(helio_source_health_logic_is_stale(mid.health_score));
}

TEST(HelioSourceHealth, DefaultPollPeriodWhenZero) {
  SourceHealthScoreInput in;
  in.last_poll_ms_ago = 100;
  in.poll_period_ms = 0;
  in.last_poll_ok = true;
  in.error_streak = 0;
  const auto r = helio_source_health_logic_compute(in);
  EXPECT_EQ(r.freshness_pts, 40);
  EXPECT_EQ(r.health_score, 100);
}

TEST(HelioSourceHealth, StaleThresholdAndHighErrorStreak) {
  SourceHealthScoreInput in;
  in.last_poll_ms_ago = 2000;
  in.poll_period_ms = 400;
  in.last_poll_ok = false;
  in.error_streak = 5;
  const auto r = helio_source_health_logic_compute(in);
  EXPECT_TRUE(helio_source_health_logic_is_stale(r.health_score));
  EXPECT_EQ(r.streak_pts, 0);

  in.error_streak = 2;
  const auto r2 = helio_source_health_logic_compute(in);
  EXPECT_EQ(r2.streak_pts, 0);

  in.error_streak = 3;
  const auto r3 = helio_source_health_logic_compute(in);
  EXPECT_EQ(r3.streak_pts, 0);
  in.last_poll_ms_ago = 500;
  in.last_poll_ok = true;
  const auto aged = helio_source_health_logic_compute(in);
  EXPECT_GE(aged.freshness_pts, 0);
  EXPECT_LT(aged.freshness_pts, 40);
}
