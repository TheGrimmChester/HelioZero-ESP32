/*
 * Auto-split from api_v1_routes.cpp — see api_v1_common.h
 */
#include "api_v1_common.h"
#include "helio_ha_state_payload.h"
#include "helio_regulation_modes.h"
#include "helio_self_test.h"
#include "helio_triac_isr.h"
#include "triac_api_shim.h"
void handle_get_measurements() {
  API_AUTH_GUARD();
  DynamicJsonDocument doc(2048);
  api_append_measurements_object(doc.to<JsonObject>());
  String out;
  serializeJson(doc, out);
  api_send_json(server,200, out);
}

void handle_get_telemetry_snapshot() {
  API_AUTH_GUARD();
  DynamicJsonDocument doc(1536);
  helio_append_ha_state_payload(doc.to<JsonObject>());
  String out;
  serializeJson(doc, out);
  api_send_json(server, 200, out);
}

void handle_post_triac_override() {
  API_AUTH_GUARD();
  if (!server.hasArg("plain")) {
    api_error(server, 400, "missing_json", "JSON body required");
    return;
  }
  StaticJsonDocument<128> body;
  DeserializationError err = deserializeJson(body, server.arg("plain"));
  if (err) {
    api_error(server, 400, "invalid_json", err.c_str());
    return;
  }
  const char *command = body["command"] | "";
  String applyErr;
  if (!helio_apply_triac_command(command, applyErr)) {
    api_error(server, 400, "invalid_command", applyErr.c_str());
    return;
  }
  StaticJsonDocument<64> out;
  out["ok"] = true;
  String serialized;
  serializeJson(out, serialized);
  api_send_json(server, 200, serialized);
}

void handle_get_tariff_tempo() {
  API_AUTH_GUARD();
  DynamicJsonDocument doc(512);
  tempo_rte_append_api_json(doc.to<JsonObject>());
  String out;
  serializeJson(doc, out);
  api_send_json(server, 200, out);
}

void handle_get_system() {
  API_AUTH_GUARD();
  int itIn = 0;
  int itRaw = 0;
  TriacReadAndResetCounters(itIn, itRaw);
  int T = int(millis() / 36000);
  float hoursUp = float(T) / 100.0f;
  DynamicJsonDocument doc(1536);
  doc["uptime_hours"] = hoursUp;
  doc["wifi_rssi_dbm"] = WiFi.RSSI();
  doc["wifi_bssid"] = WiFi.BSSIDstr();
  doc["mac"] = WiFi.macAddress();
  doc["ssid"] = ssid;
  doc["ip"] = WiFi.localIP().toString();
  doc["gateway"] = WiFi.gatewayIP().toString();
  doc["subnet"] = WiFi.subnetMask().toString();
  doc["dns"] = WiFi.dnsIP().toString();
  JsonArray rmsT = doc.createNestedArray("metering_task_ms");
  rmsT.add((int)metering_task_ms_min);
  rmsT.add((int)metering_task_ms_avg);
  rmsT.add((int)metering_task_ms_max);
  JsonArray loopT = doc.createNestedArray("loop_task_ms");
  loopT.add((int)previousLoopMin);
  loopT.add((int)previousLoopMoy);
  loopT.add((int)previousLoopMax);
  doc["eeprom_used_percent"] = P_cent_EEPROM;
  doc["irq_half_period_raw_vs_in"] = String(itIn) + "/" + String(itRaw);
  doc["triac_itmode"] = (int)zc_sync_state;
  doc["triac_zc_synced"] = zc_sync_state > 0;
  doc["configured_frequency_hz"] = helio_mains_effective_frequency_hz();
  String out;
  serializeJson(doc, out);
  api_send_json(server,200, out);
}

