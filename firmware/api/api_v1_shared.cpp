/*
 * Auto-split from api_v1_routes.cpp — see api_v1_common.h
 */
#include "api_v1_common.h"
#include "helio_config_daily_cap_logic.h"
#include "helio_diag.h"
#include "helio_triac_calibration_logic.h"
#include "metering/pmqtt_bindings.h"
void api_append_config_object(JsonObject o) {
  o["dhcp_on"] = (bool)(dhcpOn == 1);
  o["ip_fixed"] = ip32ToDotted(IP_Fixe);
  o["gateway"] = ip32ToDotted(Gateway);
  o["subnet_mask"] = ip32ToDotted(subnetMask);
  o["dns"] = ip32ToDotted(dns);
  o["source"] = Source;
  o["ext_peer_ip"] = ip32ToDotted(ext_peer_ip);
  o["ext_peer_port"] = ext_peer_port;
  o["ext_peer_path"] = ext_peer_path;
  o["ext_protocol"] = ext_peer_protocol.length() ? ext_peer_protocol : "json";
  o["enphase_user"] = EnphaseUser;
  o["enphase_password"] = EnphasePwd;
  o["meter_channel"] = meter_channel;
  o["enphase_serial"] = meter_channel;
  o["mqtt_repeat_sec"] = mqtt_publish_period_sec;
  o["mqtt_ip"] = ip32ToDotted(MQTTIP);
  o["mqtt_port"] = MQTTPort;
  o["mqtt_user"] = MQTTUser;
  o["mqtt_password"] = MQTTPwd;
  o["mqtt_prefix"] = MQTTPrefix;
  o["mqtt_device_name"] = MQTTdeviceName;
  o["router_name"] = routerName;
  o["probe_second_name"] = probeSecondName;
  o["probe_house_name"] = probeHouseName;
  o["temperature_label"] = temperatureSensorName;
  o["calib_u"] = CalibU;
  o["calib_i"] = CalibI;
  o["pmqtt_topic"] = PmqttTopic;
  o["pmqtt_schema"] = PmqttSchema;
  {
    DynamicJsonDocument bindsDoc(kPmqttBindingsDocCap);
    if (!deserializeJson(bindsDoc, PmqttBindingsJson)) {
      if (bindsDoc.is<JsonArray>()) {
        JsonArray outArr = o.createNestedArray("pmqtt_bindings");
        for (JsonVariantConst v : bindsDoc.as<JsonArrayConst>()) {
          outArr.add(v);
        }
      }
    }
  }
  o["uxix3_serial_baud"] = UxIx3SerialBaud;
  o["install_country"] = helio_mains_install_country();
  o["install_country_variant"] = helio_mains_install_variant();
  o["mains_nominal_v"] = helio_mains_nominal_v();
  o["mains_frequency_mode"] =
      (helio_mains_frequency_mode() == MainsFrequencyMode::Manual) ? "manual" : "auto";
  o["mains_frequency_hz_manual"] = helio_mains_frequency_hz_manual();
  o["mains_frequency_effective_hz"] = helio_mains_effective_frequency_hz();
  o["mains_frequency_source"] = helio_mains_frequency_source_string();
  const char *warn = helio_mains_frequency_warning_string();
  if (warn) {
    o["mains_frequency_warning"] = warn;
  } else {
    o["mains_frequency_warning"] = nullptr;
  }
  o["triac_override_max_temp_c"] = triacOverrideMaxTempC;
  o["http_cors_enabled"] = httpCorsEnabled;
  o["pwm_gpio"] = pwmGpio;
  o["pwm_mode"] = pwmMode.length() ? pwmMode : "off";
  o["pwm_duty_percent"] = pwmDutyPercent;
  o["pwm_inverted"] = pwmInverted;
  o["tempo_rte_enabled"] = tempoRteEnabled;
  o["expert_regulation_mode"] = expert_regulation_mode;
  o["regulation_gain"] = regulation_gain;
  o["triac_cal_enabled"] = g_triac_cal.enabled;
  JsonArray cal = o.createNestedArray("triac_calibration");
  for (int i = 0; i < 3; i++) {
    JsonObject p = cal.createNestedObject();
    p["duty_pct"] = g_triac_cal.points[i].duty_pct;
    p["measured_w"] = g_triac_cal.points[i].measured_w;
  }
  o["hunting_reversal_threshold"] = g_regulation_hunting_config.reversal_threshold;
  o["hunting_window_min"] = g_regulation_hunting_config.window_min;
  o["vacation_enabled"] = vacationEnabled;
  o["vacation_end_epoch"] = vacationEndEpoch;
  o["max_routed_w"] = maxRoutedW;
  o["mqtt_json_commands"] = mqttJsonCommands;
  o["triac_off_when_source_stale"] = triacOffWhenSourceStale;
  o["triac_backoff_when_heater_idle"] = triacBackoffWhenHeaterIdle;
  JsonArray caps = o.createNestedArray("action_daily_cap_wh");
  for (int i = 0; i < NbActions; i++) {
    caps.add(actionDailyCapWh[i]);
  }
}

