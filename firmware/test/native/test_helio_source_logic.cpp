#include <gtest/gtest.h>

#include "helio_source_logic.h"

TEST(HelioSourceLogic, RegistryCount) { EXPECT_EQ(helio_source_logic_registry_count(), 12u); }

TEST(HelioSourceLogic, ParseKnownWires) {
  EXPECT_EQ(helio_source_logic_parse_wire("UxIx2"), SourceId::UxIx2);
  EXPECT_EQ(helio_source_logic_parse_wire("Linky"), SourceId::Linky);
  EXPECT_EQ(helio_source_logic_parse_wire("Ext"), SourceId::Ext);
  EXPECT_EQ(helio_source_logic_parse_wire("bad"), SourceId::Unknown);
}

TEST(HelioSourceLogic, EffectiveIdForExt) {
  EXPECT_EQ(helio_source_logic_effective_id(SourceId::Ext, "Linky"), SourceId::Linky);
  EXPECT_EQ(helio_source_logic_effective_id(SourceId::UxI, "ignored"), SourceId::UxI);
}

TEST(HelioSourceLogic, CapabilityMatrix) {
  EXPECT_TRUE(helio_source_logic_cap_mqtt_triac_channel_block_for(SourceId::UxIx2));
  EXPECT_FALSE(helio_source_logic_cap_mqtt_triac_channel_block_for(SourceId::Linky));
  EXPECT_TRUE(helio_source_logic_cap_mqtt_linky_tariff_for(SourceId::Linky, false));
  EXPECT_TRUE(helio_source_logic_cap_mqtt_linky_tariff_for(SourceId::UxI, true));
  EXPECT_FALSE(helio_source_logic_cap_mqtt_linky_tariff_for(SourceId::UxI, false));
  EXPECT_TRUE(helio_source_logic_cap_serial_adc_gpio_restrict_for(SourceId::UxI));
}

TEST(HelioSourceLogic, BasePollPeriodUxIx3) {
  EXPECT_EQ(helio_source_logic_base_poll_period_ms(SourceId::UxIx2, 9600), 400);
  EXPECT_EQ(helio_source_logic_base_poll_period_ms(SourceId::UxIx3, 9600), 800);
  EXPECT_EQ(helio_source_logic_base_poll_period_ms(SourceId::UxIx3, 19200), 500);
  EXPECT_EQ(helio_source_logic_base_poll_period_ms(SourceId::ShellyEm, 0), 300);
  EXPECT_EQ(helio_source_logic_base_poll_period_ms(SourceId::Enphase, 0), 600);
  EXPECT_EQ(helio_source_logic_base_poll_period_ms(SourceId::Ext, 0), 800);
  EXPECT_EQ(helio_source_logic_base_poll_period_ms(SourceId::Unknown, 0), 1000);
}

TEST(HelioSourceLogic, WireRegistryAndNullWire) {
  EXPECT_STREQ(helio_source_logic_wire_at(0), "UxIx2");
  EXPECT_STREQ(helio_source_logic_wire_at(99), "");
  EXPECT_EQ(helio_source_logic_parse_wire(nullptr), SourceId::Unknown);
  EXPECT_EQ(helio_source_logic_parse_wire("Pmqtt"), SourceId::Pmqtt);
  EXPECT_TRUE(helio_source_logic_cap_mqtt_triac_channel_block_for(SourceId::UxIx3));
  EXPECT_TRUE(helio_source_logic_cap_mqtt_triac_channel_block_for(SourceId::ShellyEm));
  EXPECT_TRUE(helio_source_logic_cap_serial_adc_gpio_restrict_for(SourceId::UxIx3));
  EXPECT_EQ(helio_source_logic_base_poll_period_ms(SourceId::UxI, 0), 40);
  EXPECT_EQ(helio_source_logic_base_poll_period_ms(SourceId::Linky, 0), 2);
  EXPECT_EQ(helio_source_logic_base_poll_period_ms(SourceId::SmartG, 0), 300);
  EXPECT_EQ(helio_source_logic_base_poll_period_ms(SourceId::NotDef, 0), 600);
  EXPECT_EQ(helio_source_logic_base_poll_period_ms(SourceId::Pmqtt, 0), 600);
  EXPECT_EQ(helio_source_logic_base_poll_period_ms(SourceId::ShellyPro, 0), 300);
  EXPECT_EQ(helio_source_logic_base_poll_period_ms(SourceId::HomeW, 0), 300);
  EXPECT_FALSE(helio_source_logic_cap_mqtt_linky_tariff_for(SourceId::Enphase, false));
  EXPECT_EQ(helio_source_logic_parse_wire("ShellyPro"), SourceId::ShellyPro);
  EXPECT_EQ(helio_source_logic_parse_wire("HomeW"), SourceId::HomeW);
}
