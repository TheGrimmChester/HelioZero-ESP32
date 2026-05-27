#pragma once
#include <Arduino.h>

/** Build `HOSTNAME` + efuse chip id, set `helio_ap_ssid_storage` / `ap_default_ssid`, `WiFi.hostname`, ensure STA mode. */
void helio_wifi_prepare_hostname(void);

/** After EEPROM + NTP config: connect STA (static IP if `dhcpOn==0`) or start soft AP. */
void helio_wifi_connect_sta_or_ap(unsigned long boot_start_ms);

/** True while the soft-AP is up for captive-portal / first-run Wi‑Fi configuration. */
bool helio_wifi_soft_ap_setup_active(void);
