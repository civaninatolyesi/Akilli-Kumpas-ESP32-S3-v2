#pragma once

#include <Arduino.h>

void webBegin();
void webLoop();

// Call this whenever you have an accepted/stable measurement.
void webUpdateMeasurement(uint32_t rawKey, float mm, float inch, bool isInch, bool isNegative);

// Call this when button state changes (true = pressed).
void webUpdateButton(bool isPressed);
