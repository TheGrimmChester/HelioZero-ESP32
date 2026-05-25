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
#include <EEPROM.h>
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

static void extension_fields_to_globals(const EepromExtensionFields &f) {
  PmqttTopic = String(f.pmqttTopic.c_str());
  PmqttSchema = String(f.pmqttSchema.c_str());
  PmqttBindingsJson = String(f.pmqttBindingsJson.c_str());
  if (PmqttBindingsJson.length() == 0) PmqttBindingsJson = "[]";
  UxIx3SerialBaud = f.uxIx3SerialBaud;
  ext_peer_port = f.ext_peer_port;
  ext_peer_path = String(f.ext_peer_path.c_str());
  if (ext_peer_path.length() == 0) {
    ext_peer_path = "/api/v1/measurements";
  }
  ext_peer_protocol = "json";
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
  f.uxIx3SerialBaud = UxIx3SerialBaud;
  f.ext_peer_port = static_cast<uint16_t>(ext_peer_port);
  f.ext_peer_path = std::string(ext_peer_path.c_str());
  f.ext_peer_protocol_mode = 1;
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
  UxIx3SerialBaud = 9600;
  ext_peer_port = 80;
  ext_peer_path = "/api/v1/measurements";
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
  //Mise a zero Zone stockage
  int Adr_SoutInjec = adr_HistoAn;
  for (int i = 0; i < NbJour; i++) {
    EEPROM.writeLong(Adr_SoutInjec, 0);
    Adr_SoutInjec = Adr_SoutInjec + 4;
  }
  EEPROM.writeULong(kEepromAdrTriacImportJ0, 0);
  EEPROM.writeULong(adr_E_T_injecte0, 0);
  EEPROM.writeULong(kEepromAdrHouseImportJ0, 0);
  EEPROM.writeULong(adr_E_M_injecte0, 0);
  EEPROM.writeString(adr_currentDateStr, "");
  EEPROM.writeUShort(adr_lastStockConso, 0);
  EEPROM.commit();
}

void eepromLoadMorningDayEnergy(void) {
  EAS_T_J0 = EEPROM.readULong(kEepromAdrTriacImportJ0);  //Triac
  EAI_T_J0 = EEPROM.readULong(adr_E_T_injecte0);
  EAS_M_J0 = EEPROM.readULong(kEepromAdrHouseImportJ0);  // house day anchor
  EAI_M_J0 = EEPROM.readULong(adr_E_M_injecte0);
  currentDateStr = EEPROM.readString(adr_currentDateStr);
  idxPromDuJour = EEPROM.readUShort(adr_lastStockConso);
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
        idxPromDuJour = (idxPromDuJour + 1 + NbJour) % NbJour;
        // Store start-of-day energy snapshot for yearly history ring
        long energie = house_energy_import_wh - house_energy_export_wh;  // Net house Wh for the day
        EEPROM.writeLong(idxPromDuJour * 4, energie);
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
  int Adr_SoutInjec = 0;
  long EnergieJour = 0;
  long DeltaEnergieJour = 0;
  int iS = 0;
  long lastDay = 0;

  for (int i = 0; i < NbJour; i++) {
    iS = (idxPromDuJour + i + 1) % NbJour;
    Adr_SoutInjec = adr_HistoAn + iS * 4;
    EnergieJour = EEPROM.readLong(Adr_SoutInjec);
    if (lastDay == 0) { lastDay = EnergieJour; }
    DeltaEnergieJour = EnergieJour - lastDay;
    lastDay = EnergieJour;
    S += String(DeltaEnergieJour) + ",";
  }
  return S;
}
unsigned long eepromReadLayoutKey() {
  return EEPROM.readULong(adr_ParaActions);
}

int persistConfigToEeprom();

void loadConfigFromEeprom() {
  int period_start;
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
  ext_peer_ip = EEPROM.readULong(address);
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
  EEPROM.writeULong(address, ext_peer_ip);
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
