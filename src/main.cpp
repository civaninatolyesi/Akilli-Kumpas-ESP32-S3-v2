#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <math.h>

#include "web_ui.h"

// ---- User config ----
static constexpr int CALIPER_DATA_PIN = 8; // GPIO8
static constexpr int CALIPER_CLK_PIN = 9;  // GPIO9 (interrupt)

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

// ---- Runtime layout (selectable over Serial for debugging) ----
static uint8_t g_signBit = CALIPER_SIGN_BIT;
static uint8_t g_unitBit = CALIPER_UNIT_BIT;
static bool g_unitBitMeansInchWhen1 = true;
static bool g_signBitMeansNegativeWhen1 = true;
static bool g_debugRaw = false;
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

// Unit-bit probing support: capture one stable frame in mm and one in inch,
// then compare bits (especially 20..23) to find a real unit flag.
static uint32_t g_lastAcceptedRaw24 = 0;
static bool g_hasLastAcceptedRaw24 = false;
static uint32_t g_probeMmRaw24 = 0;
static uint32_t g_probeInRaw24 = 0;
static bool g_hasProbeMm = false;
static bool g_hasProbeIn = false;

static void printBitSummary(const char *tag, uint32_t raw24) {
  const uint32_t b20 = (raw24 >> 20) & 1u;
  const uint32_t b21 = (raw24 >> 21) & 1u;
  const uint32_t b22 = (raw24 >> 22) & 1u;
  const uint32_t b23 = (raw24 >> 23) & 1u;
  Serial.printf("[%s] raw=0x%06lX  b20=%lu b21=%lu b22=%lu b23=%lu\n",
                tag,
                (unsigned long)(raw24 & 0xFFFFFFu),
                (unsigned long)b20,
                (unsigned long)b21,
                (unsigned long)b22,
                (unsigned long)b23);
}

static void analyzeUnitProbe() {
  if (!g_hasProbeMm || !g_hasProbeIn) {
    Serial.println("[probe] Need both samples: press 'm' (mm sample) and 'i' (inch sample) first.");
    return;
  }

  const uint32_t mmRaw = g_probeMmRaw24 & 0xFFFFFFu;
  const uint32_t inRaw = g_probeInRaw24 & 0xFFFFFFu;
  const uint32_t diff = (mmRaw ^ inRaw) & 0xFFFFFFu;
  const uint32_t diffTop = diff & 0xF00000u; // bits 20..23

  Serial.printf("[probe] mmRaw=0x%06lX  inRaw=0x%06lX  diff=0x%06lX\n",
                (unsigned long)mmRaw,
                (unsigned long)inRaw,
                (unsigned long)diff);
  printBitSummary("probe:mm", mmRaw);
  printBitSummary("probe:in", inRaw);

  if (diffTop == 0u) {
    Serial.println("[probe] No change in bits 20..23 between mm/inch samples.");
    Serial.println("[probe] This caliper may not transmit a dedicated unit flag bit, or the bit is outside 20..23.");
    Serial.println("[probe] In that case, autoUnitFix (continuity) or a debounced heuristic is the practical option.");
    return;
  }

  Serial.print("[probe] Top-bit changes:");
  for (uint8_t b = 20; b <= 23; b++) {
    if (diffTop & (1u << b)) {
      const uint32_t mmv = (mmRaw >> b) & 1u;
      const uint32_t inv = (inRaw >> b) & 1u;
      Serial.printf(" b%u(mm=%lu,in=%lu)", (unsigned)b, (unsigned long)mmv, (unsigned long)inv);
    }
  }
  Serial.println();

  // Suggest a unit bit among 20..23 that isn't the configured sign bit.
  int8_t suggestedBit = -1;
  for (int8_t b = 23; b >= 20; b--) {
    if ((diffTop & (1u << b)) == 0u) continue;
    if ((uint8_t)b == g_signBit) continue;
    suggestedBit = b;
    break;
  }

  if (suggestedBit < 0) {
    Serial.println("[probe] Only the configured sign bit changed in the top bits; can't suggest a separate unit bit.");
    return;
  }

  const bool inchWhen1 = (((inRaw >> (uint8_t)suggestedBit) & 1u) != 0u);
  Serial.printf("[probe] Suggested UNIT bit: %d  (inchWhen1=%u)\n", (int)suggestedBit, (unsigned)(inchWhen1 ? 1 : 0));
  Serial.println("[probe] You can set it quickly by cycling unit bit with 'U' until it matches, then toggle polarity with 'u' if needed.");
}

