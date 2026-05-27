/*
 * Auto-split from api_v1_routes.cpp — see api_v1_common.h
 */
#include "api_v1_common.h"
#include "metering/pmqtt_bindings.h"
void handle_get_sources_diagnostics() {
  API_AUTH_GUARD();
  DynamicJsonDocument doc(8192);
  doc["source"] = Source;
  doc["source_data"] = Source_data;
  doc["date"] = time_sync_valid ? sync_clock_str : "";
  doc["date_valid"] = time_sync_valid;
  doc["temperature_c"] = temperature;
  doc["frequency_hz"] = mains_frequency_hz;
  doc["voltage_house_v"] = house_voltage_v;
  doc["current_house_a"] = house_current_a;
  doc["pf_house"] = house_power_factor;
  doc["voltage_second_v"] = second_voltage_v;
  doc["current_second_a"] = second_current_a;
  doc["pf_second"] = second_power_factor;
  int linky_tail_max = 768;
  if (server.hasArg("linky_tail")) {
    int q = server.arg("linky_tail").toInt();
    if (q > 0 && q <= 2048) {
      linky_tail_max = q;
    }
  }
  JsonObject root = doc.as<JsonObject>();
  helio_sources_diagnostics_append_meter_payload(root, linky_tail_max);
  helio_sources_diagnostics_append_summary(root);
  pmqtt_bindings_append_diagnostics(root);
  String out;
  serializeJson(doc, out);
  api_send_json(server, 200, out);
}

void handle_get_sources_brute_panel() {
  API_AUTH_GUARD();
  String out;
  helio_sources_brute_panel_json(out);
  api_send_json(server, 200, out);
}

void handle_post_history_reset() {
  API_AUTH_GUARD();
  eepromClearConsumptionHistory();
  for (int i = 0; i < 600; i++) {
    tabPwHouse_5mn[i] = 0;
    tabPw_Triac_5mn[i] = 0;
    tabTemperature_5mn[i] = 0;
  }
  for (int i = 0; i < 300; i++) {
    tabPwHouse_2s[i] = 0;
    tabPw_Triac_2s[i] = 0;
    tabPvaHouse_2s[i] = 0;
    tabPva_Triac_2s[i] = 0;
  }
  house_day_energy_import_wh = 0;
  house_day_energy_export_wh = 0;
  second_day_energy_import_wh = 0;
  second_day_energy_export_wh = 0;
  api_send_json(server, 200, "{\"ok\":true,\"message\":\"history reset\"}");
}

void handle_post_mqtt_discover() {
  API_AUTH_GUARD();
  ApiMqttRepublishDiscovery();
  api_send_json(server, 200, "{\"ok\":true,\"action\":\"discovery_republished\"}");
}

void handle_post_mqtt_reconnect() {
  API_AUTH_GUARD();
  ApiMqttReconnect();
  api_send_json(server, 200, "{\"ok\":true,\"action\":\"reconnect_scheduled\"}");
}

void handle_post_mqtt_publish_now() {
  API_AUTH_GUARD();
  ApiMqttPublishNow();
  api_send_json(server, 200, "{\"ok\":true,\"action\":\"publish_now\"}");
}

void handle_post_mqtt_test() {
  API_AUTH_GUARD();
  String host = ip32ToDotted(MQTTIP);
  uint16_t port = (uint16_t)MQTTPort;
  String user = MQTTUser;
  String pwd = MQTTPwd;
  String deviceName = MQTTdeviceName;
  String body;
  if (server.hasArg("plain") && server.arg("plain").length() > 0) {
    body = server.arg("plain");
    if (body.length() > 384) {
      api_error(server, 413, "payload_too_large", "body exceeds limit");
      return;
    }
    StaticJsonDocument<384> doc;
    if (deserializeJson(doc, body)) {
      api_error(server, 400, "validation", "invalid json");
      return;
    }
    if (doc.containsKey("mqtt_ip")) {
      const char *s = doc["mqtt_ip"];
      if (!s || !s[0]) {
        api_error(server, 400, "validation", "bad ip for mqtt_ip");
        return;
      }
      host = String(s);
    }
    if (doc.containsKey("mqtt_port")) port = (uint16_t)(int)doc["mqtt_port"];
    if (doc.containsKey("mqtt_user")) user = doc["mqtt_user"].as<String>();
    if (doc.containsKey("mqtt_password")) pwd = doc["mqtt_password"].as<String>();
    if (doc.containsKey("mqtt_device_name")) deviceName = doc["mqtt_device_name"].as<String>();
  }
  helio_apply_default_mqtt_device_name(deviceName);
  int errCode = 0;
  String msg;
  const bool connected = ApiMqttTestConnection(host, port, user, pwd, deviceName, errCode, msg);
  StaticJsonDocument<192> out;
  out["ok"] = connected;
  out["mqtt_connected"] = connected;
  out["error_code"] = errCode;
  out["message"] = msg;
  String json;
  serializeJson(out, json);
  api_send_json(server, connected ? 200 : 502, json);
}

void handle_post_pmqtt_preview() {
  API_AUTH_GUARD();
  String body;
  if (!api_require_json_body(server, body, 4096, false)) return;
  DynamicJsonDocument doc(4096);
  if (deserializeJson(doc, body)) {
    api_error(server, 400, "validation", "invalid json");
    return;
  }
  if (!doc.containsKey("pmqtt_bindings") || !doc["pmqtt_bindings"].is<JsonArray>()) {
    api_error(server, 400, "validation", "pmqtt_bindings array required");
    return;
  }
  DynamicJsonDocument out(4096);
  JsonArray results = out.createNestedArray("results");
  String err;
  if (!pmqtt_bindings_preview(doc["pmqtt_bindings"].as<JsonArray>(), results, &err)) {
    api_error(server, 400, "validation", err.c_str());
    return;
  }
  out["ok"] = true;
  String json;
  serializeJson(out, json);
  api_send_json(server, 200, json);
}

#if defined(HELIO_ZERO_ENABLE_SOURCE_TEST_API)
void handle_post_sources_test_inject() {
  API_AUTH_GUARD();
  String body;
  if (!api_require_json_body(server, body, 2048, false)) return;
  {
    std::string errStd;
    if (!helio_meter_json_logic_validate_inject(body.c_str(), &errStd)) {
      api_error(server, 400, "validation", errStd.c_str());
      return;
    }
  }
  DynamicJsonDocument doc(2048);
  DeserializationError e = deserializeJson(doc, body);
  if (e) {
    api_error(server, 400, "json_parse", e.c_str());
    return;
  }
  if (doc.containsKey("sim") && doc["sim"].is<JsonObject>()) {
    JsonObject sim = doc["sim"].as<JsonObject>();
    if (sim.containsKey("wall_decihours")) {
      wall_clock_decihours = (int)sim["wall_decihours"].as<long>();
    }
    if (sim.containsKey("temperature_c")) {
      temperature = sim["temperature_c"].as<float>();
    }
  }
  String err;
  if (!ApplyMeterSnapshotFromJson(doc.as<JsonObject>(), &err)) {
    api_error(server, 400, "validation", err.c_str());
    return;
  }
  api_send_json(server, 200, "{\"ok\":true}");
}
#endif

