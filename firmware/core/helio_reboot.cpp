#include "helio_reboot.h"
#include <esp_system.h>

static bool restartPending = false;
static uint32_t restartAtMillis = 0;

void RequestReboot(uint32_t delayMs) {
  restartPending = true;
  restartAtMillis = millis() + delayMs;
}

void helio_reboot_poll(void) {
  if (!restartPending) {
    return;
  }
  if ((long)(millis() - restartAtMillis) < 0) {
    return;
  }
  restartPending = false;
  ESP.restart();
}
