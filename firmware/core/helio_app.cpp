/*
 * helio_app.cpp — helio_setup/helio_loop, helio_metering_task (core 0), helio_apply_surplus_regulation (core 1).
 * Initializes WiFi, EEPROM, triac ISRs, HTTP routes, MQTT, OTA, and the metering FreeRTOS task.
 * See: /en/project-overview/; triac ISRs in helio_triac_isr.cpp (minimal ISR work only).
 */
#include "helio_app.h"
#include "helio_diag.h"
#include "helio_pwm.h"
#include "helio_self_test.h"
#include "helio_triac_calibration_logic.h"
#include "triac_api_shim.h"
#include "helio_board.h"
#include "helio_device_id.h"
#include "helio_globals.h"
#include "actions_logic.h"
#include "helio_pulse_modes.h"
#include "helio_regulation_logic.h"
#include "helio_vacation_logic.h"
#include "helio_source_health_logic.h"
#include "helio_action_cap_logic.h"
#include "helio_site_cap_logic.h"
#include "helio_regulation_modes.h"
#include "helio_regulation_state.h"
#include "tempo_rte_logic.h"
#include "helio_forward.h"
#include "helio_triac_isr.h"
#include "helio_pub.h"
#include "helio_reboot.h"
#include "helio_source.h"
#include "helio_source_logic.h"
#include "metering/pmqtt_bindings.h"
#include "helio_measurement.h"
#include "app_wifi_setup.h"
#include "captive_dns.h"
#include "storage_eeprom.h"
#include "tempo_rte.h"

#include <ArduinoOTA.h>
#include <WiFi.h>
#include <esp_arduino_version.h>
#include <esp_sntp.h>
#include <esp_task_wdt.h>

static unsigned long previousTempoPollMillis = 0;

static void helio_watchdog_init(void) {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  esp_task_wdt_config_t wdt_cfg = {
      .timeout_ms = (uint32_t)WDT_TIMEOUT_SEC * 1000u,
      .idle_core_mask = 0,
      .trigger_panic = true,
  };
  esp_task_wdt_init(&wdt_cfg);
#else
  esp_task_wdt_init(WDT_TIMEOUT_SEC, true);
#endif
}

void helio_metering_task(void *pvParameters) {
  (void)pvParameters;
  esp_task_wdt_add(nullptr);
  esp_task_wdt_reset();
  for (;;) {
    unsigned long tps = millis();
    float deltaT = float(tps - last_metering_task_at_ms);
    last_metering_task_at_ms = tps;
    metering_task_ms_min = min(metering_task_ms_min, deltaT);
    metering_task_ms_min = metering_task_ms_min + 0.001f;
    metering_task_ms_max = max(metering_task_ms_max, deltaT);
    metering_task_ms_max = metering_task_ms_max * 0.9999f;
    metering_task_ms_avg = deltaT * 0.001f + metering_task_ms_avg * 0.999f;

    if (tps - last_metering_task_ms > poll_period_ms) {
      last_metering_task_ms = tps;
      unsigned long pollBackoffMs = static_cast<unsigned long>(house_active_import_w / 10);
      helio_source_run_poll_cycle(pollBackoffMs);
      HelioPublishFromGlobals();
      helio_measurement_refresh_last();
    }
    delay(2);
    esp_task_wdt_reset();
  }
}

