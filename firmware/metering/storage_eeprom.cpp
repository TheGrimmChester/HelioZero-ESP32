#include <Arduino.h>
#include "helio_globals.h"
#include "helio_mains_profile.h"
#include "storage_eeprom_arduino_backend.h"
#include "storage_eeprom_extension.h"
#include "storage_eeprom_layout.h"
#include "helio_regulation_modes.h"
#include "tempo_rte_logic.h"
#include "actions_api.h"
#include "api_access_token.h"
#include "helio_source_logic.h"
#include <EEPROM.h>
#include <Preferences.h>
#include <string>

/*
 * storage_eeprom.cpp — Load/save all persistent settings (Arduino EEPROM emulation).
 * Fixed regions: see storage_eeprom_layout.h; extension tail via storage_eeprom_extension.cpp.
 * eeprom_layout_key mismatch triggers factory-style re-init (kEepromLayoutInit in helio_board.h).
 * See: /en/project-overview/ § Persistence.
 */
// --- Fixed adr_* aliases (must match storage_eeprom_layout.h) ---
#define EEPROM_SIZE kEepromSize
#define NbJour kEepromNbJour
#define adr_HistoAn kEepromAdrHistoAn
#define kEepromAdrTriacImportJ0 kEepromAdrTriacImportJ0
#define adr_E_T_injecte0 kEepromAdrETinjecte0
#define kEepromAdrHouseImportJ0 kEepromAdrHouseImportJ0
#define adr_E_M_injecte0 kEepromAdrEMinjecte0
#define adr_currentDateStr kEepromAdrcurrentDateStr
#define adr_lastStockConso kEepromAdrLastStockConso
#define adr_ParaActions kEepromAdrParaActions

