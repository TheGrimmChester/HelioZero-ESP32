#pragma once

#include <ArduinoJson.h>

struct SelfTestPersisted {
  bool pending = false;
  bool skipped = false;
  bool zc_ok = false;
  bool triac_ok = false;
  bool source_ok = false;
  uint32_t run_epoch = 0;
  uint16_t zc_edges_per_sec = 0;
};

extern SelfTestPersisted g_self_test;

void helio_self_test_set_pending(bool pending);
void helio_self_test_tick(unsigned long now_ms);
void helio_self_test_start_run();
void helio_self_test_skip();
void helio_self_test_append_health_json(JsonObject obj);
