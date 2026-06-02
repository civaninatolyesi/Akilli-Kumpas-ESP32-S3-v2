# ESP32-S3 Super Mini — Dijital Kumpas Veri Okuma

> **Bu belge sıfırdan başlayan biri için yazılmıştır.**  
> Hiç ESP32 görmemiş, Arduino yazmamış, elektronikten anlamayan biri de bu projeyi tamamlayabilmeli.  
> Her adımın *ne* yapıldığı ve *neden* yapıldığı açıklanır.

---

## İçindekiler

1. [Projenin Amacı ve Özet](#1-projenin-amacı-ve-özet)
2. [Nasıl Çalışır — Büyük Resim](#2-nasıl-çalışır--büyük-resim)
3. [Gerekli Malzemeler](#3-gerekli-malzemeler)
4. [Yazılım Kurulumu](#4-yazılım-kurulumu)
5. [Donanım Bağlantısı](#5-donanım-bağlantısı)
6. [WiFi Ayarı ve İlk Yükleme](#6-wifi-ayarı-ve-ilk-yükleme)
7. [Web Arayüzünü Kullanmak](#7-web-arayüzünü-kullanmak)
8. [Python Otomasyon Scripti](#8-python-otomasyon-scripti)
9. [Kalibrasyon](#9-kalibrasyon)
10. [OTA ile Kablosuz Güncelleme](#10-ota-ile-kablosuz-güncelleme)
11. [Sorun Giderme](#11-sorun-giderme)
12. [Proje Dosyaları Rehberi](#12-proje-dosyaları-rehberi)
13. [Protokol Detayı — Kumpas 24-bit Veri](#13-protokol-detayı--kumpas-24-bit-veri)
14. [Yazılım Mimarisi — Kod Nasıl Çalışır](#14-yazılım-mimarisi--kod-nasıl-çalışır)
15. [Ayarlanabilir Parametreler](#15-ayarlanabilir-parametreler)
16. [Batarya Ölçümü ve Şarj Tahmini](#16-batarya-ölçümü-ve-şarj-tahmini)
17. [AI Asistanı ile Çalışmak](#17-ai-asistanı-ile-çalışmak)
18. [Tasarım Kararları ve Nedenleri](#18-tasarım-kararları-ve-nedenleri)

---

## 1. Projenin Amacı ve Özet

Bu proje, **ucuz 24-bit dijital bir kumpası** ESP32-S3 Super Mini mikrodenetleyiciye bağlar ve ölçümleri:

- Tarayıcıdan izlenebilir bir **Web arayüzünde** gösterir
- WebSocket üzerinden **Python'a** iletir (Excel, CAD, vs.'ye otomatik yazar)
- LiPo **batarya seviyesini** izler
- **OTA** (havadan, USB kablosu olmadan) yazılım güncellemesi yapar

Kumpasın DATA ve CLOCK pinlerini GPIO'ya bağlamak yeterlidir. Kumpas sürekli 24-bit veri yayınlar, ESP32 bu veriyi doğrusal bir protokolle okur.

---

## 2. Nasıl Çalışır — Büyük Resim

```
[ Dijital Kumpas ]
   DATA ──────► GPIO8
   CLOCK ─────► GPIO9 ──► Kesme (ISR) tetikler
   GND ────────► GND

   Her CLOCK düşen kenarında ISR çalışır, DATA'dan 1 bit okur.
   24 bit dolunca tam bir "frame" oluşur.

[ ESP32-S3 firmware ]
   Frame → decode → mm / inch hesap → filtrele → Web'e gönder

[ Tarayıcı / Python ]
   HTTP veya WebSocket ile anlık ölçüm alır
```

**Neden ISR (interrupt) kullanılır?**  
Kumpas clock darbeleri ~1ms aralıklarla gelir. `loop()` içinde polling yapılırsa darbeler kaçar. ISR her kenar değişiminde doğrudan tetiklendiğinden hiç bir bit atlanmaz.

---

## 3. Gerekli Malzemeler

### Zorunlu

| Parça | Açıklama | Nereden Bulunur |
|---|---|---|
| ESP32-S3 Super Mini | 4MB flash, USB-C | AliExpress, Robotistan, Direnc.net |
| 24-bit dijital kumpas | DATA/CLOCK çıkışı olan herhangi biri | AliExpress, donanım mağazaları |
| Seviye dönüştürücü | 1.5V → 3.3V logic level shifter | AliExpress, Robotistan |
| USB-C data kablosu | Sadece şarj değil, data kablo | Yanınızdaki kablo (genelde çalışır) |
| 2.4 GHz WiFi | Telefon hotspot da olur | — |

### Opsiyonel

| Parça | Ne İçin |
|---|---|
| LiPo batarya | Batarya seviyesi takibi için |
| 2× 100kΩ direnç | Voltaj bölücü (batarya ADC) |
| Anlık buton | Ölçümü loglamak için |
| Logic analyzer | Kumpas sinyalini görmek için (sorun gidermede çok işe yarar) |

### Neden Seviye Dönüştürücü Zorunlu?

Kumpas DATA ve CLOCK sinyalleri **1.5V** seviyesinde çalışır.  
ESP32 GPIO'ları **3.3V lojik** bekler.  
1.5V direkte bağlandığında ESP32 "1" ve "0"ı düzgün ayırt edemez, okuma tutarsızlaşır.

**En basit çözüm:** Her iki hat için ayrı 10kΩ pull-up direnci (DATA ve CLOCK'u 3.3V'a bağlar). Ya da hazır bir 4-kanal logic level shifter modülü kullanın.

---

## 4. Yazılım Kurulumu

### Adım 1: VS Code Kur

1. [https://code.visualstudio.com](https://code.visualstudio.com) adresinden indir ve kur.

### Adım 2: PlatformIO Eklentisini Kur

1. VS Code'u aç.
2. Sol çubukta Extensions simgesine tıkla (ya da `Ctrl+Shift+X`).
3. Arama kutusuna `PlatformIO` yaz.
4. "PlatformIO IDE" eklentisini bul, **Install** düğmesine bas.
5. Kurulum bittikten sonra VS Code'u yeniden başlat.

> **Not:** PlatformIO kurulumu birkaç dakika sürebilir. İndirme tamamlanana kadar bekle.

### Adım 3: Projeyi Aç

1. VS Code'da `File → Open Folder` seç.
2. Bu proje klasörünü seç.
3. Sol altta PlatformIO simgesi (karınca ikonu) görünmeli.

### Adım 4: USB Sürücüsü (Gerekirse)

Windows 10/11 çoğu durumda otomatik tanır. Tanımazsa:

- ESP32-S3 Super Mini genelde **CH340** veya **CP2102** çip kullanır.
- Seri portu görünmüyorsa, cihaz üreticisinin sitesinden sürücüyü indir.
- Aygıt Yöneticisi'nde (Device Manager) "COM port" altında görünmeli.

---

## 5. Donanım Bağlantısı

### Temel Bağlantı (Kumpas → ESP32)

```
Kumpas              Seviye Dönüştürücü    ESP32-S3 Super Mini
------              ------------------    -------------------
GND    ──────────────────────────────────► GND
DATA   ──► LV1 (düşük taraf)  HV1 ───────► GPIO8
CLOCK  ──► LV2 (düşük taraf)  HV2 ───────► GPIO9
           LV  ◄── 1.5V (kumpas Vcc)
           HV  ◄── 3.3V (ESP32 3.3V)
```

> **Önemli:** Kumpas GND ile ESP32 GND ortak toprak olmalı. Aksi halde sinyaller düzgün çalışmaz.

### Kumpas Pin Konumu

Kumpasın data konektörü genelde pil yuvası yanındadır. 4 pin içerir:

```
┌─────────────────┐
│  GND  DATA  CLK  Vcc(1.5V) │
└─────────────────┘
```

> Farklı modellerde pin sırası değişebilir. Multimetre ile GND'yi bul (pil negatifi ile devamlılık gösterir), geri kalan pinleri logic analyzer veya osiloskopla incele.

![Kumpas Pin Bağlantısı](kumpas-pinler.png)

### Buton Bağlantısı (Opsiyonel)

```
ESP32 GPIO10 ──► Buton ──► 3.3V
```

Kod `INPUT_PULLUP` kullanır, buton basılınca GPIO10 HIGH olur. Bu ölçüm loglamak için kullanılır.

### Batarya ADC Bağlantısı (Opsiyonel)

```
LiPo (+) ──► R1 (100kΩ) ──► GPIO5 ──► R2 (100kΩ) ──► GND
```

Voltaj bölücü, 4.2V'luk bataryayı ESP32'nin okuyabileceği ~2.1V'a düşürür.

---

## 6. WiFi Ayarı ve İlk Yükleme

### Adım 1: WiFi Bilgilerini Gir

`include/wifi_secrets.h` dosyasını aç ve düzenle:

```cpp
#pragma once

static constexpr const char *WIFI_SSID     = "WiFi_Adin_Buraya";
static constexpr const char *WIFI_PASSWORD = "WiFi_Sifren_Buraya";
```

> **Güvenlik:** Bu dosyayı asla GitHub'a yükleyin. Şifrenizi içerir. `.gitignore`'a eklenmiştir.

### Adım 2: Derleme ve Yükleme

PlatformIO sol panelinden ya da terminal ile:

```bash
# Sadece derle (hata var mı kontrol et)
pio run

# Derle ve ESP32'ye yükle
pio run -t upload

# Yükleme bittikten sonra seri monitörü aç
pio device monitor
```

Veya VS Code'da alt çubukta bulunan **→ Upload** (sağa dönük ok) düğmesine bas.

### Adım 3: İlk Boot Çıktısını İzle

Seri monitörde şunu görmelisin:

```
ESP32-S3 dijital kumpas okuma basladi
DATA GPIO: 8, CLOCK GPIO: 9
WiFi baglandi: WiFi_Adin  IP: 192.168.1.42
mDNS: http://kumpas.local
```

IP adresini not al — `kumpas.local` çalışmazsa bu IP ile bağlan.

### Adım 4: Tarayıcıda Aç

```
http://kumpas.local/
```

veya

```
http://192.168.1.42/
```

> **Windows'ta mDNS:** Windows 10/11 bazen `.local` adreslerini çözemez. Bonjour servisi kurulu değilse direkt IP kullan.

---

## 7. Web Arayüzünü Kullanmak

### Ana Ekran (`/`)

```
┌──────────────────────────────────────────┐
│ Milimetre          İnç                   │
│  12.35 mm          0.4862 in             │
│                                          │
│ [online] [45ms] [raw=0x01A2B0]           │
│ [unit=mm] [sign=+] [btn=released]        │
│                                          │
│ [Yazılım Güncelle] [Kalibrasyon] [Log]   │
├──────────────────────────────────────────┤
│ Pil Durumu                               │
│  87%   3.98V          ● Voltaj yükseliyor│
│  ████████████████░░░  GPIO5 ADC          │
├──────────────────────────────────────────┤
│ Buton Logu                               │
│ Zaman        mm       in     Not         │
│ 14:23:01.234  12.35  0.4862  [örnek]     │
│                                          │
│ [CSV İndir] [Temizle]                    │
└──────────────────────────────────────────┘
```

**Ekrandaki göstergeler:**

| Gösterge | Anlamı |
|---|---|
| `online` / `offline` | WebSocket bağlantı durumu |
| `45 ms` | Son güncellemeden bu yana geçen süre |
| `raw=0x01A2B0` | Kumpasın ham 22-bit çerçevesi (debug için) |
| `unit=mm` | Aktif ölçü birimi |
| `sign=+` | Pozitif/negatif değer |
| `btn=released` | Buton durumu |
| `cal=0.000 mm` | Aktif kalibrasyon offseti |

### Buton Logu

Fiziksel butona her bastığında anki ölçüm tabloya kaydedilir. Notlar ekleyebilir, CSV olarak indirebilirsin.

### HTTP API Uç Noktaları

| URL | Açıklama |
|---|---|
| `GET /` | Ana HTML sayfası |
| `GET /api/measure` | JSON formatında anlık ölçüm |
| `GET /api/log?since=N` | N'den sonraki log satırları |
| `GET /log` | Log görüntüleme sayfası |
| `GET /calibrate` | Kalibrasyon formu |
| `GET /update` | OTA güncelleme sayfası |

**`/api/measure` JSON çıktısı örneği:**

```json
{
  "rawKey": 110256,
  "mm": 12.350,
  "inch": 0.48622,
  "isInch": false,
  "isNegative": false,
  "buttonPressed": false,
  "offsetMm": 0.000,
  "batV": 3.980,
  "batAdcMv": 1990,
  "batPct": 87,
  "batCharging": true,
  "ageMs": 45
}
```

---

## 8. Python Otomasyon Scripti

`scripts/measure_ws.py` scripti WebSocket üzerinden ESP32'ye bağlanır. Fiziksel butona bastığında mevcut ölçümü **aktif pencereye** (Excel, not defteri, vs.) yazar ve Enter'a basar.

### Kurulum

```bash
pip install websocket-client pyautogui
```

### Çalıştırma

```bash
python scripts/measure_ws.py
```

### Ayarlar (script içinde)

```python
WS_URL              = "ws://kumpas.local:81/"  # ESP32 WebSocket adresi
DECIMALS            = 2                         # Kaç ondalık yazılsın
DECIMAL_COMMA       = True                      # True → "12,35" / False → "12.35"
MIN_PRESS_INTERVAL_SEC = 0.2                    # Çift basış koruması (saniye)
```

> **Not:** Script çalışırken hedef uygulama (Excel gibi) **ön planda odakta** olmalı. Pyautogui aktif pencereye yazar, başka pencereye geçersen yanlış yere yazar.

---

## 9. Kalibrasyon

Kumpas sıfır noktası biraz kaymış olabilir. `/calibrate` sayfasından offset girebilirsin.

1. Tarayıcıda `http://kumpas.local/calibrate` adresini aç.
2. Offset değerini mm cinsinden gir. (Örnek: `-0.015`)
3. **Kaydet** düğmesine bas.
4. Offset ESP32'nin kalıcı belleğine (NVS) yazılır, yeniden başlatmadan sonra da korunur.
5. Sıfırlamak için **Offset Sıfırla** düğmesine bas.

> **Not:** Kalibrasyon offseti yalnızca `mm` değerine eklenir. `inch` değeri ham tutulur.

---

## 10. OTA ile Kablosuz Güncelleme

USB kablosu takmadan tarayıcıdan firmware güncelleyebilirsin.

### Adım 1: Binary Oluştur

```bash
pio run
```

Çıktı dosyası: `.pio/build/esp32s3/firmware.bin`

### Adım 2: Tarayıcıdan Yükle

1. `http://kumpas.local/update` adresini aç.
2. `firmware.bin` dosyasını seç.
3. **Yükle** düğmesine bas.
4. Yükleme tamamlanınca cihaz otomatik yeniden başlar.

> **Limit:** Maksimum firmware boyutu 3MB'dır (partition tablosuna göre). `pio run` çıktısında boyutu kontrol et.

---

## 11. Sorun Giderme

### Seri port görünmüyor / upload başarısız

1. Farklı USB kablosu dene (birçok kablo sadece şarj yapar, data aktaramaz).
2. Aygıt Yöneticisi'nde "Ports (COM & LPT)" altında ESP32 görünüyor mu?
3. CH340 veya CP2102 sürücüsünü kur.
4. ESP32 üzerindeki BOOT düğmesine basılı tutarak USB'ye tak, sonra bırak.

### Boot döngüsü / "partition invalid" hatası

Partition tablosu uyuşmazlığı:

```bash
pio run -t erase
pio run -t upload
```

Flash tamamen sılinir, temiz kurulum yapılır.

### Ölçüm gelmiyor (tarayıcıda `--` görünüyor)

1. Kumpas açık mı?
2. Seviye dönüştürücü bağlı mı? GND ortak mı?
3. Seri monitörde ne yazıyor? `webUpdateMeasurement` çağrılıyor mu?
4. FRAME_GAP_US değeri kumpasına uygun mu? (bkz. Bölüm 15)

### Ölçüm 2× büyük veya 2× küçük geliyor

`src/main.cpp` içinde:

```cpp
static constexpr float COUNTS_PER_MM   = 200.0f;  // 100 veya 400 dene
static constexpr float COUNTS_PER_INCH = 4000.0f;
```

Farklı kumpas modelleri farklı ölçek kullanır. 100, 200, 400 değerlerini dene.

### Negatif değer hiç çıkmıyor

```cpp
static constexpr uint8_t CALIPER_SIGN_BIT = 21;  // 20 veya 23 dene
```

### Birim (mm/inch) sürekli değişiyor

Kumpasın unit biti gürültülü olabilir. Otomatik düzeltme zaten açık (`g_autoUnitFix = true`). Sinyalin temizlemek için level shifter bağlantısını kontrol et.

### WiFi bağlanamıyor

1. `include/wifi_secrets.h` doğru yazıldı mı? (büyük/küçük harf önemli)
2. 2.4 GHz ağ mı? (ESP32 5 GHz desteklemez)
3. Seri monitörde timeout mesajı varsa cihaz offline modda çalışır, WiFi olmadan kumpas okumaya devam eder.

### OTA yükleme başarısız

1. `.pio/build/esp32s3/firmware.bin` dosyası mı seçildi?
2. Firmware boyutu 3 MB'dan büyük mü? Seri monitörde OTA hata mesajına bak.
3. Partition tablosu `partitions_ota_4mb.csv` ile eşleşiyor mu?

### kumpas.local açılmıyor

Windows'ta mDNS bazen çalışmaz:

- Seri monitörden IP adresini al.
- `http://192.168.x.x/` şeklinde direkt IP ile bağlan.
- Veya Bonjour / iTunes kurarak mDNS desteği ekle.

---

## 12. Proje Dosyaları Rehberi

```
kumpas-veri-okuma-s3-super-mini/
│
├── src/
│   ├── main.cpp          ← Ana ölçüm döngüsü, ISR, kumpas protokolü
│   └── web_ui.cpp        ← Web sunucu, WebSocket, HTML/CSS/JS, OTA
│
├── include/
│   ├── web_ui.h          ← main.cpp ve web_ui.cpp arasındaki arayüz
│   └── wifi_secrets.h    ← WiFi SSID ve şifre (git'e yükleme!)
│
├── scripts/
│   └── measure_ws.py     ← Python: butona basınca Excel'e ölçüm yaz
│
├── platformio.ini         ← PlatformIO derleme ayarları
├── partitions_ota_4mb.csv ← 4MB flash partition tablosu (OTA için)
└── README.md              ← Bu dosya
```

### Her Dosyanın Görevi

**`src/main.cpp`**  
Ölçüm mantığının tamamı burada. ISR kumpas clock darbelerini yakalar, 24-bit frame biriktir, decode eder, mm/inch'e çevirir, web katmanına gönderir. Batarya ADC ve buton yönetimi de burada.

**`src/web_ui.cpp`**  
Ağa açılan tüm katman: HTTP sunucu, WebSocket sunucu, mDNS, OTA, kalibrasyon. Tüm HTML/CSS/JS PROGMEM içinde saklı (flash'tan sunulur, RAM harcamaz).

**`include/web_ui.h`**  
İki kaynak dosya arasındaki sözleşme. Sadece 4 fonksiyon tanımlar: `webBegin()`, `webLoop()`, `webUpdateMeasurement()`, `webUpdateButton()`, `webUpdateBattery()`, `webLogf()`.

**`platformio.ini`**  
Kartın adı, flash boyutu, partition dosyası, USB CDC ayarları ve bağımlı kütüphane (`WebSockets`) burada tanımlanır.

**`partitions_ota_4mb.csv`**  
4MB flash'ı nasıl bölüştüreceğini tanımlar. `app0` ve `app1` iki OTA slotudur; biri çalışırken diğerine yeni firmware yazılır.

---

## 13. Protokol Detayı — Kumpas 24-bit Veri

Yaptığımız kumpaslar "Chinese 24-bit caliper" protokolü kullanır. Basit ama belgelenmemiştir.

### Sinyal Yapısı

```
CLOCK: ──┐  ┌──┐  ┌──┐  ┌── ...
          └──┘  └──┘  └──┘
DATA:   ──── bit0 ─── bit1 ─── bit2 ...
```

- Her CLOCK **düşen kenarında** DATA pininden 1 bit okunur.
- Bitler **LSB-first** gelir (en az anlamlı bit önce).
- Her frame **24 bit**tir.
- Frameler arası boşluk (tipik ~100ms) frame sınırını belirler.

### 24-bit Frame Yapısı

```
Bit 0..19  → Büyüklük (magnitude), 20 bit, her zaman pozitif
Bit 21     → İşaret biti (bu kumpas için): 1 = negatif değer
Bit 23     → Birim biti (bu kumpas için): 1 = inch, 0 = mm
Bit 20,22  → Genellikle 0 (model bağımlı)
```

> **Dikkat:** Bit düzeni modelden modele değişir. CALIPER_SIGN_BIT ve CALIPER_UNIT_BIT sabitleri buna göre ayarlanmalı.

### mm/inch Dönüşümü

```
mm   = magnitude / 200        (COUNTS_PER_MM = 200)
inch = magnitude / 4000       (COUNTS_PER_INCH = 4000)
```

Negatif değer için sonuç negatife çevrilir.

### Frame Toplama ve Filtreleme

```
ISR (her CLK düşen kenar)
  │
  ├─ Boşluk > FRAME_GAP_US (1500µs)?
  │    EVET → bit sayacını sıfırla (yeni frame başlıyor)
  │
  ├─ Boşluk < MIN_EDGE_US (5µs)?
  │    EVET → gürültü, bu biti atla
  │
  └─ DATA'yı oku, shift register'a ekle
       24. bit doldu? → frame tamamlandı, ana döngüye bildir
```

Ana döngüde frame stabilitesi kontrol edilir:

- Aynı frame **2 kez üst üste** gelmezse yayınlanmaz.
- Büyük sıçrama varsa (>0.5mm) **4 kez üst üste** gelmesi beklenir.

---

## 14. Yazılım Mimarisi — Kod Nasıl Çalışır

### Veri Akışı

```
Kumpas CLK/DATA
    │
    ▼
onCaliperClockFalling() [ISR — her CLK düşen kenarında]
    │  g_shiftReg'e bit ekle
    │  24. bit dolunca: g_lastFrame = g_shiftReg, g_hasFrame = true
    ▼
loop() [ana döngü, ~2ms arayla]
    │
    ├─ caliperReadFrame(raw) → ham 24-bit frame al
    │
    ├─ stabilizasyon filtresi (pendingCount)
    │
    ├─ decode24(raw) → {magnitude, isNegative, isInch}
    │
    ├─ updateUnitStateByContinuity() → birim tutarlılık kontrolü
    │
    ├─ countsToMillimeters() / countsToInches()
    │
    └─ webUpdateMeasurement(rawKey, mm, inch, ...) 
            │
            ├─ lastMm = mm + calOffsetMm  (kalibrasyon offseti)
            ├─ buildMeasureJson()
            └─ wsServer.broadcastTXT()    (WebSocket broadcast)
```

### Önemli Fonksiyonlar

| Fonksiyon | Dosya | Satır | Ne Yapar |
|---|---|---|---|
| `onCaliperClockFalling` | [main.cpp](src/main.cpp#L186) | 186 | ISR: her CLK kenarında 1 bit okur |
| `caliperBegin` | [main.cpp](src/main.cpp#L217) | 217 | GPIO ve interrupt kurulumu |
| `caliperReadFrame` | [main.cpp](src/main.cpp#L229) | 229 | ISR'dan tamamlanmış frame'i al |
| `decode24` | [main.cpp](src/main.cpp#L244) | 244 | 24-bit ham veriyi sign/unit/magnitude'a böl |
| `updateUnitStateByContinuity` | [main.cpp](src/main.cpp#L133) | 133 | mm/inch birim tutarlılığını kontrol et |
| `countsToMillimeters` | [main.cpp](src/main.cpp#L112) | 112 | Count sayısını mm'ye çevir |
| `setup` | [main.cpp](src/main.cpp#L285) | 285 | Tek seferlik başlatma |
| `loop` | [main.cpp](src/main.cpp#L304) | 304 | Ana döngü |
| `batteryPercentFromVolts` | [main.cpp](src/main.cpp#L79) | 79 | Voltajı % pil seviyesine çevir |
| `webBegin` | [web_ui.cpp](src/web_ui.cpp#L789) | 789 | WiFi, HTTP, WS başlat |
| `webLoop` | [web_ui.cpp](src/web_ui.cpp#L879) | 879 | HTTP ve WS isteklerini işle |
| `buildMeasureJson` | [web_ui.cpp](src/web_ui.cpp#L685) | 685 | JSON string oluştur |
| `webUpdateMeasurement` | [web_ui.cpp](src/web_ui.cpp#L884) | 884 | Ölçümü sakla ve WS'e broadcast et |

### Kritik Tasarım: ISR ve portMUX

ISR çok hızlı çalışır ve ana döngüyle veri paylaşır. `g_lastFrame` ve `g_hasFrame` değişkenleri her ikisinden erişilir. Çakışmayı önlemek için:

```cpp
// ISR içinde:
portENTER_CRITICAL_ISR(&g_mux);
g_lastFrame = g_shiftReg;
g_hasFrame  = true;
portEXIT_CRITICAL_ISR(&g_mux);

// Ana döngüde:
portENTER_CRITICAL(&g_mux);
rawOut    = g_lastFrame;
g_hasFrame = false;
portEXIT_CRITICAL(&g_mux);
```

Bu kritik bölge olmadan veri bozulabilir (race condition).

### Otomatik Birim Düzeltme

Bazı kumpas modellerinde unit biti gürültülü veya hiç yok. Fonksiyon fiziksel süreklilik kontrolü yapar:

```
Mevcut ölçüm (mm cinsinden): 12.35
Yeni ham değer → mm yorumlanırsa: 12.40  (fark 0.05mm — mantıklı)
Yeni ham değer → inch yorumlanırsa: 313.6mm (fark 301mm — imkansız)

→ mm yorumu doğru, birim değiştirilmez.
```

Ardı ardına 3 kez güçlü bir ipucu gelirse birim değiştirilir.

---

## 15. Ayarlanabilir Parametreler

Tüm kullanıcı ayarları `src/main.cpp` başındadır.

### GPIO Pinleri

```cpp
static constexpr int CALIPER_DATA_PIN  = 8;   // Kumpas DATA → GPIO8
static constexpr int CALIPER_CLK_PIN   = 9;   // Kumpas CLOCK → GPIO9
static constexpr int BUTTON_PIN        = 10;  // Buton → GPIO10
static constexpr int BATTERY_ADC_PIN   = 5;   // Batarya ADC → GPIO5
```

### Ölçek Katsayıları

```cpp
static constexpr float COUNTS_PER_MM   = 200.0f;   // mm başına count sayısı
static constexpr float COUNTS_PER_INCH = 4000.0f;  // inch başına count sayısı
```

Kumpas okuma 2× büyükse → 400 yap.  
Kumpas okuma 2× küçükse → 100 yap.

### Kumpas Bit Düzeni

```cpp
static constexpr uint8_t CALIPER_VALUE_BITS = 20;  // Büyüklük bit sayısı
static constexpr uint8_t CALIPER_SIGN_BIT   = 21;  // İşaret biti (0=pozitif, 1=negatif)
static constexpr uint8_t CALIPER_UNIT_BIT   = 23;  // Birim biti (0=mm, 1=inch)
```

Negatif değer yoksa CALIPER_SIGN_BIT'i 20 veya 23 dene.

### Zamanlama

```cpp
static constexpr uint32_t FRAME_GAP_US  = 1500;  // Frame sınırı (mikrosaniye)
static constexpr uint32_t MIN_EDGE_US   = 5;     // Gürültü eşiği (mikrosaniye)
```

Kumpas sürekli veri göndermiyorsa FRAME_GAP_US'u büyüt (2000-3000 dene).

### Batarya Voltaj Bölücü

```cpp
static constexpr float BAT_R1_OHMS   = 100000.0f;  // Üst direnç
static constexpr float BAT_R2_OHMS   = 100000.0f;  // Alt direnç
static constexpr float BATTERY_MIN_V = 3.0f;        // %0 voltajı
static constexpr float BATTERY_MAX_V = 4.2f;        // %100 voltajı
```

---

## 16. Batarya Ölçümü ve Şarj Tahmini

### Voltaj Bölücü Formülü

```
Vbat = Vadc × (R1 + R2) / R2
     = Vadc × (100k + 100k) / 100k
     = Vadc × 2
```

ADC 2.0V okursa → batarya 4.0V.

### Şarj Tespiti

İki ayrı yöntemle şarj algılanır:

1. **Kısa vadeli:** Son `CHARGE_WINDOW_MS` (30 saniye) içinde voltaj `CHARGE_MIN_DELTA_V` (10mV) artmış mı?
2. **Uzun vadeli:** Voltaj `STABLE_PERIOD_MS` (2 dakika) boyunca sabit kaldıktan sonra `LONG_CHARGE_MIN_DELTA_V` (10mV) artmış mı?

İkisinden biri doğruysa şarj göstergesi yanar.

### Düşük Pil Uyarısı

Batarya %20 altına inince UI'da dolum çubuğu kırmızıya döner.

---

## 17. AI Asistanı ile Çalışmak

Bu projeyi bir AI asistanına (Claude, ChatGPT, Gemini, Copilot) yaptırmak veya değiştirmek istersen etkili prompt şablonları:

### Temel Prompt Şablonu

```
Rol: ESP32-S3 ve Arduino C++ bilen geliştirici
Dosyalar: [ilgili dosyaların adı]
Hedef: [ne yapmak istiyorsun, tek cümle]
Kısıtlar: [varsa, örn. "API bozulmasın", "UI id'leri değişmesin"]
Girdi: [log çıktısı, hata mesajı, ya da beklenen/gerçek sonuç]
Beklenen: [kısa çözüm veya kod değişikliği]
```

### Hazır Prompt Örnekleri

**1. Ölçüm 2× büyük geliyor:**
```
Rol: ESP32-S3 Arduino geliştirici
Dosya: src/main.cpp
Hedef: Kumpas ölçümü 2× büyük geliyor. COUNTS_PER_MM değerini düzelt.
Girdi: 10mm'lik obje için 20.00 okuyorum.
Beklenen: COUNTS_PER_MM için doğru değeri bul ve değiştir.
```

**2. Negatif değer hiç gelmiyor:**
```
Rol: ESP32-S3 Arduino geliştirici
Dosya: src/main.cpp
Hedef: Kumpas sıfırın soluna geçince negatif değer göstermiyor, hep 0 kalıyor.
Girdi: CALIPER_SIGN_BIT = 21 şu an.
Beklenen: CALIPER_SIGN_BIT için 20 ve 23 değerlerini dene, hangisi çalışır açıkla.
```

**3. UI'ye yeni alan ekleme:**
```
Rol: ESP32-S3 Arduino geliştirici
Dosya: src/web_ui.cpp
Hedef: Ana sayfada mm değerinin altına "Ham ADC (mV): 1990" satırı ekle.
Kısıt: Mevcut HTML id'leri değişmesin, batAdcMv zaten JSON'da var.
Beklenen: INDEX_HTML içinde ilgili HTML ve JS satırlarını yaz.
```

**4. WebSocket'e bağlanma:**
```
Rol: Python geliştirici
Dosya: scripts/measure_ws.py
Hedef: Script'i Excel yerine bir CSV dosyasına yazdıracak şekilde değiştir.
Kısıt: Mevcut WebSocket mantığı bozulmasın.
Beklenen: Dosya yazma kodu ekle, dosya adı tarihli olsun.
```

**5. Yeni GPIO pin eklemek:**
```
Rol: ESP32-S3 Arduino geliştirici
Dosya: src/main.cpp, src/web_ui.cpp
Hedef: GPIO13'e bağlı bir LED'i butona basınca yakıp söndür.
Kısıt: Mevcut buton log mantığı çalışmaya devam etsin.
Beklenen: LED için pin tanımı, setup'a pinMode, loop'a digitalWrite ekle.
```

### AI'ya Proje Bağlamı Vermek

AI asistanına bu README'yi ve ilgili kaynak dosyayı ver. Şunu de:

```
Bu proje hakkında şu belgeyi ve kaynak kodları sana veriyorum.
Değişiklik yaparken mevcut kod stilini koru, gereksiz yorum ekleme,
sadece sorduğum değişikliği yap.
```

---

## 18. Tasarım Kararları ve Nedenleri

Her tasarım kararının "neden" sorusuna cevabı burada.

| Karar | Neden |
|---|---|
| ISR ile veri okuma | `loop()` yavaş, darbeler kaçar. ISR anında tetiklenir. |
| FRAME_GAP_US ile frame ayırma | Kumpas preamble göndermez, sadece sessizlik. Gap frame sınırı. |
| pendingCount stabilizasyon | Tek bozuk frame anlık sıçrama gibi görünür. 2 tekrar → güvenilir. |
| Büyük sıçramada daha fazla tekrar | Kumpas kaydırılırken geçici hatalı frame geçmez. |
| Otomatik birim düzeltme | Ucuz kumpas modellerinde unit bit hiç çalışmayabilir. Süreklilik her zaman vardır. |
| portMUX kritik bölge | ISR ve loop aynı değişkeni paylaşır. Mutex olmadan veri bozulur. |
| PROGMEM'de HTML | RAM kısıtlı (320KB). 4KB HTML RAM'i çok harcar. Flash'ta tutar. |
| HTTP polling + WebSocket | UI için basit 200ms polling yeterli. Python gibi dışarıdan bağlananlar için WS push. |
| Preferences ile kalibrasyon | NVS (Non-Volatile Storage) güç kesilince sıfırlanmaz. Kullanıcı kalibrasyon kaybetmez. |
| İki OTA slotu | Yeni firmware bozuksa eski firmware'e güvenli dönüş. |
| Voltaj bölücü + IIR filtre | Ham ADC gürültülü, filtreleme sabit bir değer verir. |

---

*Bu belge hem "ne" hem "neden" sorusunu yanıtlamak için hazırlandı. Takıldığın yerde hangi kumpas modelini kullandığını, seri monitör çıktısını ve ölçüm davranışını yaz — bit düzenini birlikte düzeltebiliriz.*
