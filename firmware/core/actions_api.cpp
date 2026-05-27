#include "actions_api.h"
#include "Actions.h"
#include "helio_regulation_modes.h"
#include "triac_api_shim.h"
#include <IPAddress.h>
#include <strings.h>

extern Action load_channels[];
extern int NbActions;
extern String RS;
extern String GS;

extern int persistConfigToEeprom(void);
extern void helio_init_action_gpios(void);

#ifndef kMaxRoutingActions
#define kMaxRoutingActions 20
#endif

static const char *kModeOff = "off";
static const char *kModeOn = "on";
static const char *kModePower = "power";

static byte modeStrToType(const char *m) {
  if (!m) return 0;
  if (strcasecmp(m, kModeOff) == 0) return 1;
  if (strcasecmp(m, kModeOn) == 0) return 2;
  if (strcasecmp(m, kModePower) == 0) return 3;
  return 0;
}

static const char *typeToModeStr(byte t) {
  if (t == 1) return kModeOff;
  if (t == 2) return kModeOn;
  if (t == 3) return kModePower;
  return "unknown";
}

static void appendPeriod(JsonArray periods, int idx, int p) {
  JsonObject po = periods.createNestedObject();
  po["mode"] = typeToModeStr(load_channels[idx].Type[p]);
  po["hour_end"] = load_channels[idx].period_end[p];
  po["power_min_w"] = load_channels[idx].power_min[p];
  po["power_max_w"] = load_channels[idx].power_max[p];
  po["temp_inf_c"] = load_channels[idx].temp_min[p];
  po["temp_sup_c"] = load_channels[idx].temp_max[p];
}

void api_action_append_one_config(JsonObject out, int index) {
  if (index < 0 || index >= NbActions) return;
  out["index"] = index;
  out["regulation_mode"] = load_channels[index].Actif;
  out["title"] = load_channels[index].title;
  if (index == 0) {
    out["kind"] = "triac";
    out["triac_sensitivity"] = load_channels[index].Ki;
    out["ki"] = load_channels[index].Ki;
    out["kp"] = load_channels[index].Kp;
    out["kd"] = load_channels[index].Kd;
    out["pid_enabled"] = load_channels[index].PID;
  } else if (load_channels[index].Host == "localhost") {
    out["kind"] = "local_gpio";
    out["host"] = "localhost";
    out["port"] = load_channels[index].Port;
    out["path_on"] = load_channels[index].path_on;
    out["path_off"] = load_channels[index].path_off;
  } else {
    out["kind"] = "remote_http";
    out["host"] = load_channels[index].Host;
    out["port"] = load_channels[index].Port;
    out["path_on"] = load_channels[index].path_on;
    out["path_off"] = load_channels[index].path_off;
  }
  out["repeat_sec"] = load_channels[index].Repet;
  out["tempo_sec"] = load_channels[index].Tempo;
  JsonArray per = out.createNestedArray("periods");
  for (int p = 0; p < load_channels[index].periodCount; p++) {
    appendPeriod(per, index, p);
  }
}

void api_action_append_config_array(JsonArray actionsOut) {
  for (int i = 0; i < NbActions; i++) {
    JsonObject o = actionsOut.createNestedObject();
    api_action_append_one_config(o, i);
  }
}

void api_action_append_live_state(JsonArray out) {
  for (int i = 0; i < NbActions; i++) {
    if (!action_regulation_enabled(load_channels[i].Actif)) continue;
    JsonObject o = out.createNestedObject();
    o["index"] = i;
    o["title"] = load_channels[i].title;
    if (i == 0) {
      o["triac_open_percent"] = TriacGetOpenPercent();
    } else {
      o["on"] = load_channels[i].On;
    }
  }
}

void api_action_append_schema(JsonObject root) {
  root["schema_version"] = API_ACTION_SCHEMA_VERSION;
  root["max_actions"] = kMaxRoutingActions;
  root["index_0_role"] = "triac";
  root["period_modes"] = "off | on | power — maps to firmware types 1,2,3";
  root["time_scale"] = "hour_end uses same 0..2400 scale as wall_clock_decihours (minutes*10/6 style upper bound 2400)";
  JsonObject units = root.createNestedObject("units");
  units["power"] = "W";
  units["temperature_band"] = "°C sentinel rules unchanged vs firmware temp_min/temp_max";
}