bool config_apply_from_json(JsonObject root, bool fullPut, String &err) {
  const char *keys[] = {"dhcp_on", "ip_fixed", "gateway", "subnet_mask", "dns", "source", "ext_peer_ip",
                       "ext_peer_port", "ext_peer_path", "ext_protocol", "enphase_user", "enphase_password",
                       "meter_channel", "enphase_serial",
                       "mqtt_repeat_sec", "mqtt_ip", "mqtt_port", "mqtt_user", "mqtt_password", "mqtt_prefix",
                       "mqtt_device_name", "router_name", "probe_second_name", "probe_house_name",
                       "temperature_label", "calib_u", "calib_i", "pmqtt_topic", "pmqtt_schema",
                       "uxix3_serial_baud",
                       "install_country", "install_country_variant", "mains_nominal_v", "mains_frequency_mode",
                       "mains_frequency_hz_manual", "triac_override_max_temp_c", "http_cors_enabled"};
  const int nkeys = sizeof(keys) / sizeof(keys[0]);
  if (fullPut) {
    for (int i = 0; i < nkeys; i++) {
      if (!root.containsKey(keys[i])) {
        err = String("missing key ") + keys[i];
        return false;
      }
    }
  }
  auto applyIp = [&](const char *k, unsigned long &dst) -> bool {
    if (!root.containsKey(k)) return true;
    uint32_t v;
    const char *s = root[k].as<const char *>();
    if (!s || !dottedToIp32(s, v)) {
      err = String("bad ip for ") + k;
      return false;
    }
    dst = v;
    return true;
  };
  if (root.containsKey("dhcp_on")) dhcpOn = root["dhcp_on"].as<bool>() ? 1 : 0;
  if (!applyIp("ip_fixed", IP_Fixe)) return false;
  if (!applyIp("gateway", Gateway)) return false;
  if (!applyIp("subnet_mask", subnetMask)) return false;
  if (!applyIp("dns", dns)) return false;
  if (root.containsKey("source")) {
    Source = root["source"].as<String>();
    helio_active_source_refresh_from_global_string();
  }
  if (root.containsKey("ext_peer_ip")) {
    if (!applyIp("ext_peer_ip", ext_peer_ip)) return false;
  }
  if (root.containsKey("ext_peer_port")) {
    int p = (int)root["ext_peer_port"];
    if (p < 1 || p > 65535) {
      err = "ext_peer_port must be 1..65535";
      return false;
    }
    ext_peer_port = (unsigned int)p;
  }
  if (root.containsKey("ext_peer_path")) {
    String path = root["ext_peer_path"].as<String>();
    path.trim();
    if (path.length() == 0 || path[0] != '/') {
      err = "ext_peer_path must be non-empty and start with /";
      return false;
    }
    if (path.indexOf("..") >= 0) {
      err = "ext_peer_path invalid";
      return false;
    }
    if (path.length() > 48) {
      err = "ext_peer_path too long (max 48)";
      return false;
    }
    ext_peer_path = path;
  }
  if (root.containsKey("ext_protocol")) {
    String proto = root["ext_protocol"].as<String>();
    proto.trim();
    proto.toLowerCase();
    if (proto != "json") {
      err = "ext_protocol must be json";
      return false;
    }
    ext_peer_protocol = "json";
  }
  if (root.containsKey("enphase_user")) EnphaseUser = root["enphase_user"].as<String>();
  if (root.containsKey("enphase_password")) EnphasePwd = root["enphase_password"].as<String>();
  if (root.containsKey("meter_channel")) meter_channel = root["meter_channel"].as<String>();
  else if (root.containsKey("enphase_serial")) meter_channel = root["enphase_serial"].as<String>();
  if (root.containsKey("mqtt_repeat_sec")) mqtt_publish_period_sec = (unsigned int)(int)root["mqtt_repeat_sec"];
  if (!applyIp("mqtt_ip", MQTTIP)) return false;
  if (root.containsKey("mqtt_port")) MQTTPort = (unsigned int)(int)root["mqtt_port"];
  if (root.containsKey("mqtt_user")) MQTTUser = root["mqtt_user"].as<String>();
  if (root.containsKey("mqtt_password")) MQTTPwd = root["mqtt_password"].as<String>();
  if (root.containsKey("mqtt_prefix")) MQTTPrefix = root["mqtt_prefix"].as<String>();
  if (root.containsKey("mqtt_device_name")) {
    MQTTdeviceName = root["mqtt_device_name"].as<String>();
    helio_apply_default_mqtt_device_name(MQTTdeviceName);
  }
  if (root.containsKey("router_name")) routerName = root["router_name"].as<String>();
  if (root.containsKey("probe_second_name")) probeSecondName = root["probe_second_name"].as<String>();
  if (root.containsKey("probe_house_name")) probeHouseName = root["probe_house_name"].as<String>();
  if (root.containsKey("temperature_label")) temperatureSensorName = root["temperature_label"].as<String>();
  if (root.containsKey("calib_u")) CalibU = (unsigned int)(int)root["calib_u"];
  if (root.containsKey("calib_i")) CalibI = (unsigned int)(int)root["calib_i"];
  if (root.containsKey("pmqtt_topic")) PmqttTopic = root["pmqtt_topic"].as<String>();
  if (root.containsKey("pmqtt_schema")) PmqttSchema = root["pmqtt_schema"].as<String>();
  if (root.containsKey("pmqtt_bindings")) {
    if (!root["pmqtt_bindings"].is<JsonArray>()) {
      err = "pmqtt_bindings must be an array";
      return false;
    }
    JsonArray arr = root["pmqtt_bindings"].as<JsonArray>();
    if (arr.size() > 24) {
      err = "pmqtt_bindings max 24";
      return false;
    }
    DynamicJsonDocument bindsDoc(kPmqttBindingsDocCap);
    JsonArray out = bindsDoc.to<JsonArray>();
    for (JsonVariantConst v : arr) {
      if (!v.is<JsonObjectConst>()) {
        err = "pmqtt_bindings items must be objects";
        return false;
      }
      JsonObjectConst inObj = v.as<JsonObjectConst>();
      const char *metric = inObj["metric"] | "";
      const char *topic = inObj["topic"] | "";
      const char *format = inObj["format"] | "";
      const char *path = inObj["path"] | "";
      const bool enabled = inObj.containsKey("enabled") ? inObj["enabled"].as<bool>() : true;
      String metricS = String(metric);
      String topicS = String(topic);
      String formatS = String(format);
      String pathS = String(path);
      metricS.trim();
      topicS.trim();
      formatS.trim();
      formatS.toLowerCase();
      pathS.trim();
      if (metricS.length() == 0) {
        err = "pmqtt_bindings.metric required";
        return false;
      }
      if (topicS.length() == 0) {
        err = "pmqtt_bindings.topic required";
        return false;
      }
      if (topicS.length() > 128) {
        err = "pmqtt_bindings.topic too long";
        return false;
      }
      if (formatS != "plain" && formatS != "json" && formatS != "snapshot") {
        err = "pmqtt_bindings.format must be plain/json/snapshot";
        return false;
      }
      if (pathS.startsWith("$.") || pathS == "$") {
        pathS.remove(0, 1);
      }
      if (pathS.startsWith(".")) pathS.remove(0, 1);
      JsonObject o = out.createNestedObject();
      o["metric"] = metricS;
      o["topic"] = topicS;
      o["format"] = formatS;
      if (pathS.length() > 0) o["path"] = pathS;
      o["enabled"] = enabled;
    }
    String ser;
    serializeJson(out, ser);
    PmqttBindingsJson = ser;
  }
  if (root.containsKey("uxix3_serial_baud")) UxIx3SerialBaud = (uint32_t)root["uxix3_serial_baud"].as<unsigned long>();
  if (root.containsKey("install_country") || root.containsKey("install_country_variant")) {
    String cc = root.containsKey("install_country") ? root["install_country"].as<String>()
                                                    : String(helio_mains_install_country());
    String var = root.containsKey("install_country_variant")
                     ? root["install_country_variant"].as<String>()
                     : String(helio_mains_install_variant());
    cc.trim();
    cc.toUpperCase();
    if (cc.length() > 2) cc = cc.substring(0, 2);
    var.trim();
    char countryBuf[4] = "FR";
    char variantBuf[12] = "";
    cc.toCharArray(countryBuf, sizeof(countryBuf));
    var.toCharArray(variantBuf, sizeof(variantBuf));
    helio_mains_country_apply(countryBuf, variantBuf[0] ? variantBuf : nullptr);
  }
  if (root.containsKey("mains_frequency_mode")) {
    const char *fm = root["mains_frequency_mode"];
    helio_mains_set_frequency_mode((fm && strcmp(fm, "manual") == 0) ? MainsFrequencyMode::Manual
                                                                     : MainsFrequencyMode::Auto);
  }
  if (root.containsKey("mains_frequency_hz_manual")) {
    int hz = (int)root["mains_frequency_hz_manual"];
    helio_mains_set_frequency_hz_manual((uint8_t)((hz == 60) ? 60 : 50));
  }
  if (root.containsKey("triac_override_max_temp_c")) {
    int cap = (int)root["triac_override_max_temp_c"];
    triacOverrideMaxTempC = (cap < 0) ? 0 : (cap > 120 ? 120 : cap);
  }
  if (root.containsKey("http_cors_enabled")) {
    httpCorsEnabled = root["http_cors_enabled"].as<bool>();
  }
  if (root.containsKey("mains_nominal_v")) {
    helio_mains_nominal_v_set((uint16_t)(int)root["mains_nominal_v"]);
  }
  if (helio_mains_frequency_mode() == MainsFrequencyMode::Manual) {
    helio_mains_apply_effective_hz(helio_mains_frequency_hz_manual(), MainsFrequencySource::Manual);
  } else if (mains_frequency_hz >= 45.0f && mains_frequency_hz <= 65.0f) {
    helio_mains_on_meter_frequency(mains_frequency_hz);
  }
  if (Source != "Ext") {
    Source_data = Source;
  }
  if (root.containsKey("pwm_gpio")) {
    int g = (int)root["pwm_gpio"];
    std::string errStd;
    if (!helio_pwm_logic_validate_gpio(g, errStd)) {
      err = String(errStd.c_str());
      return false;
    }
    pwmGpio = g;
  }
  if (root.containsKey("pwm_mode")) {
    PwmMode mode;
    const char *ms = root["pwm_mode"].as<const char *>();
    std::string errStd;
    if (!helio_pwm_logic_parse_mode(ms, mode, errStd)) {
      err = String(errStd.c_str());
      return false;
    }
    pwmMode = String(ms);
  }
  if (root.containsKey("pwm_duty_percent")) {
    int d = (int)root["pwm_duty_percent"];
    if (d < 0 || d > 100) {
      err = "pwm_duty_percent must be 0..100";
      return false;
    }
    pwmDutyPercent = d;
  }
  if (root.containsKey("pwm_inverted")) pwmInverted = root["pwm_inverted"].as<bool>();
  if (root.containsKey("tempo_rte_enabled")) {
    const bool next = root["tempo_rte_enabled"].as<bool>();
    if (next && !tempoRteEnabled) {
      tempoRteLastPollDecihours = -1;
      tempoRteEnabled = next;
      tempo_rte_poll();
    } else {
      tempoRteEnabled = next;
    }
  }
  if (root.containsKey("expert_regulation_mode")) {
    int m = (int)root["expert_regulation_mode"];
    expert_regulation_mode = (m > 0) ? 1 : 0;
  }
  if (root.containsKey("regulation_gain")) {
    int r = (int)root["regulation_gain"];
    if (r < 1) r = 1;
    if (r > 99) r = 99;
    regulation_gain = static_cast<uint8_t>(r);
  }
  if (root.containsKey("triac_cal_enabled")) {
    g_triac_cal.enabled = root["triac_cal_enabled"].as<bool>();
  }
  if (root.containsKey("triac_calibration") && root["triac_calibration"].is<JsonArray>()) {
    JsonArray arr = root["triac_calibration"].as<JsonArray>();
    for (size_t i = 0; i < 3 && i < arr.size(); i++) {
      JsonObject p = arr[i];
      g_triac_cal.points[i].duty_pct = (uint8_t)(int)p["duty_pct"];
      g_triac_cal.points[i].measured_w = (uint16_t)(int)p["measured_w"];
    }
  }
  if (root.containsKey("hunting_reversal_threshold")) {
    int t = (int)root["hunting_reversal_threshold"];
    if (t >= 3 && t <= 30) g_regulation_hunting_config.reversal_threshold = (uint8_t)t;
  }
  if (root.containsKey("hunting_window_min")) {
    int w = (int)root["hunting_window_min"];
    if (w >= 2 && w <= 30) g_regulation_hunting_config.window_min = (uint16_t)w;
  }
  if (root.containsKey("vacation_enabled")) vacationEnabled = root["vacation_enabled"].as<bool>();
  if (root.containsKey("vacation_end_epoch")) {
    vacationEndEpoch = (uint32_t)root["vacation_end_epoch"].as<unsigned long>();
  }
  if (root.containsKey("max_routed_w")) {
    int mw = (int)root["max_routed_w"];
    if (mw < 0) mw = 0;
    if (mw > 65535) mw = 65535;
    maxRoutedW = static_cast<uint16_t>(mw);
  }
  if (root.containsKey("mqtt_json_commands")) mqttJsonCommands = root["mqtt_json_commands"].as<bool>();
  if (root.containsKey("triac_off_when_source_stale")) {
    triacOffWhenSourceStale = root["triac_off_when_source_stale"].as<bool>();
  }
  if (root.containsKey("triac_backoff_when_heater_idle")) {
    triacBackoffWhenHeaterIdle = root["triac_backoff_when_heater_idle"].as<bool>();
  }
  if (root.containsKey("action_daily_cap_wh") && root["action_daily_cap_wh"].is<JsonArray>()) {
    JsonArray arr = root["action_daily_cap_wh"].as<JsonArray>();
    uint32_t patch[20];
    const size_t n = arr.size() < 20u ? arr.size() : 20u;
    for (size_t i = 0; i < n; i++) {
      patch[i] = (uint32_t)arr[i].as<unsigned long>();
    }
    helio_config_daily_cap_apply(actionDailyCapWh, NbActions, patch, n);
  }
  if (Source == "Pmqtt" && PmqttBindingsJson.length() > 2) {
    String missing;
    if (!pmqtt_bindings_has_required_power(&missing)) {
      err = "pmqtt_missing_required_power";
      return false;
    }
  }
  return true;
}