namespace {
constexpr int kHistoryDaysRetained = 90;
constexpr int kDailyMetricSlotBytes = 16; // 4 x int32 (CH1/CH2 import+export)
constexpr int kDailyMetricFieldBytes = 4;
constexpr const char *kHistoryNvsNamespace = "histdaily";
constexpr const char *kHistoryNvsIndexKey = "idx";
constexpr const char *kHistoryNvsBlobKey = "blob";

struct DailyMetricsSlot {
  int32_t ch1ImportWh;
  int32_t ch1ExportWh;
  int32_t ch2ImportWh;
  int32_t ch2ExportWh;
};

struct HistoryRingCache {
  std::vector<DailyMetricsSlot> slots;
  bool valid = false;
};

HistoryRingCache g_historyRingCache;
bool g_historyNvsPendingCommit = false;

constexpr uint32_t kHistoryNvsCommitMinFreeHeap = 18000;

bool history_use_nvs();
bool history_nvs_save_blob(const std::vector<DailyMetricsSlot> &slots);

void history_mark_nvs_pending() {
  if (history_use_nvs()) g_historyNvsPendingCommit = true;
}

bool history_commit_pending_nvs() {
  if (!g_historyNvsPendingCommit || !history_use_nvs()) return true;
  if (!g_historyRingCache.valid) {
    g_historyNvsPendingCommit = false;
    return true;
  }
  if (ESP.getFreeHeap() < kHistoryNvsCommitMinFreeHeap) return false;
  if (!history_nvs_save_blob(g_historyRingCache.slots)) return false;
  g_historyNvsPendingCommit = false;
  return true;
}

int history_max_by_eeprom_region() {
  return (kEepromAdrTriacImportJ0 - adr_HistoAn) / kDailyMetricSlotBytes;
}

bool history_use_nvs() { return kHistoryDaysRetained > history_max_by_eeprom_region(); }

int history_days_capacity() {
  if (history_use_nvs()) return kHistoryDaysRetained;
  const int maxByRegion = history_max_by_eeprom_region();
  return maxByRegion < kHistoryDaysRetained ? maxByRegion : kHistoryDaysRetained;
}

int metric_addr_for(int ringIndex, int fieldIdx) {
  return adr_HistoAn + ringIndex * kDailyMetricSlotBytes + fieldIdx * kDailyMetricFieldBytes;
}

bool history_nvs_load_blob(std::vector<DailyMetricsSlot> &slotsOut) {
  const int cap = history_days_capacity();
  slotsOut.assign(cap, DailyMetricsSlot{});
  Preferences prefs;
  if (!prefs.begin(kHistoryNvsNamespace, true)) return false;
  const size_t expected = static_cast<size_t>(cap) * sizeof(DailyMetricsSlot);
  const size_t got = prefs.getBytes(kHistoryNvsBlobKey, slotsOut.data(), expected);
  prefs.end();
  if (got != expected) {
    for (int i = 0; i < cap; i++) slotsOut[i] = DailyMetricsSlot{};
  }
  return true;
}

bool history_nvs_save_blob(const std::vector<DailyMetricsSlot> &slots) {
  Preferences prefs;
  if (!prefs.begin(kHistoryNvsNamespace, false)) return false;
  const size_t wrote = prefs.putBytes(kHistoryNvsBlobKey, slots.data(), slots.size() * sizeof(DailyMetricsSlot));
  prefs.end();
  return wrote == slots.size() * sizeof(DailyMetricsSlot);
}

bool history_nvs_clear_slots(int cap) {
  Preferences prefs;
  if (!prefs.begin(kHistoryNvsNamespace, false)) return false;
  (void)cap;
  // Full namespace wipe drops legacy per-slot keys from older layouts.
  prefs.clear();
  prefs.end();
  return true;
}

void history_ring_cache_invalidate() { g_historyRingCache.valid = false; }

bool history_ring_cache_ensure() {
  if (!history_use_nvs()) return false;
  if (g_historyRingCache.valid) return true;
  g_historyRingCache.valid = history_nvs_load_blob(g_historyRingCache.slots);
  return g_historyRingCache.valid;
}

void history_read_slot(int ringIndex, long &ch1ImportWh, long &ch1ExportWh, long &ch2ImportWh, long &ch2ExportWh) {
  if (history_use_nvs()) {
    if (!history_ring_cache_ensure() || ringIndex < 0 ||
        ringIndex >= static_cast<int>(g_historyRingCache.slots.size())) {
      ch1ImportWh = ch1ExportWh = ch2ImportWh = ch2ExportWh = 0;
      return;
    }
    const DailyMetricsSlot &slot = g_historyRingCache.slots[ringIndex];
    ch1ImportWh = slot.ch1ImportWh;
    ch1ExportWh = slot.ch1ExportWh;
    ch2ImportWh = slot.ch2ImportWh;
    ch2ExportWh = slot.ch2ExportWh;
    return;
  }
  ch1ImportWh = EEPROM.readLong(metric_addr_for(ringIndex, 0));
  ch1ExportWh = EEPROM.readLong(metric_addr_for(ringIndex, 1));
  ch2ImportWh = EEPROM.readLong(metric_addr_for(ringIndex, 2));
  ch2ExportWh = EEPROM.readLong(metric_addr_for(ringIndex, 3));
}

void history_write_slot(int ringIndex, long ch1ImportWh, long ch1ExportWh, long ch2ImportWh, long ch2ExportWh) {
  if (history_use_nvs()) {
    if (!history_ring_cache_ensure() || ringIndex < 0 || ringIndex >= static_cast<int>(g_historyRingCache.slots.size())) {
      return;
    }
    DailyMetricsSlot slot = {static_cast<int32_t>(ch1ImportWh), static_cast<int32_t>(ch1ExportWh),
                             static_cast<int32_t>(ch2ImportWh), static_cast<int32_t>(ch2ExportWh)};
    g_historyRingCache.slots[ringIndex] = slot;
    g_historyRingCache.valid = true;
    history_mark_nvs_pending();
    return;
  }
  EEPROM.writeLong(metric_addr_for(ringIndex, 0), ch1ImportWh);
  EEPROM.writeLong(metric_addr_for(ringIndex, 1), ch1ExportWh);
  EEPROM.writeLong(metric_addr_for(ringIndex, 2), ch2ImportWh);
  EEPROM.writeLong(metric_addr_for(ringIndex, 3), ch2ExportWh);
}

bool parse_ddmmyyyy_to_iso(const String &ddmmyyyy, String &isoOut) {
  if (ddmmyyyy.length() != 8) return false;
  const int d = ddmmyyyy.substring(0, 2).toInt();
  const int m = ddmmyyyy.substring(2, 4).toInt();
  const int y = ddmmyyyy.substring(4, 8).toInt();
  if (d < 1 || d > 31 || m < 1 || m > 12 || y < 1970) return false;
  char out[16];
  snprintf(out, sizeof(out), "%04d-%02d-%02d", y, m, d);
  isoOut = String(out);
  return true;
}

bool parse_iso_to_ddmmyyyy(const String &iso, String &ddmmyyyyOut) {
  if (iso.length() != 10 || iso.charAt(4) != '-' || iso.charAt(7) != '-') return false;
  const int y = iso.substring(0, 4).toInt();
  const int m = iso.substring(5, 7).toInt();
  const int d = iso.substring(8, 10).toInt();
  if (d < 1 || d > 31 || m < 1 || m > 12 || y < 1970) return false;
  char out[16];
  snprintf(out, sizeof(out), "%02d%02d%04d", d, m, y);
  ddmmyyyyOut = String(out);
  return true;
}
} // namespace

