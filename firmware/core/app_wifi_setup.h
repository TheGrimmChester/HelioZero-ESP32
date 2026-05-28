#pragma once
#include <Arduino.h>

/** Build `HOSTNAME` + efuse chip id; set `helio_ap_ssid_storage` / `ap_default_ssid` (Wi-Fi hostname applied later). */
void helio_wifi_prepare_hostname(void);

/** Init esp_wifi before large EEPROM heap alloc; does not start AP or join STA. */
void helio_wifi_prepare_stack(void);

/** Load/save STA credentials via esp_wifi NVS profile (separate from EEPROM blob). */
bool helio_wifi_load_sta_from_nvs(void);
bool helio_wifi_save_sta_profile(void);
/** Remove persisted STA profile (helio_wifi NVS namespace). */
bool helio_wifi_clear_sta_profile(void);

/** After EEPROM load: connect STA (static IP if `dhcpOn==0`) or start soft AP. */
void helio_wifi_connect_sta_or_ap(void);

/** True while the soft-AP is up for captive-portal / first-run Wi‑Fi configuration. */
bool helio_wifi_soft_ap_setup_active(void);