// Filtering: require more repeats if the value jumps a lot.
static constexpr uint8_t STABLE_REPEATS_NORMAL = 2;
static constexpr uint8_t STABLE_REPEATS_ON_JUMP = 4;
static constexpr float JUMP_MM_THRESHOLD = 0.50f;

static inline uint32_t stabilityMaskRuntime() {
  return valueMask() | (1u << g_signBit) | (1u << g_unitBit);
}

struct LayoutOption {
  const char *label;
  uint8_t signBit;
  uint8_t unitBit;
  bool unitInchWhen1;
  bool signNegWhen1;
};

enum class SignMode : uint8_t {
  SignBit = 0,
  TwosComplement = 1,
};

static SignMode g_signMode = SignMode::SignBit;

static constexpr LayoutOption LAYOUTS[] = {
    {"0: sign20 unit21 (1=inch)", 20, 21, true, true},
    {"1: sign23 unit21 (1=inch)", 23, 21, true, true},
    {"2: sign20 unit23 (1=inch)", 20, 23, true, true},
    {"3: sign23 unit23 (1=inch)", 23, 23, true, true},
    {"4: sign20 unit21 (0=inch)", 20, 21, false, true},
    {"5: sign23 unit21 (0=inch)", 23, 21, false, true},
    {"6: sign20 unit23 (0=inch)", 20, 23, false, true},
    {"7: sign23 unit23 (0=inch)", 23, 23, false, true},
  {"8: sign21 unit23 (1=inch)", 21, 23, true, true},
  {"9: sign21 unit22 (1=inch)", 21, 22, true, true},
};

static void applyLayout(uint8_t idx) {
  if (idx >= (sizeof(LAYOUTS) / sizeof(LAYOUTS[0]))) return;
  g_signBit = LAYOUTS[idx].signBit;
  g_unitBit = LAYOUTS[idx].unitBit;
  g_unitBitMeansInchWhen1 = LAYOUTS[idx].unitInchWhen1;
  g_signBitMeansNegativeWhen1 = LAYOUTS[idx].signNegWhen1;

  Serial.printf("[layout] %s\n", LAYOUTS[idx].label);
  Serial.printf("[layout] signBit=%u unitBit=%u unitInchWhen1=%u signNegWhen1=%u\n",
                (unsigned)g_signBit,
                (unsigned)g_unitBit,
                (unsigned)(g_unitBitMeansInchWhen1 ? 1 : 0),
                (unsigned)(g_signBitMeansNegativeWhen1 ? 1 : 0));
}

static void printHelp() {
  Serial.println("\n--- Caliper debug help ---");
  Serial.println("Commands:");
  Serial.println("  ?  : help");
  Serial.println("  d  : toggle raw debug prints");
  Serial.println("  a  : toggle auto unit fix (mm/inch inference by continuity)");
  Serial.println("  m  : capture probe sample (current caliper unit MUST be mm)");
  Serial.println("  i  : capture probe sample (current caliper unit MUST be inch)");
  Serial.println("  p  : analyze probe samples and suggest unit bit");
  Serial.println("  U  : cycle unit bit among {20,22,23} (avoids sign bit)");
  Serial.println("  t  : toggle sign mode (sign-bit <-> two's complement)");
  Serial.println("  s  : force sign mode = sign-bit");
  Serial.println("  c  : force sign mode = two's complement");
  Serial.println("  n  : toggle sign polarity (bit=1 means negative?)");
  Serial.println("  u  : toggle unit polarity (bit=1 means inch?)");
  Serial.println("  0-9: select bit layout option");
  Serial.println("How to debug:");
  Serial.println("  - Enable raw debug (d)");
  Serial.println("  - Press mm/inch on caliper -> watch which bit changes");
  Serial.println("  - Press ZERO then go negative -> pick layout where sign becomes '-'");
  Serial.println("  - If negative never appears, toggle sign mode (t) and retry");
  Serial.println("Layouts:");
  for (uint8_t i = 0; i < (sizeof(LAYOUTS) / sizeof(LAYOUTS[0])); i++) {
    Serial.println(LAYOUTS[i].label);
  }
  Serial.println("-------------------------\n");
}