void helio_setup(void) {
  startMillis = millis();
  previousLEDsMillis = startMillis;

  pinMode(LedYellow, OUTPUT);
  pinMode(LedGreen, OUTPUT);
  pinMode(kZeroCrossGpio, INPUT);
  pinMode(kTriacDimGpio, OUTPUT);
  digitalWrite(LedYellow, LOW);
  digitalWrite(LedGreen, LOW);
  digitalWrite(kTriacDimGpio, LOW);

  helio_watchdog_init();

  Serial.begin(115200);
  Serial.println(F("Booting"));

  for (int i = 0; i < kMaxRoutingActions; i++) {
    load_channels[i] = Action(i);
  }

  esp_task_wdt_reset();

  helio_wifi_prepare_hostname();
  helio_wifi_prepare_stack();

  eepromInit();
  eeprom_layout_key = kEepromLayoutInit;
  unsigned long Rcle = eepromReadLayoutKey();
  Serial.println(String(F("ROM key: ")) + String(Rcle));
  const bool eepromConfigLoaded = (Rcle == eeprom_layout_key);
  if (eepromConfigLoaded) {
    loadConfigFromEeprom();
    eepromLoadMorningDayEnergy();
    helio_init_action_gpios();
  } else {
    eepromClearConsumptionHistory();
  }
  if (helio_apply_default_mqtt_device_name(MQTTdeviceName) && eepromConfigLoaded) {
    persistConfigToEeprom();
  }
  helio_wifi_load_sta_from_nvs();
  helio_active_source_refresh_from_global_string();

  sntp_set_time_sync_notification_cb(time_sync_notification);
  sntp_servermode_dhcp(1);
  configTzTime(TimeTz.c_str(), TimeNtp1.c_str(), TimeNtp2.c_str());

  /* Register HTTP routes before WiFi join so the setup AP can serve /wifi during STA attempts. */
  Init_Server();

  helio_wifi_connect_sta_or_ap();
  helio_http_ensure_listening();

#if HELIO_REMOTE_DEBUG
  Debug.begin("ESP32");
  Debug.println(F("Ready"));
  Debug.print(F("IP address: "));
  Debug.println(WiFi.localIP());
#endif

#ifndef METER_ONLY_BUILD
  helio_regulation_state_init();
  helio_pulse_modes_init_tables();
  helio_triac_hw_init();
#endif

  ArduinoOTA.setHostname(helio_ap_ssid_storage.c_str());
  if (arduinoOtaPassword.length() > 0) {
    ArduinoOTA.setPassword(arduinoOtaPassword.c_str());
  }
  ArduinoOTA.begin();

  Serial.println(String(F("Source: ")) + Source);
  helio_source_apply_hardware_setup();

  xTaskCreatePinnedToCore(helio_metering_task, "helio_metering_task", 10000, nullptr, 10, &Task1, 0);

  ds18b20.begin();
  helio_poll_temperature();

  previousWifiMillis = millis() + 300000;
  previousHistoryMillis = millis() - 290000;
  previousTimer2sMillis = millis();
  previousTempoPollMillis = millis();
  previousLoop = millis();
  last_metering_task_at_ms = millis();
  previousMqttMillis = millis() - 5000;
  previousETX = millis();
  previousOverProdMillis = millis();
  if (helio_active_source_get() == SourceId::UxIx3) {
    last_metering_task_ms = millis();
  } else {
    last_metering_task_ms = millis() + 500;
  }
}