static void extension_fields_to_globals(const EepromExtensionFields &f) {
  PmqttTopic = String(f.pmqttTopic.c_str());
  PmqttSchema = String(f.pmqttSchema.c_str());
  PmqttBindingsJson = String(f.pmqttBindingsJson.c_str());
  if (PmqttBindingsJson.length() == 0) PmqttBindingsJson = "[]";
  JsyMk333SerialBaud = f.jsyMk333SerialBaud;
  peer_port = f.peer_port;
  peer_path = String(f.peer_path.c_str());
  if (peer_path.length() == 0) {
    peer_path = "/api/v1/measurements";
  }
  peer_protocol = "json";
  arduinoOtaPassword = String(f.arduinoOtaPassword.c_str());
  httpApiPassword = String(f.httpApiPassword.c_str());
  httpCorsEnabled = f.httpCorsEnabled;
  pwmGpio = f.pwmGpio;
  if (f.pwmMode == 1) {
    pwmMode = "follow_triac";
  } else if (f.pwmMode == 2) {
    pwmMode = "independent";
  } else {
    pwmMode = "off";
  }
  pwmDutyPercent = f.pwmDutyPercent;
  pwmInverted = f.pwmInverted;
  fleetTrustKey = String(f.fleetTrustKey.c_str());
  tempoRteEnabled = f.tempoRteEnabled;
  if (!f.tempoRteLtarfCache.empty()) LTARF = String(f.tempoRteLtarfCache.c_str());
  STGEt = String(f.tempoRteStgeCache.c_str());
  rte_today = f.tempoRteJourCache.empty() ? String(kTempoRteLabelUndefined) : String(f.tempoRteJourCache.c_str());
  rte_tomorrow =
      f.tempoRteDemainCache.empty() ? String(kTempoRteLabelUndefined) : String(f.tempoRteDemainCache.c_str());
  tempoRteLastFetchEpoch = f.tempoRteLastFetchEpoch;
  expert_regulation_mode = f.expertRegulationMode;
  regulation_gain = f.regulationGain;
  if (regulation_gain < 1) regulation_gain = 1;
  if (f.regulationPersistPresent) {
    for (int i = 0; i < kMaxRoutingActions; i++) {
      load_channels[i].Kp = f.actionRegCoeffs[i].kp;
      load_channels[i].Ki = f.actionRegCoeffs[i].ki > 0 ? f.actionRegCoeffs[i].ki : 4;
      load_channels[i].Kd = f.actionRegCoeffs[i].kd;
      load_channels[i].PID = f.actionRegCoeffs[i].pid;
    }
  }
  vacationEnabled = f.vacationEnabled;
  vacationEndEpoch = f.vacationEndEpoch;
  maxRoutedW = f.maxRoutedW;
  mqttJsonCommands = f.mqttJsonCommands;
  triacOffWhenSourceStale = f.triacOffWhenSourceStale;
  triacBackoffWhenHeaterIdle = f.triacBackoffWhenHeaterIdle;
  for (int i = 0; i < kMaxRoutingActions; i++) {
    actionDailyCapWh[i] = f.actionDailyCapWh[i];
    actionCapHit[i] = false;
  }
  ApiAccessTokenEntry loaded[kApiAccessTokenMax];
  const int n = f.apiAccessTokenCount > kApiAccessTokenMax ? kApiAccessTokenMax : f.apiAccessTokenCount;
  for (int i = 0; i < n; i++) {
    loaded[i].id = f.apiAccessTokens[i].id;
    loaded[i].label = f.apiAccessTokens[i].label;
    loaded[i].token_hex = f.apiAccessTokens[i].token_hex;
  }
  api_access_tokens_load(loaded, n);
}

