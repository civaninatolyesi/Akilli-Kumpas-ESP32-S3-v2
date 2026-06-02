#include <Arduino.h>
#include <stdarg.h>

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
static float lastBatV = 0.0f;
static uint16_t lastBatAdcMv = 0;
static uint8_t lastBatPct = 0;
static bool lastBatCharging = false;
static uint32_t lastUpdateMs = 0;

static constexpr uint16_t LOG_CAP = 100;
static constexpr uint16_t LOG_LINE_MAX = 96;
static uint32_t logSeq = 0;
static uint16_t logCount = 0;
static char logLines[LOG_CAP][LOG_LINE_MAX];

static Preferences prefs;
static float calOffsetMm = 0.0f;

static void appendLogLine(const char *line) {
  if (!line || !line[0]) return;
  const uint16_t index = (uint16_t)(logSeq % LOG_CAP);
  strncpy(logLines[index], line, LOG_LINE_MAX - 1);
  logLines[index][LOG_LINE_MAX - 1] = '\0';
  logSeq++;
  if (logCount < LOG_CAP) {
    logCount++;
  }
}

void webLogf(const char *fmt, ...) {
  char buf[LOG_LINE_MAX];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  appendLogLine(buf);
}

static void appendJsonEscaped(String &out, const char *s) {
  while (*s) {
    const char c = *s++;
    if (c == '"') {
      out += "\\\"";
    } else if (c == '\\') {
      out += "\\\\";
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else if (c == '\t') {
      out += "\\t";
    } else if ((uint8_t)c < 0x20) {
      // Skip other control characters.
    } else {
      out += c;
    }
  }
}

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="tr">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Kumpas Ölçüm</title>
  <style>
    :root {
      color-scheme: light dark;
      --accent: #3cc7b0;
      --accent-strong: #24a88f;
      --accent-soft: rgba(60,199,176,.28);
      --accent-2: #5fe3a6;
    }
    :root {
      color-scheme: light dark;
      --accent: #3cc7b0;
      --accent-strong: #24a88f;
      --accent-soft: rgba(60,199,176,.28);
    }
    :root {
      color-scheme: light dark;
      --accent: #3cc7b0;
      --accent-strong: #24a88f;
      --accent-soft: rgba(60,199,176,.28);
    }
    body {
      font-family: "Trebuchet MS", "Lucida Sans", "Gill Sans", sans-serif;
      margin: 20px;
      background: radial-gradient(1200px 480px at 10% -10%, rgba(120,180,255,.18), transparent 60%),
                  radial-gradient(900px 420px at 110% 10%, rgba(80,180,120,.18), transparent 55%),
                  linear-gradient(180deg, rgba(255,255,255,.02), rgba(0,0,0,.05));
    }
    .card { max-width: 760px; margin: 0 auto; padding: 16px 18px; border: 1px solid rgba(127,127,127,.35); border-radius: 16px; background: rgba(255,255,255,.02); box-shadow: 0 12px 30px rgba(0,0,0,.08); }
    .row { display: flex; gap: 14px; flex-wrap: wrap; align-items: baseline; }
    .big { font-size: 52px; font-weight: 700; letter-spacing: -1px; }
    .unit { font-size: 18px; opacity: .75; margin-left: 8px; }
    .meta { margin-top: 12px; opacity: .75; font-size: 14px; }
    .pill { display:inline-block; padding: 2px 8px; border-radius: 999px; border: 1px solid rgba(127,127,127,.35); }
    .muted { opacity: .6; }
    .battery { display: grid; grid-template-columns: 1fr auto; gap: 12px; align-items: center; }
    .battery-bar { position: relative; height: 18px; border-radius: 999px; background: rgba(127,127,127,.2); overflow: hidden; }
    .battery-fill { height: 100%; width: 0%; background: linear-gradient(90deg, var(--accent-strong), var(--accent-2)); transition: width .25s ease; }
    .battery-fill.low { background: linear-gradient(90deg, #e05a5a, #f0b35a); }
    .battery-label { display: flex; align-items: baseline; gap: 10px; font-size: 22px; }
    .battery-volts { font-size: 16px; opacity: .75; }
    .charge-pill { display:inline-flex; align-items:center; gap:8px; padding: 4px 10px; border-radius: 999px; border: 1px solid rgba(127,127,127,.35); font-size: 12px; letter-spacing: .3px; text-transform: uppercase; }
    .charge-dot { width: 8px; height: 8px; border-radius: 50%; background: #a6a6a6; }
    .charge-pill.on { border-color: var(--accent-soft); color: var(--accent); }
    .charge-pill.on .charge-dot { background: var(--accent); box-shadow: 0 0 0 6px var(--accent-soft); animation: pulse 1.6s ease-in-out infinite; }
    @keyframes pulse { 0% { transform: scale(.85); opacity: .5; } 50% { transform: scale(1.1); opacity: 1; } 100% { transform: scale(.85); opacity: .5; } }
    .actions { margin-top: 12px; display: flex; gap: 10px; flex-wrap: wrap; }
    .action-link {
      text-decoration: none;
      padding: 6px 12px;
      border-radius: 999px;
      border: 1px solid rgba(127,127,127,.35);
      background: linear-gradient(160deg, rgba(255,255,255,.08), rgba(0,0,0,.05));
      box-shadow: inset 0 0 0 1px rgba(255,255,255,.05), 0 6px 16px rgba(0,0,0,.12);
      font-size: 13px;
      letter-spacing: .2px;
      transition: transform .12s ease, box-shadow .12s ease;
    }
    .action-link:hover { transform: translateY(-1px); box-shadow: inset 0 0 0 1px rgba(255,255,255,.08), 0 10px 22px rgba(0,0,0,.16); }
    .action-link:focus-visible { outline: 2px solid var(--accent-soft); outline-offset: 2px; }
    .action-link.log { border-color: var(--accent-soft); color: var(--accent); }
    .log-wrap { margin-top: 10px; border: 1px solid rgba(127,127,127,.25); border-radius: 10px; padding: 10px; background: rgba(0,0,0,.08); max-height: 260px; overflow: auto; font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, "Liberation Mono", monospace; font-size: 12px; }
    .btn { padding: 8px 12px; border-radius: 10px; border: 1px solid rgba(127,127,127,.35); background: rgba(255,255,255,.04); color: inherit; }
    .input { padding: 8px 10px; border-radius: 10px; border: 1px solid rgba(127,127,127,.35); background: rgba(255,255,255,.03); color: inherit; }
    .btn-ghost { padding: 6px 8px; border-radius: 8px; border: 1px solid rgba(127,127,127,.35); background: rgba(255,255,255,.02); }
    .note-wrap { position: relative; display: flex; align-items: center; gap: 8px; }
    .note-text { flex: 1; min-width: 0; }
    .note-edit { position: absolute; right: 0; top: 110%; z-index: 3; display: none; min-width: 220px; padding: 10px; border-radius: 12px; border: 1px solid rgba(127,127,127,.35); background: rgba(30,30,30,.9); box-shadow: 0 12px 24px rgba(0,0,0,.25); }
    .note-edit.open { display: block; }
    .note-actions { display: none; }
    .note-backdrop { position: fixed; inset: 0; background: rgba(0,0,0,.35); backdrop-filter: blur(1px); display: none; z-index: 2; }
    .note-backdrop.open { display: block; }
    @media (max-width: 640px) {
      body { margin: 12px; }
      .card { padding: 14px; border-radius: 14px; }
      .row { gap: 10px; }
      .big { font-size: 42px; }
      .unit { font-size: 16px; }
      .meta { font-size: 13px; }
      .actions { gap: 8px; }
      .action-link { padding: 6px 10px; font-size: 12px; }
      .battery-label { font-size: 20px; }
      .note-edit {
        position: fixed;
        left: 50%;
        top: 50%;
        transform: translate(-50%, -50%);
        width: calc(100% - 48px);
        max-width: 360px;
      }
    }
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

    <div class="actions">
      <a class="action-link" href="/update">Yazılım Güncelle</a>
      <a class="action-link" href="/calibrate">Kalibrasyon</a>
      <a class="action-link log" href="/log">Log</a>
    </div>
  </div>

  <div class="card" style="margin-top:16px;">
    <div class="row" style="justify-content: space-between; align-items:center;">
      <div>
        <div class="muted">Pil Durumu</div>
        <div class="battery-label"><span id="batPct">--%</span><span id="batV" class="battery-volts">-- V</span></div>
      </div>
      <div class="charge-pill" id="batCharge"><span class="charge-dot"></span><span id="batChargeText"></span></div>
    </div>
    <div class="battery" style="margin-top:10px;">
      <div class="battery-bar"><div id="batFill" class="battery-fill"></div></div>
      <div class="muted" style="font-size:12px;">GPIO5 ADC</div>
    </div>
  </div>

  <div id="noteBackdrop" class="note-backdrop"></div>

  <div class="card" style="margin-top:16px;">
    <div class="row" style="justify-content: space-between;">
      <div>
        <div class="muted">Buton Logu</div>
        <div class="muted" style="font-size:12px;">Butona basildiginda kayit alinir</div>
      </div>
      <div style="display:flex; gap:8px; align-items:center;">
        <button id="exportCsv" class="btn" type="button">CSV Indir</button>
        <button id="clearLog" class="btn" type="button">Temizle</button>
      </div>
    </div>
    <div class="row" style="margin-top:10px; align-items:center;">
      <label for="noteInput" class="muted" style="font-size:12px;">Not:</label>
      <input id="noteInput" class="input" type="text" maxlength="80" placeholder="Olcum notu" style="flex:1; min-width:180px;" />
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
  const exportCsv = document.getElementById('exportCsv');
  const noteInput = document.getElementById('noteInput');
  const noteBackdrop = document.getElementById('noteBackdrop');
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
    const userNote = noteInput.value.trim();
    const noteText = userNote;
    tr.innerHTML = `
      <td style="padding:6px 4px; border-bottom:1px solid rgba(127,127,127,.2);">${ts}</td>
      <td style="padding:6px 4px; text-align:right; border-bottom:1px solid rgba(127,127,127,.2);">${j.mm.toFixed(2)}</td>
      <td style="padding:6px 4px; text-align:right; border-bottom:1px solid rgba(127,127,127,.2);">${j.inch.toFixed(4)}</td>
      <td style="padding:4px; border-bottom:1px solid rgba(127,127,127,.2);">
        <div class="note-wrap">
          <span class="note-text"></span>
          <button class="btn-ghost note-btn" type="button" title="Notu duzenle">✎</button>
          <div class="note-edit">
            <input class="input note-input" type="text" maxlength="80" placeholder="Not" />
          </div>
        </div>
      </td>
    `;
    logBody.prepend(tr);

    const noteSpan = tr.querySelector('.note-text');
    const noteInputEl = tr.querySelector('.note-input');
    if (noteSpan) noteSpan.textContent = noteText;
    if (noteInputEl) noteInputEl.value = noteText;

    if (userNote) noteInput.value = '';

    const maxRows = 200;
    while (logBody.children.length > maxRows) {
      logBody.removeChild(logBody.lastElementChild);
    }
  }

  clearLog.addEventListener('click', () => {
    logBody.innerHTML = '';
  });

  function closeAllEditors(){
    logBody.querySelectorAll('.note-edit.open').forEach((el) => {
      el.classList.remove('open');
    });
    if (noteBackdrop) noteBackdrop.classList.remove('open');
  }

  logBody.addEventListener('click', (e) => {
    const btn = e.target.closest('.note-btn');
    if (btn) {
      closeAllEditors();
      const wrap = btn.closest('.note-wrap');
      const editor = wrap ? wrap.querySelector('.note-edit') : null;
      const input = wrap ? wrap.querySelector('.note-input') : null;
      const text = wrap ? wrap.querySelector('.note-text') : null;
      if (editor) {
        if (input && text) input.value = text.textContent || '';
        editor.classList.add('open');
        if (input) input.focus();
        if (noteBackdrop) noteBackdrop.classList.add('open');
      }
      return;
    }

  });

  logBody.addEventListener('focusout', (e) => {
    const input = e.target.closest('.note-input');
    if (!input) return;
    const wrap = input.closest('.note-wrap');
    const text = wrap ? wrap.querySelector('.note-text') : null;
    const editor = wrap ? wrap.querySelector('.note-edit') : null;
    if (text) text.textContent = input.value.trim();
    if (editor) editor.classList.remove('open');
    if (noteBackdrop) noteBackdrop.classList.remove('open');
  });

  document.addEventListener('click', (e) => {
    if (!e.target.closest('.note-wrap')) closeAllEditors();
  });

  if (noteBackdrop) {
    noteBackdrop.addEventListener('click', () => {
      closeAllEditors();
    });
  }

  exportCsv.addEventListener('click', () => {
    const now = new Date();
    const pad2 = (n) => String(n).padStart(2, '0');
    const stamp = `${now.getFullYear()}${pad2(now.getMonth() + 1)}${pad2(now.getDate())}-${pad2(now.getHours())}${pad2(now.getMinutes())}${pad2(now.getSeconds())}`;
    const rows = [
      ['Zaman', 'mm', 'in', 'Not']
    ];
    Array.from(logBody.children).reverse().forEach((tr) => {
      const cells = Array.from(tr.children).map((td, idx) => {
        if (idx === 3) {
          const span = td.querySelector('.note-text');
          return span ? span.textContent : '';
        }
        return td.textContent || '';
      });
      rows.push(cells);
    });
    const csv = rows.map((row) => row.map((v) => {
      const s = String(v).replace(/"/g, '""');
      return `"${s}"`;
    }).join(',')).join('\n');
    const bom = '\ufeff';
    const blob = new Blob([bom + csv], { type: 'text/csv;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `buton-logu-${stamp}.csv`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
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
      const pct = Math.max(0, Math.min(100, j.batPct));
      document.getElementById('batPct').textContent = pct + '%';
      document.getElementById('batV').textContent = j.batV.toFixed(2) + ' V';
      const charge = document.getElementById('batCharge');
      const chargeText = document.getElementById('batChargeText');
      if (j.batCharging) {
        charge.classList.add('on');
        chargeText.textContent = 'Voltaj yükseliyor';
      } else {
        charge.classList.remove('on');
        chargeText.textContent = '';
      }
      const fill = document.getElementById('batFill');
      fill.style.width = pct + '%';
      if (pct <= 20) {
        fill.classList.add('low');
      } else {
        fill.classList.remove('low');
      }

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

static const char LOG_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="tr">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Log</title>
  <style>
    :root {
      color-scheme: light dark;
      --accent: #3cc7b0;
      --accent-strong: #24a88f;
      --accent-soft: rgba(60,199,176,.28);
      --accent-2: #5fe3a6;
    }
    body {
      font-family: "Trebuchet MS", "Lucida Sans", "Gill Sans", sans-serif;
      margin: 20px;
      background: radial-gradient(1200px 480px at 10% -10%, rgba(120,180,255,.18), transparent 60%),
                  radial-gradient(900px 420px at 110% 10%, rgba(80,180,120,.18), transparent 55%),
                  linear-gradient(180deg, rgba(255,255,255,.02), rgba(0,0,0,.05));
    }
    .card { max-width: 760px; margin: 0 auto; padding: 16px 18px; border: 1px solid rgba(127,127,127,.35); border-radius: 16px; background: rgba(255,255,255,.02); box-shadow: 0 12px 30px rgba(0,0,0,.08); }
    .row { display: flex; gap: 14px; flex-wrap: wrap; align-items: center; justify-content: space-between; }
    .muted { opacity: .7; }
    .action-link {
      text-decoration: none;
      padding: 6px 12px;
      border-radius: 999px;
      border: 1px solid rgba(127,127,127,.35);
      background: linear-gradient(160deg, rgba(255,255,255,.08), rgba(0,0,0,.05));
      box-shadow: inset 0 0 0 1px rgba(255,255,255,.05), 0 6px 16px rgba(0,0,0,.12);
      font-size: 13px;
      letter-spacing: .2px;
      display: inline-flex;
      align-items: center;
      gap: 8px;
    }
    .action-link:focus-visible { outline: 2px solid var(--accent-soft); outline-offset: 2px; }
    .log-wrap { margin-top: 10px; border: 1px solid rgba(127,127,127,.25); border-radius: 10px; padding: 10px; background: rgba(0,0,0,.08); max-height: 420px; overflow: auto; font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, "Liberation Mono", monospace; font-size: 12px; }
    .log-line { white-space: pre-wrap; line-height: 1.35; border-bottom: 1px dashed rgba(127,127,127,.15); padding: 2px 0; }
    .log-line:last-child { border-bottom: none; }
    .btn { padding: 8px 12px; border-radius: 10px; border: 1px solid var(--accent-soft); background: rgba(255,255,255,.04); color: inherit; }
    @media (max-width: 640px) {
      body { margin: 12px; }
      .card { padding: 14px; border-radius: 14px; }
      .row { gap: 10px; }
      .action-link { padding: 6px 10px; font-size: 12px; }
      .log-wrap { max-height: 520px; }
    }
  </style>
</head>
<body>
  <div class="card">
    <div class="row">
      <div>
        <div class="muted">Log</div>
        <div class="muted" style="font-size:12px;">Hata ve durum kayitlari</div>
      </div>
      <div style="display:flex; gap:10px; align-items:center;">
        <button id="clearLogUi" class="btn" type="button">Temizle</button>
        <a class="action-link" href="/">Ana sayfa <-</a>
      </div>
    </div>
    <div id="logWrap" class="log-wrap"></div>
  </div>

<script>
  const logWrap = document.getElementById('logWrap');
  const clearLogUi = document.getElementById('clearLogUi');
  let lastLogSeq = 0;

  clearLogUi.addEventListener('click', () => {
    logWrap.innerHTML = '';
    lastLogSeq = 0;
  });

  async function tick(){
    try {
      const r = await fetch('/api/log?since=' + lastLogSeq, { cache: 'no-store' });
      const j = await r.json();
      if (Array.isArray(j.lines)) {
        j.lines.forEach((line) => {
          const div = document.createElement('div');
          div.className = 'log-line';
          div.textContent = line.text;
          logWrap.appendChild(div);
        });
        if (j.lines.length > 0) {
          logWrap.scrollTop = logWrap.scrollHeight;
        }
      }
      if (typeof j.seq === 'number') lastLogSeq = j.seq;
    } catch(e) {
      // ignore
    }
  }

  setInterval(tick, 250);
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
    body {
      font-family: "Trebuchet MS", "Lucida Sans", "Gill Sans", sans-serif;
      margin: 20px;
      background: radial-gradient(1200px 480px at 10% -10%, rgba(120,180,255,.18), transparent 60%),
                  radial-gradient(900px 420px at 110% 10%, rgba(80,180,120,.18), transparent 55%),
                  linear-gradient(180deg, rgba(255,255,255,.02), rgba(0,0,0,.05));
    }
    .card { max-width: 640px; margin: 0 auto; padding: 16px 18px; border: 1px solid rgba(127,127,127,.35); border-radius: 16px; background: rgba(255,255,255,.02); box-shadow: 0 12px 30px rgba(0,0,0,.08); }
    .row { display: flex; gap: 12px; align-items: center; flex-wrap: wrap; }
    .muted { opacity: .7; }
    .input { padding: 8px 10px; border-radius: 10px; border: 1px solid rgba(127,127,127,.35); background: rgba(255,255,255,.03); color: inherit; font-size: 16px; width: 180px; }
    .btn { padding: 8px 14px; border-radius: 10px; border: 1px solid var(--accent-soft); background: linear-gradient(160deg, rgba(60,199,176,.18), rgba(0,0,0,.05)); color: inherit; font-size: 16px; }
    @media (max-width: 640px) {
      body { margin: 12px; }
      .card { padding: 14px; border-radius: 14px; }
      .input { width: 100%; }
    }
  </style>
</head>
<body>
  <div class="card">
    <h3>Kalibrasyon (mm)</h3>
    <p class="muted">Offset mm degerini girin. Ornek: -0.01</p>
    <form method="POST" action="/calibrate">
      <div class="row">
        <input class="input" type="number" name="offset" step="0.001" required />
        <button class="btn" type="submit">Kaydet</button>
      </div>
    </form>
    <form method="POST" action="/calibrate" style="margin-top:12px;">
      <input type="hidden" name="offset" value="0" />
      <button class="btn" type="submit">Offset Sifirla</button>
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
    body {
      font-family: "Trebuchet MS", "Lucida Sans", "Gill Sans", sans-serif;
      margin: 20px;
      background: radial-gradient(1200px 480px at 10% -10%, rgba(120,180,255,.18), transparent 60%),
                  radial-gradient(900px 420px at 110% 10%, rgba(80,180,120,.18), transparent 55%),
                  linear-gradient(180deg, rgba(255,255,255,.02), rgba(0,0,0,.05));
    }
    .card { max-width: 640px; margin: 0 auto; padding: 16px 18px; border: 1px solid rgba(127,127,127,.35); border-radius: 16px; background: rgba(255,255,255,.02); box-shadow: 0 12px 30px rgba(0,0,0,.08); }
    .row { display: flex; gap: 12px; align-items: center; flex-wrap: wrap; }
    .muted { opacity: .7; }
    .input { width: 100%; padding: 8px 10px; border-radius: 10px; border: 1px solid rgba(127,127,127,.35); background: rgba(255,255,255,.03); color: inherit; }
    .btn { padding: 8px 14px; border-radius: 10px; border: 1px solid var(--accent-soft); background: linear-gradient(160deg, rgba(60,199,176,.18), rgba(0,0,0,.05)); color: inherit; font-size: 16px; }
    @media (max-width: 640px) {
      body { margin: 12px; }
      .card { padding: 14px; border-radius: 14px; }
    }
  </style>
</head>
<body>
  <div class="card">
    <h3>Yazılım Güncelle</h3>
    <p class="muted">PlatformIO ile derlediginiz .bin dosyasini secin.</p>
    <form method="POST" action="/update" enctype="multipart/form-data">
      <div class="row">
        <input class="input" type="file" name="firmware" accept=".bin" required />
        <button class="btn" type="submit">Yukle</button>
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

static void handleLogPage() {
  server.send(200, "text/html; charset=utf-8", LOG_HTML);
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
  json += ",\"batV\":" + String(lastBatV, 3);
  json += ",\"batAdcMv\":" + String((unsigned long)lastBatAdcMv);
  json += ",\"batPct\":" + String((unsigned int)lastBatPct);
  json += ",\"batCharging\":" + String(lastBatCharging ? "true" : "false");
  json += ",\"ageMs\":" + String((unsigned long)ageMs);
  json += '}';
  return json;
}

static void handleApiMeasure() {
  const uint32_t age = (uint32_t)(millis() - lastUpdateMs);
  server.send(200, "application/json", buildMeasureJson(age));
}

static void handleApiLog() {
  const uint32_t since = server.hasArg("since") ? (uint32_t)server.arg("since").toInt() : 0;
  String json;
  json.reserve(512);
  json += '{';
  json += "\"seq\":" + String((unsigned long)logSeq);
  json += ",\"lines\":[";

  if (logCount > 0) {
    const uint32_t firstSeq = (logSeq >= logCount) ? (logSeq - logCount) : 0;
    bool first = true;
    for (uint16_t i = 0; i < logCount; i++) {
      const uint32_t id = firstSeq + i + 1;
      if (id <= since) continue;
      const uint16_t index = (uint16_t)((id - 1) % LOG_CAP);
      if (!first) json += ',';
      json += '{';
      json += "\"id\":" + String((unsigned long)id) + ",\"text\":";
      json += '"';
      appendJsonEscaped(json, logLines[index]);
      json += '"';
      json += '}';
      first = false;
    }
  }

  json += ']';
  json += '}';
  server.send(200, "application/json", json);
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
  webLogf("Boot: web ui start");
  const bool ok = connectWiFi(15000);

  if (ok) {
    Serial.printf("WiFi baglandi: %s  IP: %s\n", WIFI_SSID, WiFi.localIP().toString().c_str());
    webLogf("WiFi: connected %s", WiFi.localIP().toString().c_str());
    startMdns();
  } else {
    Serial.printf("WiFi baglanamadi: %s (timeout)\n", WIFI_SSID);
    webLogf("WiFi: connect timeout");
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/measure", HTTP_GET, handleApiMeasure);
  server.on("/api/log", HTTP_GET, handleApiLog);
  server.on("/log", HTTP_GET, handleLogPage);
  server.on(UPDATE_PATH, HTTP_GET, handleUpdatePage);
  server.on(CAL_PATH, HTTP_GET, handleCalPage);
  server.on(CAL_PATH, HTTP_POST, []() {
    if (!server.hasArg("offset")) {
      server.send(400, "text/plain", "offset missing");
      webLogf("Cal: offset missing");
      return;
    }
    const float offset = server.arg("offset").toFloat();
    saveCalibration(offset);
    webLogf("Cal: offset=%.3f", offset);
    server.sendHeader("Location", "/calibrate");
    server.send(303);
  });
  server.on(UPDATE_PATH, HTTP_POST,
            []() {
              if (!updateInProgress) {
                handleUpdateResult(false, "Yukleme baslamadi.");
                webLogf("OTA: upload not started");
                return;
              }
              const bool ok = Update.end(true);
              updateInProgress = false;
              if (!ok) {
                webLogf("OTA: end failed %s", Update.errorString());
                handleUpdateResult(false, Update.errorString());
                return;
              }
              webLogf("OTA: update ok, rebooting");
              handleUpdateResult(true, "Guncelleme tamamlandi. Cihaz yeniden baslatiliyor...");
              delay(400);
              ESP.restart();
            },
            []() {
              HTTPUpload &upload = server.upload();
              if (upload.status == UPLOAD_FILE_START) {
                updateInProgress = false;
                if (upload.totalSize > UPDATE_MAX_SIZE) {
                  webLogf("OTA: size too large (%lu)", (unsigned long)upload.totalSize);
                  Update.abort();
                  return;
                }
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                  webLogf("OTA: begin failed %s", Update.errorString());
                  Update.abort();
                  return;
                }
                updateInProgress = true;
              } else if (upload.status == UPLOAD_FILE_WRITE) {
                if (updateInProgress) {
                  if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                    webLogf("OTA: write failed %s", Update.errorString());
                    Update.abort();
                    updateInProgress = false;
                  }
                }
              } else if (upload.status == UPLOAD_FILE_END) {
                // Finalize in POST handler.
              } else if (upload.status == UPLOAD_FILE_ABORTED) {
                webLogf("OTA: upload aborted");
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

void webUpdateBattery(float volts, uint16_t adcMv, uint8_t percent, bool isCharging) {
  lastBatV = volts;
  lastBatAdcMv = adcMv;
  lastBatPct = percent;
  lastBatCharging = isCharging;
  lastUpdateMs = millis();

  if (wsClientCount > 0) {
    String payload = buildMeasureJson(0);
    wsServer.broadcastTXT(payload);
  }
}

