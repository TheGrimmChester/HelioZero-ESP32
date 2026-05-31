#include <gtest/gtest.h>

#include "helio_source_logic.h"

TEST(HelioSourceLogic, RegistryCount) { EXPECT_EQ(helio_source_logic_registry_count(), 12u); }

TEST(HelioSourceLogic, ParseKnownWires) {
  EXPECT_EQ(helio_source_logic_parse_wire("JsyMk194"), SourceId::JsyMk194);
  EXPECT_EQ(helio_source_logic_parse_wire("Linky"), SourceId::Linky);
  EXPECT_EQ(helio_source_logic_parse_wire("HelioPeer"), SourceId::HelioPeer);
  EXPECT_EQ(helio_source_logic_parse_wire("bad"), SourceId::Unknown);
}

TEST(HelioSourceLogic, EffectiveIdForHelioPeer) {
  EXPECT_EQ(helio_source_logic_effective_id(SourceId::HelioPeer, "Linky"), SourceId::Linky);
  EXPECT_EQ(helio_source_logic_effective_id(SourceId::Analog, "ignored"), SourceId::Analog);
}

TEST(HelioSourceLogic, SecondChannelSnapshotVisible) {
  EXPECT_FALSE(helio_source_logic_second_channel_snapshot_visible(0.0f, 0, 0, 0.0f));
  EXPECT_TRUE(helio_source_logic_second_channel_snapshot_visible(235.0f, 0, 0, 0.0f));
  EXPECT_TRUE(helio_source_logic_second_channel_snapshot_visible(0.0f, 879, 0, 0.0f));
  EXPECT_FALSE(helio_source_logic_cap_mqtt_triac_channel_block_for(SourceId::Pmqtt));
  EXPECT_TRUE(helio_source_logic_second_channel_snapshot_visible(0.0f, 879, 0, 0.0f));
}

TEST(HelioSourceLogic, CapabilityMatrix) {
  EXPECT_TRUE(helio_source_logic_cap_mqtt_triac_channel_block_for(SourceId::JsyMk194));
  EXPECT_FALSE(helio_source_logic_cap_mqtt_triac_channel_block_for(SourceId::Linky));
  EXPECT_TRUE(helio_source_logic_cap_mqtt_linky_tariff_for(SourceId::Linky, false));
  EXPECT_TRUE(helio_source_logic_cap_mqtt_linky_tariff_for(SourceId::Analog, true));
  EXPECT_FALSE(helio_source_logic_cap_mqtt_linky_tariff_for(SourceId::Analog, false));
  EXPECT_TRUE(helio_source_logic_cap_serial_adc_gpio_restrict_for(SourceId::Analog));
}

TEST(HelioSourceLogic, BasePollPeriodJsyMk333) {
  EXPECT_EQ(helio_source_logic_base_poll_period_ms(SourceId::JsyMk194, 9600), 400);
  EXPECT_EQ(helio_source_logic_base_poll_period_ms(SourceId::JsyMk333, 9600), 800);
  EXPECT_EQ(helio_source_logic_base_poll_period_ms(SourceId::JsyMk333, 19200), 500);
  EXPECT_EQ(helio_source_logic_base_poll_period_ms(SourceId::ShellyEm, 0), 300);
  EXPECT_EQ(helio_source_logic_base_poll_period_ms(SourceId::Enphase, 0), 600);
  EXPECT_EQ(helio_source_logic_base_poll_period_ms(SourceId::HelioPeer, 0), 800);
  EXPECT_EQ(helio_source_logic_base_poll_period_ms(SourceId::Unknown, 0), 1000);
}

TEST(HelioSourceLogic, WireRegistryAndNullWire) {
  EXPECT_STREQ(helio_source_logic_wire_at(0), "JsyMk194");
  EXPECT_STREQ(helio_source_logic_wire_at(99), "");
  EXPECT_EQ(helio_source_logic_parse_wire(nullptr), SourceId::Unknown);
  EXPECT_EQ(helio_source_logic_parse_wire("Pmqtt"), SourceId::Pmqtt);
  EXPECT_TRUE(helio_source_logic_cap_mqtt_triac_channel_block_for(SourceId::JsyMk333));
  EXPECT_TRUE(helio_source_logic_cap_mqtt_triac_channel_block_for(SourceId::ShellyEm));
  EXPECT_TRUE(helio_source_logic_cap_serial_adc_gpio_restrict_for(SourceId::JsyMk333));
  EXPECT_EQ(helio_source_logic_base_poll_period_ms(SourceId::Analog, 0), 40);
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
