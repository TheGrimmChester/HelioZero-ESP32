#pragma once

/*
 * helio_source_logic.h — Pure source registry/capabilities (host-testable, no Arduino I/O).
 * Maps wire strings (UxI, Linky, Ext, …) to SourceId and MQTT/REST/GPIO capability flags.
 */

#include "helio_source_types.h"

#include <cstddef>

size_t helio_source_logic_registry_count();
const char *helio_source_logic_wire_at(size_t i);
SourceId helio_source_logic_parse_wire(const char *wire);
SourceId helio_source_logic_effective_id(SourceId active, const char *source_data_wire);

bool helio_source_logic_cap_mqtt_triac_channel_block_for(SourceId id);
/** True when CH2 metering should appear on REST/MQTT state snapshot (any source). */
bool helio_source_logic_second_channel_snapshot_visible(float voltage_v, int active_import_w,
                                                        int active_export_w, float current_a);
bool helio_source_logic_cap_mqtt_linky_tariff_for(SourceId id, bool tempo_rte_enabled);
bool helio_source_logic_cap_serial_adc_gpio_restrict_for(SourceId id);

/** Default base poll period (ms) before house-import backoff. UxIx3: 500 @ 19200 baud, else 800. */
uint16_t helio_source_logic_base_poll_period_ms(SourceId id, uint32_t uxix3_serial_baud);
