#pragma once

#include <Arduino.h>

/** Schedule a reboot after `delayMs` (non-blocking; polled from `helio_loop`). */
void RequestReboot(uint32_t delayMs);
void helio_reboot_poll(void);