bool ApiSetSource(const String &nextSource, bool persist, String &err) {
  if (!helio_source_wire_supported(nextSource)) {
    err = "unsupported source";
    return false;
  }
  Source = nextSource;
  helio_active_source_refresh_from_global_string();
  if (Source != "Ext") Source_data = Source;
  if (persist) persistConfigToEeprom();
  ApiMqttReconnect();
  return true;
}

const char *override_state_name(byte state) {
  if (state == ACTION_OVERRIDE_ON) return "on";
  if (state == ACTION_OVERRIDE_OFF) return "off";
  if (state == ACTION_OVERRIDE_TRIAC_FIXED) return "triac_fixed";
  return "auto";
}

byte override_state_from_name(const char *state) {
  if (!state) return 255;
  if (strcasecmp(state, "auto") == 0) return ACTION_OVERRIDE_AUTO;
  if (strcasecmp(state, "on") == 0) return ACTION_OVERRIDE_ON;
  if (strcasecmp(state, "off") == 0) return ACTION_OVERRIDE_OFF;
  if (strcasecmp(state, "triac_fixed") == 0) return ACTION_OVERRIDE_TRIAC_FIXED;
  return 255;
}

bool ApiSetActionOverride(int idx, const char *state, int triacPercent, unsigned long durationSec, String &err) {
  const ActionOverrideValidation v = actions_api_logic_validate_override(
      idx, kMaxRoutingActions, state, triacPercent, temperature, triacOverrideMaxTempC);
  if (!v.ok) {
    err = String(v.error.c_str());
    return false;
  }
  const byte st = v.override_state;
  if (st == ACTION_OVERRIDE_AUTO) {
    load_channels[idx].ClearOverride();
  } else {
    load_channels[idx].SetOverride(st, (byte)triacPercent, durationSec);
  }
  return true;
}

