#include <gtest/gtest.h>

#include <fstream>
#include <sstream>

#include "helio_meter_json_logic.h"

namespace {
std::string read_file(const char *path) {
  std::ifstream in(path);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}
}  // namespace

TEST(HelioMeterJsonLogic, AcceptsValidHouse) {
  std::string err;
  EXPECT_TRUE(helio_meter_json_logic_validate_inject(read_file("firmware/test/fixtures/inject/valid_house.json"), &err));
}

TEST(HelioMeterJsonLogic, RejectsMissingMeterFields) {
  std::string err;
  EXPECT_FALSE(helio_meter_json_logic_validate_inject(read_file("firmware/test/fixtures/inject/invalid_missing_house.json"), &err));
  EXPECT_EQ(err, "no_meter_fields");
}

TEST(HelioMeterJsonLogic, RejectsNegativePower) {
  std::string err;
  EXPECT_FALSE(helio_meter_json_logic_validate_inject(read_file("firmware/test/fixtures/inject/invalid_negative_power.json"), &err));
  EXPECT_EQ(err, "negative_power");
}

TEST(HelioMeterJsonLogic, EmptyBodyAndRawMeter) {
  std::string err;
  EXPECT_FALSE(helio_meter_json_logic_validate_inject("", &err));
  EXPECT_EQ(err, "empty_body");
  EXPECT_TRUE(helio_meter_json_logic_validate_inject(R"({"raw_meter":{"v":1}})", nullptr));
}

TEST(HelioMeterJsonLogic, SecondChannelValid) {
  std::string err;
  EXPECT_TRUE(helio_meter_json_logic_validate_inject(
      R"({"second":{"active_import_w":10,"active_export_w":0}})", &err));
  std::string err2;
  EXPECT_FALSE(helio_meter_json_logic_validate_inject(
      R"({"second":{"active_import_w":-5}})", &err2));
  EXPECT_EQ(err2, "negative_power");
}

TEST(HelioMeterJsonLogic, HouseNegativeApparentAndBothBlocks) {
  std::string err;
  EXPECT_FALSE(helio_meter_json_logic_validate_inject(
      R"({"house":{"apparent_export_va":-1}})", &err));
  EXPECT_EQ(err, "negative_power");
  EXPECT_TRUE(helio_meter_json_logic_validate_inject(
      R"({"house":{"active_import_w":1},"second":{"active_import_w":2}})", nullptr));
  EXPECT_FALSE(helio_meter_json_logic_validate_inject(R"({"house":{}})", &err));
  EXPECT_EQ(err, "negative_power");
  EXPECT_FALSE(helio_meter_json_logic_validate_inject(R"({"house":{"label":"x"}})", &err));
  EXPECT_FALSE(helio_meter_json_logic_validate_inject(
      R"({"house":{"apparent_import_va":-2,"active_import_w":1}})", &err));
  EXPECT_TRUE(helio_meter_json_logic_validate_inject(
      R"({"house":{"active_import_w":0,"active_export_w":0}})", nullptr));
  EXPECT_FALSE(helio_meter_json_logic_validate_inject(
      R"({"second":{"active_export_w":-3}})", nullptr));
  EXPECT_TRUE(helio_meter_json_logic_validate_inject(R"({"house":{"active_import_w":1)", nullptr));
  EXPECT_TRUE(helio_meter_json_logic_validate_inject(R"({"house":"only"})", nullptr));
}

TEST(HelioMeterJsonLogic, NoMeterFieldsSetsError) {
  std::string err;
  EXPECT_FALSE(helio_meter_json_logic_validate_inject("{}", &err));
  EXPECT_EQ(err, "no_meter_fields");
  EXPECT_FALSE(helio_meter_json_logic_validate_inject(R"({"house":{"active_import_w":-5}})", &err));
  EXPECT_EQ(err, "negative_power");
}

TEST(HelioMeterJsonLogic, HouseSubstringInRawMeterValue) {
  EXPECT_TRUE(helio_meter_json_logic_validate_inject(
      R"({"raw_meter":"{\"house\":true}"})", nullptr));
}

TEST(HelioMeterJsonLogic, RejectsNonNumericPowerField) {
  std::string err;
  EXPECT_FALSE(helio_meter_json_logic_validate_inject(
      R"({"house":{"active_import_w":"not_a_number"}})", &err));
  EXPECT_EQ(err, "negative_power");
}

TEST(HelioMeterJsonLogic, UnclosedHouseObjectStillAccepted) {
  EXPECT_TRUE(helio_meter_json_logic_validate_inject(R"({"house":{"active_import_w":1})", nullptr));
}

TEST(HelioMeterJsonLogic, ErrorsWithoutErrOutPointer) {
  EXPECT_FALSE(helio_meter_json_logic_validate_inject("", nullptr));
  EXPECT_FALSE(helio_meter_json_logic_validate_inject("{}", nullptr));
  EXPECT_FALSE(helio_meter_json_logic_validate_inject(R"({"house":{"active_import_w":-1}})", nullptr));
  EXPECT_FALSE(helio_meter_json_logic_validate_inject(R"({"second":{"active_export_w":-2}})", nullptr));
}

TEST(HelioMeterJsonLogic, MalformedHouseBlockStructure) {
  std::string err;
  EXPECT_TRUE(helio_meter_json_logic_validate_inject(R"({"house":true})", nullptr));
  EXPECT_TRUE(helio_meter_json_logic_validate_inject(R"({"house":123})", nullptr));
  EXPECT_FALSE(helio_meter_json_logic_validate_inject(R"({"house":{"label":"x"}})", &err));
  EXPECT_EQ(err, "negative_power");
  EXPECT_FALSE(helio_meter_json_logic_validate_inject(R"({"second":{"active_import_w":}})", &err));
  EXPECT_EQ(err, "negative_power");
}

