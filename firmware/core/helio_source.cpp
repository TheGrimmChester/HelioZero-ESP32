/*
 * helio_source.cpp — Poll dispatch to metering/*.cpp, HA wire labels, JSON diagnostics.
 * helio_metering_task calls helio_source_run_poll_cycle(); refresh Source after EEPROM/REST/MQTT.
 */
#include "helio_source.h"
#include "helio_source_logic.h"
#include "helio_source_health_logic.h"
#include "helio_diag.h"
#include "helio_forward.h"
#include "helio_globals.h"
#include "helio_mains_profile.h"
#include "api_util.h"
#include <ArduinoJson.h>

// ---- Implemented in metering/*.cpp ----
extern void uxi_probe_setup(void);
extern void jsy_mk194t_setup(void);
extern void jsy_mk333_setup(void);
extern void linky_meter_setup(void);
extern void enphase_envoy_setup(void);

extern void uxi_probe_poll(void);
extern void jsy_mk194t_poll(void);
extern void jsy_mk333_poll(void);
extern void linky_meter_poll(void);
extern void enphase_envoy_poll(void);
extern void smart_gateway_poll(void);
extern void shelly_em_poll(void);
extern void shelly_pro_em_poll(void);
extern void homewizard_poll(void);
extern void bench_sim_poll(void);
extern void external_peer_poll(void);

static void poll_nop(void) {}

enum : uint8_t {
  RSF_NONE = 0,
  RSF_POLL_BACKOFF = 1,
  RSF_TOUCH_LAST_MS = 2,
};

struct HelioSourceRow {
  SourceId id;
  const char *wire;
  void (*setup_fn)(void);
  void (*poll_fn)(void);
  uint16_t base_period_ms;
  uint8_t flags;
};

static SourceId g_active_source = SourceId::Unknown;

/** Row order matches Home Assistant MQTT `select` options (stable order). */
/** base_period_ms from helio_source_logic_base_poll_period_ms (UxIx3 baud-dependent). */
static const HelioSourceRow kRegistry[] = {
    {SourceId::UxIx2, "UxIx2", jsy_mk194t_setup, jsy_mk194t_poll, 400, RSF_NONE},
    {SourceId::UxIx3, "UxIx3", jsy_mk333_setup, jsy_mk333_poll, 800, RSF_NONE},
    {SourceId::UxI, "UxI", uxi_probe_setup, uxi_probe_poll, 40, RSF_NONE},
    {SourceId::Linky, "Linky", linky_meter_setup, linky_meter_poll, 2, RSF_NONE},
    {SourceId::Enphase, "Enphase", enphase_envoy_setup, enphase_envoy_poll, 600, (uint8_t)(RSF_POLL_BACKOFF | RSF_TOUCH_LAST_MS)},
    {SourceId::ShellyEm, "ShellyEm", nullptr, shelly_em_poll, 300, (uint8_t)(RSF_POLL_BACKOFF | RSF_TOUCH_LAST_MS)},
    {SourceId::ShellyPro, "ShellyPro", nullptr, shelly_pro_em_poll, 300, (uint8_t)(RSF_POLL_BACKOFF | RSF_TOUCH_LAST_MS)},
    {SourceId::SmartG, "SmartG", nullptr, smart_gateway_poll, 300, (uint8_t)(RSF_POLL_BACKOFF | RSF_TOUCH_LAST_MS)},
    {SourceId::HomeW, "HomeW", nullptr, homewizard_poll, 300, (uint8_t)(RSF_POLL_BACKOFF | RSF_TOUCH_LAST_MS)},
    {SourceId::Pmqtt, "Pmqtt", nullptr, poll_nop, 600, RSF_TOUCH_LAST_MS},
    {SourceId::NotDef, "NotDef", nullptr, bench_sim_poll, 600, RSF_NONE},
    {SourceId::Ext, "Ext", nullptr, external_peer_poll, 800, (uint8_t)(RSF_POLL_BACKOFF | RSF_TOUCH_LAST_MS)},
};

