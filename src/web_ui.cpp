#include <Arduino.h>

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WebSocketsServer.h>
#include <Preferences.h>

#include "wifi_secrets.h"
#include "web_ui.h"

static WebServer server(80);
static WebSocketsServer wsServer(81);

static const char MDNS_HOSTNAME[] = "kumpas";
static const char UPDATE_PATH[] = "/update";
static const char CAL_PATH[] = "/calibrate";
static const size_t UPDATE_MAX_SIZE = 3 * 1024 * 1024;
static const char CAL_NAMESPACE[] = "kumpas";
static const char CAL_KEY_OFFSET[] = "offset_mm";

static bool updateInProgress = false;
static uint8_t wsClientCount = 0;

static uint32_t lastRawKey = 0;
static float lastMm = 0.0f;
static float lastInch = 0.0f;
static bool lastIsInch = false;
static bool lastIsNegative = false;
static bool lastButtonPressed = false;
static uint32_t lastUpdateMs = 0;

static Preferences prefs;
static float calOffsetMm = 0.0f;

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
      <span class="pill" id="btn">btn=--</span>
      <span class="pill" id="cal">cal=--</span>
    </div>

    <div class="meta">
      <a href="/update">Yazılım Güncelle</a>
      <span class="muted"> | </span>
      <a href="/calibrate">Kalibrasyon</a>
    </div>
  </div>

  <div class="card" style="margin-top:16px;">
    <div class="row" style="justify-content: space-between;">
      <div>
        <div class="muted">Buton Logu</div>
        <div class="muted" style="font-size:12px;">Butona basildiginda kayit alinir</div>
      </div>
      <button id="clearLog" type="button">Temizle</button>
    </div>
    <div style="margin-top:12px; overflow:auto; max-height:260px;">
      <table style="width:100%; border-collapse:collapse; font-size:14px;">
        <thead>
          <tr>
            <th style="text-align:left; padding:6px 4px; border-bottom:1px solid rgba(127,127,127,.35);">Zaman</th>
            <th style="text-align:right; padding:6px 4px; border-bottom:1px solid rgba(127,127,127,.35);">mm</th>
            <th style="text-align:right; padding:6px 4px; border-bottom:1px solid rgba(127,127,127,.35);">in</th>
            <th style="text-align:left; padding:6px 4px; border-bottom:1px solid rgba(127,127,127,.35);">Not</th>
          </tr>
        </thead>
        <tbody id="logBody"></tbody>
      </table>
    </div>
  </div>

<script>
  const logBody = document.getElementById('logBody');
  const clearLog = document.getElementById('clearLog');
  let lastBtnPressed = null;

  function fmtTime(ts){
    const d = new Date(ts);
    const pad = (n) => String(n).padStart(2,'0');
    const ms = String(d.getMilliseconds()).padStart(3,'0');
    return `${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}.${ms}`;
  }

  function appendLogRow(j){
    const tr = document.createElement('tr');
    const ts = fmtTime(Date.now());
    const unitNote = j.isInch ? 'inch kaynagi' : 'mm kaynagi';
    tr.innerHTML = `
      <td style="padding:6px 4px; border-bottom:1px solid rgba(127,127,127,.2);">${ts}</td>
      <td style="padding:6px 4px; text-align:right; border-bottom:1px solid rgba(127,127,127,.2);">${j.mm.toFixed(2)}</td>
      <td style="padding:6px 4px; text-align:right; border-bottom:1px solid rgba(127,127,127,.2);">${j.inch.toFixed(4)}</td>
      <td style="padding:6px 4px; border-bottom:1px solid rgba(127,127,127,.2);">${unitNote}</td>
    `;
    logBody.prepend(tr);

    const maxRows = 200;
    while (logBody.children.length > maxRows) {
      logBody.removeChild(logBody.lastElementChild);
    }
  }

  clearLog.addEventListener('click', () => {
    logBody.innerHTML = '';
  });

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
      document.getElementById('btn').textContent = 'btn=' + (j.buttonPressed ? 'pressed' : 'released');
      document.getElementById('cal').textContent = 'cal=' + j.offsetMm.toFixed(3) + ' mm';
      document.getElementById('age').textContent = j.ageMs + ' ms';

      if (lastBtnPressed === null) {
        lastBtnPressed = j.buttonPressed;
      } else if (!lastBtnPressed && j.buttonPressed) {
        appendLogRow(j);
        lastBtnPressed = j.buttonPressed;
      } else {
        lastBtnPressed = j.buttonPressed;
      }

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

