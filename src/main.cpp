#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <math.h>

#include "web_ui.h"

// ---- User config ----
static constexpr int CALIPER_DATA_PIN = 8; // GPIO8
static constexpr int CALIPER_CLK_PIN = 9;  // GPIO9 (interrupt)
static constexpr int BUTTON_PIN = 10;      // GPIO10 (pullup, active-high)
static constexpr int BATTERY_ADC_PIN = 5;  // GPIO5 ADC (LiPo via divider)

// Battery measurement (update to match your divider and cell range)
static constexpr float BAT_R1_OHMS = 100000.0f;  // top resistor (battery -> ADC)
static constexpr float BAT_R2_OHMS = 100000.0f;  // bottom resistor (ADC -> GND)
static constexpr float BATTERY_MIN_V = 3.0f;
static constexpr float BATTERY_MAX_V = 4.2f;
static constexpr uint32_t BATTERY_SAMPLE_MS = 500;
static constexpr uint8_t BATTERY_SAMPLES = 8;
static constexpr uint32_t BATTERY_SAMPLE_MS_CHARGING = 2000; // when charging, sample more often
static constexpr uint32_t BATTERY_SAMPLE_MS_IDLE = 2000;    // when idle, sample less often
static constexpr uint32_t BATTERY_REPORT_MAX_INTERVAL_MS = 5000; // force report at least every minute
static constexpr uint32_t CHARGE_WINDOW_MS = 30000;
static constexpr float CHARGE_MIN_DELTA_V = 0.010f;
static constexpr float LONG_CHARGE_MIN_DELTA_V = 0.010f;
static constexpr float CHARGE_NEAR_FULL_V = 4.15f;
// Long-term stability detection for charging inference
static constexpr uint32_t STABLE_PERIOD_MS = 120000;
static constexpr float STABLE_DELTA_V = 0.006f;

// Scale factors (adjust if readings are off by a constant factor)
// Many cheap 24-bit calipers use:
// - mm: 0.005mm per count  => 200 counts per mm
// - inch: 0.00025" per count => 4000 counts per inch
static constexpr float COUNTS_PER_MM = 200.0f;
static constexpr float COUNTS_PER_INCH = 4000.0f;

// If your caliper sends a burst roughly every ~100ms, a gap of a few ms
// between clock edges is a good frame delimiter.
static constexpr uint32_t FRAME_GAP_US = 1500;

// Ignore clock edges that are unrealistically close together (contact bounce / noise).
// Typical caliper clocks are in the hundreds of microseconds per edge, not < 20us.
static constexpr uint32_t MIN_EDGE_US = 5;

// Basic plausibility limit for output. Adjust to your caliper range.
static constexpr float MAX_ABS_MM = 500.0f;

// Some calipers toggle or use bits 22..23. We do NOT reject based on these by default.
static constexpr bool REJECT_IF_TOP_BITS_SET = false;

// Bit layout configuration (cheap 24-bit "Chinese caliper" variants differ here)
// If negative values never appear, try changing CALIPER_SIGN_BIT from 20 to 23.
static constexpr uint8_t CALIPER_VALUE_BITS = 20;   // usually 20
// Based on observed frames for this caliper: negative flag appears to be bit21.
static constexpr uint8_t CALIPER_SIGN_BIT = 21;     // try 20/23 on other variants
// IMPORTANT: On this caliper variant, bit21 appears to be the SIGN flag.
// Do not set UNIT bit to the same position as SIGN; otherwise negative values
// will be misinterpreted as "inch".
// If your caliper has a real unit bit, set it here. If not, keep this away
// from SIGN and rely on autoUnitFix.
static constexpr uint8_t CALIPER_UNIT_BIT = 23;     // often 21 on other variants (0=mm, 1=inch)

static constexpr uint32_t valueMask() {
  return (CALIPER_VALUE_BITS >= 32) ? 0xFFFFFFFFu : ((1u << CALIPER_VALUE_BITS) - 1u);
}

static float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static float batteryDividerRatio() {
  return (BAT_R1_OHMS + BAT_R2_OHMS) / BAT_R2_OHMS;
}