static const HelioSourceRow *row_for_id(SourceId id) {
  for (size_t i = 0; i < sizeof(kRegistry) / sizeof(kRegistry[0]); i++) {
    if (kRegistry[i].id == id) {
      return &kRegistry[i];
    }
  }
  return nullptr;
}

static SourceId parse_wire(const String &w) { return helio_source_logic_parse_wire(w.c_str()); }

void helio_active_source_refresh_from_global_string() {
  g_active_source = parse_wire(Source);
}

SourceId helio_active_source_get() {
  return g_active_source;
}

SourceId helio_effective_meter_id() {
  return helio_source_logic_effective_id(g_active_source, Source_data.c_str());
}

size_t helio_source_registry_count() { return helio_source_logic_registry_count(); }

const char *helio_source_wire_at(size_t i) { return helio_source_logic_wire_at(i); }

const char *helio_source_wire_for_id(SourceId id) {
  const HelioSourceRow *row = row_for_id(id);
  return row ? row->wire : "";
}

bool helio_source_wire_supported(const String &wire) {
  return parse_wire(wire) != SourceId::Unknown;
}

void helio_source_apply_hardware_setup() {
  helio_active_source_refresh_from_global_string();
  const HelioSourceRow *row = row_for_id(g_active_source);
  if (row && row->setup_fn) {
    row->setup_fn();
  }
  if (g_active_source == SourceId::UxIx3) {
    delay(100);
    jsy_mk333_send_request();
    poll_period_ms = helio_source_logic_base_poll_period_ms(SourceId::UxIx3, UxIx3SerialBaud);
  }
  if (g_active_source != SourceId::Ext) {
    Source_data = Source;
  }
}

void helio_source_run_poll_cycle(unsigned long pollBackoffMs) {
  const HelioSourceRow *row = row_for_id(g_active_source);
  if (!row) {
    poll_period_ms = 1000;
    return;
  }
  if (row->poll_fn) {
    row->poll_fn();
  }
  if (row->flags & RSF_TOUCH_LAST_MS) {
    last_metering_task_ms = millis();
  }
  uint32_t p = helio_source_logic_base_poll_period_ms(g_active_source, UxIx3SerialBaud);
  if (row->flags & RSF_POLL_BACKOFF) {
    p += (uint32_t)pollBackoffMs;
  }
  poll_period_ms = p;
  if (mains_frequency_hz >= 45.0f && mains_frequency_hz <= 65.0f) {
    helio_mains_on_meter_frequency(mains_frequency_hz);
  }
  bool poll_ok = true;
  if (g_active_source == SourceId::Ext) {
    poll_ok = ext_peer_last_poll_ok;
  } else {
    poll_ok = time_sync_valid;
  }
  helio_diag_on_source_poll_result(poll_ok);
}