static const char CAL_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="tr">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Kalibrasyon</title>
  <style>
    :root { color-scheme: light dark; }
    body { font-family: system-ui, -apple-system, Segoe UI, Roboto, Arial, sans-serif; margin: 24px; }
    .card { max-width: 640px; padding: 16px 18px; border: 1px solid rgba(127,127,127,.35); border-radius: 12px; }
    .row { display: flex; gap: 12px; align-items: center; flex-wrap: wrap; }
    .muted { opacity: .7; }
    input[type=number] { padding: 6px 8px; font-size: 16px; width: 160px; }
    button { padding: 8px 14px; font-size: 16px; }
  </style>
</head>
<body>
  <div class="card">
    <h3>Kalibrasyon (mm)</h3>
    <p class="muted">Offset mm degerini girin. Ornek: -0.01</p>
    <form method="POST" action="/calibrate">
      <div class="row">
        <input type="number" name="offset" step="0.001" required />
        <button type="submit">Kaydet</button>
      </div>
    </form>
    <form method="POST" action="/calibrate" style="margin-top:12px;">
      <input type="hidden" name="offset" value="0" />
      <button type="submit">Offset Sifirla</button>
    </form>
    <p class="muted"><a href="/">Ana sayfaya don</a></p>
  </div>
</body>
</html>
)HTML";

static const char UPDATE_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="tr">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Yazılım Güncelle</title>
  <style>
    :root { color-scheme: light dark; }
    body { font-family: system-ui, -apple-system, Segoe UI, Roboto, Arial, sans-serif; margin: 24px; }
    .card { max-width: 640px; padding: 16px 18px; border: 1px solid rgba(127,127,127,.35); border-radius: 12px; }
    .row { display: flex; gap: 12px; align-items: center; flex-wrap: wrap; }
    .muted { opacity: .7; }
    input[type=file] { width: 100%; }
    button { padding: 8px 14px; font-size: 16px; }
  </style>
</head>
<body>
  <div class="card">
    <h3>Yazılım Güncelle</h3>
    <p class="muted">PlatformIO ile derlediginiz .bin dosyasini secin.</p>
    <form method="POST" action="/update" enctype="multipart/form-data">
      <div class="row">
        <input type="file" name="firmware" accept=".bin" required />
        <button type="submit">Yukle</button>
      </div>
    </form>
    <p class="muted">Guncelleme basarili olursa cihaz otomatik yeniden baslar.</p>
  </div>
</body>
</html>
)HTML";

static void handleRoot() {
  server.send(200, "text/html; charset=utf-8", INDEX_HTML);
}

static void handleUpdatePage() {
  server.send(200, "text/html; charset=utf-8", UPDATE_HTML);
}

static void handleCalPage() {
  server.send(200, "text/html; charset=utf-8", CAL_HTML);
}

static void handleUpdateResult(bool ok, const char *message) {
  String html;
  html.reserve(256);
  html += "<!doctype html><html lang=\"tr\"><head><meta charset=\"utf-8\" />";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />";
  html += "<title>Guncelleme</title></head><body>";
  html += ok ? "<h3>Basarili</h3>" : "<h3>Basarisiz</h3>";
  html += "<p>";
  html += message;
  html += "</p>";
  html += "<p><a href=\"/\">Ana sayfaya don</a></p>";
  html += "</body></html>";
  server.send(ok ? 200 : 400, "text/html; charset=utf-8", html);
}

static String buildMeasureJson(uint32_t ageMs) {
  // Keep JSON small and simple for clients.
  String json;
  json.reserve(160);
  json += '{';
  json += "\"rawKey\":" + String((unsigned long)(lastRawKey & 0x3FFFFFu));
  json += ",\"mm\":" + String(lastMm, 3);
  json += ",\"inch\":" + String(lastInch, 5);
  json += ",\"isInch\":" + String(lastIsInch ? "true" : "false");
  json += ",\"isNegative\":" + String(lastIsNegative ? "true" : "false");
  json += ",\"buttonPressed\":" + String(lastButtonPressed ? "true" : "false");
  json += ",\"offsetMm\":" + String(calOffsetMm, 3);
  json += ",\"ageMs\":" + String((unsigned long)ageMs);
  json += '}';
  return json;
}

static void handleApiMeasure() {
  const uint32_t age = (uint32_t)(millis() - lastUpdateMs);
  server.send(200, "application/json", buildMeasureJson(age));
}

