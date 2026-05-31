#include "helio_lan_http_client.h"

#include "helio_http_response_logic.h"

#include <WiFi.h>

bool helio_lan_http_get(const String &host, uint16_t port, const String &path, String &body_out,
                        uint32_t timeout_ms) {
  body_out = "";
  WiFiClient client;
  if (!client.connect(host.c_str(), port)) {
    return false;
  }
  // HTTP/1.0 + Connection: close avoids chunked bodies on many public servers (parse bug otherwise).
  client.print(String("GET ") + path + " HTTP/1.0\r\nHost: " + host +
                 "\r\nConnection: close\r\nUser-Agent: HelioZero/1.0\r\nAccept: */*\r\n\r\n");

  const unsigned long start = millis();
  String wire;
  wire.reserve(768);
  while (client.connected() || client.available()) {
    if (client.available()) {
      while (client.available() && wire.length() < 8192) {
        wire += static_cast<char>(client.read());
      }
    }
    if (!client.connected() && !client.available()) {
      break;
    }
    if (millis() - start > timeout_ms) {
      client.stop();
      return false;
    }
    yield();
  }
  client.stop();

  const std::string body = helio_http_response_extract_body(std::string(wire.c_str()));
  if (body.empty()) {
    return false;
  }
  body_out = String(body.c_str());
  return true;
}
