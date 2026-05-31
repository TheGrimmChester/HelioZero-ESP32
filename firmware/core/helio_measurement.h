#pragma once

#include "helio_pub.h"

/** Stable alias for published meter readings (same layout as HelioPublic). */
using MeasurementSnapshot = HelioPublic;

/** Last coherent snapshot after `HelioPublishFromGlobals()` (core-0 task). */
extern MeasurementSnapshot g_lastRmsMeasurement;

void helio_measurement_refresh_last();
