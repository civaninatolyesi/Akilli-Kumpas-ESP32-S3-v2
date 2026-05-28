#pragma once

#include <Arduino.h>

void webBegin();
void webLoop();

// Call this whenever you have an accepted/stable measurement.
void webUpdateMeasurement(uint32_t rawKey, float mm, float inch, bool isInch, bool isNegative);

// Call this when button state changes (true = pressed).
void webUpdateButton(bool isPressed);

// Call this when battery voltage is updated.
void webUpdateBattery(float volts, uint16_t adcMv, uint8_t percent, bool isCharging);

// Append a log line to the web UI log.
void webLogf(const char *fmt, ...);
