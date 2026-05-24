#include "helio_measurement.h"

MeasurementSnapshot g_lastRmsMeasurement;

void helio_measurement_refresh_last() {
  g_lastRmsMeasurement = HelioReadSnapshot();
}
