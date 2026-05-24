/*
 * Auto-split from api_v1_routes.cpp — see api_v1_common.h
 */
#include "api_v1_common.h"
#include "api_access_token.h"
#include "app_wifi_setup.h"
#include "captive_dns.h"
#include "helio_forward.h"
#include "helio_self_test.h"
static void api_wifi_append_public_fields(JsonObject wifi) {
  const bool setupAp = helio_wifi_soft_ap_setup_active();
  wifi["setup_ap"] = setupAp;
  if (setupAp) {
    wifi["mode"] = "ap";
    wifi["connected"] = false;
    return;
  }
  bool staMode = false;
  bool wifiConnected = false;
  api_wifi_public_status(staMode, wifiConnected);
  wifi["mode"] = staMode ? "sta" : "ap";
  wifi["connected"] = wifiConnected;
}

void handle_get_wifi() {
  API_AUTH_GUARD();
  StaticJsonDocument<384> doc;
  doc["ssid"] = ssid;
  doc["password"] = password;
  const bool setupAp = helio_wifi_soft_ap_setup_active();
  doc["setup_ap"] = setupAp;
  if (setupAp) {
    doc["mode"] = "ap";
    doc["connected"] = false;
    doc["rssi"] = WiFi.RSSI();
    doc["ip"] = WiFi.softAPIP().toString();
  } else {
    bool staMode = false;
    bool wifiConnected = false;
    api_wifi_public_status(staMode, wifiConnected);
    doc["mode"] = staMode ? "sta" : "ap";
    doc["connected"] = wifiConnected;
    doc["rssi"] = WiFi.RSSI();
    doc["ip"] = WiFi.localIP().toString();
  }
  String out;
  serializeJson(doc, out);
  api_send_json(server, 200, out);
}

void handle_put_wifi() {
  API_AUTH_GUARD();
  String body;
  if (!api_require_json_body(server, body, kWifiBodyMax, false)) return;
  StaticJsonDocument<512> doc;
  DeserializationError e = deserializeJson(doc, body);
  if (e) {
    api_error(server, 400, "json_parse", e.c_str());
    return;
  }
  const char *newSsid = doc["ssid"] | "";
  const char *newPassword = doc["password"] | "";
  if (strlen(newSsid) == 0) {
    api_error(server, 400, "validation", "ssid required");
    return;
  }
  ssid = String(newSsid);
  password = String(newPassword);
  bool persist = doc["persist"] | true;
  int addr = persist ? persistConfigToEeprom() : 0;
  StaticJsonDocument<160> outDoc;
  outDoc["ok"] = true;
  outDoc["persisted"] = persist;
  outDoc["eeprom_bytes"] = addr;
  outDoc["reconnect_started"] = true;
  outDoc["reboot_scheduled"] = persist;
  String out;
  serializeJson(outDoc, out);
  api_send_json(server, 200, out);
  if (persist) {
    RequestReboot(2000);
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.begin(ssid.c_str(), password.c_str());
  }
}

/** Scan needs the STA radio; pure WIFI_AP cannot see surrounding networks. */
void wifi_ensure_scan_capable() {
  if (WiFi.getMode() == WIFI_AP) {
    WiFi.mode(WIFI_AP_STA);
    delay(100);
  }
  if (WiFi.getMode() != WIFI_STA && ap_default_ssid != nullptr && WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) {
    const char *psk = (ap_default_psk != nullptr && ap_default_psk[0] != '\0') ? ap_default_psk : nullptr;
    WiFi.softAP(ap_default_ssid, psk, 6, 0, 4);
    delay(100);
  }
}

