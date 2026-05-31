#include <gtest/gtest.h>

#include "analog_adc_clip_logic.h"

TEST(AnalogAdcClipLogic, SetsAndClears) {
  AnalogAdcClipState st;
  analog_adc_clip_logic_update(st, 100, 100);
  EXPECT_FALSE(analog_adc_clip_logic_is_clipping(st));
  analog_adc_clip_logic_update(st, 4095, 0);
  EXPECT_TRUE(analog_adc_clip_logic_is_clipping(st));
  analog_adc_clip_logic_update(st, 100, 100);
  EXPECT_TRUE(analog_adc_clip_logic_is_clipping(st));
  analog_adc_clip_logic_update(st, 100, 100);
  EXPECT_FALSE(analog_adc_clip_logic_is_clipping(st));
}

TEST(AnalogAdcClipLogic, VoltOnlyClip) {
  AnalogAdcClipState st;
  analog_adc_clip_logic_update(st, 4095, 100);
  EXPECT_TRUE(analog_adc_clip_logic_is_clipping(st));
}

TEST(AnalogAdcClipLogic, AmpOnlyClip) {
  AnalogAdcClipState st;
  analog_adc_clip_logic_update(st, 100, 4095);
  EXPECT_TRUE(analog_adc_clip_logic_is_clipping(st));
}

TEST(AnalogAdcClipLogic, SingleClearPollStaysClipped) {
  AnalogAdcClipState st;
  analog_adc_clip_logic_update(st, 4095, 4095);
  ASSERT_TRUE(analog_adc_clip_logic_is_clipping(st));
  analog_adc_clip_logic_update(st, 100, 100);
  EXPECT_TRUE(analog_adc_clip_logic_is_clipping(st));
}

TEST(AnalogAdcClipLogic, NegativeRawClampedToZero) {
  AnalogAdcClipState st;
  analog_adc_clip_logic_update(st, -50, -100);
  EXPECT_FALSE(analog_adc_clip_logic_is_clipping(st));
}

TEST(AnalogAdcClipLogic, ClippingMidBandHoldsUntilClear) {
  AnalogAdcClipState st;
  analog_adc_clip_logic_update(st, 4095, 100);
  ASSERT_TRUE(analog_adc_clip_logic_is_clipping(st));
  analog_adc_clip_logic_update(st, 4085, 4085);
  EXPECT_TRUE(analog_adc_clip_logic_is_clipping(st));
  analog_adc_clip_logic_update(st, 100, 100);
  EXPECT_TRUE(analog_adc_clip_logic_is_clipping(st));
  analog_adc_clip_logic_update(st, 100, 100);
  EXPECT_FALSE(analog_adc_clip_logic_is_clipping(st));
}

TEST(AnalogAdcClipLogic, ClipThresholdExactly4090) {
  AnalogAdcClipState st;
  analog_adc_clip_logic_update(st, 4090, 4090);
  EXPECT_TRUE(analog_adc_clip_logic_is_clipping(st));
}

TEST(AnalogAdcClipLogic, ClippingHighVoltOnlyBlocksClear) {
  AnalogAdcClipState st;
  analog_adc_clip_logic_update(st, 4095, 100);
  ASSERT_TRUE(analog_adc_clip_logic_is_clipping(st));
  analog_adc_clip_logic_update(st, 4085, 100);
  EXPECT_TRUE(analog_adc_clip_logic_is_clipping(st));
}

TEST(AnalogAdcClipLogic, ClippingHighAmpOnlyBlocksClear) {
  AnalogAdcClipState st;
  analog_adc_clip_logic_update(st, 100, 4095);
  ASSERT_TRUE(analog_adc_clip_logic_is_clipping(st));
  analog_adc_clip_logic_update(st, 100, 4085);
  EXPECT_TRUE(analog_adc_clip_logic_is_clipping(st));
}

TEST(AnalogAdcClipLogic, IdleMidBandDoesNotClip) {
  AnalogAdcClipState st;
  analog_adc_clip_logic_update(st, 4085, 4085);
  EXPECT_FALSE(analog_adc_clip_logic_is_clipping(st));
}

TEST(AnalogAdcClipLogic, NotClippingResetsClearStreakOnReclip) {
  AnalogAdcClipState st;
  analog_adc_clip_logic_update(st, 4095, 100);
  ASSERT_TRUE(analog_adc_clip_logic_is_clipping(st));
  analog_adc_clip_logic_update(st, 100, 100);
  ASSERT_TRUE(analog_adc_clip_logic_is_clipping(st));
  analog_adc_clip_logic_update(st, 4095, 100);
  EXPECT_TRUE(analog_adc_clip_logic_is_clipping(st));
  analog_adc_clip_logic_update(st, 100, 100);
  EXPECT_TRUE(analog_adc_clip_logic_is_clipping(st));
  analog_adc_clip_logic_update(st, 100, 100);
  EXPECT_FALSE(analog_adc_clip_logic_is_clipping(st));
  analog_adc_clip_logic_update(st, 200, 300);
  EXPECT_FALSE(analog_adc_clip_logic_is_clipping(st));
}