static EepromExtensionFields extension_fields_from_globals() {
  EepromExtensionFields f;
  f.pmqttTopic = std::string(PmqttTopic.c_str());
  f.pmqttSchema = std::string(PmqttSchema.c_str());
  f.pmqttBindingsJson = std::string(PmqttBindingsJson.c_str());
  f.jsyMk333SerialBaud = JsyMk333SerialBaud;
  f.peer_port = static_cast<uint16_t>(peer_port);
  f.peer_path = std::string(peer_path.c_str());
  f.peer_protocol_mode = 1;
  f.arduinoOtaPassword = std::string(arduinoOtaPassword.c_str());
  f.httpApiPassword = std::string(httpApiPassword.c_str());
  f.httpCorsEnabled = httpCorsEnabled;
  f.pwmGpio = static_cast<int8_t>(pwmGpio);
  if (pwmMode == "follow_triac") {
    f.pwmMode = 1;
  } else if (pwmMode == "independent") {
    f.pwmMode = 2;
  } else {
    f.pwmMode = 0;
  }
  f.pwmDutyPercent = static_cast<uint8_t>(pwmDutyPercent > 100 ? 100 : (pwmDutyPercent < 0 ? 0 : pwmDutyPercent));
  f.pwmInverted = pwmInverted;
  f.fleetTrustKey = std::string(fleetTrustKey.c_str());
  f.tempoRteEnabled = tempoRteEnabled;
  f.tempoRteLtarfCache = std::string(LTARF.c_str());
  f.tempoRteStgeCache = std::string(STGEt.c_str());
  f.tempoRteJourCache = std::string(rte_today.c_str());
  f.tempoRteDemainCache = std::string(rte_tomorrow.c_str());
  f.tempoRteLastFetchEpoch = tempoRteLastFetchEpoch;
  f.expertRegulationMode = expert_regulation_mode;
  f.regulationGain = regulation_gain;
  f.regulationPersistPresent = true;
  for (int i = 0; i < kMaxRoutingActions; i++) {
    f.actionRegCoeffs[i].kp = load_channels[i].Kp;
    f.actionRegCoeffs[i].ki = load_channels[i].Ki;
    f.actionRegCoeffs[i].kd = load_channels[i].Kd;
    f.actionRegCoeffs[i].pid = load_channels[i].PID;
  }
  f.haSitePersistPresent = true;
  f.vacationEnabled = vacationEnabled;
  f.vacationEndEpoch = vacationEndEpoch;
  f.maxRoutedW = maxRoutedW;
  f.mqttJsonCommands = mqttJsonCommands;
  f.triacOffWhenSourceStale = triacOffWhenSourceStale;
  f.triacBackoffWhenHeaterIdle = triacBackoffWhenHeaterIdle;
  for (int i = 0; i < kMaxRoutingActions; i++) {
    f.actionDailyCapWh[i] = actionDailyCapWh[i];
  }
  const String actionsJson = helio_actions_serialize_eeprom_json();
  f.actionsJson = std::string(actionsJson.c_str());
  f.actionsJsonPresent = !f.actionsJson.empty();
  ApiAccessTokenEntry entries[kApiAccessTokenMax];
  int tokCount = 0;
  api_access_tokens_to_eeprom(entries, tokCount);
  f.apiAccessTokenCount = static_cast<uint8_t>(tokCount);
  for (int i = 0; i < tokCount && i < EepromExtensionFields::kApiAccessTokenStoredMax; i++) {
    f.apiAccessTokens[i].id = entries[i].id;
    f.apiAccessTokens[i].label = entries[i].label;
    f.apiAccessTokens[i].token_hex = entries[i].token_hex;
  }
  return f;
}

static int eeprom_read_extension(int address) {
  arduinoOtaPassword = "";
  httpApiPassword = "";
  api_access_tokens_clear();
  PmqttTopic = "";
  PmqttSchema = "Pw";
  PmqttBindingsJson = "[]";
  JsyMk333SerialBaud = 9600;
  peer_port = 80;
  peer_path = "/api/v1/measurements";
  EepromExtensionFields fields;
  auto &be = storage_eeprom_arduino_backend();
  address = storage_eeprom_extension_read(
      address, be, fields, [](int a) { return helio_mains_profile_read_from_eeprom(a); });
  extension_fields_to_globals(fields);
  if (fields.actionsJsonPresent && !fields.actionsJson.empty()) {
    String err;
    if (!helio_actions_load_eeprom_json(String(fields.actionsJson.c_str()), err)) {
      Serial.println("EEPROM actions JSON load: " + err);
    }
  }
  return address;
}

