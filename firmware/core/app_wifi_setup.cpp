#include "app_wifi_setup.h"
#include "captive_dns.h"
#include "helio_forward.h"
#include "helio_globals.h"
#include <WiFi.h>
#include <esp_wifi.h>
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
  WiFi.hostname(hostname);
  Serial.println(hostname);
}

static void helio_wifi_apply_country(void) {
  wifi_country_t country = {};
  strncpy(country.cc, "FR", sizeof(country.cc));
  country.schan = 1;
  country.nchan = 13;
  country.policy = WIFI_COUNTRY_POLICY_AUTO;
  esp_wifi_set_country(&country);
}

/** @param apSta true when STA radio is needed (Wi‑Fi scan); false = AP-only for best beacon visibility. */
static void helio_wifi_start_soft_ap(bool apSta) {
  WiFi.persistent(false);
  WiFi.setSleep(WIFI_PS_NONE);
  WiFi.disconnect(true, true);
  delay(100);
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(apSta ? WIFI_AP_STA : WIFI_AP);
  delay(100);
  helio_wifi_apply_country();

  const IPAddress apIp(192, 168, 4, 1);
  const IPAddress apGw(192, 168, 4, 1);
  const IPAddress apMask(255, 255, 255, 0);
  if (!WiFi.softAPConfig(apIp, apGw, apMask)) {
    Serial.println(F("softAPConfig failed"));
  }

  const char *psk = (ap_default_psk != nullptr && ap_default_psk[0] != '\0') ? ap_default_psk : nullptr;
  bool apOk = WiFi.softAP(ap_default_ssid, psk, kSetupApChannel, 0, 4);
  if (!apOk) {
    Serial.println(F("softAP() failed — retrying"));
    delay(300);
    apOk = WiFi.softAP(ap_default_ssid, psk, kSetupApChannel, 0, 4);
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

void helio_wifi_connect_sta_or_ap(unsigned long boot_start_ms) {
  Serial.println("SSID:" + ssid);
  Serial.println("Pass:" + password);
  if (ssid.length() == 0) {
    Serial.println(F("No WiFi credentials — starting setup AP."));
    helio_wifi_start_soft_ap(false);
    return;
  }
  /* AP+STA during join so http://192.168.4.1 works while STA is still trying (up to 15 s). */
  Serial.println(F("Saved WiFi — starting setup AP, then joining station."));
  helio_wifi_start_soft_ap(true);
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
    while (WiFi.status() != WL_CONNECTED && (millis() - boot_start_ms < 15000)) {
      helio_http_ensure_listening();
      for (int h = 0; h < 4; h++) {
        server.handleClient();
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
    helio_http_invalidate_binding();
    helio_http_ensure_listening();
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("Connected IP address: " + WiFi.localIP().toString() + " or <a href='http://" + helio_ap_ssid_storage +
                   ".local' >" + helio_ap_ssid_storage + ".local</a>");
  } else {
    Serial.println(F("Can not connect to WiFi station — setup AP remains active."));
    helio_http_ensure_listening();
    helio_captive_dns_start(WiFi.softAPIP());
  }
}

bool helio_wifi_soft_ap_setup_active(void) {
  return WiFi.getMode() != WIFI_STA && WiFi.softAPIP() != IPAddress(0, 0, 0, 0);
}