void handle_get_device() {
  API_AUTH_GUARD();
  DynamicJsonDocument doc(1024);
  doc["source"] = Source;
  doc["source_data"] = Source_data;
  doc["router_name"] = routerName;
  doc["firmware_version"] = Version;
  doc["probe_second_name"] = probeSecondName;
  doc["probe_house_name"] = probeHouseName;
  doc["peer_ip"] = ip32ToDotted(peer_ip);
  doc["peer_port"] = peer_port;
  doc["peer_path"] = peer_path;
  doc["temperature_label"] = temperatureSensorName;
  doc["device_uid"] = helio_device_uid();
  JsonObject fw = doc.createNestedObject("firmware");
  fw["sketch_md5"] = ESP.getSketchMD5();
  fw["sketch_size"] = ESP.getSketchSize();
  fw["free_sketch_space"] = ESP.getFreeSketchSpace();
  fw["sdk_version"] = ESP.getSdkVersion();
  JsonObject chip = doc.createNestedObject("chip");
  chip["model"] = ESP.getChipModel();
  chip["revision"] = ESP.getChipRevision();
  chip["cores"] = ESP.getChipCores();
  chip["cpu_mhz"] = ESP.getCpuFreqMHz();
  chip["flash_size"] = ESP.getFlashChipSize();
  chip["flash_mhz"] = ESP.getFlashChipSpeed();
  chip["mac"] = WiFi.macAddress();
  String out;
  serializeJson(doc, out);
  api_send_json(server,200, out);
}

void handle_get_state() {
  API_AUTH_GUARD();
  DynamicJsonDocument doc(4096);
  JsonObject measurements = doc.createNestedObject("measurements");
  api_append_measurements_object(measurements);
  JsonObject actions = doc.createNestedObject("actions_live");
  actions["temperature_c"] = temperature;
  actions["source"] = Source_data;
  actions["peer_ip"] = ip32ToDotted(peer_ip);
  actions["peer_port"] = peer_port;
  actions["peer_path"] = peer_path;
  int nb = 0;
  for (int i = 0; i < NbActions; i++) {
    if (action_regulation_enabled(load_channels[i].Actif)) nb++;
  }
  actions["active_actions_count"] = nb;
  JsonArray slots = actions.createNestedArray("active_slots");
  api_action_append_live_state(slots);
  doc["triac_open_percent"] = TriacGetOpenPercent();
  JsonObject status = doc.createNestedObject("status");
  const int health = helio_compute_source_health_score();
  status["source_health"] = health;
  status["source_stale"] = helio_source_health_is_stale(health);
  status["regulation_active"] = TriacGetOpenPercent() > 5;
  status["site_cap_active"] = siteCapActive;
  status["mqtt_connected"] = clientMQTT.connected();
  doc["heater_load_backoff_active"] = heaterLoadBackoffActive;
  doc["temperature_c"] = temperature;
  doc["time"] = time_sync_valid ? sync_clock_str : "";
  doc["date_valid"] = time_sync_valid;
  doc["source"] = Source_data;
  JsonArray overrides = doc.createNestedArray("override_summary");
  for (int i = 0; i < NbActions; i++) {
    if (load_channels[i].OverrideState == ACTION_OVERRIDE_AUTO) continue;
    JsonObject o = overrides.createNestedObject();
    api_append_action_override(o, i);
  }
  String out;
  serializeJson(doc, out);
  api_send_json(server, 200, out);
}

void handle_get_health() {
  API_AUTH_GUARD();
  StaticJsonDocument<512> doc;
  doc["ok"] = true;
  doc["uptime_s"] = (unsigned long)(millis() / 1000UL);
  doc["source_ok"] = meter_reading_valid;
  doc["date_valid"] = time_sync_valid;
  doc["mqtt_connected"] = clientMQTT.connected();
  doc["free_heap"] = ESP.getFreeHeap();
  doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
  JsonObject st = doc.createNestedObject("self_test");
  helio_self_test_append_health_json(st);
  String out;
  serializeJson(doc, out);
  api_send_json(server, 200, out);
}

void handle_get_sources() {
  API_AUTH_GUARD();
  StaticJsonDocument<512> doc;
  JsonArray supported = doc.createNestedArray("supported");
  for (size_t i = 0; i < helio_source_registry_count(); i++) {
    supported.add(helio_source_wire_at(i));
  }
  doc["current"] = Source;
  doc["current_data"] = Source_data;
  String out;
  serializeJson(doc, out);
  api_send_json(server, 200, out);
}

void handle_get_gpio() {
  API_AUTH_GUARD();
  DynamicJsonDocument doc(1536);
  JsonObject levels = doc.createNestedObject("levels");
  for (int gpio = 0; gpio <= 33; gpio++) {
    if (IsRestrictedGpioRead(gpio)) continue;
    levels[String(gpio)] = digitalRead(gpio);
  }
  doc["updated_ms"] = (unsigned long)millis();
  String out;
  serializeJson(doc, out);
  api_send_json(server, 200, out);
}

