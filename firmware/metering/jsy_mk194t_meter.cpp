/*
 * jsy_mk194t_meter.cpp — Source JsyMk194: JSY-MK-194T Modbus on Serial2 @ 4800, reg block 0x0048.
 * See: /en/hardware-pinout/ — source_jsy_mk194; GUIDE A.5.1.
 */
#include "helio_globals.h"
#include "jsy_mk194_logic.h"
void jsy_mk194t_setup() {
  Serial2.begin(4800, SERIAL_8N1, RXD2, TXD2);  // JSY-MK-194 on Serial2
}
void jsy_mk194t_poll() {
  int i;
  byte msg_send[] = { 0x01, 0x03, 0x00, 0x48, 0x00, 0x0E, 0x44, 0x18 };
  // Modbus RTU request on Serial2
  for (i = 0; i < 8; i++) {
    Serial2.write(msg_send[i]);
  }

  // Response to previous poll (4800 baud only)
  int a = 0;
  while (Serial2.available()) {
    ByteArray[a] = Serial2.read();
    a++;
  }


  if (a == 61) {
    JsyMk194Reading rd;
    if (jsy_mk194_parse_modbus_frame(ByteArray, a, rd)) {
      Sens_1 = rd.sens_1;
      Sens_2 = rd.sens_2;
      second_voltage_v = rd.voltage_second_v;
      second_current_a = rd.current_second_a;
      second_energy_import_wh = rd.energy_import_wh;
      second_power_factor = rd.pf_second;
      second_energy_export_wh = rd.energy_export_wh;
      mains_frequency_hz = rd.frequence_hz;
      second_active_import_w = rd.second_active_import_w;
      second_active_export_w = rd.second_active_export_w;
      second_apparent_import_va = rd.pva_t;
      second_apparent_export_va = (rd.sens_1 > 0) ? rd.pva_t : 0;
      house_voltage_v = rd.voltage_house_v;
      house_current_a = rd.current_house_a;
      house_energy_import_wh = rd.house_energy_import_wh_wh;
      house_power_factor = rd.pf_house;
      house_energy_export_wh = rd.house_energy_export_wh_wh;
      house_active_import_w = rd.house_active_import_w;
      house_active_export_w = rd.house_active_export_w;
      house_apparent_import_va = rd.pva_m;
      house_apparent_export_va = (rd.sens_2 > 0) ? rd.pva_m : 0;
      meter_reading_valid = true;
      esp_task_wdt_reset();
      if (cptLEDyellow > 30) {
        cptLEDyellow = 4;
      }
    }
  }
}