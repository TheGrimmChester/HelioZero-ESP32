#include "helio_ha_state_payload.h"

#include "helio_device_id.h"
#include "helio_globals.h"
#include "helio_diag.h"
#include "triac_api_shim.h"
#include "helio_source.h"
#include "helio_source_health_logic.h"
#include "helio_triac_isr.h"
#include "helio_vacation_logic.h"
#include "mqtt_ha_command_logic.h"
#include "tempo_rte_logic.h"
#include "api.h"

int helio_compute_source_health_score(void) {
  SourceHealthScoreInput in;
  in.last_poll_ms_ago =
      time_sync_valid ? static_cast<int>((millis() - last_metering_task_ms) & 0x7FFFFFFF) : -1;
  in.poll_period_ms = 500;
  in.last_poll_ok = time_sync_valid;
  in.error_streak = 0;
  return helio_source_health_logic_compute(in).health_score;
}

bool helio_source_health_is_stale(int health_score) {
  return helio_source_health_logic_is_stale(health_score);
}

void helio_append_measurements_diagnostics(JsonObject diag) {
  if (helio_diag_uxi_adc_clipping_active()) {
    diag["adc_clipping"] = true;
  }
  if (g_regulation_hunting_active) {
    diag["regulation_hunting"] = true;
  }
  const int health = helio_compute_source_health_score();
  diag["source_health"] = health;
  diag["source_stale"] = helio_source_health_is_stale(health);
  const int triac_open = TriacGetOpenPercent();
  diag["regulation_active"] = triac_open > 5;
  diag["site_cap_active"] = siteCapActive;
  diag["mqtt_connected"] = clientMQTT.connected();
}

void helio_append_ha_state_payload(JsonObject doc) {
  doc["source"] = Source;
  doc["device_uid"] = helio_device_uid();
  if (helio_cap_mqtt_triac_channel_block()) {
    doc["second_active_import_w"] = second_active_import_w;
    doc["second_active_export_w"] = second_active_export_w;
    doc["second_voltage_v"] = second_voltage_v;
    doc["second_current_a"] = second_current_a;
    doc["second_power_factor"] = second_power_factor;
    doc["second_energy_import_wh"] = second_energy_import_wh;
    doc["second_energy_export_wh"] = second_energy_export_wh;
    doc["second_day_energy_import_wh"] = second_day_energy_import_wh;
    doc["second_day_energy_export_wh"] = second_day_energy_export_wh;
    doc["mains_frequency_hz"] = mains_frequency_hz;
  }
  if (temperature > -100) {
    doc["temperature_c"] = temperature;
  }
  if (helio_cap_mqtt_linky_tariff()) {
    doc["linky_ltarf"] = LTARF;
    doc["tariff_code"] = tempo_rte_logic_tariff_code(std::string(LTARF.c_str()));
  }
  if (tempoRteEnabled) {
    doc["rte_today"] = rte_today;
    doc["rte_tomorrow"] = rte_tomorrow;
  }
  doc["house_net_power_w"] = house_active_import_w - house_active_export_w;
  doc["house_active_import_w"] = house_active_import_w;
  doc["house_active_export_w"] = house_active_export_w;
  doc["house_voltage_v"] = house_voltage_v;
  doc["house_current_a"] = house_current_a;
  doc["house_power_factor"] = house_power_factor;
  doc["house_energy_import_wh"] = house_energy_import_wh;
  doc["house_energy_export_wh"] = house_energy_export_wh;
  doc["house_day_energy_import_wh"] = house_day_energy_import_wh;
  doc["house_day_energy_export_wh"] = house_day_energy_export_wh;

  doc["triac_open_percent"] = TriacGetOpenPercent();
  doc["adc_clipping"] = helio_diag_uxi_adc_clipping_active() ? "ON" : "OFF";
  doc["regulation_hunting"] = g_regulation_hunting_active ? "ON" : "OFF";
  const int health = helio_compute_source_health_score();
  doc["source_health"] = health;
  doc["source_stale"] = helio_source_health_is_stale(health) ? "ON" : "OFF";
  const int triac_open = TriacGetOpenPercent();
  doc["regulation_active"] = triac_open > 5 ? "ON" : "OFF";
  doc["mqtt_connected"] = clientMQTT.connected() ? "ON" : "OFF";
  doc["site_cap_active"] = siteCapActive ? "ON" : "OFF";
  doc["heater_load_backoff_active"] = heaterLoadBackoffActive ? "ON" : "OFF";
  doc["max_routed_w"] = maxRoutedW;
  const uint32_t now_epoch = static_cast<uint32_t>(time(NULL));
  doc["vacation"] =
      helio_vacation_logic_active(vacationEnabled, vacationEndEpoch, now_epoch) ? "ON" : "OFF";
  char action_key[20];
  for (int i = 0; i < NbActions; i++) {
    snprintf(action_key, sizeof(action_key), "action_%d", i);
    doc[action_key] = load_channels[i].On ? "ON" : "OFF";
  }
}

bool helio_apply_triac_command(const char *msg, String &err) {
  if (!msg) {
    err = "empty command";
    return false;
  }
  MqttTriacCmd triacCmd;
  if (!mqtt_ha_command_parse_triac(msg, &triacCmd)) {
    err = "invalid triac command (use AUTO or 0-100)";
    return false;
  }
  if (triacCmd.kind == MqttTriacCmdKind::Auto) {
    return ApiSetActionOverride(0, "auto", 0, 0, err);
  }
  return ApiSetActionOverride(0, "triac_fixed", triacCmd.percent, 0, err);
}
