/*
 * Auto-split from api_v1_routes.cpp — see api_v1_common.h
 */
#include "api_v1_common.h"
void handle_get_history_power() {
  API_AUTH_GUARD();
  String w = server.arg("window");
  if (w.length() == 0) w = "48h";
  int maxPts = server.hasArg("max_points") ? server.arg("max_points").toInt() : kHistDefaultMax;
  if (maxPts < 1) maxPts = kHistDefaultMax;
  if (maxPts > kHistAbsMax) maxPts = kHistAbsMax;
  DynamicJsonDocument doc(kHistPowerJsonCap);
  if (doc.capacity() == 0) {
    api_error(server, 503, "json_buffer", "history power JSON allocation failed");
    return;
  }
  doc["source"] = Source_data;
  doc["window"] = w;
  doc["max_points"] = maxPts;
  if (w == "10m") {
    doc["sample_period_s"] = 2;
    doc["temperature_now_c"] = temperature;
    int iS = IdxStock2s;
    JsonArray hm = doc.createNestedArray("house_active_w");
    JsonArray hmva = doc.createNestedArray("house_apparent_va");
    JsonArray ht = doc.createNestedArray("triac_active_w");
    JsonArray htva = doc.createNestedArray("triac_apparent_va");
    int step = (300 + maxPts - 1) / maxPts;
    if (step < 1) step = 1;
    int count = 0;
    for (int i = 0; i < 300 && count < maxPts; i += step) {
      int j = (iS + i) % 300;
      hm.add(tabPwHouse_2s[j]);
      hmva.add(tabPvaHouse_2s[j]);
      ht.add(tabPw_Triac_2s[j]);
      htva.add(tabPva_Triac_2s[j]);
      count++;
    }
  } else {
    doc["sample_period_s"] = 300;
    int iS = IdxStockPW;
    JsonArray hm = doc.createNestedArray("house_active_w");
    JsonArray ht = doc.createNestedArray("triac_active_w");
    JsonArray tt = doc.createNestedArray("temperature_series_c");
    int step = (600 + maxPts - 1) / maxPts;
    if (step < 1) step = 1;
    int count = 0;
    for (int i = 0; i < 600 && count < maxPts; i += step) {
      int j = (iS + i) % 600;
      hm.add(tabPwHouse_5mn[j]);
      ht.add(tabPw_Triac_5mn[j]);
      tt.add(tabTemperature_5mn[j]);
      count++;
    }
  }
  if (doc.overflowed()) {
    api_error(server, 503, "json_buffer", "history power JSON overflow");
    return;
  }
  String out;
  serializeJson(doc, out);
  if (out.length() < 3) {
    api_error(server, 503, "json_buffer", "history power JSON serialize failed");
    return;
  }
  api_send_json(server,200, out);
}

void handle_get_history_energy_daily() {
  API_AUTH_GUARD();
  String raw = eepromFormatYearlyEnergyHistory();
  DynamicJsonDocument doc(8192);
  JsonArray arr = doc.createNestedArray("delta_wh_per_day");
  int start = 0;
  while (start < (int)raw.length()) {
    int c = raw.indexOf(',', start);
    if (c < 0) break;
    long v = raw.substring(start, c).toInt();
    arr.add(v);
    start = c + 1;
  }
  doc["count"] = arr.size();
  doc["idx_prom_du_jour"] = idxPromDuJour;
  if (time_sync_valid) {
    time_t now = time(NULL);
    struct tm dTm;
#if defined(ESP_PLATFORM)
    localtime_r(&now, &dTm);
#else
    struct tm *pTime = localtime(&now);
    if (pTime) dTm = *pTime;
#endif
    char isoDay[16];
    strftime(isoDay, sizeof(isoDay), "%Y-%m-%d", &dTm);
    doc["reference_date_iso"] = isoDay;
  }
  String out;
  serializeJson(doc, out);
  api_send_json(server,200, out);
}

void handle_put_gpio() {
  API_AUTH_GUARD();
  String body;
  if (!api_require_json_body(server, body, 256, false)) return;
  StaticJsonDocument<128> doc;
  DeserializationError er = deserializeJson(doc, body);
  if (er) {
    api_error(server,400, "json_parse", er.c_str());
    return;
  }
  int gpio = (int)(doc["gpio"] | -1);
  int level = (int)(doc["level"] | -1);
  if (IsRestrictedGpioWrite(gpio) || level < 0 || level > 1) {
    api_error(server,400, "validation", "gpio forbidden/reserved or invalid level");
    return;
  }
  pinMode(gpio, OUTPUT);
  digitalWrite(gpio, level);
  StaticJsonDocument<64> o;
  o["ok"] = true;
  o["gpio"] = gpio;
  o["level"] = level;
  String s;
  serializeJson(o, s);
  api_send_json(server,200, s);
}

void handle_get_pwm() {
  API_AUTH_GUARD();
  StaticJsonDocument<256> doc;
  doc["pwm_gpio"] = pwmGpio;
  doc["pwm_mode"] = pwmMode.length() ? pwmMode : "off";
  doc["pwm_duty_percent"] = pwmDutyPercent;
  doc["pwm_inverted"] = pwmInverted;
  PwmConfig pwmCfg;
  pwmCfg.gpio = pwmGpio;
  if (pwmMode == "follow_triac") {
    pwmCfg.mode = PwmMode::FollowTriac;
  } else if (pwmMode == "independent") {
    pwmCfg.mode = PwmMode::Independent;
  } else {
    pwmCfg.mode = PwmMode::Off;
  }
  pwmCfg.duty_percent = pwmDutyPercent;
  pwmCfg.inverted = pwmInverted;
  doc["effective_duty_percent"] = helio_pwm_logic_effective_duty(pwmCfg, TriacGetOpenPercent());
  String out;
  serializeJson(doc, out);
  api_send_json(server, 200, out);
}

void handle_put_pwm() {
  API_AUTH_GUARD();
  String body;
  if (!api_require_json_body(server, body, 512, false)) return;
  StaticJsonDocument<256> doc;
  DeserializationError er = deserializeJson(doc, body);
  if (er) {
    api_error(server, 400, "json_parse", er.c_str());
    return;
  }
  std::string errStd;
  if (doc.containsKey("pwm_gpio")) {
    int g = (int)doc["pwm_gpio"];
    if (!helio_pwm_logic_validate_gpio(g, errStd)) {
      api_error(server, 400, "validation", errStd.c_str());
      return;
    }
    pwmGpio = g;
  }
  if (doc.containsKey("pwm_mode")) {
    PwmMode mode;
    const char *ms = doc["pwm_mode"].as<const char *>();
    if (!helio_pwm_logic_parse_mode(ms, mode, errStd)) {
      api_error(server, 400, "validation", errStd.c_str());
      return;
    }
    pwmMode = ms;
  }
  if (doc.containsKey("pwm_duty_percent")) {
    int d = (int)doc["pwm_duty_percent"];
    if (d < 0 || d > 100) {
      api_error(server, 400, "validation", "pwm_duty_percent must be 0..100");
      return;
    }
    pwmDutyPercent = d;
  }
  if (doc.containsKey("pwm_inverted")) pwmInverted = doc["pwm_inverted"].as<bool>();
  helio_pwm_hw_reinit();
  persistConfigToEeprom();
  handle_get_pwm();
}

