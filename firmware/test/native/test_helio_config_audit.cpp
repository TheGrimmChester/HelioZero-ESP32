#include <gtest/gtest.h>

#include "helio_config_audit_keys.h"

TEST(HelioConfigAudit, RedactsSecrets) {
  EXPECT_TRUE(helio_config_audit_is_secret_key("mqtt_password"));
  EXPECT_TRUE(helio_config_audit_is_secret_key("http_api_password"));
  EXPECT_FALSE(helio_config_audit_is_secret_key("source"));
}
