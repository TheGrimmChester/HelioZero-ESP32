#pragma once

#include "helio_self_test.h"
#include "helio_triac_calibration_logic.h"
#include "storage_eeprom_backend.h"

int helio_diag_persist_read(int address, IEepromBackend &eeprom, SelfTestPersisted &self_test,
                          TriacCalibrationTable &triac_cal);
int helio_diag_persist_write(int address, IEepromBackend &eeprom, const SelfTestPersisted &self_test,
                           const TriacCalibrationTable &triac_cal);
