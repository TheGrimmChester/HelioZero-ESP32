/*
 * helio_globals.cpp — Default values for globals declared in helio_globals.h.
 * Lab-only compile-time WiFi/MQTT defaults via HELIO_ZERO_DEFAULT_* build_flags (see FIRMWARE_BUILD.md).
 */
#include "helio_globals.h"

#ifndef HELIO_ZERO_DEFAULT_WIFI_SSID
#define HELIO_ZERO_DEFAULT_WIFI_SSID ""
#endif
#ifndef HELIO_ZERO_DEFAULT_WIFI_PASSWORD
#define HELIO_ZERO_DEFAULT_WIFI_PASSWORD ""
#endif
#ifndef HELIO_ZERO_DEFAULT_MQTT_USER
#define HELIO_ZERO_DEFAULT_MQTT_USER ""
#endif
#ifndef HELIO_ZERO_DEFAULT_MQTT_PASSWORD
#define HELIO_ZERO_DEFAULT_MQTT_PASSWORD ""
#endif

const char *ap_default_ssid = nullptr;
const char *ap_default_psk = nullptr;

unsigned long eeprom_layout_key;

String helio_ap_ssid_storage;

String ssid = HELIO_ZERO_DEFAULT_WIFI_SSID;
String password = HELIO_ZERO_DEFAULT_WIFI_PASSWORD;
String Source = "UxIx2";
String Source_data = "UxIx2";
byte dhcpOn = 1;
unsigned long IP_Fixe = 0;
unsigned long Gateway = 0;
unsigned long subnetMask = 4294967040;
unsigned long dns = 0;
unsigned long ext_peer_ip = 0;
unsigned int ext_peer_port = 80;
String ext_peer_path = "/api/v1/measurements";
String ext_peer_protocol = "json";
unsigned long ext_peer_last_poll_ms = 0;
bool ext_peer_last_poll_ok = false;
String ext_peer_last_poll_err = "";
String ext_peer_last_poll_preview = "";
String ext_peer_last_poll_protocol = "";
bool LinkyEaitFromTic = false;
bool LinkySinstiSeen = false;
bool httpCorsEnabled = false;
int pwmGpio = -1;
String pwmMode = "off";
int pwmDutyPercent = 0;
bool pwmInverted = false;
String fleetTrustKey = "";
unsigned int mqtt_publish_period_sec = 0;
unsigned long MQTTIP = 0;
unsigned int MQTTPort = 1883;
String MQTTUser = HELIO_ZERO_DEFAULT_MQTT_USER;
String MQTTPwd = HELIO_ZERO_DEFAULT_MQTT_PASSWORD;
String MQTTPrefix = "helio_zero";
String MQTTdeviceName = "helio_zero";
String arduinoOtaPassword = "";
String httpApiPassword = "";
int RomUsedBytes = 0;
String routerName = "HelioZero";
String probeSecondName = "Second channel";
String probeHouseName = "House metering";
String temperatureSensorName = "temperature_c";
String GS = String((char)29);
String RS = String((char)30);
int P_cent_EEPROM;
int cptLEDyellow = 0;
int cptLEDgreen = 0;

unsigned int CalibU = 1000;
unsigned int CalibI = 1000;
int value0;
int volt[100];
int amp[100];
float KV = 0.2083f;
float KI = 0.0642f;
float kV = 0.2083f;
float kI = 0.0642f;
float voltM[100];
float ampM[100];

bool meter_reading_valid = false;
long EAS_T_J0 = 0;
long EAI_T_J0 = 0;
long EAS_M_J0 = 0;
long EAI_M_J0 = 0;

int adr_debut_para = 0;

float second_voltage_v, second_current_a, second_power_factor, mains_frequency_hz;
float house_voltage_v, house_current_a, house_power_factor;
float second_energy_import_wh = 0;
float second_energy_export_wh = 0;
float house_energy_import_wh = 0;
float house_energy_export_wh = 0;
float second_day_energy_export_wh = 0;
float house_day_energy_export_wh = 0;
float second_day_energy_import_wh = 0;
float house_day_energy_import_wh = 0;
int second_active_import_w, house_active_import_w, second_active_export_w, house_active_export_w;
int second_apparent_import_va, house_apparent_import_va, second_apparent_export_va, house_apparent_export_va;
int enphase_house_active_w, enphase_production_w;
int tabPwHouse_5mn[600];
int tabPw_Triac_5mn[600];
int tabTemperature_5mn[600];
int tabPwHouse_2s[300];
int tabPw_Triac_2s[300];
int tabPvaHouse_2s[300];
int tabPva_Triac_2s[300];
int IdxStock2s = 0;
int IdxStockPW = 0;

