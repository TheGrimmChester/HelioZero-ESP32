/*
 * helio_device_id.cpp — Arduino device_uid from eFuse MAC.
 */
#include "helio_device_id.h"

#include "helio_device_id_logic.h"

#include <esp_mac.h>

static uint64_t helio_efuse_mac48() {
  uint64_t mac = 0;
  esp_efuse_mac_get_default(reinterpret_cast<uint8_t *>(&mac));
  return mac;
}

const String &helio_device_uid() {
  static String cached;
  if (cached.length() == 0) {
    char buf[13];
    if (helio_device_uid_format(helio_efuse_mac48(), buf, sizeof(buf))) {
      cached = String(buf);
    }
  }
  return cached;
}

bool helio_apply_default_mqtt_device_name(String &name) {
  if (!helio_mqtt_device_name_is_factory_default(name.c_str())) return false;
  const String &uid = helio_device_uid();
  if (uid.length() == 0) return false;
  name = uid;
  return true;
}