void helio_loop(void) {
  helio_reboot_poll();

  unsigned long tps = millis();
  float deltaT = float(tps - previousLoop);
  previousLoop = tps;
  previousLoopMin = min(previousLoopMin, deltaT);
  previousLoopMin = previousLoopMin + 0.001f;
  previousLoopMax = max(previousLoopMax, deltaT);
  previousLoopMax = previousLoopMax * 0.9999f;
  previousLoopMoy = deltaT * 0.001f + previousLoopMoy * 0.999f;

  ArduinoOTA.handle();
#if HELIO_REMOTE_DEBUG
  Debug.handle();
#endif
  helio_http_ensure_listening();
  if (helio_wifi_soft_ap_setup_active()) {
    helio_captive_dns_process();
  } else if (helio_captive_dns_active()) {
    helio_captive_dns_stop();
  }
  const int httpDrainMax = helio_wifi_soft_ap_setup_active() ? 32 : 8;
  for (int httpDrain = 0; httpDrain < httpDrainMax; httpDrain++) {
    server.handleClient();
  }
  eepromHistoryServicePendingCommit();

  if (tps - previousTempoPollMillis >= 2000) {
    previousTempoPollMillis = tps;
    tempo_rte_poll();
    LTARFbin = tempo_rte_logic_ltarf_bin(std::string(LTARF.c_str()));
  }

  if (helio_active_source_get() == SourceId::Pmqtt) {
    pmqtt_mqtt_service_tick();
  }

  if (meter_reading_valid) {
    if (tps - previousHistoryMillis >= 300000) {
      previousHistoryMillis = tps;
      tabPwHouse_5mn[IdxStockPW] = house_active_import_w - house_active_export_w;
      tabPw_Triac_5mn[IdxStockPW] = second_active_import_w - second_active_export_w;
      tabTemperature_5mn[IdxStockPW] = int(temperature);
      IdxStockPW = (IdxStockPW + 1) % 600;
    }

    if (tps - previousTimer2sMillis >= 2000) {
      previousTimer2sMillis = tps;
      tabPwHouse_2s[IdxStock2s] = house_active_import_w - house_active_export_w;
      tabPw_Triac_2s[IdxStock2s] = second_active_import_w - second_active_export_w;
      tabPvaHouse_2s[IdxStock2s] = house_apparent_import_va - house_apparent_export_va;
      tabPva_Triac_2s[IdxStock2s] = second_apparent_import_va - second_apparent_export_va;
      IdxStock2s = (IdxStock2s + 1) % 300;
      publishMqttLoop();
      helio_on_clock_tick();
      helio_daily_energy_tick();
    }

    if (tps - previousOverProdMillis >= 200) {
      previousOverProdMillis = tps;
#ifndef METER_ONLY_BUILD
      helio_apply_surplus_regulation();
      helio_diag_regulation_hunting_tick(millis(), TriacGetOpenPercent());
      helio_self_test_tick(millis());
      helio_pwm_tick(TriacGetOpenPercent());
#endif
    }
  }
  if (tps - previousLEDsMillis >= 50) {
    previousLEDsMillis = tps;
    helio_update_status_leds();
  }
  /* Signed delta: `previousWifiMillis = millis() + delay` must not use unsigned subtraction. */
  if ((int32_t)(tps - previousWifiMillis) > 30000) {
    previousWifiMillis = tps;
    /* Non-blocking reconnect check: avoid blocking the web stack for seconds. */
    if (WiFi.status() == WL_CONNECTED) {
      WIFIbug = 0;
      if (helio_wifi_soft_ap_setup_active()) {
        helio_captive_dns_stop();
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        delay(50);
        helio_http_invalidate_binding();
      }
      helio_http_ensure_listening();
    } else if (!helio_wifi_soft_ap_setup_active()) {
      const uint32_t wifi_poll_deadline = millis() + 500;
      while (WiFi.status() != WL_CONNECTED && (int32_t)(millis() - wifi_poll_deadline) < 0) {
        delay(10);
        yield();
      }
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println(String(F("Connection Failed! #")) + String(WIFIbug));
        WIFIbug++;
        if (WIFIbug > 2) {
          ESP.restart();
        }
      } else {
        WIFIbug = 0;
      }
    }

    if (WiFi.getMode() != WIFI_STA) {
      Serial.print(F("Access Point Mode. IP address: "));
      Serial.println(WiFi.softAPIP());
    } else {
      Serial.print(F("WiFi RSSI (dBm): "));
      Serial.println(WiFi.RSSI());
      Serial.print(F("IP address: "));
      Serial.println(WiFi.localIP());
      Serial.print(F("WIFIbug:"));
      Serial.println(WIFIbug);
      Serial.print(F("meterPeerFailures:"));
      Serial.println(meterPeerFailures);
#if HELIO_REMOTE_DEBUG
      Debug.print(F("WiFi RSSI (dBm): "));
      Debug.println(WiFi.RSSI());
      Debug.print(F("WIFIbug:"));
      Debug.println(WIFIbug);
      Debug.print(F("meterPeerFailures:"));
      Debug.println(meterPeerFailures);
#endif
      Serial.println(String(F("Metering task load (core 0) ms — min: ")) + String(int(metering_task_ms_min)) +
                     F(" avg: ") + String(int(metering_task_ms_avg)) + F(" max: ") + String(int(metering_task_ms_max)));
#if HELIO_REMOTE_DEBUG
      Debug.println(String(F("Metering task load (core 0) ms — min: ")) + String(int(metering_task_ms_min)) +
                    F(" avg: ") + String(int(metering_task_ms_avg)) + F(" max: ") + String(int(metering_task_ms_max)));
#endif
      Serial.println(String(F("Main loop load (core 1) ms — min: ")) + String(int(previousLoopMin)) + F(" avg: ") +
                     String(int(previousLoopMoy)) + F(" max: ") + String(int(previousLoopMax)));
#if HELIO_REMOTE_DEBUG
      Debug.println(String(F("Main loop load (core 1) ms — min: ")) + String(int(previousLoopMin)) + F(" avg: ") +
                    String(int(previousLoopMoy)) + F(" max: ") + String(int(previousLoopMax)));
#endif
    }
    int T = int(millis() / 1000);
    float DureeOn = float(T) / 3600.0f;
    Serial.println(String(F("ESP32 uptime (hours): ")) + String(DureeOn));
#if HELIO_REMOTE_DEBUG
    Debug.println(String(F("ESP32 uptime (hours): ")) + String(DureeOn));
#endif
    helio_poll_temperature();
    helio_on_clock_tick();
  }
}

