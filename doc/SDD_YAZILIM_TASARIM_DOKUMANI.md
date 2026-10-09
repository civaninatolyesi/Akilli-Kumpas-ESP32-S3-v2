# Yazılım Tasarım Dokümanı (SDD)

**Proje:** ESP32-S3 Super Mini — Dijital Kumpas Veri Okuma  
**Doküman kimliği:** SDD-KUMPAS  
**Sürüm:** 1.0  
**Tarih:** 2026-10-10  
**Durum:** Yürürlükte  
**Kapsadığı kod:** commit `2e2619b`  
**İlgili dokümanlar:** [SRS](SRS_YAZILIM_GEREKSINIM_DOKUMANI.md) · [Bilinen Sorunlar](BILINEN_SORUNLAR.md) · [ADR dizini](adr/) · [GUIDE dizini](guide/)

> Bu doküman IEEE 1016 (Software Design Descriptions) görünüm yapısı örnek alınarak hazırlanmıştır: bağlam, bileşen, veri, dinamik davranış, zamanlama, arayüz ve güvenlik görünümleri. Her tasarım öğesi ilgili kaynak satırına ve onu gerekçelendiren ADR'ye bağlanmıştır.

---

## İçindekiler

1. [Giriş](#1-giriş)
2. [Sistem Bağlamı](#2-sistem-bağlamı)
3. [Mimari Genel Bakış](#3-mimari-genel-bakış)
4. [Bileşen Tasarımı](#4-bileşen-tasarımı)
5. [Dinamik Davranış](#5-dinamik-davranış)
6. [Veri Tasarımı](#6-veri-tasarımı)
7. [Zamanlama Analizi](#7-zamanlama-analizi)
8. [Modül Arayüz Sözleşmesi](#8-modül-arayüz-sözleşmesi)
9. [Hata Yönetimi](#9-hata-yönetimi)
10. [Güvenlik Tasarımı](#10-güvenlik-tasarımı)
11. [Yapılandırma ve Derleme](#11-yapılandırma-ve-derleme)
12. [Tasarım Değişmezleri](#12-tasarım-değişmezleri)
13. [Genişletme Noktaları](#13-genişletme-noktaları)
14. [Bilinen Sorunlar ve Teknik Borç](#14-bilinen-sorunlar-ve-teknik-borç)
15. [Değişiklik Geçmişi](#15-değişiklik-geçmişi)

---

## 1. Giriş

### 1.1 Amaç

SRS'de tanımlanan gereksinimlerin **nasıl** gerçekleştirildiğini anlatır. Bir geliştiricinin kodu okumadan önce sistemin yapısını, veri akışını, zamanlama varsayımlarını ve kırılgan noktalarını anlamasını hedefler.

### 1.2 Tasarım Hedefleri (öncelik sırasıyla)

1. **Bit kaybetmemek** — kumpas protokolü yeniden gönderim yapmaz; kaçan bit çerçeveyi bozar.
2. **Yanlış değeri asla yayınlamamak** — gecikme, yanlış ölçümden daha kabul edilebilirdir.
3. **Ağdan bağımsız çalışmak** — WiFi sorunu ölçümü durdurmamalı.
4. **Basitlik** — iki kaynak dosya, tek döngü, tek dış kütüphane.
5. **Sıfır kurulumlu istemci** — tarayıcı yeterli; uygulama yükleme gerekmez.

### 1.3 Okuma Rehberi

| Okuyucu | Önerilen bölümler |
|---|---|
| Yeni geliştirici | 2, 3, 5, 8, 12 |
| Kumpas uyarlayan | 4.1–4.4, 6.2, 7.1 |
| Entegrasyon geliştiricisi | 4.7, 6.3, 6.4 |
| Güvenlik değerlendiricisi | 10, 14 |

---

## 2. Sistem Bağlamı

```
                         ┌──────────────────────────────────────────────┐
                         │             KUMPAS OKUYUCU (ESP32-S3)        │
                         │                                              │
  Dijital kumpas ──CLK──►│ GPIO9 ─► ISR                                 │
                 ──DATA─►│ GPIO8 ─┘                                     │
  LiPo + bölücü ────────►│ GPIO5 (ADC1)                                 │◄── USB-CDC (seri log, ilk yükleme)
  Buton ────────────────►│ GPIO10                                       │
                         │                                              │
                         │ WiFi STA ── HTTP :80 ── WS :81 ── mDNS       │
                         └──────────┬───────────────┬───────────────────┘
                                    │               │
                      HTTP GET/POST │               │ WS push (JSON)
                                    ▼               ▼
                         ┌────────────────┐  ┌──────────────────────────┐
                         │ Tarayıcı       │  │ measure_ws.py            │──► pyautogui ──► Aktif pencere
                         │ (index/log/    │  │ (veya başka WS istemcisi)│                 (Excel, CAD…)
                         │  cal/update)   │  └──────────────────────────┘
                         └────────────────┘
```

**Dış aktörler:**

| Aktör | Yön | Kanal | Veri |
|---|---|---|---|
| Kumpas | → cihaz | GPIO8/9 | 24-bit çerçeveler |
| Batarya | → cihaz | GPIO5 | Analog gerilim |
| Buton | → cihaz | GPIO10 | Dijital seviye |
| Tarayıcı | ↔ cihaz | HTTP | HTML, JSON, form, firmware |
| WS istemcisi | ← cihaz | WebSocket | JSON |
| Geliştirici | ↔ cihaz | USB | Firmware, seri log |

---

## 3. Mimari Genel Bakış

### 3.1 Katmanlar

```
┌─────────────────────────────────────────────────────────────────┐
│ Sunum katmanı (tarayıcıda)   INDEX_HTML / LOG_HTML / CAL / UPDATE│  JS + CSS
├─────────────────────────────────────────────────────────────────┤
│ Ağ/servis katmanı            web_ui.cpp                          │  WiFi, mDNS, WebServer,
│                              (durum önbelleği, JSON, NVS, OTA,   │  WebSocketsServer, Preferences,
│                               log halka tamponu)                 │  Update
├──────────────────────── web_ui.h (sözleşme) ────────────────────┤
│ Uygulama/ölçüm katmanı       main.cpp: loop()                    │  filtre, birim, batarya,
│                                                                  │  buton
├─────────────────────────────────────────────────────────────────┤
│ Sürücü katmanı               main.cpp: ISR + caliperBegin/Read   │  esp_timer, gpio_get_level,
│                                                                  │  attachInterrupt, ADC
├─────────────────────────────────────────────────────────────────┤
│ Platform                     Arduino-ESP32 / ESP-IDF / FreeRTOS  │
└─────────────────────────────────────────────────────────────────┘
```

Bağımlılık yönü yalnızca aşağıya doğrudur. `web_ui.cpp`, `main.cpp`'deki hiçbir sembolü bilmez; `main.cpp` ağ katmanını yalnızca `web_ui.h` üzerinden çağırır ([ADR-010](adr/ADR-010_MODUL_AYRIMI_VE_WEB_UI_ARAYUZU.md)).

### 3.2 Bileşen Kataloğu

| Kimlik | Bileşen | Dosya | Sorumluluk |
|---|---|---|---|
| C1 | Kumpas sürücüsü | main.cpp | Kenar yakalama, bit toplama, çerçeve teslimi |
| C2 | Çözümleyici | main.cpp | Büyüklük/işaret/birim ayrıştırma, birim dönüşümü |
| C3 | Stabilizasyon filtresi | main.cpp (`loop`) | Ardışık tekrar doğrulaması |
| C4 | Birim çıkarımı | main.cpp | Süreklilik tabanlı mm/inç kararı |
| C5 | Batarya izleyici | main.cpp (`loop`) | ADC, filtre, yüzde, şarj tahmini, raporlama |
| C6 | Buton okuyucu | main.cpp (`loop`) | Debounce, durum değişimi bildirimi |
| C7 | Web servis katmanı | web_ui.cpp | WiFi, mDNS, HTTP, WS, kalibrasyon, OTA, log |
| C8 | Tarayıcı istemcisi | web_ui.cpp (gömülü HTML/JS) | Gösterim, buton logu, CSV |
| C9 | Python istemcisi | scripts/measure_ws.py | WS dinleme, klavye emülasyonu |

### 3.3 Dosya Yapısı

| Dosya | Satır | İçerik |
|---|---|---|
| [src/main.cpp](../src/main.cpp) | 473 | C1–C6, `setup()`, `loop()` |
| [src/web_ui.cpp](../src/web_ui.cpp) | 920 | C7 ve C8 (4 gömülü sayfa: 15 898 + 3 874 + 2 110 + 1 937 bayt) |
| [include/web_ui.h](../include/web_ui.h) | 18 | Modüller arası sözleşme |
| include/wifi_secrets.h | — | `WIFI_SSID`, `WIFI_PASSWORD` (git dışı) |
| [scripts/measure_ws.py](../scripts/measure_ws.py) | 103 | C9 |
| [platformio.ini](../platformio.ini) | — | Kart, flash, bölüm, bayraklar, bağımlılık |
| [partitions_ota_4mb.csv](../partitions_ota_4mb.csv) | 6 | Flash yerleşimi |
| [pio.ps1](../pio.ps1) | 9 | PlatformIO penv sarmalayıcısı (Windows) |
| [lib/ESP32_BLE_Keyboard/](../lib/ESP32_BLE_Keyboard/) | — | Kullanılmayan iskelet (KS-15) |

### 3.4 Yürütme Modeli

Tek "süper döngü" + tek kesme ([ADR-009](adr/ADR-009_TEK_SUPER_DONGU_MIMARISI.md)):

```
               ┌───────────────── Arduino loopTask (FreeRTOS, çekirdek 1) ─────────────────┐
 setup() ───►  │ loop():                                                                   │
               │   webLoop()            // HTTP istemcisi + WS olayları                    │
               │   buton debounce       // her tur                                         │
               │   batarya              // 2000 ms'de bir                                  │
               │   caliperReadFrame()   // çerçeve varsa filtre → birim → yayın            │
               │   delay(2)                                                                 │
               └───────────────────────────────────────────────────────────────────────────┘
                        ▲  g_lastFrame / g_hasFrame  (portMUX kritik bölge)
               ┌────────┴────────┐
               │ ISR (GPIO9 ↓)   │  her düşen kenarda; IRAM'de
               └─────────────────┘
```

WiFi/TCP yığını ESP-IDF'in kendi görevlerinde çalışır; uygulama kodu yalnızca `loopTask` içindedir. Bu yüzden C7'deki durum değişkenleri için kilit gerekmez (tek yazıcı/okuyucu görev).

---

## 4. Bileşen Tasarımı

### 4.1 C1 — Kumpas Sürücüsü (ISR)

**Kod:** [main.cpp:175-236](../src/main.cpp#L175-L236) · **ADR:** [003](adr/ADR-003_KESME_TABANLI_KUMPAS_OKUMA.md), [004](adr/ADR-004_ZAMAN_BOSLUGU_ILE_CERCEVE_AYIRMA.md), [005](adr/ADR-005_ISR_ANA_DONGU_VERI_PAYLASIMI.md)

**Durum değişkenleri:**

| Değişken | Tip | Yazan | Okuyan | Açıklama |
|---|---|---|---|---|
| `g_lastEdgeUs` | `volatile uint32_t` | ISR | ISR | Son düşen kenar zamanı (µs) |
| `g_shiftReg` | `volatile uint32_t` | ISR | ISR | Toplanan bitler |
| `g_bitCount` | `volatile uint8_t` | ISR | ISR | Toplanan bit sayısı (0–23) |
| `g_lastFrame` | `volatile uint32_t` | ISR | loop | Teslim edilen çerçeve |
| `g_hasFrame` | `volatile bool` | ISR, loop | loop | Posta kutusu bayrağı |
| `g_mux` | `portMUX_TYPE` | — | — | Kritik bölge spinlock'u |

**Algoritma (`onCaliperClockFalling`):**

```
now  ← esp_timer_get_time()          // 64-bit µs → 32-bit'e kesilir; fark aritmetiği taşmaya dayanıklı
gap  ← now − g_lastEdgeUs
g_lastEdgeUs ← now
if gap < MIN_EDGE_US (5):   return                       // gürültü
if gap > FRAME_GAP_US (1500): g_bitCount ← 0; g_shiftReg ← 0   // yeni çerçeve
bit  ← gpio_get_level(DATA) & 1
g_shiftReg |= bit << g_bitCount; g_bitCount++
if g_bitCount ≥ 24:
    [kritik bölge] g_lastFrame ← g_shiftReg; g_hasFrame ← true
    g_bitCount ← 0; g_shiftReg ← 0
```

**Tasarım notları:**

- `micros()`/`digitalRead()` yerine `esp_timer_get_time()`/`gpio_get_level()` kullanılır; ISR `ARDUINO_ISR_ATTR` (IRAM) ile işaretlidir — flash önbelleği devre dışıyken (ör. NVS yazımı, OTA) bile çalışabilmesi hedeflenir.
- Gürültü olarak atılan kenar da `g_lastEdgeUs`'i günceller. Böylece bir sıçrama dizisi, sonraki gerçek kenarın "gap" hesabını kısaltır; bu bilinçli bir sadeleştirmedir (gürültü patlamasından sonra ilk gerçek kenar da atılabilir, bu durumda çerçeve bozulur ve C3 filtresi tarafından elenir).
- Çerçeve teslimi **tek slotluk posta kutusudur**: loop gecikirse eski çerçevenin üstüne yazılır. Kuyruk yoktur, çünkü yalnızca en güncel değer anlamlıdır.
- `caliperReadFrame()` hızlı yol olarak `g_hasFrame`'i kilitsiz okur, ardından kritik bölgede kopyalar ve bayrağı temizler.

**`caliperBegin(dataPin, clkPin)`:** pinleri `INPUT` yapar, durum değişkenlerini sıfırlar, `attachInterrupt(clkPin, …, FALLING)` çağırır. `setup()` içinde **`webBegin()`'den önce** çağrılır; böylece WiFi bağlanırken de çerçeveler toplanır (loop çalışmadığı için yalnızca sonuncusu korunur).

### 4.2 C2 — Çözümleyici

**Kod:** [main.cpp:50-67](../src/main.cpp#L50-L67), [main.cpp:112-131](../src/main.cpp#L112-L131), [main.cpp:238-283](../src/main.cpp#L238-L283) · **ADR:** [006](adr/ADR-006_KUMPAS_BIT_DUZENI_VE_ISARET_BITI.md)

```
struct CaliperDecoded { bool isNegative; bool isInch; uint32_t magnitude; };

decode24(raw):
  magnitude  ← raw & valueMask()                  // valueMask = (1<<20)−1 = 0x0FFFFF
  signSet    ← bit(raw, g_signBit)                 // 21
  unitSet    ← (g_unitBit == g_signBit) ? false : bit(raw, g_unitBit)   // 23
  isNegative ← g_signBitMeansNegativeWhen1 ? signSet : ¬signSet
  isInch     ← g_unitBitMeansInchWhen1     ? unitSet : ¬unitSet

countsSignedFromRaw(raw, d) = d.isNegative ? −magnitude : magnitude
countsToMillimeters(c, inch) = inch ? c/4000·25,4 : c/200
countsToInches(c, inch)      = inch ? c/4000      : c/200/25,4
```

| Büyüklük | Değer |
|---|---|
| Çözünürlük (mm modu) | 1/200 = 0,005 mm |
| Çözünürlük (inç modu) | 1/4000 = 0,00025 in |
| Teorik azami büyüklük | 2²⁰ − 1 = 1 048 575 count ≈ 5 242,9 mm |
| Uygulanan sınır | `MAX_ABS_MM` = 500 mm ⇒ 100 000 count |

`toMillimeters()` ve `toInches()` fonksiyonları tanımlıdır ancak `loop()` bunları değil, işaretli count üzerinden çalışan `countsToMillimeters()`/`countsToInches()` fonksiyonlarını kullanır (birim çıkarımının sonucu ile hesaplamak için). İlk ikisi ölü koddur.

`g_signBit`, `g_unitBit` ve polarite bayrakları **çalışma zamanı değişkenleridir**; eskiden seri komutlarla değiştirilebiliyordu (commit `4fed220` ile kaldırıldı). Şu an derleme zamanı sabitlerinden başlatılır ve değişmez.

### 4.3 C3 — Stabilizasyon Filtresi

**Kod:** [main.cpp:422-470](../src/main.cpp#L422-L470) · **ADR:** [007](adr/ADR-007_TEKRAR_TABANLI_STABILIZASYON_FILTRESI.md)

`loop()` içinde statik değişkenlerle tutulan bir durum makinesi:

| Değişken | Anlam |
|---|---|
| `pendingKey` | Son görülen anahtar |
| `pendingCount` | `pendingKey`'in ardışık görülme sayısı (255'te doyar) |
| `lastStableKey` | Son yayınlanan anahtar |
| `hasLastPublished`, `lastPublishedMm` | Son yayınlanan değer (ofsetsiz) |

```
raw ← raw & 0xFFFFFF
key ← raw & (valueMask | 1<<signBit | 1<<unitBit)      // bit 20 ve 22 karşılaştırmaya girmez
[REJECT_IF_TOP_BITS_SET ise ve bit22|23 ≠ 0 → çerçeveyi at]
key == pendingKey ? pendingCount++ : (pendingKey ← key, pendingCount ← 1)
if pendingCount ≥ 2 and pendingKey ≠ lastStableKey:
    counts, isInch ← decode + C4
    mm, inch ← dönüştür
    required ← (hasLastPublished and |mm − lastPublishedMm| > 0,50) ? 4 : 2
    if pendingCount < required: return
    if isfinite(mm) and |mm| ≤ 500:
        lastStableKey ← pendingKey; lastPublished* ← …
        webUpdateMeasurement(pendingKey, mm, inch, isInch, counts < 0)
```

**Durum diyagramı (tek anahtar için):**

```
           yeni key                   aynı key                aynı key (sıçrama yoksa)
 [Yok] ───────────────► [Görüldü:1] ──────────► [Görüldü:2] ───────────────────────────► [Yayınlandı]
                              ▲                      │ sıçrama > 0,5 mm
                              │ farklı key           ▼
                              └──────────── [Görüldü:2..3] ──aynı key──► [Görüldü:4] ──► [Yayınlandı]
```

**Not:** Sıçrama eşiği `lastPublishedMm` ile karşılaştırılır; yani kumpas hızlıca kaydırılıp yeni bir noktada durdurulduğunda ilk yayın 4 çerçeve sonra gelir, ardından gelen küçük değişimler 2 çerçeve ile yayınlanır.

### 4.4 C4 — Birim Çıkarımı

**Kod:** [main.cpp:90-101](../src/main.cpp#L90-L101), [main.cpp:133-173](../src/main.cpp#L133-L173) · **ADR:** [008](adr/ADR-008_SUREKLILIK_ILE_BIRIM_CIKARIMI.md)

```
updateUnitStateByContinuity(counts, bitSaysInch, hasLast, lastMm):
  ilk çağrı:  state ← bitSaysInch; return state
  autoFix kapalı veya hasLast yok: pending ← 0; return state
  dCur ← |mm(counts, state)  − lastMm|
  dAlt ← |mm(counts, ¬state) − lastMm|
  strong ← dCur ≥ 4,0 mm and dAlt ≤ 0,40 mm
  strong değilse: pending ← 0; return state
  aday ← ¬state; aynı aday için pending++  (değilse pending ← 1)
  pending ≥ 3 ise: state ← aday; pending ← 0
  return state
```

**Analiz — algılanabilirlik sınırı:** Aynı fiziksel uzunluk X mm için mm modunda count = 200·X, inç modunda count ≈ 157,48·X. Yanlış yorumla elde edilen değer doğru değerin 0,787 ya da 1,27 katıdır. "Mevcut yorum en az 4 mm yanlış" koşulu:

| Gerçek geçiş | Yanlış yorumdaki sapma | Algılama için gereken X |
|---|---|---|
| mm → inç (durum mm iken kumpas inç'e alındı) | 0,2126·X | X ≥ ~18,8 mm |
| inç → mm (durum inç iken kumpas mm'ye alındı) | 0,27·X | X ≥ ~14,8 mm |

Bu sınırın altında birim değişimi **algılanmaz** ve yanlış birim, sonraki küçük hareketler sürekli göründüğü için kalıcı hale gelir (bir sonraki açılışa kadar). Ayrıca ölçüm, son yayınlanan değerin tam 0,787 (veya 1,27) katına (±0,4 mm) ani olarak atlayıp en az 3–4 çerçeve sabit kalırsa yanlış pozitif birim değişimi oluşabilir. Ayrıntı ve öneriler: KS-24.

### 4.5 C5 — Batarya İzleyici

**Kod:** [main.cpp:14-30](../src/main.cpp#L14-L30), [main.cpp:75-83](../src/main.cpp#L75-L83), [main.cpp:345-420](../src/main.cpp#L345-L420) · **ADR:** [017](adr/ADR-017_BATARYA_OLCUMU_VE_SARJ_CIKARIMI.md)

**İşlem hattı:**

```
8 × analogReadMilliVolts(GPIO5) → ortalama adcMv
batV      = adcMv/1000 · (R1+R2)/R2                 (= ×2,0)
filtered  = ilk örnekte batV, sonra 0,85·filtered + 0,15·batV
pct       = clamp((filtered − 3,0)/(4,2 − 3,0), 0, 1) · 100, yuvarla
charging  = quickCharging ∨ longTermCharging
rapor     = |Δfiltered| ≥ 10 mV ∨ Δpct ∨ Δcharging ∨ son rapordan ≥ 5000 ms
```

**Şarj tahmini durum değişkenleri:**

| Değişken | Rol |
|---|---|
| `chargeWindowStartV/Ms` | Kısa vadeli taban: her 30 s'de veya gerilim tabanın 20 mV altına inince yenilenir |
| `stableSinceMs/stableBaseV` | Uzun vadeli kararlılık: filtrelenmiş adım ≤ 6 mV ise başlar, büyük adımda sıfırlanır |
| `quickCharging` | `filtered ≥ windowStart + 10 mV` ve `filtered < 4,15 V` |
| `longTermCharging` | ≥ 120 s kararlı ve `filtered ≥ stableBase + 10 mV` ve `< 4,15 V` |

**Zaman sabiti:** örnekleme 2 s, α = 0,15 ⇒ τ ≈ −2 s / ln(0,85) ≈ 12,3 s. Adım tepkisinin %95'i ≈ 37 s.

**Sınırlamalar:** Şarj entegresinin durum pini okunmaz; "şarj" yalnızca gerilim eğilimidir (UI metni de bu yüzden "Voltaj yükseliyor"dur). LiPo deşarj eğrisi doğrusal değildir; yüzde yaklaşık değerdir. 200 kΩ bölücü sürekli ≈ 21 µA çeker. Yüksek empedanslı kaynak ADC örnekleme kapasitörünü tam dolduramayabilir; GPIO5–GND arasına ~100 nF kondansatör önerilir ([GUIDE-011](guide/GUIDE-011_BATARYA_OLCUMU_AYARI.md)).

### 4.6 C6 — Buton Okuyucu

**Kod:** [main.cpp:294](../src/main.cpp#L294), [main.cpp:332-343](../src/main.cpp#L332-L343) · **ADR:** [018](adr/ADR-018_BUTON_GIRISI_VE_DEBOUNCE.md)

```
reading ← (digitalRead(GPIO10) ≠ LOW)          // HIGH → true
reading değiştiyse: lastChange ← now
now − lastChange ≥ 20 ms ve stable ≠ reading ise:
    stable ← reading; webUpdateButton(pressed = stable)
```

Başlangıç değerleri `lastButtonReading = lastButtonStable = true`. Pin `INPUT_PULLUP`'tır; kod "HIGH = basılı" yorumunu yapar. Bu yalnızca **aktif-yüksek çıkış veren** bir kaynakla (ör. dokunmatik TTP223 modülü veya 3,3 V'a bağlı buton + harici pull-down) doğru çalışır. README'de anlatılan "buton → 3,3 V" bağlantısında dahili pull-up yüzünden pin hep HIGH kalır ve basış algılanmaz; "buton → GND" bağlantısında ise mantık terstir. Ayrıntı: KS-05.

### 4.7 C7 — Web Servis Katmanı

**Kod:** [web_ui.cpp](../src/web_ui.cpp)

**Durum önbelleği** (yalnızca `loopTask` erişir):

| Değişken | Kaynak çağrı |
|---|---|
| `lastRawKey`, `lastMm` (+ofset), `lastInch`, `lastIsInch`, `lastIsNegative` | `webUpdateMeasurement` |
| `lastButtonPressed` | `webUpdateButton` |
| `lastBatV`, `lastBatAdcMv`, `lastBatPct`, `lastBatCharging` | `webUpdateBattery` |
| `lastUpdateMs` | Üç `webUpdate*` fonksiyonunun **hepsi** (KS-06) |
| `calOffsetMm` | `loadCalibration`, `saveCalibration` |
| `wsClientCount` | WS olayları |
| `updateInProgress` | OTA işleyicileri |

#### 4.7.1 WiFi ve mDNS

**ADR:** [016](adr/ADR-016_WIFI_STA_MDNS_VE_GIZLI_BILGI_YONETIMI.md)

- `connectWiFi(15000)`: `WIFI_STA`, `setSleep(false)`, `begin(SSID, PASS)`; 250 ms aralıkla durum sorgusu, 15 s'de zaman aşımı. **Bloklayıcıdır**; bu sürede `loop()` çalışmaz (ISR çalışır).
- Başarılıysa `MDNS.begin("kumpas")` ve `addService("http","tcp",80)`.
- Başarısızsa yalnızca loglanır; HTTP/WS sunucuları yine başlatılır (WiFi sonradan bağlanırsa erişilebilir olurlar, ancak mDNS başlatılmaz — KS-09).
- Açık bir yeniden bağlanma döngüsü yoktur; Arduino-ESP32'nin varsayılan otomatik yeniden bağlanma davranışına güvenilir.

#### 4.7.2 HTTP Sunucusu

**ADR:** [011](adr/ADR-011_HTTP_POLLING_VE_WEBSOCKET_PUSH.md), [013](adr/ADR-013_JSON_OLCUM_SOZLESMESI.md)

Arduino `WebServer` (senkron, tek istemci/istek). `webLoop()` → `server.handleClient()`.

| Yöntem | Yol | İşleyici | Yanıt |
|---|---|---|---|
| GET | `/` | `handleRoot` | 200 `text/html` — INDEX_HTML |
| GET | `/api/measure` | `handleApiMeasure` | 200 `application/json` — §6.3 |
| GET | `/api/log` | `handleApiLog` | 200 `application/json` — §6.4 |
| GET | `/log` | `handleLogPage` | 200 `text/html` — LOG_HTML |
| GET | `/calibrate` | `handleCalPage` | 200 `text/html` — CAL_HTML |
| POST | `/calibrate` | lambda | 303 → `/calibrate` · 400 `offset missing` |
| GET | `/update` | `handleUpdatePage` | 200 `text/html` — UPDATE_HTML |
| POST | `/update` | lambda + upload lambda | 200/400 sonuç sayfası, başarıda yeniden başlatma |
| * | diğer | `onNotFound` | 404 `Not Found` |

Sayfalar `server.send(code, type, const char*)` ile gönderilir. ESP32'de `PROGMEM` etkisizdir (sabit diziler zaten flash'ta eşlenir), ancak bu `send` aşırı yüklemesi içeriği geçici bir `String`'e kopyalar; ana sayfa için istek başına ~16 KB geçici heap kullanılır (KS-25).

#### 4.7.3 WebSocket Sunucusu

- `WebSocketsServer wsServer(81)`; `webLoop()` → `wsServer.loop()`.
- `handleWsEvent` yalnızca `CONNECTED`/`DISCONNECTED` olaylarını sayar (`wsClientCount`, 0–255). Gelen metin mesajları yok sayılır.
- Üç `webUpdate*` fonksiyonu `wsClientCount > 0` ise `buildMeasureJson(0)` sonucunu `broadcastTXT` ile yayınlar. Yani WS istemcileri **her** ölçüm, buton değişimi ve batarya raporunda tam durum anlık görüntüsü alır.
- WS üzerinden `ageMs` her zaman `0`'dır.

#### 4.7.4 Kalibrasyon

**ADR:** [014](adr/ADR-014_KALIBRASYON_OFSETI_NVS_PREFERENCES.md)

- `loadCalibration()`: `Preferences.begin("kumpas", readOnly=true)`, `getFloat("offset_mm", 0)`.
- `saveCalibration(v)`: `begin("kumpas", false)`, `putFloat`, `end`; RAM'deki `calOffsetMm` güncellenir.
- `POST /calibrate`: `offset` yoksa 400; varsa `String::toFloat()` (geçersiz metin → 0,0) ile kaydeder, loglar, 303 döner.
- Uygulama noktası: `webUpdateMeasurement` içinde `lastMm = mm + calOffsetMm`. Ofset **yeni bir ölçüm gelene kadar** gösterilen değere yansımaz; inç değerine ve `isNegative`'e uygulanmaz (KS-12).

#### 4.7.5 OTA

**ADR:** [015](adr/ADR-015_TARAYICI_UZERINDEN_OTA_VE_CIFT_SLOT.md)

Upload geri çağrısı (parça parça):

| `upload.status` | Eylem |
|---|---|
| `UPLOAD_FILE_START` | `updateInProgress ← false`; `totalSize > 3 MB` ise iptal (bu noktada `totalSize` henüz 0'dır, kontrol etkisizdir — KS-01); `Update.begin(UPDATE_SIZE_UNKNOWN)` başarısızsa iptal; aksi halde `updateInProgress ← true` |
| `UPLOAD_FILE_WRITE` | `Update.write`; eksik yazımda iptal ve `updateInProgress ← false` |
| `UPLOAD_FILE_END` | (sonlandırma POST işleyicisinde) |
| `UPLOAD_FILE_ABORTED` | iptal ve log |

POST işleyicisi (yükleme bittikten sonra): `updateInProgress` değilse "Yukleme baslamadi." (400) — yazma hatasında da bu mesaj görünür (KS-02); `Update.end(true)` başarısızsa hata metni (400); başarılıysa başarı sayfası, 400 ms bekleme, `ESP.restart()`.

`Update.begin(UPDATE_SIZE_UNKNOWN)` hedef boyut olarak bir sonraki OTA bölümünün tamamını alır, dolayısıyla etkin sınır 1 310 720 bayttır. `Update.end(true)` imaj başlığını doğrular ve önyükleme bölümünü değiştirir; bu adıma kadar eski firmware aktif kalır.

#### 4.7.6 Cihaz Logu

**ADR:** [019](adr/ADR-019_RAM_TABANLI_HALKA_LOG_TAMPONU.md)

```
logLines[100][96]; logSeq (toplam yazılan satır); logCount (≤ 100)
appendLogLine: index = logSeq % 100; strncpy(…, 95); logSeq++; logCount = min(logCount+1, 100)
Satır kimliği = yazıldığı andaki logSeq + 1  (1'den başlar)
/api/log?since=N : firstSeq = logSeq − logCount; i = 0..logCount−1 için id = firstSeq + i + 1; id > N olanlar
```

`webLogf` `vsnprintf` ile 96 baytlık yığın tamponuna yazar; ISR içinden **çağrılmamalıdır**. Loglanan olaylar: `Boot: main setup`, `Boot: web ui start`, `WiFi: connected <ip>` / `WiFi: connect timeout`, `Cal: offset=…` / `Cal: offset missing`, `OTA: …` (7 farklı mesaj).

### 4.8 C8 — Tarayıcı İstemcisi

**Kod:** [web_ui.cpp:89-652](../src/web_ui.cpp#L89-L652) · **ADR:** [012](adr/ADR-012_GOMULU_HTML_PROGMEM.md), [020](adr/ADR-020_ISTEMCI_TARAFI_BUTON_LOGU_VE_CSV.md)

**Ana sayfa (INDEX_HTML) mantığı:**

| İşlev | Ayrıntı |
|---|---|
| `tick()` | 200 ms'de bir `fetch('/api/measure', {cache:'no-store'})`; tüm göstergeleri günceller; hata → `offline` |
| Buton kenarı | `lastBtnPressed` ilk yanıtta başlatılır; `false → true` geçişinde `appendLogRow(j)` |
| `appendLogRow` | Tarayıcı saati `HH:MM:SS.mmm`, mm (2 ondalık), inç (4 ondalık), not; satır en üste eklenir; 200 satır sınırı |
| Not | Üst "Not" alanı bir sonraki satıra kopyalanır ve temizlenir; satır notu ✎ ile açılan editörde düzenlenir, `focusout`'ta kaydedilir |
| CSV | Eskiden yeniye sırayla, tüm hücreler `"…"` içinde, `""` kaçışlı, UTF-8 BOM, `buton-logu-YYYYMMDD-HHMMSS.csv` |
| Temizle | Yalnızca tarayıcıdaki tabloyu temizler |
| XSS | Not metni `textContent` ile basılır; tablo hücrelerindeki sayılar `toFixed` çıktısıdır |

**Log sayfası (LOG_HTML):** 250 ms'de bir `/api/log?since=lastLogSeq`; satırları `textContent` ile ekler; `seq`'i saklar. "Temizle" `lastLogSeq = 0` yaptığı için sonraki sorguda tampondaki tüm satırlar geri gelir (KS-08).

**Önemli sonuç:** Buton logu cihazda değil tarayıcı sekmesinde yaşar; sekme kapanır/yenilenirse kaybolur. Birden çok sekme açıksa her biri kendi logunu tutar. 200 ms sorgu aralığından kısa basışlar kaçırılabilir (KS-07).

### 4.9 C9 — Python İstemcisi

**Kod:** [scripts/measure_ws.py](../scripts/measure_ws.py) · **ADR:** [021](adr/ADR-021_PYTHON_ILE_KLAVYE_EMULASYONU.md)

```
main: sonsuz döngü { WebSocketApp(WS_URL).run_forever(ping_interval=20, ping_timeout=5); sleep(2) }
on_message:
  JSON çöz (hata → yazdır, dön)
  konsola mm/inch/unit/sign/age/raw yazdır
  buttonPressed veya mm yoksa dön
  ilk mesaj → yalnızca durumu kaydet
  false→true ve son tetiklemeden ≥ 0,2 s → typewrite(f"{mm:.{DECIMALS}f}" [, virgül]); 0,1 s; press("enter")
```

| Ayar | Varsayılan |
|---|---|
| `WS_URL` | `ws://kumpas.local:81/` |
| `DECIMALS` | 2 |
| `DECIMAL_COMMA` | `True` (Türkçe Excel için) |
| `MIN_PRESS_INTERVAL_SEC` | 0,2 |
| `ENTER_DELAY_SEC` | 0,1 |

WS her durum değişiminde push ettiği için Python istemcisi, tarayıcının aksine kısa basışları kaçırmaz.

---

## 5. Dinamik Davranış

### 5.1 Açılış Sırası

```
Güç/Reset
  │
  ├─ Serial.begin(115200); delay(300); açılış mesajları
  ├─ webLogf("Boot: main setup")
  ├─ GPIO10 INPUT_PULLUP; GPIO5 INPUT; ADC 12 bit / 11 dB
  ├─ caliperBegin(8, 9)  ──► ISR etkin, çerçeveler toplanmaya başlar
  └─ webBegin()
       ├─ loadCalibration()          (NVS)
       ├─ connectWiFi(15000)  ……… en fazla 15 s blok
       │     ├─ OK  → seri: IP; log; startMdns()
       │     └─ ZA  → seri + log: timeout
       ├─ HTTP rotaları; server.begin()
       └─ wsServer.begin(); onEvent; lastUpdateMs = millis()
  loop() başlar
```

### 5.2 Ölçüm Akışı

```
Kumpas      ISR                 loop()                         web_ui                 Tarayıcı / WS
  │ 24 kenar │                    │                               │                        │
  │─────────►│ bitleri topla       │                               │                        │
  │          │ g_lastFrame,hasFrame│                               │                        │
  │          │────────────────────►│ caliperReadFrame              │                        │
  │          │                    │ key, pendingCount=1            │                        │
  │ (≈ bir çerçeve periyodu)       │                               │                        │
  │─────────►│────────────────────►│ aynı key → pendingCount=2     │                        │
  │          │                    │ decode, C4, mm/inch            │                        │
  │          │                    │ sıçrama yok → yayınla          │                        │
  │          │                    │──────────────────────────────►│ lastMm = mm+ofset       │
  │          │                    │                               │── broadcastTXT ───────►│ WS istemcileri
  │          │                    │                               │◄── GET /api/measure ───│ (≤ 200 ms sonra)
  │          │                    │                               │── JSON ───────────────►│
```

### 5.3 Buton → Kayıt

```
Buton ─► GPIO10 ─► 20 ms debounce ─► webUpdateButton(true)
                                        ├─► WS push ─► measure_ws.py: false→true ─► typewrite + Enter
                                        └─► durum önbelleği ─► (≤ 200 ms) tarayıcı tick(): false→true ─► appendLogRow
```

### 5.4 Kalibrasyon

```
Tarayıcı ── POST /calibrate offset=-0.015 ──► toFloat ─► NVS putFloat ─► calOffsetMm
         ◄── 303 Location: /calibrate ──────
         (gösterilen mm değeri bir sonraki stabil ölçümde ofsetli olur; `cal=` göstergesi hemen değişir)
```

### 5.5 OTA

```
Tarayıcı ── POST /update (multipart, firmware) ──►
             START: Update.begin(UNKNOWN) → app1 (veya app0) silinmeye/yazılmaya başlar
             WRITE × n: Update.write(parça)          … bu sürede loop() bloklu, ölçüm durur
             END
          ◄── POST işleyici: Update.end(true) ── başarı sayfası
             delay(400) → ESP.restart() → bootloader otadata'ya göre yeni slottan açar
```

### 5.6 Birim Çıkarım Durum Makinesi

```
               ilk çerçeve: bit23'e göre
                      │
          ┌───────────▼───────────┐   strong hint (aday=inç) ×3   ┌───────────────────┐
          │       DURUM = mm      │ ─────────────────────────────►│     DURUM = inç   │
          │  pending = 0          │◄───────────────────────────── │  pending = 0      │
          └───────────┬───────────┘   strong hint (aday=mm) ×3    └─────────┬─────────┘
                      │ strong hint ×1..2                                    │
                      ▼                                                      ▼
               pending++ ; zayıf ipucu → pending = 0                (simetrik)
```

---

## 6. Veri Tasarımı

### 6.1 Global Durum Envanteri

| Modül | Değişken grubu | Ömür | Erişim |
|---|---|---|---|
| main.cpp | ISR değişkenleri (§4.1) | Statik | ISR + loop (`g_mux`) |
| main.cpp | Birim çıkarımı: `g_unitStateIsInch`, `g_hasUnitState`, `g_unitPendingIsInch`, `g_unitPendingCount` | Statik | loop |
| main.cpp | Yerleşim: `g_signBit`, `g_unitBit`, polarite bayrakları, `g_autoUnitFix` | Statik | loop |
| main.cpp | `loop()` içi `static` değişkenler (filtre, buton, batarya; 22 adet) | Statik | loop |
| web_ui.cpp | Durum önbelleği (§4.7) | Statik | loop |
| web_ui.cpp | `logLines`, `logSeq`, `logCount` | Statik | loop |
| NVS | `kumpas/offset_mm` | Kalıcı | loop |

### 6.2 Kumpas Çerçevesi Bit Düzeni

```
 Bit:  23   22   21   20   19 ………………………………………………… 0
      ┌────┬────┬────┬────┬──────────────────────────────┐
      │UNIT│ ?  │SIGN│ ?  │     MAGNITUDE (20 bit)       │   LSB önce iletilir (bit 0 ilk)
      └────┴────┴────┴────┴──────────────────────────────┘
       1=inç     1=neg       count; mm: /200, inç: /4000
```

| Bit | Sabit | Varsayılan | Not |
|---|---|---|---|
| 0–19 | `CALIPER_VALUE_BITS` | 20 | Büyüklük |
| 20 | — | yok sayılır | Bazı varyantlarda işaret |
| 21 | `CALIPER_SIGN_BIT` | 21 | Bu kumpasta gözlenen işaret biti |
| 22 | — | yok sayılır | |
| 23 | `CALIPER_UNIT_BIT` | 23 | Yalnızca ilk birim durumu için; gerçek birim biti olmayabilir |

### 6.3 Ölçüm JSON Sözleşmesi

`GET /api/measure` yanıtı ve WS mesajı ([ADR-013](adr/ADR-013_JSON_OLCUM_SOZLESMESI.md)):

| Alan | Tip | Birim/biçim | Kaynak | Açıklama |
|---|---|---|---|---|
| `rawKey` | uint | 0…0x3FFFFF | `lastRawKey & 0x3FFFFF` | Stabil anahtarın alt 22 biti (bit 22–23 düşürülür) |
| `mm` | number (3 ondalık) | mm | `mm + calOffsetMm` | Ofsetli değer |
| `inch` | number (5 ondalık) | in | `inch` | **Ofsetsiz** değer |
| `isInch` | bool | — | C4 | Çıkarılmış birim |
| `isNegative` | bool | — | `counts < 0` | Ofset öncesi işaret |
| `buttonPressed` | bool | — | C6 | Debounce sonrası durum |
| `offsetMm` | number (3 ondalık) | mm | NVS | Aktif kalibrasyon ofseti |
| `batV` | number (3 ondalık) | V | C5 | Filtrelenmiş batarya gerilimi |
| `batAdcMv` | uint | mV | C5 | Son raporlanan ham ADC ortalaması |
| `batPct` | uint | % | C5 | 0–100 |
| `batCharging` | bool | — | C5 | Gerilim artış eğilimi |
| `ageMs` | uint | ms | `millis() − lastUpdateMs` | HTTP'de son herhangi bir güncellemeden beri; WS'de daima 0 |

Örnek:

```json
{"rawKey":2470,"mm":12.350,"inch":0.48622,"isInch":false,"isNegative":false,"buttonPressed":false,"offsetMm":0.000,"batV":3.980,"batAdcMv":1990,"batPct":82,"batCharging":false,"ageMs":45}
```

**Uyumluluk kuralı:** Alanlar eklenebilir; mevcut alanların adı, tipi ve anlamı değiştirilemez. UI JS'i ve `measure_ws.py` bu alanlara doğrudan erişir.

### 6.4 Log JSON Sözleşmesi

```json
{"seq":7,"lines":[{"id":6,"text":"Cal: offset=-0.015"},{"id":7,"text":"OTA: upload aborted"}]}
```

| Alan | Açıklama |
|---|---|
| `seq` | Şimdiye kadar yazılmış toplam satır sayısı (= son satırın `id`'si) |
| `lines[].id` | 1'den başlayan satır kimliği |
| `lines[].text` | Kaçışlanmış satır metni (≤ 95 karakter) |

İstemci bir sonraki istekte `since=seq` gönderir. Tamponda olmayan (üzerine yazılmış) satırlar sessizce atlanır.

### 6.5 Kalıcı Depolama (NVS)

| Ad alanı | Anahtar | Tip | Varsayılan | Yazan |
|---|---|---|---|---|
| `kumpas` | `offset_mm` | float | 0,0 | `POST /calibrate` |

NVS bölümü 20 KB'tır; tek anahtar için fazlasıyla yeterlidir. `pio run -t erase` ofseti siler.

### 6.6 Flash Bölüm Tablosu

| Ad | Tip | Alt tip | Başlangıç | Boyut | Bayt | Kullanım |
|---|---|---|---|---|---|---|
| (bootloader) | — | — | 0x0000 | — | — | ESP-IDF önyükleyici |
| (partition table) | — | — | 0x8000 | 0x1000 | 4 096 | Bölüm tablosu |
| nvs | data | nvs | 0x9000 | 0x5000 | 20 480 | Kalibrasyon, WiFi yığını |
| otadata | data | ota | 0xE000 | 0x2000 | 8 192 | Aktif slot seçimi |
| app0 | app | ota_0 | 0x10000 | 0x140000 | 1 310 720 | Firmware slot A |
| app1 | app | ota_1 | 0x150000 | 0x140000 | 1 310 720 | Firmware slot B |
| spiffs | data | spiffs | 0x290000 | 0x170000 | 1 507 328 | **Kullanılmıyor** (KS-22) |

Toplam bitiş 0x400000 = 4 MB.

### 6.7 Bellek Bütçesi (statik ve önemli dinamik kalemler)

| Kalem | Yer | Boyut |
|---|---|---|
| `logLines` | DRAM (.bss) | 9 600 B |
| Gömülü HTML'ler | Flash (.rodata) | 23 819 B |
| Ana sayfa gönderimi | Heap (geçici) | ~16 KB / istek |
| JSON ölçüm `String` | Heap (geçici) | ~200 B (160 rezerve) |
| JSON log `String` | Heap (geçici) | ≤ ~10 KB (100 satır × ~100 B) |
| WebSockets kütüphanesi | Heap | İstemci başına birkaç KB |

---

## 7. Zamanlama Analizi

### 7.1 ISR ve Çerçeve Zamanlaması

Doğru çalışma için şu sıralama **değişmezdir** (bkz. [ADR-004](adr/ADR-004_ZAMAN_BOSLUGU_ILE_CERCEVE_AYIRMA.md)):

```
MIN_EDGE_US  <  kumpas saat periyodu (düşen kenardan düşen kenara)  <  FRAME_GAP_US  <  çerçeveler arası sessizlik
    5 µs                          (modele bağlı, ölçülmeli)               1500 µs          (modele bağlı, ms mertebesi)
```

- ISR yalnızca düşen kenarda tetiklendiği için ölçülen "gap" tam saat periyodudur.
- Sabitlerin tarihçesi: ilk sürümde `FRAME_GAP_US = 3000`, `MIN_EDGE_US = 40`; `e043d56` ile sırasıyla 1500 ve 5'e indirildi. `MIN_EDGE_US`'in 40'tan 5'e düşürülmesi, bu kumpasta saat periyodunun 40 µs'ye yaklaştığını/altına indiğini düşündürür. Kod yorumundaki "hundreds of microseconds per edge" ve README'deki "~1 ms" ifadeleri bu nedenle güvenilir kabul edilmemeli; gerçek değer logic analyzer ile ölçülmelidir (KS-17).
- ISR süresi: `esp_timer_get_time` + birkaç aritmetik + `gpio_get_level` + nadiren kritik bölge; tipik olarak birkaç µs. Kesme giriş gecikmesi de eklenince en kötü durumda ~10 µs mertebesi öngörülür. Saat periyodu bunun birkaç katı olduğu sürece bit kaçmaz.
- **48-bit kumpaslar:** Bazı modeller bir patlamada 2 × 24 bit (mutlak + bağıl) gönderir. Bu durumda ISR patlama başına iki çerçeve üretir ve tek slotlu posta kutusu nedeniyle loop genellikle **ikincisini** görür. Gözlenen değer tutarsızsa bkz. [GUIDE-010](guide/GUIDE-010_YENI_KUMPAS_MODELINE_UYARLAMA.md).

### 7.2 Uçtan Uca Gecikme Bütçesi

Kumpas çerçeve periyodu `Tf` olsun (modele ve kumpasın hızlı/yavaş moduna bağlı).

| Aşama | Normal | Sıçramada |
|---|---|---|
| Değerin stabil hale gelmesi → ilk çerçeve | 0…Tf | 0…Tf |
| Filtre (2 veya 4 ardışık çerçeve) | +1·Tf | +3·Tf |
| loop gecikmesi | ≤ ~2 ms + HTTP işleme | aynı |
| WS yayını | ~ms | ~ms |
| **WS istemcisinde toplam** | **≤ ~2·Tf** | **≤ ~4·Tf** |
| Tarayıcıda ek | +0…200 ms (sorgu) + RTT | aynı |

### 7.3 Periyodik İşler

| İş | Periyot | Kaynak |
|---|---|---|
| loop turu | ~2 ms + iş | `delay(2)` |
| Buton örnekleme | her tur | debounce 20 ms |
| Batarya örnekleme | 2000 ms (şarj/boşta aynı) | `BATTERY_SAMPLE_MS_*` |
| Batarya zorunlu rapor | 5000 ms | `BATTERY_REPORT_MAX_INTERVAL_MS` |
| Tarayıcı ölçüm sorgusu | 200 ms | INDEX_HTML |
| Tarayıcı log sorgusu | 250 ms | LOG_HTML |
| WS ping (Python) | 20 s (5 s zaman aşımı) | measure_ws.py |
| Python yeniden bağlanma | 2 s | measure_ws.py |

**Not:** Filtre içindeki `return` ifadeleri (`REJECT_IF_TOP_BITS_SET` ve "yetersiz tekrar" dalları) `delay(2)`'yi atlar; bu yalnızca o tur için bekleme olmaması anlamına gelir, işlevsel etkisi yoktur.

### 7.4 Bloklayıcı İşlemler

| İşlem | Süre | Etki |
|---|---|---|
| `connectWiFi` | ≤ 15 s (yalnızca açılış) | loop başlamaz; ISR çalışır |
| OTA yükleme | Dosya boyutu / ağ hızı (onlarca saniye) | loop durur; ölçüm/buton/batarya güncellenmez |
| NVS yazımı | ms mertebesi | Flash önbelleği kısa süre kapalı; ISR IRAM'de olduğu için hedeflenen davranış devam etmesidir |
| Büyük HTTP yanıtı (ana sayfa) | ~ms–onlarca ms | loop kısa süre gecikir; ISR etkilenmez |

---

## 8. Modül Arayüz Sözleşmesi

[include/web_ui.h](../include/web_ui.h) — tüm fonksiyonlar **yalnızca `loopTask` bağlamından** (setup/loop) çağrılmalıdır. ISR'den çağrılamaz.

| Fonksiyon | Ön koşul | Son koşul | Yan etki |
|---|---|---|---|
| `void webBegin()` | `Serial` başlatılmış; bir kez çağrılır | Sunucular dinlemede; ofset yüklü | ≤ 15 s blok; ağ |
| `void webLoop()` | `webBegin` çağrılmış | Bekleyen HTTP/WS işleri işlendi | OTA sırasında uzun süre bloklayabilir |
| `void webUpdateMeasurement(uint32_t rawKey, float mm, float inch, bool isInch, bool isNegative)` | `mm` sonlu ve ofsetsiz | Önbellek güncel; `lastMm = mm + ofset` | İstemci varsa WS yayını |
| `void webUpdateButton(bool isPressed)` | Debounce edilmiş durum | Önbellek güncel | İstemci varsa WS yayını |
| `void webUpdateBattery(float volts, uint16_t adcMv, uint8_t percent, bool isCharging)` | `percent` 0–100 | Önbellek güncel | İstemci varsa WS yayını |
| `void webLogf(const char *fmt, ...)` | printf biçimi; `webBegin`'den önce de çağrılabilir | Satır halka tampona eklendi (≤ 95 karakter) | En eski satır üzerine yazılabilir |

**Yeni bir alan eklemek için:** `web_ui.h`'ye yeni bir `webUpdate…` fonksiyonu ekleyin, `web_ui.cpp`'de önbelleğe ve `buildMeasureJson`'a alanı ekleyin, `main.cpp`'den çağırın. Ayrıntı: [GUIDE-014](guide/GUIDE-014_GELISTIRME_VE_KATKI_KURALLARI.md).

---

## 9. Hata Yönetimi

| Hata kaynağı | Algılama | Tepki | Kullanıcıya görünürlük |
|---|---|---|---|
| Gürültülü kenar | `gap < MIN_EDGE_US` | Kenar atlanır | Yok |
| Kayık/bozuk çerçeve | Tekrar filtresi | Yayınlanmaz | Değer güncellenmez |
| Mantıksız değer | `!isfinite` veya `> 500 mm` | Yayınlanmaz | Değer güncellenmez |
| Birim biti gürültüsü | C4 | 3 ipucu olmadan birim değişmez | `unit=` göstergesi |
| WiFi bağlanamadı | 15 s zaman aşımı | Ağsız devam | Seri + log |
| HTTP istemcisi koptu | Tarayıcı `fetch` hatası | `offline` göstergesi | UI |
| WS koptu | Python `on_close` | 2 s sonra yeniden bağlan | Python konsolu |
| Bozuk JSON (Python) | `JSONDecodeError` | Mesaj atlanır | Python konsolu |
| Eksik `offset` | `hasArg` | 400 | Düz metin |
| OTA başlatılamadı/yazılamadı | `Update.*` dönüş değeri | İptal, 400 | Sonuç sayfası + log |
| OTA bitirilemedi | `Update.end` | 400 + hata metni | Sonuç sayfası + log |
| Bilinmeyen yol | `onNotFound` | 404 | Düz metin |
| Pil bölücü yok | — | Anlamsız değer gösterilir | UI (%0 civarı) |

Sistem **hiçbir hatada kendini yeniden başlatmaz** (yalnızca başarılı OTA sonrası). Watchdog yapılandırması Arduino varsayılanıdır.

---

## 10. Güvenlik Tasarımı

**Tehdit modeli:** Cihaz, güvenilir ve kapalı bir atölye/ev LAN'ında çalışır ([ADR-022](adr/ADR-022_KIMLIK_DOGRULAMASIZ_YEREL_AG_GUVENLIK_MODELI.md)).

| Varlık | Tehdit | Mevcut önlem | Kalan risk |
|---|---|---|---|
| Firmware | Yetkisiz OTA ile kötü amaçlı firmware | Yok (kimlik doğrulama yok) | **Yüksek** — LAN'daki herkes cihazı yeniden programlayabilir (KS-03) |
| Kalibrasyon | Yetkisiz ofset değişikliği → hatalı ölçüm | Yok | Orta — `cal=` göstergesiyle fark edilebilir |
| WiFi şifresi | Depodan sızma | `.gitignore` | Düşük |
| WiFi şifresi | `firmware.bin` paylaşımı / flash dökümü | Yok | **Orta** — şifre ikili dosyada düz metin (KS-04) |
| Ölçüm verisi | Ağda dinleme | Yok (HTTP/WS şifresiz) | Düşük (veri hassas değil) |
| UI | XSS (not alanı, log) | `textContent`; JSON kaçışlama | Düşük |
| Cihaz kullanılabilirliği | Çok sayıda WS istemcisi / istek seli | Yok | Düşük–Orta |

Önerilen sertleştirmeler (öncelik sırasıyla): `/update` ve `POST /calibrate` için HTTP Basic Auth; OTA'da imaj boyutu ve MD5 kontrolü; WiFi kimlik bilgilerini NVS'ye taşıyıp ilk kurulumda AP portalı ile almak; ayrı misafir/IoT VLAN'ı.

---

## 11. Yapılandırma ve Derleme

### 11.1 Derleme Zamanı Yapılandırması

| Yer | Ayar | Değer |
|---|---|---|
| platformio.ini | `platform` / `board` / `framework` | `espressif32` / `esp32-s3-devkitc-1` / `arduino` |
| platformio.ini | `board_build.flash_size`, `board_upload.flash_size` | 4MB |
| platformio.ini | `board_build.partitions` | `partitions_ota_4mb.csv` |
| platformio.ini | `build_flags` | `-D ARDUINO_USB_CDC_ON_BOOT=1 -D ARDUINO_USB_MODE=1` |
| platformio.ini | `lib_deps` | `links2004/WebSockets` (sürüm sabitlenmemiş — KS-26) |
| platformio.ini | `monitor_speed` | 115200 |
| main.cpp | Tüm donanım/protokol/filtre/batarya sabitleri | Bkz. [GUIDE-013](guide/GUIDE-013_PARAMETRE_REFERANSI.md) |
| web_ui.cpp | `MDNS_HOSTNAME`, portlar, yollar, `UPDATE_MAX_SIZE`, NVS adları, `LOG_CAP`, `LOG_LINE_MAX` | Bkz. GUIDE-013 |
| wifi_secrets.h | `WIFI_SSID`, `WIFI_PASSWORD` | Kullanıcıya özel |

### 11.2 Çalışma Zamanı Yapılandırması

Yalnızca kalibrasyon ofseti (NVS). Diğer her şey yeniden derleme gerektirir.

### 11.3 Derleme Çıktıları

`.pio/build/esp32s3/firmware.bin` (OTA ve USB yükleme), `bootloader.bin`, `partitions.bin`, `firmware.elf` (hata ayıklama). Ayrıntı: [GUIDE-003](guide/GUIDE-003_DERLEME_YUKLEME_VE_SERI_MONITOR.md).

---

## 12. Tasarım Değişmezleri

Bu kurallar bozulursa sistem sessizce yanlış çalışabilir. Değişiklik yaparken kontrol edin:

1. **ISR'de** yalnızca IRAM-güvenli fonksiyonlar; `Serial`, `webLogf`, `String`, `malloc`, `digitalRead`, `micros` yok.
2. ISR ile paylaşılan her değişken `volatile`; çok baytlı paylaşım `g_mux` kritik bölgesinde.
3. `MIN_EDGE_US < saat periyodu < FRAME_GAP_US < çerçeve arası boşluk`.
4. `CALIPER_UNIT_BIT ≠ CALIPER_SIGN_BIT` (aksi halde birim biti yok sayılır — kod bunu koruyor ama niyet bozulur).
5. `STABLE_REPEATS_NORMAL ≥ 2` — 1 yapılırsa tek bozuk çerçeve yayınlanabilir.
6. `web_ui.h` fonksiyonları yalnızca loopTask bağlamından çağrılır.
7. JSON alanları geriye dönük uyumlu kalır (§6.3).
8. HTML `id`'leri JS ile eşleşir (SRS AG-KUA-03).
9. Batarya ADC pini ADC1 kanalında kalır (WiFi ile ADC2 çakışır).
10. Firmware boyutu < 1 310 720 bayt.

---

## 13. Genişletme Noktaları

| İhtiyaç | Nereye | Rehber |
|---|---|---|
| Farklı kumpas bit düzeni | main.cpp sabitleri | GUIDE-010 |
| Yeni JSON alanı / UI göstergesi | web_ui.h + web_ui.cpp | GUIDE-014 |
| Yeni HTTP uç noktası | `webBegin()` içinde `server.on` | GUIDE-014 |
| Başka bir istemci (Node, C#, LabVIEW) | WS :81 veya `/api/measure` | GUIDE-006 |
| Kimlik doğrulama | `server.authenticate()` / `requestAuthentication()` | ADR-022 |
| Kalıcı buton logu | Cihaz tarafı halka tampon + yeni uç nokta veya `spiffs` bölümü | ADR-020 |
| BLE klavye | `lib/ESP32_BLE_Keyboard` | ADR-021 |
| Ek sensör | main.cpp'de okuma + yeni `webUpdate…` | GUIDE-014 |

---

## 14. Bilinen Sorunlar ve Teknik Borç

Tam liste, etki ve önerilen düzeltmelerle: **[BILINEN_SORUNLAR.md](BILINEN_SORUNLAR.md)**. Tasarımı en çok etkileyenler:

- **KS-01/02** OTA boyut kontrolü etkisiz ve hata mesajı yanıltıcı
- **KS-03/04** Kimlik doğrulama yok; firmware'de düz metin WiFi şifresi
- **KS-05** Buton bağlantısı ile kod mantığı uyumsuz
- **KS-06** `ageMs` ölçüm yaşı değil
- **KS-07** Tarayıcı buton logu kısa basışları kaçırabilir, kalıcı değil
- **KS-24** Birim çıkarımı küçük değerlerde çalışmaz, nadir yanlış pozitif mümkün

---

## 15. Değişiklik Geçmişi

| Sürüm | Tarih | Açıklama |
|---|---|---|
| 1.0 | 2026-10-10 | İlk sürüm (`2e2619b`). |