void api_append_action_override(JsonObject o, int idx) {
  if (idx < 0 || idx >= kMaxRoutingActions) return;
  if (load_channels[idx].OverrideExpired(millis())) load_channels[idx].ClearOverride();
  o["index"] = idx;
  o["state"] = override_state_name(load_channels[idx].OverrideState);
  o["triac_open_percent"] = load_channels[idx].OverrideTriacPercent;
  o["sticky"] = load_channels[idx].OverrideState != ACTION_OVERRIDE_AUTO && load_channels[idx].OverrideUntilMillis == 0;
  if (load_channels[idx].OverrideState != ACTION_OVERRIDE_AUTO && load_channels[idx].OverrideUntilMillis != 0) {
    long msLeft = (long)(load_channels[idx].OverrideUntilMillis - millis());
    o["expires_in_s"] = msLeft > 0 ? (int)(msLeft / 1000) : 0;
  } else {
    o["expires_in_s"] = 0;
  }
}

void api_append_measurements_object(JsonObject doc) {
  HelioPublic r = HelioReadSnapshot();
  doc["date_valid"] = time_sync_valid;
  doc["date"] = time_sync_valid ? sync_clock_str : "Waiting for time sync (Internet)";
  doc["source"] = Source_data;
  const int grid_net_w = r.house_active_import_w - r.house_active_export_w;
  doc["house"]["active_import_w"] = r.house_active_import_w;
  doc["house"]["active_export_w"] = r.house_active_export_w;
  doc["house"]["grid_net_w"] = grid_net_w;
  doc["house"]["house_load_w"] = r.house_active_import_w;
  doc["house"]["pv_production_w"] = r.house_active_export_w;
  doc["house"]["apparent_import_va"] = r.house_apparent_import_va;
  doc["house"]["apparent_export_va"] = r.house_apparent_export_va;
  doc["house"]["energy_day_import_wh"] = r.house_day_energy_import_wh;
  doc["house"]["energy_day_export_wh"] = r.house_day_energy_export_wh;
  doc["house"]["energy_total_import_wh"] = r.house_energy_import_wh;
  doc["house"]["energy_total_export_wh"] = r.house_energy_export_wh;
  doc["second"]["active_import_w"] = r.second_active_import_w;
  doc["second"]["active_export_w"] = r.second_active_export_w;
  doc["second"]["apparent_import_va"] = r.second_apparent_import_va;
  doc["second"]["apparent_export_va"] = r.second_apparent_export_va;
  doc["second"]["energy_day_import_wh"] = r.second_day_energy_import_wh;
  doc["second"]["energy_day_export_wh"] = r.second_day_energy_export_wh;
  doc["second"]["energy_total_import_wh"] = r.second_energy_import_wh;
  doc["second"]["energy_total_export_wh"] = r.second_energy_export_wh;
  JsonObject raw = doc.createNestedObject("raw_meter");
  raw["voltage_house_v"] = r.house_voltage_v;
  raw["current_house_a"] = r.house_current_a;
  raw["pf_house"] = r.house_power_factor;
  raw["voltage_second_v"] = r.second_voltage_v;
  raw["current_second_a"] = r.second_current_a;
  raw["pf_second"] = r.second_power_factor;
  raw["freq_hz"] = r.mains_frequency_hz;
  raw["house_net_power_w"] = r.house_active_import_w - r.house_active_export_w;
  raw["second_net_power_w"] = r.second_active_import_w - r.second_active_export_w;
  if (helio_cap_mqtt_linky_tariff() && LTARF.length() > 0) {
    doc["linky_tariff"] = LTARF;
  }
  JsonObject diag = doc.createNestedObject("diagnostics");
  if (helio_diag_uxi_adc_clipping_active()) {
    diag["adc_clipping"] = true;
  }
  if (g_regulation_hunting_active) {
    diag["regulation_hunting"] = true;
  }
}
