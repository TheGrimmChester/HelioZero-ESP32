/*
 * api_v1_system_backup.cpp — schema v2 device backup export/import.
 */
#include "api_v1_common.h"
#include "api_access_token.h"
#include "helio_config_audit.h"

#include <string>

namespace {

constexpr int kBackupSchemaVersion = 2;

static String backup_exported_at_iso() {
  if (time_sync_valid && sync_clock_str.length() > 0) {
    return sync_clock_str;
  }
  char buf[32];
  snprintf(buf, sizeof(buf), "uptime-%lu", static_cast<unsigned long>(millis() / 1000UL));
  return String(buf);
}

static void backup_append_api_block(JsonObject api) {
  bool any = false;
  if (api_auth_enabled() && httpApiPassword.length() > 0) {
    api["http_api_password"] = httpApiPassword;
    any = true;
  }
  if (apiAccessTokenCount > 0) {
    JsonArray arr = api.createNestedArray("access_tokens");
    for (int i = 0; i < apiAccessTokenCount; i++) {
      JsonObject o = arr.createNestedObject();
      o["id"] = apiAccessTokens[i].id;
      o["label"] = apiAccessTokens[i].label.c_str();
      o["token"] = apiAccessTokens[i].token_hex.c_str();
    }
    any = true;
  }
  if (!any) {
    api.remove("http_api_password");
    api.remove("access_tokens");
  }
}

static bool backup_apply_api_block(JsonObject api, String &err) {
  err.clear();
  if (api.isNull()) return true;

  if (api.containsKey("http_api_password")) {
    const char *pw = api["http_api_password"] | "";
    String pwStr(pw);
    if (!api_validate_password_ascii(pwStr, err)) return false;
    httpApiPassword = pwStr;
    api_session_clear();
  }

  if (api.containsKey("access_tokens")) {
    if (!api["access_tokens"].is<JsonArray>()) {
      err = "api.access_tokens must be an array";
      return false;
    }
    JsonArray arr = api["access_tokens"].as<JsonArray>();
    if (arr.size() > kApiAccessTokenMax) {
      err = "too many access tokens";
      return false;
    }
    ApiAccessTokenEntry entries[kApiAccessTokenMax];
    int n = 0;
    for (JsonObject o : arr) {
      if (n >= kApiAccessTokenMax) break;
      if (!o.containsKey("id") || !o.containsKey("token")) {
        err = "access token entry requires id and token";
        return false;
      }
      entries[n].id = static_cast<uint8_t>(o["id"].as<int>());
      entries[n].label = o["label"] | "";
      entries[n].token_hex = o["token"].as<const char *>();
      n++;
    }
    std::string tokenErr;
    if (!api_access_tokens_replace_all(entries, n, tokenErr)) {
      err = tokenErr.c_str();
      return false;
    }
  }
  return true;
}

}  // namespace

void handle_get_system_backup() {
  API_AUTH_GUARD();
  DynamicJsonDocument doc(20480);
  doc["backupSchemaVersion"] = kBackupSchemaVersion;
  doc["exportedAt"] = backup_exported_at_iso();
  JsonObject cfg = doc.createNestedObject("config");
  api_append_config_object(cfg);
  JsonObject actions = doc.createNestedObject("actions");
  actions["schema_version"] = API_ACTION_SCHEMA_VERSION;
  actions["nb_actions"] = NbActions;
  JsonArray actArr = actions.createNestedArray("actions");
  api_action_append_config_array(actArr);
  JsonObject timeObj = doc.createNestedObject("time");
  timeObj["tz"] = TimeTz;
  timeObj["ntp1"] = TimeNtp1;
  timeObj["ntp2"] = TimeNtp2;
  JsonObject wifiObj = doc.createNestedObject("wifi");
  wifiObj["ssid"] = ssid;
  wifiObj["password"] = password;
  JsonObject apiObj = doc.createNestedObject("api");
  backup_append_api_block(apiObj);
  if (apiObj.size() == 0) {
    doc.remove("api");
  }
  String out;
  serializeJson(doc, out);
  api_send_json(server, 200, out);
}

void handle_put_system_backup() {
  API_AUTH_GUARD();
  String body;
  if (!api_require_json_body(server, body, kPutBodyMax, false)) return;
  DynamicJsonDocument doc(kPutBodyMax);
  DeserializationError e = deserializeJson(doc, body);
  if (e) {
    api_error(server, 400, "json_parse", e.c_str());
    return;
  }
  const int ver = doc["backupSchemaVersion"] | 0;
  if (ver != kBackupSchemaVersion) {
    api_error(server, 400, "bad_request", "unsupported backupSchemaVersion");
    return;
  }
  if (!doc.containsKey("config") || !doc.containsKey("actions") || !doc.containsKey("time") ||
      !doc.containsKey("wifi")) {
    api_error(server, 400, "bad_request", "missing required backup sections");
    return;
  }

  String err;
  if (!config_apply_from_json(doc["config"].as<JsonObject>(), true, err)) {
    api_error(server, 400, "validation", err.c_str());
    return;
  }

  {
    DynamicJsonDocument actWrap(12288);
    actWrap["schema_version"] = doc["actions"]["schema_version"] | API_ACTION_SCHEMA_VERSION;
    actWrap["nb_actions"] = doc["actions"]["nb_actions"] | 0;
    actWrap["actions"] = doc["actions"]["actions"];
    String actBody;
    serializeJson(actWrap, actBody);
    if (!api_action_put_collection(actBody, err)) {
      api_error(server, 400, "validation", err.c_str());
      return;
    }
  }

  JsonObject timeObj = doc["time"].as<JsonObject>();
  if (timeObj.containsKey("tz")) TimeTz = timeObj["tz"].as<String>();
  if (timeObj.containsKey("ntp1")) TimeNtp1 = timeObj["ntp1"].as<String>();
  if (timeObj.containsKey("ntp2")) TimeNtp2 = timeObj["ntp2"].as<String>();
  configTzTime(TimeTz.c_str(), TimeNtp1.c_str(), TimeNtp2.c_str());

  JsonObject wifiObj = doc["wifi"].as<JsonObject>();
  const char *newSsid = wifiObj["ssid"] | "";
  if (strlen(newSsid) == 0) {
    api_error(server, 400, "validation", "wifi.ssid required");
    return;
  }
  ssid = String(newSsid);
  password = String(wifiObj["password"] | "");

  if (doc.containsKey("api")) {
    if (!backup_apply_api_block(doc["api"].as<JsonObject>(), err)) {
      api_error(server, 400, "validation", err.c_str());
      return;
    }
  }

  helio_config_audit_record("/api/v1/system/backup", doc["config"].as<JsonObject>());
  const int addr = persistConfigToEeprom();
  if (addr < 0) {
    api_error(server, 500, "eeprom_error", "failed to persist backup");
    return;
  }

  StaticJsonDocument<128> out;
  out["ok"] = true;
  out["eeprom_bytes"] = addr;
  String s;
  serializeJson(out, s);
  api_send_json(server, 200, s);
}
