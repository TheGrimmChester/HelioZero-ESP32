#pragma once

#include <Arduino.h>

/** Cached 12-char lowercase hex uid from ESP factory MAC. */
const String &helio_device_uid();

/** Replace empty / factory `helio_zero` MQTT device name with device_uid. Returns true if changed. */
bool helio_apply_default_mqtt_device_name(String &name);
