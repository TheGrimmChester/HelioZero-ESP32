#pragma once
#include <Arduino.h>

void eepromInit(void);
void eepromClearConsumptionHistory(void);
void eepromLoadMorningDayEnergy(void);
void helio_on_clock_tick(void);
void loadConfigFromEeprom(void);
unsigned long eepromReadLayoutKey(void);
int persistConfigToEeprom(void);