static void printStatus() {
  Serial.printf("[status] signBit=%u unitBit=%u signPolarity(bit1Neg)=%u unitPolarity(bit1Inch)=%u signMode=%s autoUnitFix=%u unitState=%s\n",
                (unsigned)g_signBit,
                (unsigned)g_unitBit,
                (unsigned)(g_signBitMeansNegativeWhen1 ? 1 : 0),
                (unsigned)(g_unitBitMeansInchWhen1 ? 1 : 0),
                (g_signMode == SignMode::SignBit) ? "sign-bit" : "twos-complement",
                (unsigned)(g_autoUnitFix ? 1 : 0),
                g_hasUnitState ? (g_unitStateIsInch ? "inch" : "mm") : "(unset)");
}

static void handleSerialCommands() {
  while (Serial.available() > 0) {
    const int c = Serial.read();
    if (c < 0) return;

    if (c == '?' || c == 'h' || c == 'H') {
      printHelp();
      continue;
    }
    if (c == 'd' || c == 'D') {
      g_debugRaw = !g_debugRaw;
      Serial.printf("[debug] raw=%s\n", g_debugRaw ? "ON" : "OFF");
      printStatus();
      continue;
    }
    if (c == 'a' || c == 'A') {
      g_autoUnitFix = !g_autoUnitFix;
      Serial.printf("[toggle] autoUnitFix=%u\n", (unsigned)(g_autoUnitFix ? 1 : 0));
      printStatus();
      continue;
    }

    if (c == 'm') {
      if (!g_hasLastAcceptedRaw24) {
        Serial.println("[probe] No accepted frame yet. Move caliper slightly and wait for a 'raw=' line.");
        continue;
      }
      g_probeMmRaw24 = g_lastAcceptedRaw24;
      g_hasProbeMm = true;
      Serial.println("[probe] Captured MM sample.");
      printBitSummary("probe:mm", g_probeMmRaw24);
      continue;
    }

    if (c == 'i') {
      if (!g_hasLastAcceptedRaw24) {
        Serial.println("[probe] No accepted frame yet. Move caliper slightly and wait for a 'raw=' line.");
        continue;
      }
      g_probeInRaw24 = g_lastAcceptedRaw24;
      g_hasProbeIn = true;
      Serial.println("[probe] Captured INCH sample.");
      printBitSummary("probe:in", g_probeInRaw24);
      continue;
    }

    if (c == 'p' || c == 'P') {
      analyzeUnitProbe();
      continue;
    }

    if (c == 'U') {
      // Cycle likely unit-bit candidates (top bits), skipping the configured sign bit.
      static const uint8_t candidates[] = {20, 22, 23};
      uint8_t idx = 0;
      for (uint8_t k = 0; k < (sizeof(candidates) / sizeof(candidates[0])); k++) {
        if (g_unitBit == candidates[k]) {
          idx = (uint8_t)((k + 1u) % (sizeof(candidates) / sizeof(candidates[0])));
          break;
        }
      }
      uint8_t next = candidates[idx];
      if (next == g_signBit) {
        // If it conflicts, just advance one more.
        idx = (uint8_t)((idx + 1u) % (sizeof(candidates) / sizeof(candidates[0])));
        next = candidates[idx];
      }
      g_unitBit = next;
      Serial.printf("[unit] unitBit=%u\n", (unsigned)g_unitBit);
      printStatus();
      continue;
    }
    if (c == 't' || c == 'T') {
      g_signMode = (g_signMode == SignMode::SignBit) ? SignMode::TwosComplement : SignMode::SignBit;
      Serial.printf("[mode] sign=%s\n", (g_signMode == SignMode::SignBit) ? "sign-bit" : "twos-complement");
      printStatus();
      continue;
    }
    if (c == 's' || c == 'S') {
      g_signMode = SignMode::SignBit;
      Serial.println("[mode] sign=sign-bit");
      printStatus();
      continue;
    }
    if (c == 'c' || c == 'C') {
      g_signMode = SignMode::TwosComplement;
      Serial.println("[mode] sign=twos-complement");
      printStatus();
      continue;
    }
    if (c == 'n' || c == 'N') {
      g_signBitMeansNegativeWhen1 = !g_signBitMeansNegativeWhen1;
      Serial.printf("[toggle] signPolarity(bit1Neg)=%u\n", (unsigned)(g_signBitMeansNegativeWhen1 ? 1 : 0));
      printStatus();
      continue;
    }
    if (c == 'u' || c == 'U') {
      g_unitBitMeansInchWhen1 = !g_unitBitMeansInchWhen1;
      Serial.printf("[toggle] unitPolarity(bit1Inch)=%u\n", (unsigned)(g_unitBitMeansInchWhen1 ? 1 : 0));
      printStatus();
      continue;
    }
    if (c >= '0' && c <= '9') {
      applyLayout((uint8_t)(c - '0'));
      printStatus();
      continue;
    }
  }
}

