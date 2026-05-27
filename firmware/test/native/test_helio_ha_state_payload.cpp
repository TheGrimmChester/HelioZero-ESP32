#include <gtest/gtest.h>

#include "helio_source_health_logic.h"

TEST(HelioHaStatePayload, stale_threshold_matches_mqtt) {
  EXPECT_FALSE(helio_source_health_logic_is_stale(50));
  EXPECT_TRUE(helio_source_health_logic_is_stale(49));
}
