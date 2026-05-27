/*
 * api_v1_common.h — shared declarations for /api/v1 route modules.
 */
#ifndef API_V1_COMMON_H
#define API_V1_COMMON_H

#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <Update.h>
#include <cctype>
#include <cstring>
#include <string>
#include "helio_board.h"
#include "helio_device_id.h"
#include "Actions.h"
#include "helio_globals.h"
#include "api.h"
#include "api_util.h"
#include "actions_api.h"
#include "actions_api_logic.h"
#include "helio_pub.h"
#include "triac_api_shim.h"
#include "helio_meter_json.h"
#include "helio_meter_json_logic.h"
#include "helio_source.h"
#include "helio_mains_profile.h"
#include "helio_install_countries.h"
#include "helio_pwm.h"
#include "helio_pwm_logic.h"
#include "fleet_bundle_logic.h"
#include "tempo_rte.h"
#include "helio_forward.h"

extern WebServer server;
extern String ssid;
extern String password;
extern String sync_clock_str;
extern bool time_sync_valid;
extern String Source_data;
extern String Source;
extern String routerName;
extern String probeSecondName;
extern String probeHouseName;
extern String temperatureSensorName;
extern float temperature;
extern int triacOverrideMaxTempC;
extern unsigned long ext_peer_ip;
extern unsigned int ext_peer_port;
extern String ext_peer_path;
extern unsigned long ext_peer_last_poll_ms;
extern bool ext_peer_last_poll_ok;
extern String ext_peer_last_poll_err;
extern String ext_peer_last_poll_preview;
extern byte dhcpOn;
extern unsigned long IP_Fixe, Gateway, subnetMask, dns;
extern String EnphaseUser, EnphasePwd, meter_channel;
extern unsigned int mqtt_publish_period_sec;
extern unsigned long MQTTIP;
extern unsigned int MQTTPort;
extern String MQTTUser, MQTTPwd, MQTTPrefix, MQTTdeviceName;
extern unsigned int CalibU, CalibI;
extern float mains_frequency_hz;
extern int P_cent_EEPROM;
extern int RomUsedBytes;
extern String TimeTz;
extern String TimeNtp1;
extern String TimeNtp2;
extern float metering_task_ms_min, metering_task_ms_avg, metering_task_ms_max;
extern float previousLoopMin, previousLoopMoy, previousLoopMax;
extern int tabPwHouse_5mn[600];
extern int tabPw_Triac_5mn[600];
extern int tabTemperature_5mn[600];
extern int IdxStockPW;
extern int tabPwHouse_2s[300];
extern int tabPw_Triac_2s[300];
extern int tabPvaHouse_2s[300];
extern int tabPva_Triac_2s[300];
extern int IdxStock2s;
extern int idxPromDuJour;
extern int NbActions;
extern PubSubClient clientMQTT;
extern unsigned long eeprom_layout_key;
extern bool meter_reading_valid;
extern float voltM[100];
extern float ampM[100];
extern float house_voltage_v;
extern float house_current_a;
extern float house_power_factor;
extern float second_voltage_v;
extern float second_current_a;
extern float second_power_factor;
extern float mains_frequency_hz;
extern float house_day_energy_import_wh;
extern float house_day_energy_export_wh;
extern float second_day_energy_import_wh;
extern float second_day_energy_export_wh;
extern int IdxDataRawLinky;
extern char DataRawLinky[10000];
extern String LTARF;
extern bool tempoRteEnabled;
extern String TokenEnphase;
extern String Session_id;
extern int enphase_house_active_w;
extern int enphase_production_w;
extern String SG_rawData;
extern String ShEm_rawData;
extern int shellyEmPollCounter;
extern String HW_rawData;
extern String ShPro_rawData;
extern String MK333_rawData;
extern String PmqttTopic;
extern String PmqttSchema;
extern String PmqttBindingsJson;
extern float PwMQTT_last;
extern uint32_t UxIx3SerialBaud;

extern int persistConfigToEeprom(void);
extern void helio_init_action_gpios(void);
extern String eepromFormatYearlyEnergyHistory(void);
#include "helio_reboot.h"
extern void eepromClearConsumptionHistory(void);

#ifndef kMaxRoutingActions
#define kMaxRoutingActions 20
#endif

static const size_t kPutBodyMax = 16384;
static const size_t kPatchBodyMax = 16384;
static const size_t kPmqttBindingsDocCap = 16384;
static const int kHistDefaultMax = 200;
static const int kHistAbsMax = 600;
/** JSON cap for GET /api/v1/history/power (32 KB often fails to allocate on ESP32 heap). */
static const size_t kHistPowerJsonCap = 12288;
static const size_t kWifiBodyMax = 512;
static const size_t kTimeBodyMax = 512;
static const size_t kArduinoOtaBodyMax = 512;
static const int kEepromSize = 4090;
static const int kAdrParaActions = 1507;
static const size_t kHttpAuthBodyMax = 128;

#define API_AUTH_GUARD() \
  do {                 \
    if (!api_require_auth(server)) return; \
  } while (0)
#define API_AUTH_GUARD_R() \
  do {                     \
    if (!api_require_auth(server)) return true; \
  } while (0)

void api_append_config_object(JsonObject o);
bool config_apply_from_json(JsonObject root, bool fullPut, String &err);
void api_append_action_override(JsonObject o, int idx);
void api_append_measurements_object(JsonObject doc);
const char *override_state_name(byte state);
byte override_state_from_name(const char *state);

void handle_get_measurements();
void handle_get_telemetry_snapshot();
void handle_post_triac_override();
void handle_get_tariff_tempo();
void handle_get_system();
void handle_get_device();
void handle_get_state();
void handle_get_health();
void handle_get_system_audit();
void handle_post_health_self_test_run();
void handle_post_health_self_test_skip();
void handle_get_sources();
void handle_get_gpio();
void handle_get_config();
void handle_put_config();
void handle_patch_config();
void handle_get_actions_live();
void handle_get_actions_schema();
void handle_get_actions_config();
void handle_put_actions_config();
void handle_patch_actions_config_batch();
void handle_get_action_override(int idx);
void handle_post_action_override(int idx);
void handle_clear_action_override(int idx);
void handle_get_history_power();
void handle_get_history_energy_daily();
void handle_put_gpio();
void handle_get_pwm();
void handle_put_pwm();
void handle_get_fleet_export();
void handle_post_fleet_import();
void handle_put_fleet_trust_key();
void handle_post_reboot();
void handle_get_wifi();
void handle_put_wifi();
void handle_get_wifi_scan();
void handle_post_factory_reset();
void handle_post_save_now();
void handle_get_eeprom();
void handle_get_time();
void handle_get_system_arduino_ota();
void handle_get_public();
void handle_put_system_http_auth();
void handle_get_system_backup();
void handle_put_system_backup();
void handle_post_auth_login();
void handle_post_auth_logout();
void handle_get_auth_tokens();
void handle_post_auth_tokens();
bool Api_handle_auth_tokens_subresource();
void handle_put_system_arduino_ota();
void handle_put_time();
void handle_post_firmware_ota_done();
void handle_firmware_ota_upload();
void handle_get_sources_diagnostics();
void handle_get_sources_brute_panel();
void handle_post_history_reset();
void handle_post_mqtt_discover();
void handle_post_mqtt_reconnect();
void handle_post_mqtt_publish_now();
void handle_post_mqtt_test();
void handle_post_pmqtt_preview();
void handle_post_sources_test_inject();
void handle_get_openapi();

#endif
