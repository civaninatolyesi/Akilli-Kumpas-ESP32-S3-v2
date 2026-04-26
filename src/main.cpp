#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_timer.h>

// ---- User config ----
static constexpr int CALIPER_DATA_PIN = 8; // GPIO8
static constexpr int CALIPER_CLK_PIN = 9;  // GPIO9 (interrupt)

// If your caliper sends a burst roughly every ~100ms, a gap of a few ms
// between clock edges is a good frame delimiter.
static constexpr uint32_t FRAME_GAP_US = 3000;

// Ignore clock edges that are unrealistically close together (contact bounce / noise).
// Typical caliper clocks are in the hundreds of microseconds per edge, not < 20us.
static constexpr uint32_t MIN_EDGE_US = 40;

// Basic plausibility limit for output. Adjust to your caliper range.
static constexpr float MAX_ABS_MM = 500.0f;

// If enabled, reject frames where the top 2 bits are set.
// Many 24-bit calipers keep bits 22..23 at 0; when you are out-of-sync they become random.
static constexpr bool REJECT_IF_TOP_BITS_SET = true;

// ---- Caliper 24-bit reader ("Chinese caliper" protocol) ----
// Keep ISR flash/IRAM-safe: avoid Arduino helpers like digitalRead()/micros().
static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t g_lastEdgeUs = 0;
static volatile uint32_t g_shiftReg = 0;
static volatile uint8_t g_bitCount = 0;
static volatile bool g_hasFrame = false;
static volatile uint32_t g_lastFrame = 0;
static volatile bool g_capturing = false;

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
    // Start of a new burst/frame.
    g_bitCount = 0;
    g_shiftReg = 0;
    g_capturing = true;
  }

  if (!g_capturing) {
    // Don't start mid-frame; wait for a proper inter-frame gap.
    return;
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
    g_capturing = false;
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
  g_capturing = false;

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
  d.magnitude = raw & 0xFFFFFu;           // 20-bit magnitude
  d.isNegative = ((raw >> 20) & 0x1u);    // sign bit
  d.isInch = ((raw >> 21) & 0x1u) != 0u;  // unit bit
  return d;
}

static float toMillimeters(const CaliperDecoded &d) {
  // Common scale assumptions:
  // - mm: magnitude is in 0.01mm units  => mm = mag / 100
  // - inch: magnitude is in 0.0005" units => inch = mag / 2000
  float mm;
  if (!d.isInch) {
    mm = (float)d.magnitude / 100.0f;
  } else {
    const float inches = (float)d.magnitude / 2000.0f;
    mm = inches * 25.4f;
  }

  if (d.isNegative) mm = -mm;
  return mm;
}

static float toInches(const CaliperDecoded &d) {
  float inches;
  if (d.isInch) {
    inches = (float)d.magnitude / 2000.0f;
  } else {
    inches = ((float)d.magnitude / 100.0f) / 25.4f;
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

  caliperBegin(CALIPER_DATA_PIN, CALIPER_CLK_PIN);
}

void loop() {
  uint32_t raw;
  static uint32_t pendingRaw = 0;
  static uint8_t pendingCount = 0;
  static uint32_t lastStableRaw = 0;

  if (caliperReadFrame(raw)) {
    raw &= 0xFFFFFFu;

    if (REJECT_IF_TOP_BITS_SET) {
      const uint32_t top2 = (raw >> 22) & 0x3u;
      if (top2 != 0u) {
        // Likely out-of-sync/noisy capture.
        return;
      }
    }

    if (raw == pendingRaw) {
      if (pendingCount < 255) pendingCount++;
    } else {
      pendingRaw = raw;
      pendingCount = 1;
    }

    // Accept only after we see the same 24-bit frame twice consecutively.
    if (pendingCount >= 2 && pendingRaw != lastStableRaw) {
      const auto decoded = decode24(pendingRaw);
      const float mm = toMillimeters(decoded);
      const float inch = toInches(decoded);

      if (isfinite(mm) && fabsf(mm) <= MAX_ABS_MM) {
        lastStableRaw = pendingRaw;
        Serial.printf("raw=0x%06lX  mag=%lu  sign=%c  unit=%s  mm=%.2f  in=%.4f\n",
                      (unsigned long)pendingRaw,
                      (unsigned long)decoded.magnitude,
                      decoded.isNegative ? '-' : '+',
                      decoded.isInch ? "inch" : "mm",
                      (double)mm,
                      (double)inch);
      }
    }
  }

  delay(2);
}
