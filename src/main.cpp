#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_timer.h>

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
  d.magnitude = raw & 0xFFFFFu;           // 20-bit magnitude
  d.isNegative = ((raw >> 20) & 0x1u);    // sign bit
  d.isInch = ((raw >> 21) & 0x1u) != 0u;  // unit bit
  return d;
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

  caliperBegin(CALIPER_DATA_PIN, CALIPER_CLK_PIN);
}

void loop() {
  uint32_t raw;
  static uint32_t pendingKey = 0;
  static uint8_t pendingCount = 0;
  static uint32_t lastStableKey = 0;
  static uint32_t lastStatsMs = 0;
  static uint32_t lastAcceptedMs = 0;

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

    // Key only on the meaningful bits: magnitude(0..19), sign(20), unit(21).
    // Ignore bits 22..23 because some calipers toggle/use them.
    const uint32_t key = raw & 0x3FFFFFu;

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
      const float mm = toMillimeters(decoded);
      const float inch = toInches(decoded);

      if (isfinite(mm) && fabsf(mm) <= MAX_ABS_MM) {
        lastStableKey = pendingKey;
        lastAcceptedMs = nowMs;
        Serial.printf("raw=0x%06lX  mag=%lu  sign=%c  unit=%s  mm=%.2f  in=%.4f\n",
                      (unsigned long)pendingKey,
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
