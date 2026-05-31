/*
 * Auto-split from api_v1_routes.cpp — see api_v1_common.h
 */
#include "api_v1_common.h"
#include "helio_config_audit.h"
void handle_get_config() {
  API_AUTH_GUARD();
  DynamicJsonDocument doc(16384);
  doc["schema_version"] = 4;
  JsonObject cfg = doc.createNestedObject("config");
  api_append_config_object(cfg);
  String out;
  serializeJson(doc, out);
  api_send_json(server,200, out);
}

void handle_put_config() {
  API_AUTH_GUARD();
  String body;
  if (!api_require_json_body(server, body, kPutBodyMax, false)) return;
  DynamicJsonDocument doc(kPutBodyMax);
  DeserializationError e = deserializeJson(doc, body);
  if (e) {
    api_error(server,400, "json_parse", e.c_str());
    return;
  }
  JsonObject root = doc.as<JsonObject>();
  if (!root.containsKey("config")) {
    api_error(server,400, "bad_request", "PUT expects { \"config\": { ... } }");
    return;
  }
  String err;
  if (!config_apply_from_json(root["config"].as<JsonObject>(), true, err)) {
    api_error(server,400, "validation", err.c_str());
    return;
  }
  helio_config_audit_record("/api/v1/config", root["config"].as<JsonObject>());
  int addr = persistConfigToEeprom();
  StaticJsonDocument<128> out;
  out["ok"] = true;
  out["eeprom_bytes"] = addr;
  String s;
  serializeJson(out, s);
  api_send_json(server,200, s);
}

void handle_patch_config() {
  API_AUTH_GUARD();
  String body;
  if (!api_require_json_body(server, body, kPatchBodyMax, true)) return;
  DynamicJsonDocument doc(kPatchBodyMax);
  DeserializationError e = deserializeJson(doc, body);
  if (e) {
    api_error(server,400, "json_parse", e.c_str());
    return;
  }
  const char *allowed[] = {"dhcp_on", "ip_fixed", "gateway", "subnet_mask", "dns", "source", "peer_ip",
                           "peer_port", "peer_path", "peer_protocol", "enphase_user", "enphase_password",
                           "meter_channel", "enphase_serial",
                           "mqtt_repeat_sec", "mqtt_ip", "mqtt_port", "mqtt_user", "mqtt_password", "mqtt_prefix",
                           "mqtt_device_name", "router_name", "probe_second_name", "probe_house_name",
                           "temperature_label", "calib_u", "calib_i", "pmqtt_topic", "pmqtt_schema",
                           "pmqtt_bindings",
                           "jsy_mk333_serial_baud", "install_country", "install_country_variant", "mains_nominal_v",
                           "mains_frequency_mode", "mains_frequency_hz_manual", "triac_override_max_temp_c",
                           "http_cors_enabled", "pwm_gpio", "pwm_mode", "pwm_duty_percent", "pwm_inverted",
                           "tempo_rte_enabled", "expert_regulation_mode", "regulation_gain", "vacation_enabled",
                           "vacation_end_epoch",
                           "max_routed_w", "mqtt_json_commands", "triac_off_when_source_stale",
                           "triac_backoff_when_heater_idle", "action_daily_cap_wh"};
  JsonObject in = doc.as<JsonObject>();
  for (JsonPair kv : in) {
    bool ok = false;
    for (unsigned i = 0; i < sizeof(allowed) / sizeof(allowed[0]); i++) {
      if (strcmp(kv.key().c_str(), allowed[i]) == 0) {
        ok = true;
        break;
      }
    }
    if (!ok) {
      api_error(server,400, "unknown_key", kv.key().c_str());
      return;
    }
  }
  String err;
  if (!config_apply_from_json(in, false, err)) {
    api_error(server,400, "validation", err.c_str());
    return;
  }
  helio_config_audit_record("/api/v1/config", in);
  int addr = persistConfigToEeprom();
  StaticJsonDocument<128> out;
  out["ok"] = true;
  out["eeprom_bytes"] = addr;
  String s;
  serializeJson(out, s);
  api_send_json(server,200, s);
}

