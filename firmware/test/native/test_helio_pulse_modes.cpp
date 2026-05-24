#include <gtest/gtest.h>

#include "helio_pulse_modes.h"
TEST(HelioPulseModes, SinusTableMonotonicBounds) {
  helio_pulse_modes_init_tables();
  EXPECT_EQ(helio_pulse_sinus_on(0), 0u);
  EXPECT_GE(helio_pulse_sinus_on(50), 1u);
  EXPECT_LE(helio_pulse_sinus_on(100), 100u);
  EXPECT_GE(helio_pulse_sinus_total(50), 2u);
  // init_tables() picks minimal (T,N) pair per open %; 100% → T=N=20 in host build.
  EXPECT_EQ(helio_pulse_sinus_total(100), 20u);
  EXPECT_EQ(helio_pulse_sinus_on(100), 20u);
}
