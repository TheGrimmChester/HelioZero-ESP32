/*
 * api_util.cpp — API session auth, JSON helpers, restricted GPIO matrix for API routes.
 * See: /en/http-api-security/; helio_source capability flags for per-source GPIO rules.
 */
#include "api_util.h"

#include "app_wifi_setup.h"
#include "api_access_token.h"
#include "api_session_logic.h"
#include "api_util_logic.h"
#include "helio_globals.h"
#include "helio_source.h"
#include <esp_random.h>
#include <string>
#include <ArduinoJson.h>
#include <IPAddress.h>
#include <WiFi.h>
#include <cstring>

static String g_apiSessionToken;

bool IsRestrictedGpioRead(int gpio) { return api_logic_is_restricted_gpio_read(gpio); }

bool IsRestrictedGpioWrite(int gpio) {
  if (pwmGpio >= 0 && gpio == pwmGpio) return false;
  return api_logic_is_restricted_gpio_write(gpio, helio_cap_serial_adc_gpio_restrict());
}

String ip32ToDotted(uint32_t ip) { return String(api_logic_ip32_to_dotted(ip).c_str()); }

bool dottedToIp32(const char *s, uint32_t &out) { return api_logic_dotted_to_ip32(s, out); }

void api_apply_cors_headers(WebServer &server) {
  if (!httpCorsEnabled) return;
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (server.method() == HTTP_OPTIONS) {
    server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Authorization, Content-Type, Accept");
    server.sendHeader("Access-Control-Max-Age", "86400");
  }
}

bool api_try_handle_cors_preflight(WebServer &server) {
  const bool isOptions = server.method() == HTTP_OPTIONS;
  if (!api_logic_should_handle_cors_preflight(httpCorsEnabled, isOptions, server.uri().c_str())) {
    return false;
  }
  api_apply_cors_headers(server);
  server.sendHeader("Cache-Control", "no-store");
  server.send(204);
  return true;
}

void api_send_json(WebServer &server, int code, const String &json) {
  if (httpCorsEnabled && server.method() == HTTP_GET) {
    api_apply_cors_headers(server);
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", json);
}

void api_error(WebServer &server, int code, const char *err, const char *msg) {
  StaticJsonDocument<256> doc;
  doc["error"] = err;
  doc["message"] = msg;
  String out;
  serializeJson(doc, out);
  api_send_json(server, code, out);
}

bool api_require_json_body(WebServer &server, String &body, size_t maxLen, bool patch) {
  (void)patch;
  if (!server.hasArg("plain")) {
    api_error(server, 400, "bad_request", "JSON body required (Content-Type: application/json)");
    return false;
  }
  body = server.arg("plain");
  if (body.length() > maxLen) {
    api_error(server, 413, "payload_too_large", "body exceeds limit");
    return false;
  }
  return true;
}

bool api_auth_enabled() { return httpApiPassword.length() > 0; }

void api_wifi_public_status(bool &staMode, bool &connected) {
  const bool staUp =
      WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0);
  if (staUp) {
    staMode = true;
    connected = true;
    return;
  }
  staMode = false;
  connected = false;
  if (WiFi.getMode() != WIFI_STA && WiFi.softAPIP() != IPAddress(0, 0, 0, 0)) {
    staMode = false;
    connected = false;
  }
}

bool api_validate_password_ascii(const String &s, String &err) {
  std::string errStd;
  const bool ok = api_logic_validate_password_ascii(std::string(s.c_str()), errStd);
  if (!ok) err = String(errStd.c_str());
  return ok;
}

void api_session_clear() { g_apiSessionToken = ""; }

static String api_session_generate_token() {
  uint8_t bytes[32];
  esp_fill_random(bytes, sizeof(bytes));
  static const char hex[] = "0123456789abcdef";
  char out[65];
  for (size_t i = 0; i < sizeof(bytes); i++) {
    out[i * 2] = hex[(bytes[i] >> 4) & 0x0f];
    out[i * 2 + 1] = hex[bytes[i] & 0x0f];
  }
  out[64] = '\0';
  return String(out);
}

bool api_session_issue(String &tokenOut) {
  tokenOut = api_session_generate_token();
  g_apiSessionToken = tokenOut;
  return true;
}

bool api_session_validate_password(const String &password) {
  return api_logic_password_constant_time_eq(std::string(password.c_str()),
                                             std::string(httpApiPassword.c_str()));
}

static bool api_session_header_valid(const String &header) {
  if (g_apiSessionToken.length() == 0) return false;
  std::string token;
  if (!api_logic_parse_bearer_token(std::string(header.c_str()), token)) return false;
  return api_logic_session_token_constant_time_eq(token, std::string(g_apiSessionToken.c_str()));
}

/** AP setup: allow clearing HTTP API password without knowing the old one. */
static bool api_auth_ap_clear_http_password(WebServer &server) {
  if (server.method() != HTTP_PUT) return false;
  if (server.uri() != "/api/v1/system/http-auth") return false;
  if (!server.hasArg("plain")) return false;
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, server.arg("plain"))) return false;
  if (!doc.containsKey("password")) return false;
  const char *pw = doc["password"] | "";
  return pw[0] == '\0';
}

/** UI bootstrap reads — no secrets; always open when API auth is enabled. */
static bool api_auth_public_read(WebServer &server) {
  if (server.method() != HTTP_GET) return false;
  const String &uri = server.uri();
  return uri == "/api/v1/public";
}

static bool api_auth_login_route(WebServer &server) {
  return server.method() == HTTP_POST && server.uri() == "/api/v1/auth/login";
}

/** AP / first-run setup: restore routes stay open without session (soft-AP only). */
static bool api_auth_ap_setup_bootstrap(WebServer &server) {
  if (!helio_wifi_soft_ap_setup_active()) return false;
  const String &uri = server.uri();
  if (uri == "/api/v1/wifi") return true;
  if (uri.startsWith("/api/v1/wifi/")) return true;
  if (uri == "/api/v1/config" || uri == "/api/v1/actions/config" || uri == "/api/v1/time") {
    return true;
  }
  if (uri == "/api/v1/system/backup" && (server.method() == HTTP_GET || server.method() == HTTP_PUT)) {
    return true;
  }
  if (api_auth_ap_clear_http_password(server)) return true;
  if (uri == "/api/v1/system/http-auth" && server.method() == HTTP_PUT && !api_auth_enabled()) {
    return true;
  }
  if (server.method() == HTTP_POST && uri == "/api/v1/system/factory-reset") return true;
  if (server.method() == HTTP_POST && uri == "/api/v1/system/reboot") return true;
  return false;
}

static void api_send_unauthorized(WebServer &server) {
  api_error(server, 401, "unauthorized", "Valid API session required");
}

bool api_require_auth(WebServer &server) {
  if (!api_auth_enabled()) return true;
  if (api_logic_cors_preflight_bypasses_auth(httpCorsEnabled, server.method() == HTTP_OPTIONS,
                                             server.uri().c_str())) {
    return true;
  }
  if (api_auth_public_read(server)) return true;
  if (api_auth_login_route(server)) return true;
  if (api_auth_ap_setup_bootstrap(server)) return true;
  if (!server.hasHeader("Authorization")) {
    api_send_unauthorized(server);
    return false;
  }
  const String authHeader = server.header("Authorization");
  if (api_session_header_valid(authHeader)) return true;
  std::string token;
  if (api_logic_parse_bearer_token(std::string(authHeader.c_str()), token) &&
      api_access_tokens_verify_bearer(token)) {
    return true;
  }
  api_send_unauthorized(server);
  return false;
}
