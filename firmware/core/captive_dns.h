#pragma once
#include <Arduino.h>
#include <IPAddress.h>

/** Start wildcard DNS on the soft-AP (captive portal). No-op if already running. */
void helio_captive_dns_start(const IPAddress &apIp);

/** Stop DNS when leaving AP setup mode. */
void helio_captive_dns_stop(void);

/** Call from `helio_loop` while the setup AP is active. */
void helio_captive_dns_process(void);

bool helio_captive_dns_active(void);
