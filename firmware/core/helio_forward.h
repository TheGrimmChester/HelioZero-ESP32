#pragma once

#include <Arduino.h>

/* storage_eeprom.cpp */
void eepromInit(void);
void eepromClearConsumptionHistory(void);
void eepromLoadMorningDayEnergy(void);
void helio_on_clock_tick(void);
String eepromFormatYearlyEnergyHistory(void);
unsigned long eepromReadLayoutKey(void);
void loadConfigFromEeprom(void);
int persistConfigToEeprom(void);
void Calibration(int address);

/* http_server.cpp */
void Init_Server(void);
void helio_http_invalidate_binding(void);
void helio_http_ensure_listening(void);

/* mqtt_ha.cpp */
void publishMqttLoop(void);
void pmqtt_mqtt_service_tick(void);

/* Source drivers */
void jsy_mk194t_setup(void);
void jsy_mk194t_poll(void);
void jsy_mk333_setup(void);
void jsy_mk333_send_request(void);
void jsy_mk333_poll(void);
void uxi_probe_setup(void);
void uxi_probe_poll(void);
void linky_meter_setup(void);
void linky_meter_poll(void);
void enphase_envoy_setup(void);
void enphase_envoy_poll(void);
void smart_gateway_poll(void);
void shelly_em_poll(void);
void external_peer_poll(void);
