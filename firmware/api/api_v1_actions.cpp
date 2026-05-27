/*
 * Auto-split from api_v1_routes.cpp — see api_v1_common.h
 */
#include "api_v1_common.h"
#include "helio_regulation_modes.h"
void handle_get_actions_live() {
  API_AUTH_GUARD();
  DynamicJsonDocument doc(2048);
  doc["temperature_c"] = temperature;
  doc["source"] = Source_data;
  doc["ext_peer_ip"] = ip32ToDotted(ext_peer_ip);
  doc["ext_peer_port"] = ext_peer_port;
  doc["ext_peer_path"] = ext_peer_path;
  int nb = 0;
  for (int i = 0; i < NbActions; i++) {
    if (action_regulation_enabled(load_channels[i].Actif)) nb++;
  }
  doc["active_actions_count"] = nb;
  JsonArray slots = doc.createNestedArray("active_slots");
  api_action_append_live_state(slots);
  String out;
  serializeJson(doc, out);
  api_send_json(server,200, out);
}

void handle_get_actions_schema() {
  API_AUTH_GUARD();
  DynamicJsonDocument doc(1536);
  api_action_append_schema(doc.to<JsonObject>());
  String out;
  serializeJson(doc, out);
  api_send_json(server,200, out);
}

void handle_get_actions_config() {
  API_AUTH_GUARD();
  DynamicJsonDocument doc(12288);
  doc["schema_version"] = API_ACTION_SCHEMA_VERSION;
  doc["nb_actions"] = NbActions;
  JsonArray ar = doc.createNestedArray("actions");
  api_action_append_config_array(ar);
  String out;
  serializeJson(doc, out);
  api_send_json(server,200, out);
}

void handle_put_actions_config() {
  API_AUTH_GUARD();
  String body;
  if (!api_require_json_body(server, body, kPutBodyMax, false)) return;
  String err;
  if (!api_action_put_collection(body, err)) {
    api_error(server,400, "validation", err.c_str());
    return;
  }
  StaticJsonDocument<64> o;
  o["ok"] = true;
  String s;
  serializeJson(o, s);
  api_send_json(server,200, s);
}

void handle_patch_actions_config_batch() {
  API_AUTH_GUARD();
  String body;
  if (!api_require_json_body(server, body, kPatchBodyMax, true)) return;
  String err;
  if (!api_action_patch_collection_batch(body, err)) {
    api_error(server,400, "validation", err.c_str());
    return;
  }
  StaticJsonDocument<64> o;
  o["ok"] = true;
  String s;
  serializeJson(o, s);
  api_send_json(server,200, s);
}

void handle_get_action_override(int idx) {
  API_AUTH_GUARD();
  DynamicJsonDocument doc(256);
  api_append_action_override(doc.to<JsonObject>(), idx);
  String out;
  serializeJson(doc, out);
  api_send_json(server, 200, out);
}

void handle_post_action_override(int idx) {
  API_AUTH_GUARD();
  String body;
  if (!api_require_json_body(server, body, 256, true)) return;
  StaticJsonDocument<256> doc;
  DeserializationError e = deserializeJson(doc, body);
  if (e) {
    api_error(server, 400, "json_parse", e.c_str());
    return;
  }
  const char *state = doc["state"] | "auto";
  int triacPercent = (int)(doc["triac_open_percent"] | 0);
  unsigned long durationSec = (unsigned long)(doc["duration_s"] | 0);
  String err;
  if (!ApiSetActionOverride(idx, state, triacPercent, durationSec, err)) {
    api_error(server, 400, "validation", err.c_str());
    return;
  }
  DynamicJsonDocument outDoc(256);
  JsonObject o = outDoc.to<JsonObject>();
  o["ok"] = true;
  JsonObject ov = o.createNestedObject("override");
  api_append_action_override(ov, idx);
  String out;
  serializeJson(outDoc, out);
  api_send_json(server, 200, out);
}

void handle_clear_action_override(int idx) {
  API_AUTH_GUARD();
  String err;
  if (!ApiSetActionOverride(idx, "auto", 0, 0, err)) {
    api_error(server, 400, "validation", err.c_str());
    return;
  }
  api_send_json(server, 200, "{\"ok\":true,\"state\":\"auto\"}");
}