static uint8_t batteryPercentFromVolts(float v) {
  const float pct = (v - BATTERY_MIN_V) / (BATTERY_MAX_V - BATTERY_MIN_V);
  const float clamped = clampf(pct, 0.0f, 1.0f);
  return (uint8_t)(clamped * 100.0f + 0.5f);
}

// ---- Runtime layout (selectable over Serial for debugging) ----
static uint8_t g_signBit = CALIPER_SIGN_BIT;
static uint8_t g_unitBit = CALIPER_UNIT_BIT;
static bool g_unitBitMeansInchWhen1 = true;
static bool g_signBitMeansNegativeWhen1 = true;
// If unit bit is missing/noisy, infer mm/inch by continuity of the physical value.
static bool g_autoUnitFix = true;

// Unit inference state (prevents tiny movements from flipping unit).
static bool g_unitStateIsInch = false;
static bool g_hasUnitState = false;
static bool g_unitPendingIsInch = false;
static uint8_t g_unitPendingCount = 0;

static constexpr uint8_t UNIT_SWITCH_CONFIRM = 3;        // require N consecutive strong hints
static constexpr float UNIT_SWITCH_MIN_BAD_MM = 4.0f;    // current-unit interpretation must be this wrong (mm)
static constexpr float UNIT_SWITCH_MAX_GOOD_MM = 0.40f;  // alternate interpretation must be within this (mm)

// Filtering: require more repeats if the value jumps a lot.
static constexpr uint8_t STABLE_REPEATS_NORMAL = 2;
static constexpr uint8_t STABLE_REPEATS_ON_JUMP = 4;
static constexpr float JUMP_MM_THRESHOLD = 0.50f;

static inline uint32_t stabilityMaskRuntime() {
  return valueMask() | (1u << g_signBit) | (1u << g_unitBit);
}

static float countsToMillimeters(int32_t counts, bool isInch) {
  float mm;
  if (!isInch) {
    mm = (float)counts / COUNTS_PER_MM;
  } else {
    const float inches = (float)counts / COUNTS_PER_INCH;
    mm = inches * 25.4f;
  }
  return mm;
}

static float countsToInches(int32_t counts, bool isInch) {
  float inches;
  if (isInch) {
    inches = (float)counts / COUNTS_PER_INCH;
  } else {
    inches = ((float)counts / COUNTS_PER_MM) / 25.4f;
  }
  return inches;
}

static bool updateUnitStateByContinuity(int32_t signedCounts, bool configuredIsInch, bool hasLastMm, float lastMm) {
  // Initialize unit state from configured bit (if any) on first use.
  if (!g_hasUnitState) {
    g_unitStateIsInch = configuredIsInch;
    g_hasUnitState = true;
    g_unitPendingCount = 0;
    return g_unitStateIsInch;
  }

  if (!g_autoUnitFix || !hasLastMm) {
    g_unitPendingCount = 0;
    return g_unitStateIsInch;
  }

  const bool current = g_unitStateIsInch;
  const float mmCurrent = countsToMillimeters(signedCounts, current);
  const float mmAlt = countsToMillimeters(signedCounts, !current);
  const float dCurrent = fabsf(mmCurrent - lastMm);
  const float dAlt = fabsf(mmAlt - lastMm);

  // Strong hint: current interpretation is very wrong, alternate is very good.
  const bool strongFlipHint = (dCurrent >= UNIT_SWITCH_MIN_BAD_MM) && (dAlt <= UNIT_SWITCH_MAX_GOOD_MM);
  if (!strongFlipHint) {
    g_unitPendingCount = 0;
    return g_unitStateIsInch;
  }

  const bool candidate = !current;
  if (g_unitPendingCount == 0 || g_unitPendingIsInch != candidate) {
    g_unitPendingIsInch = candidate;
    g_unitPendingCount = 1;
  } else {
    if (g_unitPendingCount < 255) g_unitPendingCount++;
  }

  if (g_unitPendingCount >= UNIT_SWITCH_CONFIRM) {
    g_unitStateIsInch = candidate;
    g_unitPendingCount = 0;
  }
  return g_unitStateIsInch;
}

