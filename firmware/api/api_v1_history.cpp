/*
 * Auto-split from api_v1_routes.cpp — see api_v1_common.h
 */
#include "api_v1_common.h"
#include <cstring>
#include <vector>

namespace {

void copy_date_iso_field(const String &src, char *dst, size_t cap) {
  if (cap < 11) return;
  dst[0] = '\0';
  if (src.length() == 10) {
    memcpy(dst, src.c_str(), 10);
    dst[10] = '\0';
  }
}

void sanitize_daily_metrics(long &ch1ImportWh, long &ch1ExportWh, long &ch2ImportWh, long &ch2ExportWh) {
  if (ch1ImportWh < 0) ch1ImportWh = 0;
  if (ch1ExportWh < 0) ch1ExportWh = 0;
  if (ch2ImportWh < 0) ch2ImportWh = 0;
  if (ch2ExportWh < 0) ch2ExportWh = 0;
}

bool parse_iso_date_ymd(const String &iso, int &yyyymmdd) {
  if (iso.length() != 10 || iso.charAt(4) != '-' || iso.charAt(7) != '-') return false;
  const int y = iso.substring(0, 4).toInt();
  const int m = iso.substring(5, 7).toInt();
  const int d = iso.substring(8, 10).toInt();
  if (y < 1970 || m < 1 || m > 12 || d < 1 || d > 31) return false;
  yyyymmdd = y * 10000 + m * 100 + d;
  return true;
}

bool logical_day_iso(int logicalIdx, int cap, const String &referenceIso, char *isoOut, size_t isoCap) {
  if (logicalIdx < 0 || logicalIdx >= cap || referenceIso.length() != 10 || isoCap < 11) return false;
  struct tm tmRef = {};
  tmRef.tm_year = referenceIso.substring(0, 4).toInt() - 1900;
  tmRef.tm_mon = referenceIso.substring(5, 7).toInt() - 1;
  tmRef.tm_mday = referenceIso.substring(8, 10).toInt();
  time_t tRef = mktime(&tmRef);
  if (tRef <= 0) return false;
  const int daysAgo = (cap - 1) - logicalIdx;
  time_t t = tRef - static_cast<time_t>(daysAgo) * 24 * 3600;
  struct tm tmDay;
#if defined(ESP_PLATFORM)
  localtime_r(&t, &tmDay);
#else
  struct tm *pt = localtime(&t);
  if (!pt) return false;
  tmDay = *pt;
#endif
  strftime(isoOut, isoCap, "%Y-%m-%d", &tmDay);
  return isoOut[0] != '\0';
}

bool parse_history_csv(
    const String &csv,
    std::vector<long> &ch1ImportOut,
    std::vector<long> &ch1ExportOut,
    std::vector<long> &ch2ImportOut,
    std::vector<long> &ch2ExportOut,
    char *latestDateIso,
    size_t latestDateCap,
    String &err,
    int &invalidRows) {
  ch1ImportOut.clear();
  ch1ExportOut.clear();
  ch2ImportOut.clear();
  ch2ExportOut.clear();
  if (latestDateCap >= 11) latestDateIso[0] = '\0';
  invalidRows = 0;
  int pos = 0;
  bool headerChecked = false;
  while (pos <= (int)csv.length()) {
    const int nl = csv.indexOf('\n', pos);
    String line = nl < 0 ? csv.substring(pos) : csv.substring(pos, nl);
    line.trim();
    pos = nl < 0 ? (csv.length() + 1) : (nl + 1);
    if (line.length() == 0) continue;
    if (line.startsWith("#")) continue;
    if (!headerChecked) {
      headerChecked = true;
      if (line.startsWith("date_iso,") || line.startsWith("ring_index,")) continue;
    }
    String fields[16];
    int fieldCount = 0;
    int start = 0;
    for (;;) {
      int comma = line.indexOf(',', start);
      if (comma < 0) {
        if (fieldCount < 16) fields[fieldCount++] = line.substring(start);
        break;
      }
      if (fieldCount < 16) fields[fieldCount++] = line.substring(start, comma);
      start = comma + 1;
    }
    if (fieldCount != 5 && fieldCount != 7 && fieldCount != 13) {
      invalidRows++;
      continue;
    }
    char dateIso[11];
    long ch1ImportWh = 0;
    long ch1ExportWh = 0;
    long ch2ImportWh = 0;
    long ch2ExportWh = 0;
    // Supported import formats:
    // - compact5: date_iso,ch1_import_wh,ch1_export_wh,ch2_import_wh,ch2_export_wh
    // - compact7 (legacy): total columns ignored; import/export only
    // - full export: ring_index,date_iso,...,house_import/export,second_import/export,...,has_data
    if (fieldCount == 13) {
      const String hasData = fields[12];
      if (!(hasData.equalsIgnoreCase("true") || hasData == "1")) continue;
      copy_date_iso_field(fields[1], dateIso, sizeof(dateIso));
      ch1ImportWh = fields[3].toInt();
      ch1ExportWh = fields[4].toInt();
      ch2ImportWh = fields[6].toInt();
      ch2ExportWh = fields[7].toInt();
    } else if (fieldCount == 7) {
      copy_date_iso_field(fields[0], dateIso, sizeof(dateIso));
      ch1ImportWh = fields[1].toInt();
      ch1ExportWh = fields[2].toInt();
      ch2ImportWh = fields[4].toInt();
      ch2ExportWh = fields[5].toInt();
    } else {
      copy_date_iso_field(fields[0], dateIso, sizeof(dateIso));
      ch1ImportWh = fields[1].toInt();
      ch1ExportWh = fields[2].toInt();
      ch2ImportWh = fields[3].toInt();
      ch2ExportWh = fields[4].toInt();
    }
    int parsedDate = 0;
    if (!parse_iso_date_ymd(String(dateIso), parsedDate)) {
      invalidRows++;
      continue;
    }
    if (ch1ImportWh < 0 || ch1ExportWh < 0 || ch2ImportWh < 0 || ch2ExportWh < 0) {
      invalidRows++;
      continue;
    }
    sanitize_daily_metrics(ch1ImportWh, ch1ExportWh, ch2ImportWh, ch2ExportWh);
    ch1ImportOut.push_back(ch1ImportWh);
    ch1ExportOut.push_back(ch1ExportWh);
    ch2ImportOut.push_back(ch2ImportWh);
    ch2ExportOut.push_back(ch2ExportWh);
    if (latestDateCap >= 11) {
      memcpy(latestDateIso, dateIso, 11);
    }
    delay(0);
  }
  if (ch1ImportOut.empty()) {
    err = "csv contains no valid history rows";
    return false;
  }
  return true;
}
} // namespace
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
  const int cap = eepromHistoryDaysCapacity();
  int limit = cap;
  if (server.hasArg("limit")) {
    limit = server.arg("limit").toInt();
    if (limit <= 0) limit = kHistoryDailyPageMax;
  }
  if (limit > kHistoryDailyPageMax) limit = kHistoryDailyPageMax;
  if (limit > cap) limit = cap;
  int offset = server.hasArg("offset") ? server.arg("offset").toInt() : 0;
  if (offset < 0) offset = 0;
  if (offset > cap) offset = cap;
  String referenceIso;
  const bool hasRefIso = eepromHistoryReferenceDateIso(referenceIso);
  int fromYmd = 0;
  int toYmd = 0;
  const bool hasFrom = server.hasArg("from_date") && parse_iso_date_ymd(server.arg("from_date"), fromYmd);
  const bool hasTo = server.hasArg("to_date") && parse_iso_date_ymd(server.arg("to_date"), toYmd);
  if (server.hasArg("from_date") && !hasFrom) {
    api_error(server, 400, "validation", "from_date must be YYYY-MM-DD");
    return;
  }
  if (server.hasArg("to_date") && !hasTo) {
    api_error(server, 400, "validation", "to_date must be YYYY-MM-DD");
    return;
  }

  int matched = 0;
  for (int logicalIdx = 0; logicalIdx < cap; logicalIdx++) {
    long ch1Import = 0, ch1Export = 0, ch2Import = 0, ch2Export = 0;
    if (!eepromHistoryReadDailyMetrics(logicalIdx, ch1Import, ch1Export, ch2Import, ch2Export)) {
      continue;
    }
    char dayIsoBuf[16] = "";
    if (!hasRefIso || !logical_day_iso(logicalIdx, cap, referenceIso, dayIsoBuf, sizeof(dayIsoBuf))) {
      dayIsoBuf[0] = '\0';
    }
    if (hasRefIso && dayIsoBuf[0] != '\0' && strcmp(dayIsoBuf, referenceIso.c_str()) == 0) {
      ch1Import = static_cast<long>(house_day_energy_import_wh);
      ch1Export = static_cast<long>(house_day_energy_export_wh);
      ch2Import = static_cast<long>(second_day_energy_import_wh);
      ch2Export = static_cast<long>(second_day_energy_export_wh);
    }
    sanitize_daily_metrics(ch1Import, ch1Export, ch2Import, ch2Export);
    int ymd = 0;
    const bool hasYmd = parse_iso_date_ymd(String(dayIsoBuf), ymd);
    if (hasFrom && hasYmd && ymd < fromYmd) continue;
    if (hasTo && hasYmd && ymd > toYmd) continue;
    matched++;
  }

  size_t jsonCap = 512 + static_cast<size_t>(limit) * kHistEnergyDailyJsonPerRow;
  if (jsonCap > kHistEnergyDailyJsonCap) jsonCap = kHistEnergyDailyJsonCap;
  DynamicJsonDocument doc(jsonCap);
  JsonArray arr = doc.createNestedArray("delta_wh_per_day");
  JsonArray importArr = doc.createNestedArray("import_wh_per_day");
  JsonArray exportArr = doc.createNestedArray("export_wh_per_day");
  JsonArray dayIsoArr = doc.createNestedArray("day_dates_iso");
  JsonArray ch1ImportArr = doc.createNestedArray("ch1_import_wh_per_day");
  JsonArray ch1ExportArr = doc.createNestedArray("ch1_export_wh_per_day");
  JsonArray ch2ImportArr = doc.createNestedArray("ch2_import_wh_per_day");
  JsonArray ch2ExportArr = doc.createNestedArray("ch2_export_wh_per_day");

  int emitted = 0;
  matched = 0;
  for (int logicalIdx = 0; logicalIdx < cap; logicalIdx++) {
    long ch1Import = 0, ch1Export = 0, ch2Import = 0, ch2Export = 0;
    if (!eepromHistoryReadDailyMetrics(logicalIdx, ch1Import, ch1Export, ch2Import, ch2Export)) {
      continue;
    }
    char dayIsoBuf[16] = "";
    if (!hasRefIso || !logical_day_iso(logicalIdx, cap, referenceIso, dayIsoBuf, sizeof(dayIsoBuf))) {
      dayIsoBuf[0] = '\0';
    }
    // "Today" must reflect the live day counters, not imported/archived slot values.
    if (hasRefIso && dayIsoBuf[0] != '\0' && strcmp(dayIsoBuf, referenceIso.c_str()) == 0) {
      ch1Import = static_cast<long>(house_day_energy_import_wh);
      ch1Export = static_cast<long>(house_day_energy_export_wh);
      ch2Import = static_cast<long>(second_day_energy_import_wh);
      ch2Export = static_cast<long>(second_day_energy_export_wh);
    }
    sanitize_daily_metrics(ch1Import, ch1Export, ch2Import, ch2Export);
    int ymd = 0;
    const bool hasYmd = parse_iso_date_ymd(String(dayIsoBuf), ymd);
    if (hasFrom && hasYmd && ymd < fromYmd) continue;
    if (hasTo && hasYmd && ymd > toYmd) continue;
    const int matchIndex = matched++;
    if (matchIndex < offset) continue;
    if (emitted >= limit) continue;
    const long delta = ch1Import - ch1Export;
    arr.add(delta);
    importArr.add(ch1Import);
    exportArr.add(ch1Export);
    dayIsoArr.add(dayIsoBuf);
    ch1ImportArr.add(ch1Import);
    ch1ExportArr.add(ch1Export);
    ch2ImportArr.add(ch2Import);
    ch2ExportArr.add(ch2Export);
    emitted++;
    delay(0);
  }
  doc["count"] = emitted;
  doc["total_count"] = matched;
  doc["offset"] = offset;
  doc["limit"] = limit;
  doc["has_more"] = (offset + emitted) < matched;
  doc["days_capacity"] = cap;
  doc["days_retained"] = eepromHistoryDaysRetained();
  doc["idx_prom_du_jour"] = idxPromDuJour;
  doc["pending_commit"] = eepromHistoryHasPendingCommit();
  if (hasRefIso) doc["reference_date_iso"] = referenceIso;
  String out;
  serializeJson(doc, out);
  if (out.length() < 3 || doc.overflowed()) {
    api_error(server, 503, "json_buffer", "history energy daily JSON serialize failed");
    return;
  }
  api_send_json(server, 200, out);
}

