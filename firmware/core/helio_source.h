#pragma once

/*
 * helio_source.h — Metering source registry, dispatch, diagnostics JSON, capability matrix.
 * Active source: global Source string; HelioPeer uses Source_data for remote meter type.
 * See: /en/project-overview/ § Metering source; /en/hardware-pinout/ §17.
 */

#include <Arduino.h>
#include <ArduinoJson.h>

#include "helio_source_types.h"

/** Re-parse global `Source` into the active id (call after EEPROM load, REST, MQTT source/set, etc.). */
void helio_active_source_refresh_from_global_string();

SourceId helio_active_source_get();

/**
 * Meter profile for payloads and UI: local `Source`, except when `Source == "HelioPeer"` where the
 * remote meter type is carried in `Source_data`.
 */
SourceId helio_effective_meter_id();

/** Number of entries in the built-in source registry (wire labels / dispatch table). */
size_t helio_source_registry_count();

/** Wire string at index `i` (0 .. count-1); registry order matches HA MQTT source select. */
const char *helio_source_wire_at(size_t i);

/** Append `wire` string for `id`, or empty string if unknown. */
const char *helio_source_wire_for_id(SourceId id);

/** Fill `doc` with meter-specific nested objects (and Pmqtt/HelioPeer transport blocks). */
void helio_sources_diagnostics_append_meter_payload(JsonObject doc, int linky_tail_max);

/** Nested `diagnostics` object: poll timing, meter id, HelioPeer errors, mains frequency hints. */
void helio_sources_diagnostics_append_summary(JsonObject doc);

/** JSON `panel` wire label for the brute-data page (same semantics as `helio_effective_meter_id`). */
void helio_sources_brute_panel_json(String &out);

/** One-shot hardware setup for the current `Source` (Serial, etc.) and `Source_data` rule. */
void helio_source_apply_hardware_setup();

/**
 * Run one metering poll for the active source and update `poll_period_ms` / optional `last_metering_task_ms`.
 * @param pollBackoffMs same as `long(house_active_import_w / 10)` for HTTP-style sources.
 */
void helio_source_run_poll_cycle(unsigned long pollBackoffMs);

bool helio_source_wire_supported(const String &wire);

// --- Capability predicates (keep MQTT / REST / GPIO matrix in one place) ---

bool helio_cap_mqtt_triac_channel_block_for(SourceId id);
bool helio_cap_mqtt_triac_channel_block();

bool helio_cap_mqtt_linky_tariff_for(SourceId id);
bool helio_cap_mqtt_linky_tariff();

/** Serial/analog GPIO restrictions (see IsRestrictedGpioWrite). */
bool helio_cap_serial_adc_gpio_restrict_for(SourceId id);
bool helio_cap_serial_adc_gpio_restrict();

