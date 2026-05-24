#pragma once

#include <cstdint>

/** Canonical measurement / ingress backends (wire string == EEPROM / HA select value). */
enum class SourceId : uint8_t {
  Unknown = 0,
  UxI,
  UxIx2,
  UxIx3,
  Linky,
  Enphase,
  SmartG,
  ShellyEm,
  ShellyPro,
  HomeW,
  NotDef,
  Pmqtt,
  Ext,
};
