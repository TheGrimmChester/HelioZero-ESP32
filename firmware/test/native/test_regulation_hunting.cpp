#include <gtest/gtest.h>

#include "helio_regulation_hunting_logic.h"

TEST(RegulationHunting, CountsReversals) {
  const int s[] = {10, 30, 10, 40, 5, 50, 5};
  EXPECT_GE(helio_regulation_hunting_logic_count_reversals(s, 7), 3);
}

TEST(RegulationHunting, DetectsHunting) {
  RegulationHuntingState st;
  RegulationHuntingConfig cfg;
  cfg.reversal_threshold = 3;
  unsigned long t = 0;
  for (int i = 0; i < 8; i++) {
    const int open = (i % 2) ? 80 : 20;
    helio_regulation_hunting_logic_sample(st, cfg, open, t);
    t += 31000UL;
  }
  EXPECT_TRUE(helio_regulation_hunting_logic_is_hunting(st));
}

TEST(RegulationHunting, CountReversalsShortAndZeroDelta) {
  const int one[] = {10};
  EXPECT_EQ(helio_regulation_hunting_logic_count_reversals(one, 1), 0);
  const int flat[] = {10, 10, 10};
  EXPECT_EQ(helio_regulation_hunting_logic_count_reversals(flat, 3), 0);
}

TEST(RegulationHunting, ClearsAfterStableWindow) {
  RegulationHuntingState st;
  st.hunting = true;
  RegulationHuntingConfig cfg;
  cfg.reversal_threshold = 99;
  cfg.stable_clear_min = 1;
  helio_regulation_hunting_logic_sample(st, cfg, 50, 60000UL);
  helio_regulation_hunting_logic_sample(st, cfg, 50, 120000UL);
  EXPECT_FALSE(helio_regulation_hunting_logic_is_hunting(st));
}

TEST(RegulationHunting, SkipsSampleInsideInterval) {
  RegulationHuntingState st;
  st.last_sample_ms = 1000;
  RegulationHuntingConfig cfg;
  helio_regulation_hunting_logic_sample(st, cfg, 50, 5000);
  EXPECT_EQ(st.last_sample_ms, 1000UL);
}

TEST(RegulationHunting, ClampsPercentAndWrapsRing) {
  RegulationHuntingState st;
  RegulationHuntingConfig cfg;
  cfg.reversal_threshold = 2;
  unsigned long t = 0;
  for (int i = 0; i < RegulationHuntingState::kMaxSamples + 3; ++i) {
    helio_regulation_hunting_logic_sample(st, cfg, (i % 2) ? 120 : 10, t);
    t += 31000UL;
  }
  helio_regulation_hunting_logic_sample(st, cfg, -10, t);
  helio_regulation_hunting_logic_sample(st, cfg, 200, t + 31000UL);
  EXPECT_TRUE(helio_regulation_hunting_logic_is_hunting(st));
}

TEST(RegulationHunting, ClearsViaStableSinceAlreadyStarted) {
  RegulationHuntingState st;
  st.hunting = true;
  st.stable_since_ms = 1000UL;
  RegulationHuntingConfig cfg;
  cfg.reversal_threshold = 99;
  cfg.stable_clear_min = 1;
  helio_regulation_hunting_logic_sample(st, cfg, 40, 70000UL);
  EXPECT_FALSE(helio_regulation_hunting_logic_is_hunting(st));
}
