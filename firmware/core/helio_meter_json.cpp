#include "helio_meter_json.h"
#include "helio_globals.h"
#include "helio_meter_logic.h"
#include "helio_pub.h"
#include "helio_runtime.h"
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <string>

static void takeInt(JsonObject o, const char *key, int &dst, bool &any) {
  if (!o.containsKey(key)) return;
  dst = (int)o[key].as<long>();
  any = true;
}

static void takeLong(JsonObject o, const char *key, long &dst, bool &any) {
  if (!o.containsKey(key)) return;
  dst = o[key].as<long>();
  any = true;
}

static void takeFloat(JsonObject o, const char *key, float &dst, bool &any) {
  if (!o.containsKey(key)) return;
  dst = o[key].as<float>();
  any = true;
}

static void applyHouseBlockToFields(JsonObject ch, bool second, MeterSnapshotFields &fields, bool &any) {
  if (!ch) return;
  MeterChannelState &dst = second ? fields.second : fields.house;
  takeInt(ch, "active_import_w", dst.active_import_w, any);
  takeInt(ch, "active_export_w", dst.active_export_w, any);
  takeInt(ch, "apparent_import_va", dst.apparent_import_va, any);
  takeInt(ch, "apparent_export_va", dst.apparent_export_va, any);
  takeFloat(ch, "energy_day_import_wh", dst.energy_day_import_wh, any);
  takeFloat(ch, "energy_day_export_wh", dst.energy_day_export_wh, any);
  takeFloat(ch, "energy_total_import_wh", dst.energy_total_import_wh, any);
  takeFloat(ch, "energy_total_export_wh", dst.energy_total_export_wh, any);
  if (second) {
    fields.has_second = true;
  } else {
    fields.has_house = true;
  }
}

bool ApplyMeterSnapshotFromJson(JsonObject root, String *errOut) {
  MeterSnapshotFields fields;
  bool any = false;
  if (root.containsKey("house") && root["house"].is<JsonObject>()) {
    applyHouseBlockToFields(root["house"].as<JsonObject>(), false, fields, any);
  }
  if (root.containsKey("second") && root["second"].is<JsonObject>()) {
    applyHouseBlockToFields(root["second"].as<JsonObject>(), true, fields, any);
  }
  if (root.containsKey("raw_meter") && root["raw_meter"].is<JsonObject>()) {
    JsonObject rw = root["raw_meter"].as<JsonObject>();
    takeFloat(rw, "voltage_house_v", fields.raw.voltage_house_v, any);
    takeFloat(rw, "current_house_a", fields.raw.current_house_a, any);
    takeFloat(rw, "pf_house", fields.raw.pf_house, any);
    takeFloat(rw, "voltage_second_v", fields.raw.voltage_second_v, any);
    takeFloat(rw, "current_second_a", fields.raw.current_second_a, any);
    takeFloat(rw, "pf_second", fields.raw.pf_second, any);
    takeFloat(rw, "freq_hz", fields.raw.freq_hz, any);
    fields.has_raw = any;
  }
  if (!any) {
    if (errOut) *errOut = "no_meter_fields";
    return false;
  }
  RmsRuntime &rt = helio_runtime();
  rt.sync_from_globals();
  std::string errStd;
  if (!helio_meter_logic_apply_fields(rt, fields, &errStd)) {
    if (errOut) *errOut = String(errStd.c_str());
    return false;
  }
  rt.sync_to_globals();
  HelioPublishFromGlobals();
  esp_task_wdt_reset();
  if (cptLEDyellow > 30) {
    cptLEDyellow = 4;
  }
  return true;
}
