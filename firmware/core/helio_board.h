#pragma once

/**
 * HelioZero — board profile for ESP32-WROOM-32 (ESP32-D0WDQ6, 4 MB flash typical).
 * GPIO mapping for the default HelioZero PCB; change only if your board differs.
 */

#ifndef Version
#define Version "0.1.0"
#endif
#ifndef HOSTNAME
#define HOSTNAME "HELIOZERO-"
#endif
#ifndef kEepromLayoutInit
/** Bump forces EEPROM factory re-init (gen-2 layout). */
#define kEepromLayoutInit 903157203UL
#endif

#define WDT_TIMEOUT_SEC 180
#define kMaxRoutingActions 20
#define MAX_SIZE_T 80

/** Triac dimmer output and zero-cross input (reference build: RBDimmer DIM / Z-C). */
constexpr int kTriacDimGpio = 22;
constexpr int kZeroCrossGpio = 23;

/* Analog probe inputs (ADC) */
#define AnalogIn0 35
#define AnalogIn1 32
#define AnalogIn2 33

/* UART2 — Linky or JSY-MK-194 (JsyMk194) */
#define RXD2 26
#define TXD2 27

#define LedYellow 18
#define LedGreen 19
#define pinTemp 13