void helio_sources_diagnostics_append_summary(JsonObject root) {
  JsonObject diag = root.createNestedObject("diagnostics");
  const SourceId active = helio_active_source_get();
  const SourceId eff = helio_effective_meter_id();
  diag["active_source"] = helio_source_wire_for_id(active);
  diag["meter"] = helio_source_wire_for_id(eff);
  if (active == SourceId::Ext && Source_data.length() > 0) {
    diag["source_data"] = Source_data;
  }
  diag["poll_period_ms"] = poll_period_ms;
  if (last_metering_task_ms > 0) {
    diag["last_poll_ms_ago"] = (int)((millis() - last_metering_task_ms) & 0x7FFFFFFF);
  } else {
    diag["last_poll_ms_ago"] = -1;
  }
  if (active == SourceId::Ext) {
    diag["last_poll_ok"] = ext_peer_last_poll_ok;
    diag["last_error"] = ext_peer_last_poll_err;
    diag["protocol_used"] = ext_peer_last_poll_protocol.length() ? ext_peer_last_poll_protocol : "json";
    diag["ext_protocol"] = ext_peer_protocol.length() ? ext_peer_protocol : "json";
    if (ext_peer_last_poll_ms > 0) {
      diag["transport_last_poll_ms_ago"] =
          (int)((millis() - ext_peer_last_poll_ms) & 0x7FFFFFFF);
    } else {
      diag["transport_last_poll_ms_ago"] = -1;
    }
  } else {
    diag["last_poll_ok"] = time_sync_valid;
    diag["last_error"] = "";
  }
  diag["mains_frequency_hz"] = helio_mains_effective_frequency_hz();
  diag["mains_frequency_source"] = helio_mains_frequency_source_string();
  const char *warn = helio_mains_frequency_warning_string();
  if (warn && warn[0]) {
    diag["mains_frequency_warning"] = warn;
  }
  SourceHealthScoreInput hin;
  hin.last_poll_ms_ago = diag.containsKey("last_poll_ms_ago") ? diag["last_poll_ms_ago"].as<int>() : -1;
  hin.poll_period_ms = poll_period_ms;
  hin.last_poll_ok = diag["last_poll_ok"].as<bool>();
  hin.error_streak = g_source_error_streak;
  const SourceHealthScoreResult score = helio_source_health_logic_compute(hin);
  diag["health_score"] = score.health_score;
  JsonObject factors = diag.createNestedObject("health_score_factors");
  factors["freshness"] = score.freshness_pts;
  factors["poll_ok"] = score.poll_ok_pts;
  factors["streak"] = score.streak_pts;
  if (helio_diag_uxi_adc_clipping_active()) {
    diag["adc_clipping"] = true;
  }
  if (g_regulation_hunting_active) {
    diag["regulation_hunting"] = true;
  }
}

void helio_sources_diagnostics_append_meter_payload(JsonObject doc, int linky_tail_max) {
  const SourceId eff = helio_effective_meter_id();

  if (eff == SourceId::UxI || eff == SourceId::UxIx2) {
    JsonObject wf = doc.createNestedObject("uxi_waveform");
    JsonArray vArr = wf.createNestedArray("volt_m");
    JsonArray aArr = wf.createNestedArray("amp_m");
    int i0 = 0;
    for (int i = 0; i < 100; i++) {
      int i1 = (i + 1) % 100;
      if (voltM[i] <= 0 && voltM[i1] > 0) {
        i0 = i1;
        break;
      }
    }
    for (int i = 0; i < 100; i++) {
      int j = (i + i0) % 100;
      vArr.add(voltM[j]);
      aArr.add(ampM[j]);
    }
  }

  if (eff == SourceId::Linky) {
    JsonObject ly = doc.createNestedObject("linky");
    ly["ltarf"] = LTARF;
    ly["idx_raw"] = IdxDataRawLinky;
    int tailMax = linky_tail_max;
    if (tailMax <= 0 || tailMax > 2048) {
      tailMax = 768;
    }
    int idx = IdxDataRawLinky;
    int start = (idx - tailMax + 10000) % 10000;
    String tail;
    tail.reserve((unsigned)tailMax);
    int k = start;
    for (int i = 0; i < tailMax; i++) {
      char c = DataRawLinky[k];
      if (c == 0) {
        c = ' ';
      }
      if ((unsigned char)c < 32) {
        c = '.';
      }
      tail += c;
      k = (k + 1) % 10000;
    }
    ly["tail"] = tail;
    ly["tail_len"] = tailMax;
    ly["linky_eait_from_tic"] = LinkyEaitFromTic;
    ly["linky_sinsti_seen"] = LinkySinstiSeen;
    ly["cacsi_no_export"] =
        (!LinkySinstiSeen && !LinkyEaitFromTic && EASTvalid);
  }

  if (eff == SourceId::Enphase) {
    JsonObject ep = doc.createNestedObject("enphase");
    ep["user"] = EnphaseUser;
    ep["serial"] = meter_channel;
    ep["has_user"] = EnphaseUser.length() > 0;
    ep["has_session"] = Session_id.length() > 0;
    ep["has_token"] = TokenEnphase.length() > 50;
    ep["pact_prod_w"] = enphase_production_w;
    ep["pact_conso_w"] = enphase_house_active_w;
  }

  if (eff == SourceId::ShellyEm) {
    JsonObject sh = doc.createNestedObject("shelly_em");
    sh["raw"] = ShEm_rawData;
    sh["poll_count"] = shellyEmPollCounter;
  }

  if (eff == SourceId::SmartG) {
    JsonObject sg = doc.createNestedObject("smartg");
    sg["raw"] = SG_rawData;
  }

  if (eff == SourceId::HomeW) {
    JsonObject hw = doc.createNestedObject("homewizard");
    hw["raw"] = HW_rawData;
  }

  if (eff == SourceId::ShellyPro) {
    JsonObject sp = doc.createNestedObject("shelly_pro");
    sp["raw"] = ShPro_rawData;
  }

  if (eff == SourceId::UxIx3) {
    JsonObject j3 = doc.createNestedObject("jsy333");
    j3["raw"] = MK333_rawData;
    j3["serial_baud"] = UxIx3SerialBaud;
  }

  if (helio_active_source_get() == SourceId::Pmqtt) {
    JsonObject pm = doc.createNestedObject("pmqtt");
    pm["topic"] = PmqttTopic;
    pm["schema"] = PmqttSchema;
    pm["last_pw"] = PwMQTT_last;
  }

  if (helio_active_source_get() == SourceId::Ext) {
    JsonObject ext = doc.createNestedObject("ext");
    ext["ext_peer_ip"] = ip32ToDotted(ext_peer_ip);
    ext["ext_peer_port"] = ext_peer_port;
    ext["ext_peer_path"] = ext_peer_path;
    ext["ext_protocol"] = ext_peer_protocol.length() ? ext_peer_protocol : "json";
    ext["last_poll_ok"] = ext_peer_last_poll_ok;
    ext["last_poll_ms_ago"] =
        (ext_peer_last_poll_ms > 0) ? (int)((millis() - ext_peer_last_poll_ms) & 0x7FFFFFFF) : -1;
    ext["last_error"] = ext_peer_last_poll_err;
    ext["last_frame_preview"] = ext_peer_last_poll_preview;
    ext["protocol_used"] = ext_peer_last_poll_protocol.length() ? ext_peer_last_poll_protocol : "json";
  }
}