void helio_init_action_gpios(void) {
  for (int i = 1; i < NbActions; i++) {
    if (action_regulation_enabled(load_channels[i].Actif)) {
      load_channels[i].InitGpio();
    }
  }
}

void helio_daily_energy_tick(void) {
  if (!time_sync_valid) return;
  const bool pmqttDayFromBroker =
      helio_active_source_get() == SourceId::Pmqtt && pmqtt_bindings_provides_day_energy();
  if (!pmqttDayFromBroker) {
    if (house_energy_import_wh < EAS_M_J0 || EAS_M_J0 == 0) {
      EAS_M_J0 = house_energy_import_wh;
    }
    house_day_energy_import_wh = house_energy_import_wh - EAS_M_J0;
    if (house_energy_export_wh < EAI_M_J0 || EAI_M_J0 == 0) {
      EAI_M_J0 = house_energy_export_wh;
    }
    house_day_energy_export_wh = house_energy_export_wh - EAI_M_J0;
    if (second_energy_import_wh < EAS_T_J0 || EAS_T_J0 == 0) {
      EAS_T_J0 = second_energy_import_wh;
    }
    second_day_energy_import_wh = second_energy_import_wh - EAS_T_J0;
    if (second_energy_export_wh < EAI_T_J0 || EAI_T_J0 == 0) {
      EAI_T_J0 = second_energy_export_wh;
    }
    second_day_energy_export_wh = second_energy_export_wh - EAI_T_J0;
    return;
  }
  /* Pmqtt: day energies come from MQTT bindings; still track J0 anchors from cumulative totals. */
  if (house_energy_import_wh < EAS_M_J0 || EAS_M_J0 == 0) {
    EAS_M_J0 = house_energy_import_wh;
  }
  if (house_energy_export_wh < EAI_M_J0 || EAI_M_J0 == 0) {
    EAI_M_J0 = house_energy_export_wh;
  }
  if (second_energy_import_wh < EAS_T_J0 || EAS_T_J0 == 0) {
    EAS_T_J0 = second_energy_import_wh;
  }
  if (second_energy_export_wh < EAI_T_J0 || EAI_T_J0 == 0) {
    EAI_T_J0 = second_energy_export_wh;
  }
}

void time_sync_notification(struct timeval *tv) {
  (void)tv;
  Serial.println(F("Time synchronization event"));
  time_sync_valid = true;
  helio_on_clock_tick();
}

void helio_poll_temperature(void) {
  float temperature_brute = -127;
  ds18b20.requestTemperatures();
  temperature_brute = ds18b20.getTempCByIndex(0);
  if (temperature_brute < -20 || temperature_brute > 130) {
    Serial.print(F("Invalid temperature reading "));
  } else {
    temperature = temperature_brute;
    Serial.print(F("Temperature: "));
    Serial.print(temperature);
    Serial.println(F("°C"));
#if HELIO_REMOTE_DEBUG
    Debug.print(F("Temperature: "));
    Debug.print(temperature);
    Debug.println(F("°C"));
#endif
  }
}

void helio_update_status_leds(void) {
  cptLEDyellow++;

  if (WiFi.status() != WL_CONNECTED) {
    if (WiFi.getMode() == WIFI_STA) {
      cptLEDyellow = (cptLEDyellow + 6) % 10;
      cptLEDgreen = cptLEDyellow;
    } else {
      cptLEDyellow = cptLEDyellow % 10;
      cptLEDgreen = (cptLEDyellow + 5) % 10;
    }
  } else {
    if (triac_delay_percent < 100) {
      cptLEDgreen = int((cptLEDgreen + 1 + 8 / (1 + triac_delay_percent / 10))) % 10;
    } else {
      cptLEDgreen = 10;
    }
  }
  if (cptLEDyellow > 5) {
    digitalWrite(LedYellow, LOW);
  } else {
    digitalWrite(LedYellow, HIGH);
  }
  if (cptLEDgreen > 5) {
    digitalWrite(LedGreen, LOW);
  } else {
    digitalWrite(LedGreen, HIGH);
  }
}
