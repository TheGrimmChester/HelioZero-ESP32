#include <gtest/gtest.h>

#include "helio_device_id_logic.h"

TEST(HelioDeviceIdLogic, FormatFullMac) {
  char buf[13];
  ASSERT_TRUE(helio_device_uid_format(0xA1B2C3D4E5F6ULL, buf, sizeof(buf)));
  EXPECT_STREQ(buf, "a1b2c3d4e5f6");
}

TEST(HelioDeviceIdLogic, LeadingZeroPadding) {
  char buf[13];
  ASSERT_TRUE(helio_device_uid_format(0x000012345678ULL, buf, sizeof(buf)));
  EXPECT_STREQ(buf, "000012345678");
}

TEST(HelioDeviceIdLogic, MasksUpperBits) {
  char buf[13];
  ASSERT_TRUE(helio_device_uid_format(0xFFFFA1B2C3D4E5F6ULL, buf, sizeof(buf)));
  EXPECT_STREQ(buf, "a1b2c3d4e5f6");
}

TEST(HelioDeviceIdLogic, BufferTooSmall) {
  char buf[8];
  EXPECT_FALSE(helio_device_uid_format(0xA1B2C3D4E5F6ULL, buf, sizeof(buf)));
  EXPECT_FALSE(helio_device_uid_format(0xA1B2C3D4E5F6ULL, nullptr, 13));
}

TEST(HelioDeviceIdLogic, FactoryMqttDeviceName) {
  EXPECT_TRUE(helio_mqtt_device_name_is_factory_default(nullptr));
  EXPECT_TRUE(helio_mqtt_device_name_is_factory_default(""));
  EXPECT_TRUE(helio_mqtt_device_name_is_factory_default("helio_zero"));
  EXPECT_FALSE(helio_mqtt_device_name_is_factory_default("6809475d1df8"));
  EXPECT_FALSE(helio_mqtt_device_name_is_factory_default("my_router"));
}
