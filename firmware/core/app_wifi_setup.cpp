#include "app_wifi_setup.h"
#include "api_http_plain_body.h"
#include "captive_dns.h"
#include "helio_forward.h"
#include "helio_globals.h"
#include "helio_reboot.h"
#include <WiFi.h>
#include <esp_err.h>
#include <esp_wifi.h>
#include <nvs.h>
#include <cstring>

/** Soft-AP channel (1–13). Avoid auto channel 0 — some clients never see the beacon. */
static const int kSetupApChannel = 6;

#ifndef HOSTNAME
#define HOSTNAME "HELIOZERO-"
#endif

void helio_update_status_leds(void);

void helio_wifi_prepare_hostname(void) {
  String hostname(HOSTNAME);
  uint32_t chipId = 0;
  for (int i = 0; i < 17; i = i + 8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  hostname += String(chipId);
  helio_ap_ssid_storage = hostname;
  ap_default_ssid = helio_ap_ssid_storage.c_str();
  Serial.println(hostname);
}

static void helio_wifi_apply_hostname(void) {
  if (helio_ap_ssid_storage.length() > 0) {
    WiFi.setHostname(helio_ap_ssid_storage.c_str());
  }
}

static const char kWifiNvsNamespace[] = "helio_wifi";
static const char kWifiNvsSsidKey[] = "ssid";
static const char kWifiNvsPassKey[] = "pass";

bool helio_wifi_load_sta_from_nvs(void) {
  nvs_handle_t handle = 0;
  if (nvs_open(kWifiNvsNamespace, NVS_READONLY, &handle) != ESP_OK) {
    return false;
  }
  size_t ssidLen = 0;
  if (nvs_get_str(handle, kWifiNvsSsidKey, nullptr, &ssidLen) != ESP_OK || ssidLen <= 1) {
    nvs_close(handle);
    return false;
  }
  char ssidBuf[33] = {0};
  if (ssidLen > sizeof(ssidBuf) ||
      nvs_get_str(handle, kWifiNvsSsidKey, ssidBuf, &ssidLen) != ESP_OK) {
    nvs_close(handle);
    return false;
  }
  size_t passLen = sizeof(char);
  char passBuf[65] = {0};
  if (nvs_get_str(handle, kWifiNvsPassKey, nullptr, &passLen) == ESP_OK && passLen <= sizeof(passBuf)) {
    nvs_get_str(handle, kWifiNvsPassKey, passBuf, &passLen);
  }
  nvs_close(handle);
  ssid = String(ssidBuf);
  password = String(passBuf);
  Serial.println(String(F("WiFi NVS profile: ")) + ssid);
  return true;
}

bool helio_wifi_clear_sta_profile(void) {
  nvs_handle_t handle = 0;
  if (nvs_open(kWifiNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) {
    return false;
  }
  esp_err_t err = nvs_erase_all(handle);
  if (err == ESP_OK) {
    err = nvs_commit(handle);
  }
  nvs_close(handle);
  if (err != ESP_OK) {
    Serial.println(F("WiFi NVS profile clear failed"));
    return false;
  }
  Serial.println(F("WiFi NVS profile cleared"));
  return true;
}

bool helio_wifi_save_sta_profile(void) {
  const size_t ssidLen = ssid.length();
  const size_t passLen = password.length();
  if (ssidLen == 0 || ssidLen > 32 || passLen > 64) {
    return false;
  }
  nvs_handle_t handle = 0;
  if (nvs_open(kWifiNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) {
    Serial.println(F("WiFi NVS open failed"));
    return false;
  }
  if (nvs_set_str(handle, kWifiNvsSsidKey, ssid.c_str()) != ESP_OK ||
      nvs_set_str(handle, kWifiNvsPassKey, password.c_str()) != ESP_OK ||
      nvs_commit(handle) != ESP_OK) {
    nvs_close(handle);
    Serial.println(F("WiFi NVS profile save failed"));
    return false;
  }
  nvs_close(handle);
  Serial.println(String(F("WiFi NVS profile saved: ")) + ssid);
  return true;
}

void helio_wifi_prepare_stack(void) {
  WiFi.persistent(false);
  WiFi.setSleep(WIFI_PS_NONE);
  WiFi.useStaticBuffers(true);
  WiFi.mode(WIFI_OFF);
  delay(50);
  const bool modeOk = WiFi.mode(WIFI_STA);
  delay(100);
  WiFi.mode(WIFI_OFF);
  delay(50);
  Serial.print(F("WiFi stack ready="));
  Serial.println(modeOk ? F("yes") : F("no"));
}

static void helio_wifi_apply_country(void) {
  wifi_country_t country = {};
  strncpy(country.cc, "FR", sizeof(country.cc));
  country.schan = 1;
  country.nchan = 13;
  country.policy = WIFI_COUNTRY_POLICY_AUTO;
  const esp_err_t err = esp_wifi_set_country(&country);
  if (err != ESP_OK) {
    Serial.print(F("esp_wifi_set_country err="));
    Serial.println(static_cast<int>(err));
  }
}

/** @param apSta true when STA radio is needed (Wi-Fi scan); false = AP-only for best beacon visibility. */
static void helio_wifi_start_soft_ap(bool apSta) {
  WiFi.persistent(apSta && ssid.length() > 0);
  WiFi.setSleep(WIFI_PS_NONE);
  WiFi.useStaticBuffers(true);

  const wifi_mode_t targetMode = apSta ? WIFI_MODE_APSTA : WIFI_MODE_AP;
  if (ssid.length() > 0) {
    WiFi.disconnect(true, false);
    delay(50);
    WiFi.mode(WIFI_OFF);
    delay(100);
  }
  const bool modeOk = WiFi.mode(targetMode);
  delay(200);
  helio_wifi_apply_hostname();
  Serial.print(F("WiFi.mode ok="));
  Serial.print(modeOk ? F("yes") : F("no"));
  Serial.print(F(" mode="));
  Serial.println(WiFi.getMode());

  const IPAddress apIp(192, 168, 4, 1);
  const IPAddress apGw(192, 168, 4, 1);
  const IPAddress apMask(255, 255, 255, 0);
  if (!WiFi.softAPConfig(apIp, apGw, apMask)) {
    Serial.println(F("softAPConfig failed"));
  }

  const char *psk = (ap_default_psk != nullptr && ap_default_psk[0] != '\0') ? ap_default_psk : nullptr;
  bool apOk = WiFi.softAP(ap_default_ssid, psk, kSetupApChannel, 0, 4);
  if (!apOk) {
    Serial.println(F("softAP() failed - retrying"));
    delay(300);
    WiFi.mode(targetMode);
    delay(200);
    helio_wifi_apply_hostname();
    WiFi.softAPConfig(apIp, apGw, apMask);
    apOk = WiFi.softAP(ap_default_ssid, psk, kSetupApChannel, 0, 4);
  }
  if (!apOk) {
    Serial.println(F("softAP() retry failed - hard reset WiFi mode"));
    WiFi.mode(WIFI_OFF);
    delay(300);
    WiFi.mode(targetMode);
    delay(300);
    helio_wifi_apply_hostname();
    WiFi.softAPConfig(apIp, apGw, apMask);
    apOk = WiFi.softAP(ap_default_ssid, psk, kSetupApChannel, 0, 4);
  }
  if (apOk) {
    helio_wifi_apply_country();
  }
  for (uint8_t i = 0; i < 25 && WiFi.softAPIP() == IPAddress(0, 0, 0, 0); ++i) {
    delay(50);
  }

  const IPAddress apAddr = WiFi.softAPIP();
  helio_captive_dns_start(apAddr);
  Serial.print(F("Access Point Mode. SSID: "));
  Serial.print(ap_default_ssid);
  Serial.print(F(" IP: "));
  Serial.print(apAddr);
  Serial.print(F(" mode="));
  Serial.print(WiFi.getMode());
  Serial.print(F(" ch="));
  Serial.print(kSetupApChannel);
  Serial.print(F(" apOk="));
  Serial.println(apOk ? F("yes") : F("no"));
}

void helio_wifi_connect_sta_or_ap(void) {
  Serial.println("SSID:" + ssid);
  Serial.println("Pass:" + password);
  if (ssid.length() == 0) {
    if (helio_wifi_soft_ap_setup_active()) {
      Serial.println(F("Setup AP already active."));
      return;
    }
    Serial.println(F("No WiFi credentials - starting setup AP."));
    helio_wifi_start_soft_ap(false);
    return;
  }
  /* AP+STA during join so http://192.168.4.1 works while STA is still trying (up to 15 s). */
  Serial.println(F("Saved WiFi - starting setup AP, then joining station."));
  helio_wifi_start_soft_ap(true);
  const unsigned long connect_start_ms = millis();
  constexpr unsigned long kStaConnectTimeoutMs = 15000UL;
  {
    if (dhcpOn == 0) {
      byte arr[4];
      arr[0] = IP_Fixe & 0xFF;
      arr[1] = (IP_Fixe >> 8) & 0xFF;
      arr[2] = (IP_Fixe >> 16) & 0xFF;
      arr[3] = (IP_Fixe >> 24) & 0xFF;
      IPAddress local_IP(arr[3], arr[2], arr[1], arr[0]);
      arr[0] = Gateway & 0xFF;
      arr[1] = (Gateway >> 8) & 0xFF;
      arr[2] = (Gateway >> 16) & 0xFF;
      arr[3] = (Gateway >> 24) & 0xFF;
      IPAddress gateway(arr[3], arr[2], arr[1], arr[0]);
      arr[0] = subnetMask & 0xFF;
      arr[1] = (subnetMask >> 8) & 0xFF;
      arr[2] = (subnetMask >> 16) & 0xFF;
      arr[3] = (subnetMask >> 24) & 0xFF;
      IPAddress subnet(arr[3], arr[2], arr[1], arr[0]);
      arr[0] = dns & 0xFF;
      arr[1] = (dns >> 8) & 0xFF;
      arr[2] = (dns >> 16) & 0xFF;
      arr[3] = (dns >> 24) & 0xFF;
      IPAddress primaryDNS(arr[3], arr[2], arr[1], arr[0]);
      IPAddress secondaryDNS(8, 8, 4, 4);
      if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
        Serial.println("WIFI STA Failed to configure");
      }
    }
    Serial.println("Wifi Begin : " + ssid);
    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED && (millis() - connect_start_ms < kStaConnectTimeoutMs)) {
      helio_reboot_poll();
      helio_http_ensure_listening();
      for (int h = 0; h < 4; h++) {
        server.handleClient();
        api_http_clear_plain_body();
        helio_reboot_poll();
      }
      Serial.write('.');
      helio_update_status_leds();
      Serial.print(WiFi.status());
      delay(300);
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
    helio_captive_dns_stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    delay(50);
    helio_wifi_apply_hostname();
    helio_http_invalidate_binding();
    helio_http_ensure_listening();
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("Connected IP address: " + WiFi.localIP().toString() + " or <a href='http://" + helio_ap_ssid_storage +
                   ".local' >" + helio_ap_ssid_storage + ".local</a>");
  } else {
    Serial.println(F("Can not connect to WiFi station - setup AP remains active."));
    helio_http_ensure_listening();
    helio_captive_dns_start(WiFi.softAPIP());
  }
}

bool helio_wifi_soft_ap_setup_active(void) {
  return WiFi.getMode() != WIFI_STA && WiFi.softAPIP() != IPAddress(0, 0, 0, 0);
}