void handle_post_history_energy_daily_import() {
  API_AUTH_GUARD();
  if (!server.hasArg("plain")) {
    api_error(server, 400, "bad_request", "CSV body required");
    return;
  }
  String body = server.arg("plain");
  if (body.length() == 0) {
    api_error(server, 400, "validation", "empty import body");
    return;
  }
  if (body.length() > kHistoryDailyImportBodyMax) {
    api_error(server, 413, "payload_too_large", "history CSV exceeds size limit");
    return;
  }
  String csv = body;
  // Optional JSON envelope for clients that cannot send raw text/csv body:
  // {"csv":"date_iso,...\\n..."}.
  body.trim();
  if (body.startsWith("{")) {
    DynamicJsonDocument env(4096);
    if (deserializeJson(env, body)) {
      api_error(server, 400, "validation", "invalid JSON import envelope");
      return;
    }
    if (!env.containsKey("csv")) {
      api_error(server, 400, "validation", "missing csv field in import envelope");
      return;
    }
    csv = env["csv"].as<String>();
    if (csv.length() == 0) {
      api_error(server, 400, "validation", "empty csv field in import envelope");
      return;
    }
  }
  std::vector<long> ch1Import;
  std::vector<long> ch1Export;
  std::vector<long> ch2Import;
  std::vector<long> ch2Export;
  char latestDateIso[11] = "";
  String err;
  int invalidRows = 0;
  if (!parse_history_csv(
          csv, ch1Import, ch1Export, ch2Import, ch2Export, latestDateIso, sizeof(latestDateIso), err, invalidRows)) {
    api_error(server, 400, "validation", err.c_str());
    return;
  }
  if ((int)ch1Import.size() > eepromHistoryDaysCapacity()) {
    api_error(server, 400, "validation", "too many rows for retained history window");
    return;
  }
  if (!eepromHistoryImportDailyMetrics(
          ch1Import.data(),
          ch1Export.data(),
          ch2Import.data(),
          ch2Export.data(),
          static_cast<int>(ch1Import.size()),
          latestDateIso,
          err)) {
    api_error(server, 400, "validation", err.c_str());
    return;
  }
  const size_t importedRows = ch1Import.size();
  ch1Import.clear();
  ch1Import.shrink_to_fit();
  ch1Export.clear();
  ch1Export.shrink_to_fit();
  ch2Import.clear();
  ch2Import.shrink_to_fit();
  ch2Export.clear();
  ch2Export.shrink_to_fit();
  csv = String();
  body = String();
  eepromHistoryServicePendingCommit();
  for (int flushPass = 0; flushPass < 3; flushPass++) {
    delay(0);
    server.handleClient();
    eepromHistoryServicePendingCommit();
  }
  DynamicJsonDocument outDoc(512);
  outDoc["ok"] = true;
  outDoc["imported_rows"] = importedRows;
  outDoc["invalid_rows"] = invalidRows;
  outDoc["days_capacity"] = eepromHistoryDaysCapacity();
  outDoc["days_retained"] = eepromHistoryDaysRetained();
  outDoc["pending_commit"] = eepromHistoryHasPendingCommit();
  String out;
  serializeJson(outDoc, out);
  api_send_json(server, 200, out);
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

