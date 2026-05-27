#pragma once

#include <ArduinoJson.h>

#include "helio_config_audit_keys.h"

void helio_config_audit_record(const char *route, JsonObjectConst body);
void helio_config_audit_append_json(JsonArray arr, int max_entries = 20);