static void apply_regulationCoeffsFromJson(int index, JsonObject o) {
  if (index != 0) return;
  if (o.containsKey("ki")) {
    load_channels[0].Ki = (byte)min(255, max(1, (int)o["ki"]));
  } else if (o.containsKey("triac_sensitivity")) {
    load_channels[0].Ki = (byte)min(255, max(1, (int)o["triac_sensitivity"]));
  }
  if (o.containsKey("kp")) load_channels[0].Kp = (byte)(int)o["kp"];
  if (o.containsKey("kd")) load_channels[0].Kd = (byte)(int)o["kd"];
  if (o.containsKey("pid_enabled")) load_channels[0].PID = o["pid_enabled"].as<bool>();
}

static uint8_t regulation_mode_from_json(JsonObject o) {
  if (o.containsKey("regulation_mode")) {
    int m = (int)o["regulation_mode"];
    if (m >= kModeInactif && m <= kModeDemisinus) return static_cast<uint8_t>(m);
  }
  return kModeInactif;
}

static bool buildLineFromJson(JsonObject o, int index, String& line, String& err) {
  const uint8_t actif = regulation_mode_from_json(o);
  if (!o.containsKey("title")) {
    err = "missing title";
    return false;
  }
  String title = o["title"].as<String>();
  String host;
  int port = 0;
  String ordreOn = "";
  String ordreOff = "";
  if (index == 0) {
    host = "";
    port = 80;
  } else {
    const char *kind = o["kind"] | "";
    if (strcasecmp(kind, "local_gpio") == 0) {
      host = "localhost";
    } else if (!o.containsKey("host")) {
      err = "missing host";
      return false;
    } else {
      host = o["host"].as<String>();
    }
    port = (int)(o["port"] | 80);
    ordreOn = o["path_on"] | "";
    ordreOff = o["path_off"] | "";
  }
  int repet = (int)(o["repeat_sec"] | 0);
  int tempo = (int)(o["tempo_sec"] | 0);
  if (!o.containsKey("periods") || !o["periods"].is<JsonArray>()) {
    err = "missing periods array";
    return false;
  }
  JsonArray pa = o["periods"].as<JsonArray>();
  if (pa.size() == 0 || pa.size() > 8) {
    err = "periods length 1..8";
    return false;
  }
  line = String((int)actif) + RS + title + RS + host + RS + String(port) + RS + ordreOn + RS + ordreOff + RS +
         String(repet) + RS + String(tempo) + RS + String((int)pa.size());
  for (size_t i = 0; i < pa.size(); i++) {
    JsonObject p = pa[i];
    if (!p.containsKey("mode")) {
      err = "period missing mode";
      return false;
    }
    String ms = p["mode"].as<String>();
    byte ty = modeStrToType(ms.c_str());
    if (ty == 0) {
      err = "invalid period mode";
      return false;
    }
    int period_end_h = (int)(p["hour_end"] | 0);
    int power_min_w = (int)(p["power_min_w"] | 0);
    int power_max_w = (int)(p["power_max_w"] | 0);
    int temp_min_c = (int)(p["temp_inf_c"] | 0);
    int temp_max_c = (int)(p["temp_sup_c"] | 0);
    line += RS + String(ty) + RS + String(period_end_h) + RS + String(power_min_w) + RS + String(power_max_w) + RS +
           String(temp_min_c) + RS + String(temp_max_c);
  }
  return true;
}

static bool jsonToLine(JsonVariant v, int index, String& line, String& err) {
  if (!v.is<JsonObject>()) {
    err = "expected object";
    return false;
  }
  JsonObject o = v.as<JsonObject>();
  return buildLineFromJson(o, index, line, err);
}

void api_action_persist_and_init_gpio() {
  persistConfigToEeprom();
  helio_init_action_gpios();
}

String helio_actions_serialize_eeprom_json() {
  DynamicJsonDocument doc(12288);
  JsonArray ar = doc.createNestedArray("actions");
  api_action_append_config_array(ar);
  String out;
  serializeJson(doc, out);
  return out;
}

