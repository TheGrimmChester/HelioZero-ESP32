/*
 * Auto-split from api_v1_routes.cpp — see api_v1_common.h
 */
#include "api_v1_common.h"
bool parse_uri_index(const String &uri, const char *prefix, int &idxOut) {
  if (!uri.startsWith(prefix)) return false;
  String tail = uri.substring(String(prefix).length());
  if (tail.length() == 0) return false;
  for (unsigned i = 0; i < tail.length(); i++) {
    if (!isdigit((unsigned char)tail[i])) return false;
  }
  idxOut = tail.toInt();
  return true;
}

bool parse_action_runtime_uri(const String &uri, int &idxOut, String &tailOut) {
  const char *prefix = "/api/v1/actions/";
  if (!uri.startsWith(prefix)) return false;
  String tail = uri.substring(String(prefix).length());
  if (tail.startsWith("config/")) return false;
  int slash = tail.indexOf('/');
  if (slash < 0) return false;
  String idxStr = tail.substring(0, slash);
  if (idxStr.length() == 0) return false;
  for (unsigned i = 0; i < idxStr.length(); i++) {
    if (!isdigit((unsigned char)idxStr[i])) return false;
  }
  idxOut = idxStr.toInt();
  tailOut = tail.substring(slash + 1);
  return true;
}

bool Api_handle_actions_config_subresource() {
  String uri = server.uri();
  int runtimeIdx = -1;
  String runtimeTail;
  if (parse_action_runtime_uri(uri, runtimeIdx, runtimeTail)) {
    API_AUTH_GUARD_R();
    if (runtimeIdx < 0 || runtimeIdx >= kMaxRoutingActions) {
      api_error(server,404, "not_found", "action index out of range");
      return true;
    }
    int m = server.method();
    if (runtimeTail == "override") {
      if (m == HTTP_GET) {
        handle_get_action_override(runtimeIdx);
        return true;
      }
      if (m == HTTP_POST) {
        handle_post_action_override(runtimeIdx);
        return true;
      }
      api_error(server,405, "method_not_allowed", "use GET or POST");
      return true;
    }
    if (runtimeTail == "override/clear") {
      if (m == HTTP_POST) {
        handle_clear_action_override(runtimeIdx);
        return true;
      }
      api_error(server,405, "method_not_allowed", "use POST");
      return true;
    }
    return false;
  }
  const char *pfx = "/api/v1/actions/config/";
  int idx = -1;
  if (!parse_uri_index(uri, pfx, idx)) return false;
  API_AUTH_GUARD_R();
  if (idx < 0 || idx >= kMaxRoutingActions) {
    api_error(server,404, "not_found", "action index out of range");
    return true;
  }
  int m = server.method();
  if (m == HTTP_GET) {
    if (idx >= NbActions) {
      api_error(server,404, "not_found", "no action at index");
      return true;
    }
    DynamicJsonDocument doc(6144);
    JsonObject o = doc.to<JsonObject>();
    api_action_append_one_config(o, idx);
    String out;
    serializeJson(doc, out);
    api_send_json(server,200, out);
    return true;
  }
  if (m == HTTP_PUT) {
    String body;
    if (!api_require_json_body(server, body, kPutBodyMax, false)) return true;
    String err;
    if (!api_action_put_one(idx, body, err)) {
      api_error(server,400, "validation", err.c_str());
      return true;
    }
    StaticJsonDocument<64> o;
    o["ok"] = true;
    String s;
    serializeJson(o, s);
    api_send_json(server,200, s);
    return true;
  }
  if (m == HTTP_PATCH) {
    String body;
    if (!api_require_json_body(server, body, kPatchBodyMax, true)) return true;
    String err;
    if (!api_action_patch_one(idx, body, err)) {
      api_error(server,400, "validation", err.c_str());
      return true;
    }
    StaticJsonDocument<64> o;
    o["ok"] = true;
    String s;
    serializeJson(o, s);
    api_send_json(server,200, s);
    return true;
  }
  api_error(server,405, "method_not_allowed", "use GET, PUT, or PATCH");
  return true;
}
