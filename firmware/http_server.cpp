/*
 * http_server.cpp — SPA at GET / and REST /api/v1/* (api_v1_routes.cpp).
 * See: /en/developer/ § Control interfaces
 */
#include "helio_globals.h"
#include "helio_forward.h"
#include "helio_app.h"
void handleNotFound(void);

void handleRoot(void);

#include "api.h"
#include "web_ui.h"
#include "api_util.h"
#include "app_wifi_setup.h"

static bool httpListening = false;
static IPAddress httpBoundIp;

static void helio_http_send_wifi_portal_redirect(void) {
  server.sendHeader("Location", "/wifi");
  server.send(302, "text/plain", "");
}

/** OS captive-portal probes — answer quickly so the phone opens the setup UI. */
static void handle_captive_portal_apple(void) {
  server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
}

static void handle_captive_portal_android204(void) {
  server.send(204, "text/plain", "");
}

static void handle_captive_portal_windows(void) {
  server.send(200, "text/plain", "Microsoft Connect Test");
}

static void handle_spa_deep_link(void) { WebUi_sendSpa(); }

static void handle_pwa_manifest(void) { WebUi_sendManifest(); }
static void handle_pwa_sw(void) { WebUi_sendServiceWorker(); }
static void handle_pwa_icon192(void) { WebUi_sendPwaIcon192(); }
static void handle_pwa_icon512(void) { WebUi_sendPwaIcon512(); }

void helio_http_invalidate_binding(void) {
  if (httpListening) {
    server.stop();
  }
  httpListening = false;
  httpBoundIp = IPAddress(0, 0, 0, 0);
}

/** Bind :80 on setup AP (192.168.4.1) or STA LAN IP; rebind when AP is disabled after join. */
void helio_http_ensure_listening(void) {
  IPAddress listenIp(0, 0, 0, 0);
  if (helio_wifi_soft_ap_setup_active()) {
    listenIp = WiFi.softAPIP();
  } else if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    listenIp = WiFi.localIP();
  } else if (WiFi.softAPIP() != IPAddress(0, 0, 0, 0)) {
    listenIp = WiFi.softAPIP();
  }
  if (listenIp == IPAddress(0, 0, 0, 0)) {
    helio_http_invalidate_binding();
    return;
  }
  if (!httpListening || listenIp != httpBoundIp) {
    if (httpListening) {
      server.stop();
      delay(20);
    }
    server.begin();
    httpListening = true;
    httpBoundIp = listenIp;
    Serial.print(F("HTTP server on port 80 — "));
    Serial.println(listenIp.toString());
  }
}

void Init_Server() {
  server.on("/", handleRoot);
  server.on("/manifest.webmanifest", HTTP_GET, handle_pwa_manifest);
  server.on("/sw.js", HTTP_GET, handle_pwa_sw);
  server.on("/pwa/icon-192.png", HTTP_GET, handle_pwa_icon192);
  server.on("/pwa/icon-512.png", HTTP_GET, handle_pwa_icon512);
  server.on("/wifi", HTTP_GET, handle_spa_deep_link);
  server.on("/login", HTTP_GET, handle_spa_deep_link);
  server.on("/generate_204", HTTP_GET, handle_captive_portal_android204);
  server.on("/gen_204", HTTP_GET, handle_captive_portal_android204);
  server.on("/hotspot-detect.html", HTTP_GET, handle_captive_portal_apple);
  server.on("/library/test/success.html", HTTP_GET, handle_captive_portal_apple);
  server.on("/connecttest.txt", HTTP_GET, handle_captive_portal_windows);
  server.on("/ncsi.txt", HTTP_GET, handle_captive_portal_windows);
  server.on("/redirect", HTTP_GET, helio_http_send_wifi_portal_redirect);
  Init_ApiRoutes();
  server.onNotFound(handleNotFound);
  helio_http_ensure_listening();
  Debug.println("HTTP server started");
}

void handleRoot() {
  const bool staUp =
      WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0);
  if (!staUp) {
    server.sendHeader("Location", "/wifi");
    server.send(302, "text/plain", "");
    return;
  }
  WebUi_sendSpa();
}

void handleNotFound() {
  if (api_try_handle_cors_preflight(server)) return;
  if (Api_handle_actions_config_subresource()) return;
  if (Api_handle_auth_tokens_subresource()) return;
  if (WebUi_trySpaFallback()) return;

  Debug.println(F("File not found"));
  String message;
  message.reserve(256);
  message = "File not found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}
