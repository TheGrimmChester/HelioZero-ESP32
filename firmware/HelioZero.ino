/*
  HelioZero — PV surplus router (ESP32), HelioZero multi-source.
  Licensed under the EUPL — see LICENSE in the repository root.
  Entry: setup() → helio_setup(); loop() → helio_loop() (core 1). Metering task on core 0.
  See: /en/project-overview/, FIRMWARE_BUILD.md. Logic: firmware/core/, firmware/metering/.
*/
#include "helio_board.h"
#include "helio_app.h"

void setup() {
  helio_setup();
}

void loop() {
  helio_loop();
}
