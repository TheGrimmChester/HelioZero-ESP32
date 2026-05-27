/*
 * helio_source_logic.cpp — Wire label table and capability predicates (native tests).
 */
#include "helio_source_logic.h"

#include <cstring>

struct SourceWireRow {
  SourceId id;
  const char *wire;
};

static const char kWireOutOfRange[] = "";

static const SourceWireRow kWireTable[] = {
    {SourceId::UxIx2, "UxIx2"},   {SourceId::UxIx3, "UxIx3"},   {SourceId::UxI, "UxI"},
    {SourceId::Linky, "Linky"},   {SourceId::Enphase, "Enphase"}, {SourceId::ShellyEm, "ShellyEm"},
    {SourceId::ShellyPro, "ShellyPro"}, {SourceId::SmartG, "SmartG"}, {SourceId::HomeW, "HomeW"},
    {SourceId::Pmqtt, "Pmqtt"},   {SourceId::NotDef, "NotDef"},   {SourceId::Ext, "Ext"},
};

size_t helio_source_logic_registry_count() { return sizeof(kWireTable) / sizeof(kWireTable[0]); }

const char *helio_source_logic_wire_at(size_t i) {
  if (i >= helio_source_logic_registry_count()) return kWireOutOfRange;
  return kWireTable[i].wire;
}

SourceId helio_source_logic_parse_wire(const char *wire) {
  if (!wire) return SourceId::Unknown;
  for (const auto &row : kWireTable) {
    if (strcmp(wire, row.wire) == 0) return row.id;
  }
  return SourceId::Unknown;
}

SourceId helio_source_logic_effective_id(SourceId active, const char *source_data_wire) {
  if (active == SourceId::Ext) return helio_source_logic_parse_wire(source_data_wire);
  return active;
}

bool helio_source_logic_cap_mqtt_triac_channel_block_for(SourceId id) {
  return id == SourceId::UxIx2 || id == SourceId::UxIx3 || id == SourceId::ShellyEm;
}

bool helio_source_logic_cap_mqtt_linky_tariff_for(SourceId id, bool tempo_rte_enabled) {
  return id == SourceId::Linky || tempo_rte_enabled;
}

bool helio_source_logic_cap_serial_adc_gpio_restrict_for(SourceId id) {
  return id == SourceId::UxI || id == SourceId::UxIx3;
}

uint16_t helio_source_logic_base_poll_period_ms(SourceId id, uint32_t uxix3_serial_baud) {
  switch (id) {
    case SourceId::UxIx2:
      return 400;
    case SourceId::UxIx3:
      return (uxix3_serial_baud == 19200u) ? 500 : 800;
    case SourceId::UxI:
      return 40;
    case SourceId::Linky:
      return 2;
    case SourceId::Enphase:
      return 600;
    case SourceId::ShellyEm:
    case SourceId::ShellyPro:
    case SourceId::SmartG:
    case SourceId::HomeW:
      return 300;
    case SourceId::Ext:
      return 800;
    case SourceId::Pmqtt:
    case SourceId::NotDef:
      return 600;
    default:
      return 1000;
  }
}