void helio_sources_brute_panel_json(String &out) {
  StaticJsonDocument<512> d;
  const SourceId eff = helio_effective_meter_id();
  d["panel"] = helio_source_wire_for_id(eff);
  JsonObject p = d.createNestedObject("panels");
  p["uxi"] = (eff == SourceId::UxI);
  p["uxix2"] = (eff == SourceId::UxIx2);
  p["uxix3"] = (eff == SourceId::UxIx3);
  p["linky"] = (eff == SourceId::Linky);
  p["enphase"] = (eff == SourceId::Enphase);
  p["smartg"] = (eff == SourceId::SmartG);
  p["shelly_em"] = (eff == SourceId::ShellyEm);
  p["homewizard"] = (eff == SourceId::HomeW);
  p["shelly_pro"] = (eff == SourceId::ShellyPro);
  p["pmqtt"] = (eff == SourceId::Pmqtt);
  p["notdef"] = (eff == SourceId::NotDef);
  serializeJson(d, out);
}

bool helio_cap_mqtt_triac_channel_block_for(SourceId id) {
  return helio_source_logic_cap_mqtt_triac_channel_block_for(id);
}

bool helio_cap_mqtt_triac_channel_block() {
  return helio_cap_mqtt_triac_channel_block_for(g_active_source);
}

bool helio_cap_mqtt_linky_tariff_for(SourceId id) {
  return helio_source_logic_cap_mqtt_linky_tariff_for(id, tempoRteEnabled);
}

bool helio_cap_mqtt_linky_tariff() { return helio_cap_mqtt_linky_tariff_for(g_active_source); }

bool helio_cap_serial_adc_gpio_restrict_for(SourceId id) {
  return helio_source_logic_cap_serial_adc_gpio_restrict_for(id);
}

bool helio_cap_serial_adc_gpio_restrict() {
  return helio_cap_serial_adc_gpio_restrict_for(g_active_source);
}