static int eeprom_write_extension(int address) {
  const EepromExtensionFields fields = extension_fields_from_globals();
  auto &be = storage_eeprom_arduino_backend();
  return storage_eeprom_extension_write(
      address, be, fields, [](int a) {
        int out = a;
        helio_mains_profile_persist_to_eeprom(a, &out);
        return out;
      });
}

static void Calibration(int address);

void eepromInit(void) {
  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("Failed to initialise EEPROM");
    Serial.println("Restarting...");
    Debug.println("Failed to initialise EEPROM");
    Debug.println("Restarting...");
    delay(10000);
    ESP.restart();
  }
}

void eepromClearConsumptionHistory() {
  int Adr_SoutInjec = adr_HistoAn;
  for (int i = 0; i < (kEepromAdrTriacImportJ0 - adr_HistoAn) / 4; i++) {
    EEPROM.writeLong(Adr_SoutInjec, 0);
    Adr_SoutInjec += 4;
  }
  EEPROM.writeULong(kEepromAdrTriacImportJ0, 0);
  EEPROM.writeULong(adr_E_T_injecte0, 0);
  EEPROM.writeULong(kEepromAdrHouseImportJ0, 0);
  EEPROM.writeULong(adr_E_M_injecte0, 0);
  EEPROM.writeString(adr_currentDateStr, "");
  EEPROM.writeUShort(adr_lastStockConso, 0);
  EEPROM.commit();
  currentDateStr = "";
  idxPromDuJour = 0;
  if (history_use_nvs()) {
    history_nvs_clear_slots(history_days_capacity());
    history_ring_cache_invalidate();
    g_historyNvsPendingCommit = false;
  }
}

void eepromLoadMorningDayEnergy(void) {
  EAS_T_J0 = EEPROM.readULong(kEepromAdrTriacImportJ0);  //Triac
  EAI_T_J0 = EEPROM.readULong(adr_E_T_injecte0);
  EAS_M_J0 = EEPROM.readULong(kEepromAdrHouseImportJ0);  // house day anchor
  EAI_M_J0 = EEPROM.readULong(adr_E_M_injecte0);
  currentDateStr = EEPROM.readString(adr_currentDateStr);
  idxPromDuJour = EEPROM.readUShort(adr_lastStockConso);
  const int cap = history_days_capacity();
  if (cap > 0) idxPromDuJour = (idxPromDuJour + cap) % cap;
  if (second_energy_import_wh<EAS_T_J0){
    second_energy_import_wh=EAS_T_J0;
  }
  if (second_energy_export_wh<EAI_T_J0){
    second_energy_export_wh=EAI_T_J0;
  }
  if (house_energy_import_wh<EAS_M_J0){
    house_energy_import_wh=EAS_M_J0;
  }
  if (house_energy_export_wh<EAI_M_J0){
    house_energy_export_wh=EAI_M_J0;
  }
}