// ---- Caliper 24-bit reader ("Chinese caliper" protocol) ----
// Keep ISR flash/IRAM-safe: avoid Arduino helpers like digitalRead()/micros().
static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t g_lastEdgeUs = 0;
static volatile uint32_t g_shiftReg = 0;
static volatile uint8_t g_bitCount = 0;
static volatile bool g_hasFrame = false;
static volatile uint32_t g_lastFrame = 0;

static int g_dataPin = CALIPER_DATA_PIN;

static void ARDUINO_ISR_ATTR onCaliperClockFalling() {
  const uint32_t now = (uint32_t)esp_timer_get_time();
  const uint32_t gap = now - g_lastEdgeUs;
  g_lastEdgeUs = now;

  if (gap < MIN_EDGE_US) {
    // Very likely bounce/noise on clock line.
    return;
  }

  if (gap > FRAME_GAP_US) {
    // New burst/frame boundary.
    g_bitCount = 0;
    g_shiftReg = 0;
  }

  const uint32_t bit = (uint32_t)gpio_get_level((gpio_num_t)g_dataPin) & 0x1u;
  g_shiftReg |= (bit << g_bitCount);
  g_bitCount = (uint8_t)(g_bitCount + 1u);

  if (g_bitCount >= 24) {
    portENTER_CRITICAL_ISR(&g_mux);
    g_lastFrame = g_shiftReg;
    g_hasFrame = true;
    portEXIT_CRITICAL_ISR(&g_mux);

    g_bitCount = 0;
    g_shiftReg = 0;
  }
}

static void caliperBegin(int dataPin, int clkPin) {
  g_dataPin = dataPin;
  pinMode(dataPin, INPUT);
  pinMode(clkPin, INPUT);

  g_lastEdgeUs = (uint32_t)esp_timer_get_time();
  g_bitCount = 0;
  g_shiftReg = 0;
  g_hasFrame = false;
  attachInterrupt(clkPin, onCaliperClockFalling, FALLING);
}

static bool caliperReadFrame(uint32_t &rawOut) {
  if (!g_hasFrame) return false;
  portENTER_CRITICAL(&g_mux);
  rawOut = g_lastFrame;
  g_hasFrame = false;
  portEXIT_CRITICAL(&g_mux);
  return true;
}

struct CaliperDecoded {
  bool isNegative;
  bool isInch;
  uint32_t magnitude;
};

static CaliperDecoded decode24(uint32_t raw) {
  CaliperDecoded d{};
  d.magnitude = raw & valueMask();                           // magnitude bits
  const bool signBitSet = (((raw >> g_signBit) & 0x1u) != 0u);
  const bool unitBitSet = (g_unitBit == g_signBit) ? false : ((((raw >> g_unitBit) & 0x1u) != 0u));
  d.isNegative = g_signBitMeansNegativeWhen1 ? signBitSet : !signBitSet;
  d.isInch = g_unitBitMeansInchWhen1 ? unitBitSet : !unitBitSet;
  return d;
}

static inline int32_t countsSignedFromRaw(uint32_t raw24, const CaliperDecoded &d) {
  (void)raw24;
  // Sign-bit mode: magnitude is always positive; sign comes from configured sign bit.
  const int32_t mag = (int32_t)d.magnitude;
  return d.isNegative ? -mag : mag;
}

static float toMillimeters(const CaliperDecoded &d) {
  float mm;
  if (!d.isInch) {
    mm = (float)d.magnitude / COUNTS_PER_MM;
  } else {
    const float inches = (float)d.magnitude / COUNTS_PER_INCH;
    mm = inches * 25.4f;
  }

  if (d.isNegative) mm = -mm;
  return mm;
}