byte ByteArray[130];
long LesDatas[14];
int Sens_1, Sens_2;

bool LFon = false;
bool EASTvalid = false;
bool EAITvalid = false;
int IdxDataRawLinky = 0;
int IdxBufDecodLinky = 0;
char DataRawLinky[10000];
float moyPWS = 0;
float moyPWI = 0;
float moyPVAS = 0;
float moyPVAI = 0;
float COSphiS = 1;
float COSphiI = 1;
long TlastEASTvalide = 0;
long TlastEAITvalide = 0;
String LTARF = "";
String STGEt = "";
String rte_today = "UNDEFINED";
String rte_tomorrow = "UNDEFINED";
bool tempoRteEnabled = false;
int tempoRteLastPollDecihours = -1;
int LTARFbin = 0;
uint32_t tempoRteLastFetchEpoch = 0;

String TokenEnphase = "";
String EnphaseUser = "";
String EnphasePwd = "";
String meter_channel = "0";
String JsonToken = "";
String Session_id = "";
long LastwhDlvdCum = 0;
float EMI_Wh = 0;
float EMS_Wh = 0;

String SG_rawData = "";
String ShEm_rawData = "";
int shellyEmPollCounter = 0;
String HW_rawData = "";
String ShPro_rawData = "";
String MK333_rawData = "";
String PmqttTopic = "";
String PmqttSchema = "Pw";
String PmqttBindingsJson = "[]";
float PwMQTT_last = 0;
unsigned long LastPwMQTTMillis = 0;
uint32_t UxIx3SerialBaud = 9600;

Action load_channels[kMaxRoutingActions];
int NbActions = 0;
uint8_t expert_regulation_mode = 0;
uint8_t regulation_gain = 1;
bool vacationEnabled = false;
uint32_t vacationEndEpoch = 0;
uint16_t maxRoutedW = 0;
bool siteCapActive = false;
uint32_t actionDailyCapWh[kMaxRoutingActions] = {};
bool actionCapHit[kMaxRoutingActions] = {};
bool mqttJsonCommands = false;
bool triacOffWhenSourceStale = false;
bool triacBackoffWhenHeaterIdle = false;
bool heaterLoadBackoffActive = false;

unsigned long startMillis;
unsigned long previousWifiMillis;
unsigned long previousHistoryMillis;
unsigned long previousWsMillis;
unsigned long previousWiMillis;
unsigned long last_metering_task_ms;
unsigned long previousTimer2sMillis;
unsigned long previousOverProdMillis;
unsigned long previousLEDsMillis;
unsigned long previousLoop;
unsigned long previousETX;
unsigned long poll_period_ms = 1000;
float previousLoopMin = 1000;
float previousLoopMax = 0;
float previousLoopMoy = 0;
unsigned long last_metering_task_at_ms;
float metering_task_ms_min = 1000;
float metering_task_ms_max = 0;
float metering_task_ms_avg = 0;
unsigned long previousMqttMillis;

float triac_delay_percent_f = 100;

WebServer server(80);

const char *ntpServer1 = "fr.pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
String TimeTz = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";
String TimeNtp1 = "fr.pool.ntp.org";
String TimeNtp2 = "time.nist.gov";
String sync_clock_str = "";
String currentDateStr = "";
bool time_sync_valid = false;
int wall_clock_decihours = 0;
int idxPromDuJour = 0;

OneWire oneWire(pinTemp);
DallasTemperature ds18b20(&oneWire);
float temperature = -127;
int triacOverrideMaxTempC = 70;

#if HELIO_REMOTE_DEBUG
RemoteDebug Debug;
#else
RmsDebugStub Debug;
#endif
WiFiClient MqttClient;
PubSubClient clientMQTT(MqttClient);
int WIFIbug = 0;
int meterPeerFailures = 0;

TaskHandle_t Task1;