void helio_on_clock_tick() {
  if (time_sync_valid) {
    //Time Update / de l'heure
    time_t timestamp = time(NULL);
    char buffer[MAX_SIZE_T];
    struct tm *pTime = localtime(&timestamp);
    strftime(buffer, MAX_SIZE_T, "%d/%m/%Y %H:%M:%S", pTime);
    sync_clock_str = String(buffer);
    strftime(buffer, MAX_SIZE_T, "%d%m%Y", pTime);
    String currentDayStr = String(buffer);
    strftime(buffer, MAX_SIZE_T, "%H", pTime);
    int hour = String(buffer).toInt();
    strftime(buffer, MAX_SIZE_T, "%M", pTime);
    int minute = String(buffer).toInt();
    wall_clock_decihours = hour * 100 + minute * 10 / 6;
    if (currentDateStr != currentDayStr) {  // Midnight rollover
      if (meter_reading_valid && currentDateStr !="") {      // Valid energy data received
        const int cap = history_days_capacity();
        idxPromDuJour = (idxPromDuJour + 1 + cap) % cap;
        // Persist complete CH1/CH2 day metrics in ring slot.
        const long ch1Import = long(house_day_energy_import_wh);
        const long ch1Export = long(house_day_energy_export_wh);
        const long ch2Import = long(second_day_energy_import_wh);
        const long ch2Export = long(second_day_energy_export_wh);
        history_write_slot(idxPromDuJour, ch1Import, ch1Export, ch2Import, ch2Export);
        EEPROM.writeULong(kEepromAdrTriacImportJ0, long(second_energy_import_wh));
        EEPROM.writeULong(adr_E_T_injecte0, long(second_energy_export_wh));
        EEPROM.writeULong(kEepromAdrHouseImportJ0, long(house_energy_import_wh));
        EEPROM.writeULong(adr_E_M_injecte0, long(house_energy_export_wh));
        EEPROM.writeString(adr_currentDateStr, currentDayStr);
        EEPROM.writeUShort(adr_lastStockConso, idxPromDuJour);
        EEPROM.commit();
        eepromLoadMorningDayEnergy();
      }
      currentDateStr = currentDayStr;
    }
  }
}
String eepromFormatYearlyEnergyHistory(void) {
  String S = "";
  const int cap = history_days_capacity();
  for (int i = 0; i < cap; i++) {
    const int iS = (idxPromDuJour + i + 1) % cap;
    long ch1Import = 0, ch1Export = 0, ch2Import = 0, ch2Export = 0;
    history_read_slot(iS, ch1Import, ch1Export, ch2Import, ch2Export);
    const long delta = ch1Import - ch1Export;
    S += String(delta) + ",";
  }
  return S;
}

int eepromHistoryDaysCapacity(void) { return history_days_capacity(); }
int eepromHistoryDaysRetained(void) { return kHistoryDaysRetained; }

bool eepromHistoryReferenceDateIso(String &isoOut) {
  if (time_sync_valid) {
    time_t now = time(NULL);
    struct tm dTm;
#if defined(ESP_PLATFORM)
    localtime_r(&now, &dTm);
#else
    struct tm *pTime = localtime(&now);
    if (!pTime) return false;
    dTm = *pTime;
#endif
    char isoDay[16];
    strftime(isoDay, sizeof(isoDay), "%Y-%m-%d", &dTm);
    isoOut = String(isoDay);
    return true;
  }
  return parse_ddmmyyyy_to_iso(currentDateStr, isoOut);
}

bool eepromHistoryReadDailyMetrics(
    int logicalDayIdx,
    long &ch1ImportWh,
    long &ch1ExportWh,
    long &ch2ImportWh,
    long &ch2ExportWh) {
  const int cap = history_days_capacity();
  if (logicalDayIdx < 0 || logicalDayIdx >= cap) return false;
  const int ringIdx = (idxPromDuJour + logicalDayIdx + 1) % cap;
  history_read_slot(ringIdx, ch1ImportWh, ch1ExportWh, ch2ImportWh, ch2ExportWh);
  return true;
}

bool eepromHistoryWriteDailyMetrics(
    int logicalDayIdx,
    long ch1ImportWh,
    long ch1ExportWh,
    long ch2ImportWh,
    long ch2ExportWh) {
  const int cap = history_days_capacity();
  if (logicalDayIdx < 0 || logicalDayIdx >= cap) return false;
  const int ringIdx = (idxPromDuJour + logicalDayIdx + 1) % cap;
  history_write_slot(ringIdx, ch1ImportWh, ch1ExportWh, ch2ImportWh, ch2ExportWh);
  return true;
}

