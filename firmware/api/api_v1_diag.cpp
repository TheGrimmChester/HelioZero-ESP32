/*
 * Diagnostics API — config audit log, commissioning self-test (IDEA-D1, D7).
 */
#include "api_v1_common.h"
#include "helio_config_audit.h"
#include "helio_self_test.h"

void handle_get_system_audit() {
  API_AUTH_GUARD();
  StaticJsonDocument<2048> doc;
  JsonArray entries = doc.createNestedArray("entries");
  helio_config_audit_append_json(entries, 20);
  String out;
  serializeJson(doc, out);
  api_send_json(server, 200, out);
}

void handle_post_health_self_test_run() {
  API_AUTH_GUARD();
  helio_self_test_start_run();
  api_send_json(server, 200, "{\"ok\":true,\"running\":true}");
}

void handle_post_health_self_test_skip() {
  API_AUTH_GUARD();
  helio_self_test_skip();
  persistConfigToEeprom();
  api_send_json(server, 200, "{\"ok\":true,\"skipped\":true}");
}
