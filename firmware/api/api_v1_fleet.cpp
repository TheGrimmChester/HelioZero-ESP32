/*
 * Auto-split from api_v1_routes.cpp — see api_v1_common.h
 */
#include "api_v1_common.h"
void fleet_append_export_config(JsonObject o) {
  o["source"] = Source;
  o["peer_ip"] = ip32ToDotted(peer_ip);
  o["peer_port"] = peer_port;
  o["peer_path"] = peer_path;
  o["peer_protocol"] = peer_protocol.length() ? peer_protocol : "json";
  o["install_country"] = helio_mains_install_country();
  o["install_country_variant"] = helio_mains_install_variant();
  o["mains_nominal_v"] = helio_mains_nominal_v();
  o["mains_frequency_mode"] =
      (helio_mains_frequency_mode() == MainsFrequencyMode::Manual) ? "manual" : "auto";
  o["mains_frequency_hz_manual"] = helio_mains_frequency_hz_manual();
  o["router_name"] = routerName;
  o["probe_second_name"] = probeSecondName;
  o["probe_house_name"] = probeHouseName;
  o["temperature_label"] = temperatureSensorName;
  o["calib_u"] = CalibU;
  o["calib_i"] = CalibI;
  o["pmqtt_topic"] = PmqttTopic;
  o["pmqtt_schema"] = PmqttSchema;
  o["jsy_mk333_serial_baud"] = JsyMk333SerialBaud;
  o["triac_override_max_temp_c"] = triacOverrideMaxTempC;
  o["pwm_gpio"] = pwmGpio;
  o["pwm_mode"] = pwmMode;
  o["pwm_duty_percent"] = pwmDutyPercent;
  o["pwm_inverted"] = pwmInverted;
}

void handle_get_fleet_export() {
  API_AUTH_GUARD();
  DynamicJsonDocument cfgDoc(2048);
  JsonObject cfgObj = cfgDoc.to<JsonObject>();
  fleet_append_export_config(cfgObj);
  String cfgJson;
  serializeJson(cfgObj, cfgJson);

  DynamicJsonDocument actDoc(12288);
  actDoc["schema_version"] = API_ACTION_SCHEMA_VERSION;
  actDoc["nb_actions"] = NbActions;
  JsonArray ar = actDoc.createNestedArray("actions");
  api_action_append_config_array(ar);
  String actJson;
  serializeJson(actDoc, actJson);

  if (fleet_bundle_contains_forbidden_secret(cfgJson.c_str()) ||
      fleet_bundle_contains_forbidden_secret(actJson.c_str())) {
    api_error(server, 500, "internal", "export would contain forbidden keys");
    return;
  }

  FleetBundle parts;
  parts.exported_at = time_sync_valid ? std::string(sync_clock_str.c_str()) : std::to_string(millis() / 1000UL);
  parts.device_name = std::string(routerName.c_str());
  parts.config_json = std::string(cfgJson.c_str());
  parts.actions_json = std::string(actJson.c_str());
  const std::string unsignedBody = fleet_bundle_build_unsigned(parts);
  parts.signature = fleet_bundle_sign_hmac_sha256_hex(unsignedBody, std::string(fleetTrustKey.c_str()));

  DynamicJsonDocument out(14000);
  deserializeJson(out, unsignedBody.c_str());
  if (fleetTrustKey.length() > 0) out["signature"] = parts.signature.c_str();
  String s;
  serializeJson(out, s);
  api_send_json(server, 200, s);
}

void handle_post_fleet_import() {
  API_AUTH_GUARD();
  if (fleetTrustKey.length() == 0) {
    api_error(server, 403, "forbidden", "fleet trust key not configured");
    return;
  }
  String body;
  if (!api_require_json_body(server, body, kPutBodyMax, false)) return;
  DynamicJsonDocument doc(kPutBodyMax);
  DeserializationError e = deserializeJson(doc, body);
  if (e) {
    api_error(server, 400, "json_parse", e.c_str());
    return;
  }
  const char *sig = doc["signature"] | "";
  if (!sig[0]) {
    api_error(server, 400, "validation", "signature required");
    return;
  }
  FleetBundle parts;
  parts.schema_version = doc["schema_version"] | "1";
  parts.exported_at = doc["exported_at"] | "";
  parts.device_name = doc["device_name"] | "";
  String cfgJson;
  String actJson;
  if (doc.containsKey("config")) serializeJson(doc["config"], cfgJson);
  if (doc.containsKey("actions")) serializeJson(doc["actions"], actJson);
  parts.config_json = std::string(cfgJson.c_str());
  parts.actions_json = std::string(actJson.c_str());
  const std::string unsignedBody = fleet_bundle_build_unsigned(parts);
  if (!fleet_bundle_verify_hmac_sha256_hex(unsignedBody, std::string(fleetTrustKey.c_str()),
                                           std::string(sig))) {
    api_error(server, 400, "validation", "invalid signature");
    return;
  }
  const bool dry_run = doc["dry_run"] | false;
  if (doc.containsKey("config")) {
    String err;
    if (!config_apply_from_json(doc["config"].as<JsonObject>(), false, err)) {
      api_error(server, 400, "validation", err.c_str());
      return;
    }
  }
  if (doc.containsKey("actions") && !dry_run) {
    String actBody;
    serializeJson(doc["actions"], actBody);
    String err;
    if (!api_action_put_collection(actBody, err)) {
      api_error(server, 400, "validation", err.c_str());
      return;
    }
  }
  if (!dry_run) persistConfigToEeprom();
  StaticJsonDocument<128> o;
  o["ok"] = true;
  o["dry_run"] = dry_run;
  String s;
  serializeJson(o, s);
  api_send_json(server, 200, s);
}

void handle_put_fleet_trust_key() {
  API_AUTH_GUARD();
  String body;
  if (!api_require_json_body(server, body, 256, false)) return;
  StaticJsonDocument<192> doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    api_error(server, 400, "json_parse", "invalid json");
    return;
  }
  const char *key = doc["fleet_trust_key"] | "";
  if (strlen(key) > 64) {
    api_error(server, 400, "validation", "fleet_trust_key max 64 chars");
    return;
  }
  fleetTrustKey = String(key);
  persistConfigToEeprom();
  StaticJsonDocument<64> o;
  o["ok"] = true;
  o["configured"] = fleetTrustKey.length() > 0;
  String s;
  serializeJson(o, s);
  api_send_json(server, 200, s);
}

void handle_post_reboot() {
  API_AUTH_GUARD();
  api_send_json(server,200, "{\"ok\":true,\"message\":\"rebooting\"}");
  RequestReboot(500);
}