bool eepromHistoryImportDailyMetrics(
    const long ch1ImportWh[],
    const long ch1ExportWh[],
    const long ch2ImportWh[],
    const long ch2ExportWh[],
    int count,
    const char *latestDateIso,
    String &err) {
  const int cap = history_days_capacity();
  if (count <= 0) {
    err = "csv has no valid data rows";
    return false;
  }
  if (count > cap) {
    err = "csv exceeds retained history capacity";
    return false;
  }
  if (history_use_nvs()) {
    if (!history_ring_cache_ensure() && cap > 0) {
      g_historyRingCache.slots.assign(static_cast<size_t>(cap), DailyMetricsSlot{});
      g_historyRingCache.valid = true;
    } else if (static_cast<int>(g_historyRingCache.slots.size()) != cap) {
      g_historyRingCache.slots.assign(static_cast<size_t>(cap), DailyMetricsSlot{});
      g_historyRingCache.valid = true;
    }
    idxPromDuJour = cap - 1;
    const int startLogical = cap - count;
    for (int i = 0; i < count; i++) {
      const int logical = startLogical + i;
      const int ringIdx = (idxPromDuJour + logical + 1) % cap;
      g_historyRingCache.slots[ringIdx] = DailyMetricsSlot{
          static_cast<int32_t>(ch1ImportWh[i]), static_cast<int32_t>(ch1ExportWh[i]),
          static_cast<int32_t>(ch2ImportWh[i]), static_cast<int32_t>(ch2ExportWh[i])};
      if ((i % 8) == 0) delay(0);
    }
    history_mark_nvs_pending();
    if (latestDateIso == nullptr || latestDateIso[0] == '\0') {
      err = "invalid latest ISO date";
      return false;
    }
    String dayStamp;
    if (!parse_iso_to_ddmmyyyy(String(latestDateIso), dayStamp)) {
      err = "invalid latest ISO date";
      return false;
    }
    currentDateStr = dayStamp;
    EEPROM.writeString(adr_currentDateStr, currentDateStr);
    EEPROM.writeUShort(adr_lastStockConso, idxPromDuJour);
    EEPROM.commit();
    return true;
  }
  const int start = cap - count;
  // EEPROM ring: zero once via direct writes (no per-slot NVS saves).
  for (int i = 0; i < cap; i++) {
    EEPROM.writeLong(metric_addr_for(i, 0), 0);
    EEPROM.writeLong(metric_addr_for(i, 1), 0);
    EEPROM.writeLong(metric_addr_for(i, 2), 0);
    EEPROM.writeLong(metric_addr_for(i, 3), 0);
  }
  idxPromDuJour = cap - 1;
  for (int i = 0; i < count; i++) {
    const int logical = start + i;
    if (!eepromHistoryWriteDailyMetrics(
            logical, ch1ImportWh[i], ch1ExportWh[i], ch2ImportWh[i], ch2ExportWh[i])) {
      err = "failed to write history slot";
      return false;
    }
  }
  if (latestDateIso == nullptr || latestDateIso[0] == '\0') {
    err = "invalid latest ISO date";
    return false;
  }
  String dayStamp;
  if (!parse_iso_to_ddmmyyyy(String(latestDateIso), dayStamp)) {
    err = "invalid latest ISO date";
    return false;
  }
  currentDateStr = dayStamp;
  EEPROM.writeString(adr_currentDateStr, currentDateStr);
  EEPROM.writeUShort(adr_lastStockConso, idxPromDuJour);
  EEPROM.commit();
  return true;
}
void eepromHistoryServicePendingCommit(void) { (void)history_commit_pending_nvs(); }

bool eepromHistoryHasPendingCommit(void) { return g_historyNvsPendingCommit; }

unsigned long eepromReadLayoutKey() {
  return EEPROM.readULong(adr_ParaActions);
}

int persistConfigToEeprom();

