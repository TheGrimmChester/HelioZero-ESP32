#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

#define API_ACTION_SCHEMA_VERSION 4
#define API_ACTION_PATCH_MAX_UPDATES 3

void api_action_append_schema(JsonObject root);
void api_action_append_live_state(JsonArray out);
void api_action_append_config_array(JsonArray actionsOut);
void api_action_append_one_config(JsonObject out, int index);

bool api_action_put_collection(const String& body, String& err);
bool api_action_put_one(int index, const String& body, String& err);
bool api_action_patch_one(int index, const String& body, String& err);
bool api_action_patch_collection_batch(const String& body, String& err);

void api_action_persist_and_init_gpio();

/** EEPROM extension tail: full actions config as JSON (schema 4). */
String helio_actions_serialize_eeprom_json();
bool helio_actions_load_eeprom_json(const String &json, String &err);