static inline int32_t signExtend(uint32_t value, uint8_t bits) {
  if (bits == 0 || bits >= 32) return (int32_t)value;
  const uint32_t sign = 1u << (bits - 1u);
  const uint32_t mask = (1u << bits) - 1u;
  value &= mask;
  return (int32_t)((value ^ sign) - sign);
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

static void printChangedBits(uint32_t prev, uint32_t now) {
  const uint32_t x = (prev ^ now) & 0xFFFFFFu;
  if (x == 0) {
    Serial.print("changed: (none)");
    return;
  }
  Serial.print("changed:");
  for (uint8_t b = 0; b < 24; b++) {
    if (x & (1u << b)) {
      Serial.printf(" %u", (unsigned)b);
    }
  }
}

static void debugPrintRawFrame(uint32_t raw24) {
  static uint32_t lastRaw24 = 0;
  static uint32_t lastDbgMs = 0;

  const uint32_t nowMs = millis();
  if (!g_debugRaw) return;
  if ((uint32_t)(nowMs - lastDbgMs) < 120u && raw24 == lastRaw24) return;
  lastDbgMs = nowMs;

  const uint32_t b20 = (raw24 >> 20) & 1u;
  const uint32_t b21 = (raw24 >> 21) & 1u;
  const uint32_t b22 = (raw24 >> 22) & 1u;
  const uint32_t b23 = (raw24 >> 23) & 1u;

  const uint32_t mag = raw24 & valueMask();
  const uint32_t signBitVal = (raw24 >> g_signBit) & 1u;
  const uint32_t unitBitVal = (raw24 >> g_unitBit) & 1u;

  Serial.printf("[raw] 0x%06lX mag=%lu b20=%lu b21=%lu b22=%lu b23=%lu  ",
                (unsigned long)(raw24 & 0xFFFFFFu),
                (unsigned long)mag,
                (unsigned long)b20,
                (unsigned long)b21,
                (unsigned long)b22,
                (unsigned long)b23);
  printChangedBits(lastRaw24, raw24);
  Serial.println();

  Serial.printf("[cfg] sign@%u=%lu unit@%u=%lu (unitInchWhen1=%u)\n",
                (unsigned)g_signBit,
                (unsigned long)signBitVal,
                (unsigned)g_unitBit,
                (unsigned long)unitBitVal,
                (unsigned)(g_unitBitMeansInchWhen1 ? 1 : 0));

  // Also show two's complement interpretation quickly (helps when sign-bit never toggles).
  const int32_t twos = signExtend(mag, CALIPER_VALUE_BITS);
  Serial.printf("[hint] twos20=%ld (bit19=%lu)  signMode=%s\n",
                (long)twos,
                (unsigned long)((mag >> (CALIPER_VALUE_BITS - 1u)) & 1u),
                (g_signMode == SignMode::SignBit) ? "sign-bit" : "twos-complement");

  lastRaw24 = raw24;
}

// ---- Caliper 24-bit reader ("Chinese caliper" protocol) ----
// Keep ISR flash/IRAM-safe: avoid Arduino helpers like digitalRead()/micros().
static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t g_lastEdgeUs = 0;
static volatile uint32_t g_shiftReg = 0;
static volatile uint8_t g_bitCount = 0;
static volatile bool g_hasFrame = false;
static volatile uint32_t g_lastFrame = 0;
static volatile uint32_t g_isrEdges = 0;
static volatile uint32_t g_isrGapResets = 0;
static volatile uint32_t g_isrFrames = 0;

static int g_dataPin = CALIPER_DATA_PIN;

static void ARDUINO_ISR_ATTR onCaliperClockFalling() {
  const uint32_t now = (uint32_t)esp_timer_get_time();
  const uint32_t gap = now - g_lastEdgeUs;
  g_lastEdgeUs = now;

  g_isrEdges = g_isrEdges + 1u;

  if (gap < MIN_EDGE_US) {
    // Very likely bounce/noise on clock line.
    return;
  }

  if (gap > FRAME_GAP_US) {
    // New burst/frame boundary.
    g_bitCount = 0;
    g_shiftReg = 0;
    g_isrGapResets = g_isrGapResets + 1u;
  }

  const uint32_t bit = (uint32_t)gpio_get_level((gpio_num_t)g_dataPin) & 0x1u;
  g_shiftReg |= (bit << g_bitCount);
  g_bitCount = (uint8_t)(g_bitCount + 1u);

  if (g_bitCount >= 24) {
    portENTER_CRITICAL_ISR(&g_mux);
    g_lastFrame = g_shiftReg;
    g_hasFrame = true;
    portEXIT_CRITICAL_ISR(&g_mux);

    g_isrFrames = g_isrFrames + 1u;

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
  g_isrEdges = 0;
  g_isrGapResets = 0;
  g_isrFrames = 0;

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
  if (g_signMode == SignMode::TwosComplement) {
    return signExtend(d.magnitude, CALIPER_VALUE_BITS);
  }
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
  Serial.println("Type '?' for debug help.");
  printStatus();

  caliperBegin(CALIPER_DATA_PIN, CALIPER_CLK_PIN);

  webBegin();
}

void loop() {
  handleSerialCommands();
  webLoop();

  uint32_t raw;
  static uint32_t pendingKey = 0;
  static uint8_t pendingCount = 0;
  static uint32_t lastStableKey = 0;
  static uint32_t lastStatsMs = 0;
  static uint32_t lastAcceptedMs = 0;
  static bool hasLastPublished = false;
  static float lastPublishedMm = 0.0f;

  const uint32_t nowMs = millis();
  if ((uint32_t)(nowMs - lastStatsMs) >= 1000u) {
    lastStatsMs = nowMs;
    uint32_t edges, gaps, frames;
    portENTER_CRITICAL(&g_mux);
    edges = g_isrEdges;
    gaps = g_isrGapResets;
    frames = g_isrFrames;
    portEXIT_CRITICAL(&g_mux);

    // Serial.printf("[stats] edges=%lu gaps=%lu frames=%lu last_ok_ms_ago=%lu\n",
    //               (unsigned long)edges,
    //               (unsigned long)gaps,
    //               (unsigned long)frames,
    //               (unsigned long)(nowMs - lastAcceptedMs));
  }

  if (caliperReadFrame(raw)) {
    raw &= 0xFFFFFFu;

    debugPrintRawFrame(raw);

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
        lastAcceptedMs = nowMs;
        hasLastPublished = true;
        lastPublishedMm = mm;
        g_lastAcceptedRaw24 = (pendingKey & 0xFFFFFFu);
        g_hasLastAcceptedRaw24 = true;
        Serial.printf("raw=0x%06lX  counts=%ld  unit=%s  mm=%.2f  in=%.4f\n",
                      (unsigned long)pendingKey,
                      (long)counts,
                      isInch ? "inch" : "mm",
                      (double)mm,
                      (double)inch);

        webUpdateMeasurement(pendingKey, mm, inch, isInch, counts < 0);
      }
    }
  }

  delay(2);
}