static void handleWsEvent(uint8_t clientNum, WStype_t type, uint8_t *payload, size_t length) {
  (void)clientNum;
  (void)payload;
  (void)length;
  if (type == WStype_CONNECTED) {
    if (wsClientCount < 255) wsClientCount++;
  } else if (type == WStype_DISCONNECTED) {
    if (wsClientCount > 0) wsClientCount--;
  }
}

static void loadCalibration() {
  prefs.begin(CAL_NAMESPACE, true);
  calOffsetMm = prefs.getFloat(CAL_KEY_OFFSET, 0.0f);
  prefs.end();
}

static void saveCalibration(float offsetMm) {
  prefs.begin(CAL_NAMESPACE, false);
  prefs.putFloat(CAL_KEY_OFFSET, offsetMm);
  prefs.end();
  calOffsetMm = offsetMm;
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

static void startMdns() {
  if (!MDNS.begin(MDNS_HOSTNAME)) {
    Serial.println("mDNS baslatilamadi");
    return;
  }
  MDNS.addService("http", "tcp", 80);
  Serial.printf("mDNS: http://%s.local\n", MDNS_HOSTNAME);
}

void webBegin() {
  loadCalibration();
  const bool ok = connectWiFi(15000);

  if (ok) {
    Serial.printf("WiFi baglandi: %s  IP: %s\n", WIFI_SSID, WiFi.localIP().toString().c_str());
    startMdns();
  } else {
    Serial.printf("WiFi baglanamadi: %s (timeout)\n", WIFI_SSID);
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/measure", HTTP_GET, handleApiMeasure);
  server.on(UPDATE_PATH, HTTP_GET, handleUpdatePage);
  server.on(CAL_PATH, HTTP_GET, handleCalPage);
  server.on(CAL_PATH, HTTP_POST, []() {
    if (!server.hasArg("offset")) {
      server.send(400, "text/plain", "offset missing");
      return;
    }
    const float offset = server.arg("offset").toFloat();
    saveCalibration(offset);
    server.sendHeader("Location", "/calibrate");
    server.send(303);
  });
  server.on(UPDATE_PATH, HTTP_POST,
            []() {
              if (!updateInProgress) {
                handleUpdateResult(false, "Yukleme baslamadi.");
                return;
              }
              const bool ok = Update.end(true);
              updateInProgress = false;
              if (!ok) {
                handleUpdateResult(false, Update.errorString());
                return;
              }
              handleUpdateResult(true, "Guncelleme tamamlandi. Cihaz yeniden baslatiliyor...");
              delay(400);
              ESP.restart();
            },
            []() {
              HTTPUpload &upload = server.upload();
              if (upload.status == UPLOAD_FILE_START) {
                updateInProgress = false;
                if (upload.totalSize > UPDATE_MAX_SIZE) {
                  Update.abort();
                  return;
                }
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                  Update.abort();
                  return;
                }
                updateInProgress = true;
              } else if (upload.status == UPLOAD_FILE_WRITE) {
                if (updateInProgress) {
                  if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                    Update.abort();
                    updateInProgress = false;
                  }
                }
              } else if (upload.status == UPLOAD_FILE_END) {
                // Finalize in POST handler.
              } else if (upload.status == UPLOAD_FILE_ABORTED) {
                Update.abort();
                updateInProgress = false;
              }
            });
  server.onNotFound([]() { server.send(404, "text/plain", "Not Found"); });

  server.begin();
  wsServer.begin();
  wsServer.onEvent(handleWsEvent);
  lastUpdateMs = millis();
}

void webLoop() {
  server.handleClient();
  wsServer.loop();
}

void webUpdateMeasurement(uint32_t rawKey, float mm, float inch, bool isInch, bool isNegative) {
  lastRawKey = rawKey;
  lastMm = mm + calOffsetMm;
  lastInch = inch;
  lastIsInch = isInch;
  lastIsNegative = isNegative;
  lastUpdateMs = millis();

  if (wsClientCount > 0) {
    String payload = buildMeasureJson(0);
    wsServer.broadcastTXT(payload);
  }
}

void webUpdateButton(bool isPressed) {
  lastButtonPressed = isPressed;
  lastUpdateMs = millis();

  if (wsClientCount > 0) {
    String payload = buildMeasureJson(0);
    wsServer.broadcastTXT(payload);
  }
}