void loadConfigFromEeprom() {
  int period_start;
  bool source_migrated = false;
  int address = adr_ParaActions;
  eeprom_layout_key = EEPROM.readULong(address);
  address += sizeof(unsigned long);
  ssid = EEPROM.readString(address);
  address += ssid.length() + 1;
  password = EEPROM.readString(address);
  address += password.length() + 1;
  dhcpOn = EEPROM.readByte(address);
  address += sizeof(byte);
  IP_Fixe = EEPROM.readULong(address);
  address += sizeof(unsigned long);
  Gateway = EEPROM.readULong(address);
  address += sizeof(unsigned long);
  subnetMask = EEPROM.readULong(address);
  address += sizeof(unsigned long);
  dns = EEPROM.readULong(address);
  address += sizeof(unsigned long);
  Source = EEPROM.readString(address);
  address += Source.length() + 1;
  peer_ip = EEPROM.readULong(address);
  address += sizeof(unsigned long);
  EnphaseUser = EEPROM.readString(address);
  address += EnphaseUser.length() + 1;
  EnphasePwd = EEPROM.readString(address);
  address += EnphasePwd.length() + 1;
  meter_channel = EEPROM.readString(address);
  address += meter_channel.length() + 1;
  mqtt_publish_period_sec = EEPROM.readUShort(address);
  address += sizeof(unsigned short);
  MQTTIP = EEPROM.readULong(address);
  address += sizeof(unsigned long);
  MQTTPort = EEPROM.readUShort(address);
  address += sizeof(unsigned short);
  MQTTUser = EEPROM.readString(address);
  address += MQTTUser.length() + 1;
  MQTTPwd = EEPROM.readString(address);
  address += MQTTPwd.length() + 1;
  MQTTPrefix = EEPROM.readString(address);
  address += MQTTPrefix.length() + 1;
  MQTTdeviceName = EEPROM.readString(address);
  address += MQTTdeviceName.length() + 1;
  routerName = EEPROM.readString(address);
  address += routerName.length() + 1;
  probeSecondName = EEPROM.readString(address);
  address += probeSecondName.length() + 1;
  probeHouseName = EEPROM.readString(address);
  address += probeHouseName.length() + 1;
  temperatureSensorName = EEPROM.readString(address);
  address += temperatureSensorName.length() + 1;
  CalibU = EEPROM.readUShort(address);
  address += sizeof(unsigned short);
  CalibI = EEPROM.readUShort(address);
  address += sizeof(unsigned short);
  // --- Actions: count in fixed block; channel config in extension JSON (0xE220) ---
  NbActions = EEPROM.readUShort(address);
  address += sizeof(unsigned short);
  address = eeprom_read_extension(address);
  helio_mains_profile_init_from_eeprom();
  Calibration(address);
  if (source_migrated) {
    if (Source != "HelioPeer") Source_data = Source;
    persistConfigToEeprom();
  }
}
int persistConfigToEeprom() {
  int address = adr_ParaActions;
  EEPROM.writeULong(address, eeprom_layout_key);
  address += sizeof(unsigned long);
  EEPROM.writeString(address, ssid);
  address += ssid.length() + 1;
  EEPROM.writeString(address, password);
  address += password.length() + 1;
  EEPROM.writeByte(address, dhcpOn);
  address += sizeof(byte);
  EEPROM.writeULong(address, IP_Fixe);
  address += sizeof(unsigned long);
  EEPROM.writeULong(address, Gateway);
  address += sizeof(unsigned long);
  EEPROM.writeULong(address, subnetMask);
  address += sizeof(unsigned long);
  EEPROM.writeULong(address, dns);
  address += sizeof(unsigned long);
  EEPROM.writeString(address, Source);
  address += Source.length() + 1;
  EEPROM.writeULong(address, peer_ip);
  address += sizeof(unsigned long);
  EEPROM.writeString(address, EnphaseUser);
  address += EnphaseUser.length() + 1;
  EEPROM.writeString(address, EnphasePwd);
  address += EnphasePwd.length() + 1;
  EEPROM.writeString(address, meter_channel);
  address += meter_channel.length() + 1;
  EEPROM.writeUShort(address, mqtt_publish_period_sec);
  address += sizeof(unsigned short);
  EEPROM.writeULong(address, MQTTIP);
  address += sizeof(unsigned long);
  EEPROM.writeUShort(address, MQTTPort);
  address += sizeof(unsigned short);
  EEPROM.writeString(address, MQTTUser);
  address += MQTTUser.length() + 1;
  EEPROM.writeString(address, MQTTPwd);
  address += MQTTPwd.length() + 1;
  EEPROM.writeString(address, MQTTPrefix);
  address += MQTTPrefix.length() + 1;
  EEPROM.writeString(address, MQTTdeviceName);
  address += MQTTdeviceName.length() + 1;
  EEPROM.writeString(address, routerName);
  address += routerName.length() + 1;
  EEPROM.writeString(address, probeSecondName);
  address += probeSecondName.length() + 1;
  EEPROM.writeString(address, probeHouseName);
  address += probeHouseName.length() + 1;
  EEPROM.writeString(address, temperatureSensorName);
  address += temperatureSensorName.length() + 1;
  EEPROM.writeUShort(address, CalibU);
  address += sizeof(unsigned short);
  EEPROM.writeUShort(address, CalibI);
  address += sizeof(unsigned short);
  EEPROM.writeUShort(address, NbActions);
  address += sizeof(unsigned short);
  address = eeprom_write_extension(address);
  Calibration(address);
  EEPROM.commit();
  return address;
}
void Calibration(int address) {
  kV = KV * CalibU / 1000;  //Calibration coefficient to be applied
  kI = KI * CalibI / 1000;
  RomUsedBytes = address;
  P_cent_EEPROM = int(100 * address / EEPROM_SIZE);
  Serial.println("EEPROM used: " + String(P_cent_EEPROM) + "%");
  Debug.println("EEPROM used: " + String(P_cent_EEPROM) + "%");
}