void wifi_send_scan_json(int n, bool scanError) {
  DynamicJsonDocument doc(2048);
  doc["scanning"] = false;
  if (scanError) {
    doc["scan_error"] = true;
  }
  JsonArray networks = doc.createNestedArray("networks");
  for (int i = 0; i < n; i++) {
    JsonObject o = networks.createNestedObject();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
    o["channel"] = WiFi.channel(i);
    o["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }
  String out;
  serializeJson(doc, out);
  api_send_json(server, 200, out);
  WiFi.scanDelete();
  if (helio_wifi_soft_ap_setup_active()) {
    helio_captive_dns_start(WiFi.softAPIP());
  }
  helio_http_ensure_listening();
}

void wifi_start_async_scan() {
  WiFi.scanDelete();
  WiFi.scanNetworks(true);
}

void handle_get_wifi_scan() {
  API_AUTH_GUARD();
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  wifi_ensure_scan_capable();
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) {
    api_send_json(server, 202, "{\"scanning\":true,\"networks\":[]}");
    return;
  }
  if (n < 0) {
    wifi_start_async_scan();
    api_send_json(server, 202, "{\"scanning\":true,\"networks\":[]}");
    return;
  }
  wifi_send_scan_json(n, false);
}

void handle_post_factory_reset() {
  API_AUTH_GUARD();
  httpApiPassword = "";
  api_access_tokens_clear();
  eepromClearConsumptionHistory();
  eeprom_layout_key = 0;
  EEPROM.writeULong(kAdrParaActions, 0);
  helio_self_test_set_pending(true);
  persistConfigToEeprom();
  api_send_json(server, 200, "{\"ok\":true,\"message\":\"factory reset scheduled\"}");
  RequestReboot(500);
}

void handle_post_save_now() {
  API_AUTH_GUARD();
  int addr = persistConfigToEeprom();
  StaticJsonDocument<128> doc;
  doc["ok"] = true;
  doc["eeprom_bytes"] = addr;
  String out;
  serializeJson(doc, out);
  api_send_json(server, 200, out);
}

void handle_get_eeprom() {
  API_AUTH_GUARD();
  StaticJsonDocument<256> doc;
  doc["used_percent"] = P_cent_EEPROM;
  doc["total_bytes"] = kEepromSize;
  doc["used_bytes"] = RomUsedBytes;
  JsonObject addr = doc.createNestedObject("key_addresses");
  addr["parameters"] = kAdrParaActions;
  addr["history"] = 0;
  String out;
  serializeJson(doc, out);
  api_send_json(server, 200, out);
}

void handle_get_time() {
  API_AUTH_GUARD();
  StaticJsonDocument<384> doc;
  doc["tz"] = TimeTz;
  doc["ntp1"] = TimeNtp1;
  doc["ntp2"] = TimeNtp2;
  doc["date_valid"] = time_sync_valid;
  doc["now"] = sync_clock_str;
  String out;
  serializeJson(doc, out);
  api_send_json(server, 200, out);
}

void handle_get_system_arduino_ota() {
  API_AUTH_GUARD();
  StaticJsonDocument<128> doc;
  doc["password_set"] = arduinoOtaPassword.length() > 0;
  String out;
  serializeJson(doc, out);
  api_send_json(server, 200, out);
}

/** Non-sensitive UI bootstrap — always readable (see api_auth_public_read). */
void handle_get_public() {
  StaticJsonDocument<384> doc;
  JsonObject auth = doc.createNestedObject("http_auth");
  auth["enabled"] = api_auth_enabled();
  auth["username"] = "admin";
  JsonObject device = doc.createNestedObject("device");
  device["router_name"] = routerName;
  device["firmware_version"] = Version;
  device["source_configured"] = (Source.length() > 0 && Source != "NotDef");
  JsonObject wifi = doc.createNestedObject("wifi");
  api_wifi_append_public_fields(wifi);
  String out;
  serializeJson(doc, out);
  api_send_json(server, 200, out);
}

void handle_put_system_http_auth() {
  API_AUTH_GUARD();
  String body;
  if (!api_require_json_body(server, body, kHttpAuthBodyMax, false)) return;
  StaticJsonDocument<128> doc;
  DeserializationError e = deserializeJson(doc, body);
  if (e) {
    api_error(server, 400, "json_parse", e.c_str());
    return;
  }
  if (!doc.containsKey("password")) {
    api_error(server, 400, "bad_request", "JSON must contain \"password\" (string, empty to clear)");
    return;
  }
  String pw = doc["password"].as<String>();
  String err;
  if (!api_validate_password_ascii(pw, err)) {
    api_error(server, 400, "validation", err.c_str());
    return;
  }
  httpApiPassword = pw;
  api_session_clear();
  api_access_tokens_clear();
  const int addr = persistConfigToEeprom();
  if ((pw.length() > 0) != api_auth_enabled()) {
    api_error(server, 500, "eeprom_error", "failed to persist HTTP API password");
    return;
  }
  StaticJsonDocument<192> out;
  out["ok"] = true;
  out["enabled"] = api_auth_enabled();
  out["eeprom_bytes"] = addr;
  String s;
  serializeJson(out, s);
  api_send_json(server, 200, s);
}

bool validate_arduino_ota_password(const String &s, String &err) {
  if (s.length() > 64) {
    err = "password max length is 64";
    return false;
  }
  for (unsigned i = 0; i < s.length(); i++) {
    unsigned char c = (unsigned char)s[i];
    if (c < 32 || c > 126) {
      err = "password must be printable ASCII";
      return false;
    }
  }
  return true;
}

void handle_put_system_arduino_ota() {
  API_AUTH_GUARD();
  String body;
  if (!api_require_json_body(server, body, kArduinoOtaBodyMax, false)) return;
  StaticJsonDocument<512> doc;
  DeserializationError e = deserializeJson(doc, body);
  if (e) {
    api_error(server, 400, "json_parse", e.c_str());
    return;
  }
  if (!doc.containsKey("password")) {
    api_error(server, 400, "bad_request", "JSON must contain \"password\" (string, empty to clear)");
    return;
  }
  String pw = doc["password"].as<String>();
  String err;
  if (!validate_arduino_ota_password(pw, err)) {
    api_error(server, 400, "validation", err.c_str());
    return;
  }
  arduinoOtaPassword = pw;
  int addr = persistConfigToEeprom();
  StaticJsonDocument<192> out;
  out["ok"] = true;
  out["eeprom_bytes"] = addr;
  out["message"] = "rebooting";
  String s;
  serializeJson(out, s);
  api_send_json(server, 200, s);
  RequestReboot(500);
}

void handle_post_auth_login() {
  String body;
  if (!api_require_json_body(server, body, kHttpAuthBodyMax, false)) return;
  StaticJsonDocument<128> doc;
  DeserializationError e = deserializeJson(doc, body);
  if (e) {
    api_error(server, 400, "json_parse", e.c_str());
    return;
  }
  if (!doc.containsKey("password")) {
    api_error(server, 400, "bad_request", "JSON must contain \"password\"");
    return;
  }
  const String pw = doc["password"].as<String>();
  if (!api_auth_enabled()) {
    api_send_json(server, 200, "{\"ok\":true}");
    return;
  }
  if (!api_session_validate_password(pw)) {
    api_error(server, 401, "unauthorized", "Invalid password");
    return;
  }
  String token;
  if (!api_session_issue(token)) {
    api_error(server, 500, "internal_error", "failed to create session");
    return;
  }
  StaticJsonDocument<192> out;
  out["ok"] = true;
  out["token"] = token;
  String s;
  serializeJson(out, s);
  api_send_json(server, 200, s);
}

void handle_post_auth_logout() {
  API_AUTH_GUARD();
  api_session_clear();
  api_send_json(server, 200, "{\"ok\":true}");
}

void handle_put_time() {
  API_AUTH_GUARD();
  String body;
  if (!api_require_json_body(server, body, kTimeBodyMax, true)) return;
  StaticJsonDocument<512> doc;
  DeserializationError e = deserializeJson(doc, body);
  if (e) {
    api_error(server, 400, "json_parse", e.c_str());
    return;
  }
  if (doc.containsKey("tz")) TimeTz = doc["tz"].as<String>();
  if (doc.containsKey("ntp1")) TimeNtp1 = doc["ntp1"].as<String>();
  if (doc.containsKey("ntp2")) TimeNtp2 = doc["ntp2"].as<String>();
  configTzTime(TimeTz.c_str(), TimeNtp1.c_str(), TimeNtp2.c_str());
  api_send_json(server, 200, "{\"ok\":true}");
}

void handle_post_firmware_ota_done() {
  API_AUTH_GUARD();
  if (Update.hasError()) {
    api_error(server, 500, "ota_failed", "firmware update failed");
    return;
  }
  api_send_json(server, 200, "{\"ok\":true,\"message\":\"rebooting\"}");
  RequestReboot(500);
}

void handle_firmware_ota_upload() {
  API_AUTH_GUARD();
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    if (server.hasArg("md5")) Update.setMD5(server.arg("md5").c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (!Update.end(true)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
  }
}