static float toInches(const CaliperDecoded &d) {
  float inches;
  if (d.isInch) {
    inches = (float)d.magnitude / COUNTS_PER_INCH;
  } else {
    inches = ((float)d.magnitude / COUNTS_PER_MM) / 25.4f;
  }
  if (d.isNegative) inches = -inches;
  return inches;
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("ESP32-S3 dijital kumpas okuma basladi");
  Serial.printf("DATA GPIO: %d, CLOCK GPIO: %d\n", CALIPER_DATA_PIN, CALIPER_CLK_PIN);
  webLogf("Boot: main setup");

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BATTERY_ADC_PIN, INPUT);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  caliperBegin(CALIPER_DATA_PIN, CALIPER_CLK_PIN);

  webBegin();
}

void loop() {
  webLoop();

  uint32_t raw;
  static uint32_t pendingKey = 0;
  static uint8_t pendingCount = 0;
  static uint32_t lastStableKey = 0;
  static bool hasLastPublished = false;
  static float lastPublishedMm = 0.0f;
  static float lastPublishedInch = 0.0f;
  static bool lastPublishedIsInch = false;
  static bool lastButtonReading = true;
  static bool lastButtonStable = true;
  static uint32_t lastButtonChangeMs = 0;
  static uint32_t lastBatteryMs = 0;
  static float filteredBatV = 0.0f;
  static bool hasBattery = false;
  static uint32_t chargeWindowStartMs = 0;
  static float chargeWindowStartV = 0.0f;
  static uint32_t stableSinceMs = 0;
  static float stableBaseV = 0.0f;
  static float prevReportedBatV = 0.0f;
  static uint16_t prevReportedAdcMv = 0;
  static uint8_t prevReportedPct = 0;
  static bool prevReportedCharging = false;
  static uint32_t lastBatteryReportMs = 0;
  const uint32_t nowMs = millis();

  const bool buttonReading = (digitalRead(BUTTON_PIN) != LOW);
  if (buttonReading != lastButtonReading) {
    lastButtonReading = buttonReading;
    lastButtonChangeMs = nowMs;
  }
  if ((uint32_t)(nowMs - lastButtonChangeMs) >= 20u) {
    if (lastButtonStable != lastButtonReading) {
      lastButtonStable = lastButtonReading;
      const bool pressed = (lastButtonStable == true);
      webUpdateButton(pressed);
    }
  }

  // choose sample interval based on whether we currently appear to be charging
  const uint32_t currentSampleMs = (chargeWindowStartV > 0 &&
                                    ((filteredBatV >= (chargeWindowStartV + CHARGE_MIN_DELTA_V)) && (filteredBatV < CHARGE_NEAR_FULL_V)))
                                    ? BATTERY_SAMPLE_MS_CHARGING
                                    : BATTERY_SAMPLE_MS_IDLE;

  if ((uint32_t)(nowMs - lastBatteryMs) >= currentSampleMs) {
    lastBatteryMs = nowMs;
    uint32_t sumMv = 0;
    for (uint8_t i = 0; i < BATTERY_SAMPLES; i++) {
      sumMv += (uint32_t)analogReadMilliVolts(BATTERY_ADC_PIN);
    }
    const uint16_t adcMv = (uint16_t)(sumMv / BATTERY_SAMPLES);
    const float batV = ((float)adcMv / 1000.0f) * batteryDividerRatio();
    // Update filtered voltage and track both short-term and long-term stability.
    if (!hasBattery) {
      filteredBatV = batV;
      hasBattery = true;
      chargeWindowStartMs = nowMs;
      chargeWindowStartV = batV;
      stableSinceMs = nowMs;
      stableBaseV = batV;
    } else {
      const float prevFiltered = filteredBatV;
      filteredBatV = (filteredBatV * 0.85f) + (batV * 0.15f);
      const float deltaFiltered = fabsf(filteredBatV - prevFiltered);
      // If voltage changes very little, consider it part of a stable period.
      if (deltaFiltered <= STABLE_DELTA_V) {
        if (stableSinceMs == 0) {
          stableSinceMs = nowMs;
          stableBaseV = prevFiltered;
        }
      } else {
        // Significant movement — reset long-term stable window.
        stableSinceMs = 0;
        stableBaseV = filteredBatV;
      }
    }

    // Update short-term baseline used for quick charging hints
    if ((uint32_t)(nowMs - chargeWindowStartMs) >= CHARGE_WINDOW_MS) {
      chargeWindowStartMs = nowMs;
      chargeWindowStartV = filteredBatV;
    }
    if (filteredBatV < (chargeWindowStartV - 0.02f)) {
      chargeWindowStartMs = nowMs;
      chargeWindowStartV = filteredBatV;
    }

    // Charging if either quick baseline increased, or a long-stable base later rose.
    const bool quickCharging = (filteredBatV >= (chargeWindowStartV + CHARGE_MIN_DELTA_V)) &&
                               (filteredBatV < CHARGE_NEAR_FULL_V);
    bool longTermCharging = false;
    if (stableSinceMs != 0 && ((uint32_t)(nowMs - stableSinceMs) >= STABLE_PERIOD_MS)) {
      if (filteredBatV >= (stableBaseV + LONG_CHARGE_MIN_DELTA_V) && filteredBatV < CHARGE_NEAR_FULL_V) {
        longTermCharging = true;
      }
    }
    const bool charging = quickCharging || longTermCharging;
    const uint8_t pct = batteryPercentFromVolts(filteredBatV);

    // Only report if meaningful change occurred or max interval passed
    const bool significantVoltageChange = fabsf(filteredBatV - prevReportedBatV) >= 0.01f;
    const bool pctChanged = (pct != prevReportedPct);
    const bool chargingChanged = (charging != prevReportedCharging);
    const bool timeExpired = ((uint32_t)(nowMs - lastBatteryReportMs) >= BATTERY_REPORT_MAX_INTERVAL_MS);

    if (significantVoltageChange || pctChanged || chargingChanged || timeExpired) {
      prevReportedBatV = filteredBatV;
      prevReportedAdcMv = adcMv;
      prevReportedPct = pct;
      prevReportedCharging = charging;
      lastBatteryReportMs = nowMs;
      webUpdateBattery(filteredBatV, adcMv, pct, charging);
    }
  }

  if (caliperReadFrame(raw)) {
    raw &= 0xFFFFFFu;

    // Stabilize using only the bits we care about (value + sign + unit).
    // This also lets you move SIGN/UNIT bit positions without touching the filter logic.
    const uint32_t key = raw & stabilityMaskRuntime();

    if (REJECT_IF_TOP_BITS_SET) {
      const uint32_t top2 = (raw >> 22) & 0x3u;
      if (top2 != 0u) {
        // Likely out-of-sync/noisy capture.
        return;
      }
    }

    if (key == pendingKey) {
      if (pendingCount < 255) pendingCount++;
    } else {
      pendingKey = key;
      pendingCount = 1;
    }

    // Accept only after we see the same key twice consecutively.
    if (pendingCount >= 2 && pendingKey != lastStableKey) {
      const auto decoded = decode24(pendingKey);
      const int32_t counts = countsSignedFromRaw(pendingKey, decoded);
      const bool isInch = updateUnitStateByContinuity(counts, decoded.isInch, hasLastPublished, lastPublishedMm);
      const float mm = countsToMillimeters(counts, isInch);
      const float inch = countsToInches(counts, isInch);

      const float deltaMm = hasLastPublished ? fabsf(mm - lastPublishedMm) : 0.0f;
      const uint8_t requiredRepeats = (hasLastPublished && deltaMm > JUMP_MM_THRESHOLD)
                                          ? STABLE_REPEATS_ON_JUMP
                                          : STABLE_REPEATS_NORMAL;

      if (pendingCount < requiredRepeats) {
        return;
      }

      if (isfinite(mm) && fabsf(mm) <= MAX_ABS_MM) {
        lastStableKey = pendingKey;
        hasLastPublished = true;
        lastPublishedMm = mm;
        lastPublishedInch = inch;
        lastPublishedIsInch = isInch;
        webUpdateMeasurement(pendingKey, mm, inch, isInch, counts < 0);
      }
    }
  }

  delay(2);
}