bool api_action_put_one(int index, const String& body, String& err) {
  if (index < 0 || index >= kMaxRoutingActions) {
    err = "index out of range";
    return false;
  }
  DynamicJsonDocument doc(6144);
  DeserializationError e = deserializeJson(doc, body);
  if (e) {
    err = e.c_str();
    return false;
  }
  String line;
  if (!jsonToLine(doc.as<JsonVariant>(), index, line, err)) return false;
  load_channels[index].parse_from_wire(line);
  apply_regulationCoeffsFromJson(index, doc.as<JsonObject>());
  if (index + 1 > NbActions) NbActions = index + 1;
  api_action_persist_and_init_gpio();
  return true;
}

static bool apply_actions_collection_json(const String &body, String &err) {
  DynamicJsonDocument doc(12288);
  DeserializationError e = deserializeJson(doc, body);
  if (e) {
    err = e.c_str();
    return false;
  }
  if (!doc.containsKey("actions") || !doc["actions"].is<JsonArray>()) {
    err = "missing actions array";
    return false;
  }
  JsonArray ar = doc["actions"].as<JsonArray>();
  if (ar.size() == 0 || ar.size() > (size_t)kMaxRoutingActions) {
    err = "actions length invalid";
    return false;
  }
  const int newCount = (int)ar.size();
  for (int i = 0; i < newCount; i++) {
    String line;
    if (!jsonToLine(ar[i], i, line, err)) return false;
    load_channels[i].parse_from_wire(line);
    apply_regulationCoeffsFromJson(i, ar[i].as<JsonObject>());
  }
  NbActions = newCount;
  return true;
}

bool api_action_put_collection(const String &body, String &err) {
  if (!apply_actions_collection_json(body, err)) return false;
  api_action_persist_and_init_gpio();
  return true;
}

bool helio_actions_load_eeprom_json(const String &json, String &err) {
  if (json.length() == 0) {
    return true;
  }
  return apply_actions_collection_json(json, err);
}

bool api_action_patch_one(int index, const String& body, String& err) {
  if (index < 0 || index >= NbActions) {
    err = "index out of range for patch";
    return false;
  }
  DynamicJsonDocument cur(6144);
  JsonObject base = cur.to<JsonObject>();
  api_action_append_one_config(base, index);
  DynamicJsonDocument pat(2048);
  DeserializationError e = deserializeJson(pat, body);
  if (e) {
    err = e.c_str();
    return false;
  }
  JsonObject pobj = pat.as<JsonObject>();
  for (JsonPair kv : pobj) {
    const char *k = kv.key().c_str();
    if (strcmp(k, "index") == 0) continue;
    if (strcmp(k, "periods") == 0) {
      base["periods"].set(kv.value());
    } else {
      base[kv.key()] = kv.value();
    }
  }
  String line;
  if (!jsonToLine(cur.as<JsonVariant>(), index, line, err)) return false;
  load_channels[index].parse_from_wire(line);
  apply_regulationCoeffsFromJson(index, pobj);
  api_action_persist_and_init_gpio();
  return true;
}

bool api_action_patch_collection_batch(const String& body, String& err) {
  DynamicJsonDocument doc(4096);
  DeserializationError e = deserializeJson(doc, body);
  if (e) {
    err = e.c_str();
    return false;
  }
  JsonArray upd = doc["updates"].as<JsonArray>();
  if (upd.isNull()) {
    err = "missing updates";
    return false;
  }
  if (upd.size() == 0 || upd.size() > API_ACTION_PATCH_MAX_UPDATES) {
    err = "updates batch size";
    return false;
  }
  for (JsonObject u : upd) {
    if (!u.containsKey("index")) {
      err = "update missing index";
      return false;
    }
    int idx = (int)u["index"];
    if (idx < 0 || idx >= NbActions) {
      err = "bad update index";
      return false;
    }
    JsonObject setObj = u["set"];
    if (setObj.isNull()) {
      err = "update missing set";
      return false;
    }
    String inner;
    serializeJson(setObj, inner);
    if (!api_action_patch_one(idx, inner, err)) return false;
  }
  return true;
}
