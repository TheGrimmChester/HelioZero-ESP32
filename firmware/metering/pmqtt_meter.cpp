/*
 * pmqtt_meter.cpp — Source Pmqtt: subscribe PmqttTopic on MQTTIP broker, parse PmqttSchema JSON.
 * User: /fr/user-guide/#guide-a7-mqtt-comme-source-de-mesure-pmqtt
 */
#include "helio_globals.h"
#include "helio_pub.h"
#include "helio_meter_json.h"
#include "pmqtt_bindings.h"
#include <ArduinoJson.h>

void ApplyPmqttJsonPayload(const String &msg) {
  DynamicJsonDocument doc(2048);
  DeserializationError e = deserializeJson(doc, msg);
  if (e) {
    return;
  }
  if (doc.containsKey("house") && doc["house"].is<JsonObject>()) {
    String err;
    if (ApplyMeterSnapshotFromJson(doc.as<JsonObject>(), &err)) {
      LastPwMQTTMillis = millis();
      if (doc.containsKey("Pw")) {
        PwMQTT_last = doc["Pw"].as<float>();
      } else if (doc.containsKey("power_w")) {
        PwMQTT_last = doc["power_w"].as<float>();
      } else if (doc.containsKey("active_power_w")) {
        PwMQTT_last = doc["active_power_w"].as<float>();
      } else {
        PwMQTT_last = (float)(house_active_import_w - house_active_export_w);
      }
      return;
    }
  }
  float Pw = 0;
  bool have = false;
  if (PmqttSchema.indexOf("Pw") >= 0 && doc.containsKey("Pw")) {
    Pw = doc["Pw"].as<float>();
    have = true;
  }
  if (!have && doc.containsKey("power_w")) {
    Pw = doc["power_w"].as<float>();
    have = true;
  }
  if (!have && doc.containsKey("active_power_w")) {
    Pw = doc["active_power_w"].as<float>();
    have = true;
  }
  if (!have) return;

  LastPwMQTTMillis = millis();
  PwMQTT_last = Pw;
  float Pf = 1.0f;
  if (PmqttSchema.indexOf("Pf") >= 0 && doc.containsKey("Pf")) {
    Pf = abs(doc["Pf"].as<float>());
    if (Pf > 1.0f) Pf = 1.0f;
  }
  if (Pw >= 0) {
    house_active_import_w = (int)Pw;
    house_active_export_w = 0;
    if (Pf > 0.01f) {
      house_apparent_import_va = (int)(Pw / Pf);
    } else {
      house_apparent_import_va = house_active_import_w;
    }
    house_apparent_export_va = 0;
  } else {
    house_active_import_w = 0;
    house_active_export_w = (int)(-Pw);
    if (Pf > 0.01f) {
      house_apparent_export_va = (int)((-Pw) / Pf);
    } else {
      house_apparent_export_va = house_active_export_w;
    }
    house_apparent_import_va = 0;
  }
  meter_reading_valid = true;
  HelioPublishFromGlobals();
  esp_task_wdt_reset();
  if (cptLEDyellow > 30) {
    cptLEDyellow = 4;
  }
}

bool ApplyPmqttBindingsPayload(const String &topic, const String &msg, String *err) {
  return pmqtt_bindings_apply_message(topic, msg, err);
}
