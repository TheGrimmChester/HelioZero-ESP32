#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

struct PmqttBindingEntry {
  String metric;
  String topic;
  String format;  // plain | json | snapshot
  String path;
  bool enabled = true;
};

struct PmqttBindingPreview {
  String metric;
  String topic;
  bool ok = false;
  float value = 0.0f;
  String error;
  String raw_snippet;
  unsigned long age_ms = 0;
};

bool pmqtt_bindings_parse_config(std::vector<PmqttBindingEntry> &out, String *err = nullptr);
bool pmqtt_bindings_has_required_power(String *missingGroup = nullptr);
/** True when active bindings include house/second day energy metrics (broker provides EnergieJour_*). */
bool pmqtt_bindings_provides_day_energy();
void pmqtt_bindings_collect_topics(std::vector<String> &outTopics);
bool pmqtt_bindings_apply_message(const String &topic, const String &payload, String *err = nullptr);
void pmqtt_bindings_cache_payload(const String &topic, const String &payload);
void pmqtt_bindings_append_diagnostics(JsonObject root);
bool pmqtt_bindings_preview(JsonArray inputBindings, JsonArray outResults, String *err = nullptr);
