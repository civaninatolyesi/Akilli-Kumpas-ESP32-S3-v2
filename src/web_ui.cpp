#include <Arduino.h>

#include <WiFi.h>
#include <WebServer.h>

#include "wifi_secrets.h"
#include "web_ui.h"

static WebServer server(80);

static uint32_t lastRawKey = 0;
static float lastMm = 0.0f;
static float lastInch = 0.0f;
static bool lastIsInch = false;
static bool lastIsNegative = false;
static uint32_t lastUpdateMs = 0;

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="tr">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Kumpas Ölçüm</title>
  <style>
    :root { color-scheme: light dark; }
    body { font-family: system-ui, -apple-system, Segoe UI, Roboto, Arial, sans-serif; margin: 24px; }
    .card { max-width: 640px; padding: 16px 18px; border: 1px solid rgba(127,127,127,.35); border-radius: 12px; }
    .row { display: flex; gap: 16px; flex-wrap: wrap; align-items: baseline; }
    .big { font-size: 56px; font-weight: 700; letter-spacing: -1px; }
    .unit { font-size: 18px; opacity: .75; margin-left: 8px; }
    .meta { margin-top: 12px; opacity: .75; font-size: 14px; }
    .pill { display:inline-block; padding: 2px 8px; border-radius: 999px; border: 1px solid rgba(127,127,127,.35); }
    .muted { opacity: .6; }
  </style>
</head>
<body>
  <div class="card">
    <div class="row">
      <div>
        <div class="muted">Milimetre</div>
        <div><span id="mm" class="big">--</span><span class="unit">mm</span></div>
      </div>
      <div>
        <div class="muted">İnç</div>
        <div><span id="inch" class="big">--</span><span class="unit">in</span></div>
      </div>
    </div>

    <div class="meta">
      <span class="pill" id="status">bağlanıyor…</span>
      <span class="pill" id="age">-- ms</span>
      <span class="pill" id="raw">raw=--</span>
      <span class="pill" id="unit">unit=--</span>
      <span class="pill" id="sign">sign=--</span>
    </div>
  </div>

<script>
  function hex6(n){
    const s = (n >>> 0).toString(16).toUpperCase();
    return '0x' + s.padStart(6,'0');
  }

  async function tick(){
    try {
      const r = await fetch('/api/measure', { cache: 'no-store' });
      const j = await r.json();

      document.getElementById('mm').textContent = j.mm.toFixed(2);
      document.getElementById('inch').textContent = j.inch.toFixed(4);
      document.getElementById('raw').textContent = 'raw=' + hex6(j.rawKey);
      document.getElementById('unit').textContent = 'unit=' + (j.isInch ? 'inch' : 'mm');
      document.getElementById('sign').textContent = 'sign=' + (j.isNegative ? '-' : '+');
      document.getElementById('age').textContent = j.ageMs + ' ms';

      document.getElementById('status').textContent = 'online';
    } catch(e) {
      document.getElementById('status').textContent = 'offline';
    }
  }

  setInterval(tick, 200);
  tick();
</script>
</body>
</html>
)HTML";

static void handleRoot() {
  server.send(200, "text/html; charset=utf-8", INDEX_HTML);
}

static void handleApiMeasure() {
  const uint32_t age = (uint32_t)(millis() - lastUpdateMs);

  // Keep JSON small and simple for browser polling.
  String json;
  json.reserve(160);
  json += '{';
  json += "\"rawKey\":" + String((unsigned long)(lastRawKey & 0x3FFFFFu));
  json += ",\"mm\":" + String(lastMm, 3);
  json += ",\"inch\":" + String(lastInch, 5);
  json += ",\"isInch\":" + String(lastIsInch ? "true" : "false");
  json += ",\"isNegative\":" + String(lastIsNegative ? "true" : "false");
  json += ",\"ageMs\":" + String((unsigned long)age);
  json += '}';

  server.send(200, "application/json", json);
}

static bool connectWiFi(uint32_t timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    if ((uint32_t)(millis() - start) > timeoutMs) return false;
  }
  return true;
}

void webBegin() {
  const bool ok = connectWiFi(15000);

  if (ok) {
    Serial.printf("WiFi baglandi: %s  IP: %s\n", WIFI_SSID, WiFi.localIP().toString().c_str());
  } else {
    Serial.printf("WiFi baglanamadi: %s (timeout)\n", WIFI_SSID);
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/measure", HTTP_GET, handleApiMeasure);
  server.onNotFound([]() { server.send(404, "text/plain", "Not Found"); });

  server.begin();
  lastUpdateMs = millis();
}

void webLoop() {
  server.handleClient();
}

void webUpdateMeasurement(uint32_t rawKey, float mm, float inch, bool isInch, bool isNegative) {
  lastRawKey = rawKey;
  lastMm = mm;
  lastInch = inch;
  lastIsInch = isInch;
  lastIsNegative = isNegative;
  lastUpdateMs = millis();
}
