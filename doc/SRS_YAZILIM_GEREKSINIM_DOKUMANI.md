# Yazılım Gereksinim Dokümanı (SRS)

**Proje:** ESP32-S3 Super Mini — Dijital Kumpas Veri Okuma  
**Doküman kimliği:** SRS-KUMPAS  
**Sürüm:** 1.0  
**Tarih:** 2026-10-10  
**Durum:** Yürürlükte  
**İlgili dokümanlar:** [SDD](SDD_YAZILIM_TASARIM_DOKUMANI.md) · [Bilinen Sorunlar](BILINEN_SORUNLAR.md) · [ADR dizini](adr/) · [GUIDE dizini](guide/) · [GUIDE-015 Test ve Doğrulama](guide/GUIDE-015_TEST_VE_DOGRULAMA.md)

> Bu doküman ISO/IEC/IEEE 29148 ve IEEE 830 yapısı örnek alınarak hazırlanmıştır. Gereksinimler mevcut firmware'in (commit `2e2619b`) davranışından **tersine mühendislik** ile çıkarılmış, ardından hedeflenen davranışla karşılaştırılmıştır. Her gereksinimin yanında mevcut kodun bu gereksinimi karşılayıp karşılamadığı belirtilir.

---

## İçindekiler

1. [Giriş](#1-giriş)
2. [Genel Tanım](#2-genel-tanım)
3. [Fonksiyonel Gereksinimler](#3-fonksiyonel-gereksinimler)
4. [Dış Arayüz Gereksinimleri](#4-dış-arayüz-gereksinimleri)
5. [Fonksiyonel Olmayan Gereksinimler](#5-fonksiyonel-olmayan-gereksinimler)
6. [Tasarım ve Uygulama Kısıtları](#6-tasarım-ve-uygulama-kısıtları)
7. [Doğrulama Yaklaşımı](#7-doğrulama-yaklaşımı)
8. [İzlenebilirlik Matrisi](#8-izlenebilirlik-matrisi)
9. [Ek A — Gereksinim Durum Özeti](#ek-a--gereksinim-durum-özeti)
10. [Ek B — Değişiklik Geçmişi](#ek-b--değişiklik-geçmişi)

---

## 1. Giriş

### 1.1 Amaç

Bu doküman, ucuz 24-bit "Çin tipi" dijital kumpasların veri portundan ölçüm okuyan ve bu ölçümleri yerel ağ üzerinden sunan ESP32-S3 firmware'inin ve ona eşlik eden yardımcı yazılımların (tarayıcı arayüzü, Python otomasyon scripti) gereksinimlerini tanımlar.

Hedef okuyucular:

| Okuyucu | Kullanım amacı |
|---|---|
| Firmware geliştiricisi | Ne yapılması gerektiğini ve sınırları anlamak |
| Test eden kişi | Kabul kriterlerini ve doğrulama yöntemlerini görmek |
| Proje sahibi | Kapsamı ve mevcut durumu değerlendirmek |
| Yeni katkıcı / AI asistanı | Sistemin sözleşmesini bozmadan değişiklik yapmak |

### 1.2 Kapsam

Sistemin adı: **Kumpas Okuyucu** (mDNS adı `kumpas`).

Kapsam içinde:

- Kumpasın DATA/CLOCK hatlarından 24-bit çerçevelerin kesme tabanlı okunması
- Çerçevenin büyüklük/işaret/birim olarak çözümlenmesi ve mm/inç değerine çevrilmesi
- Gürültüye karşı tekrar tabanlı filtreleme ve birim tutarlılık çıkarımı
- WiFi (STA) bağlantısı, mDNS, HTTP sunucusu (port 80), WebSocket sunucusu (port 81)
- Tarayıcı arayüzü: ölçüm gösterimi, batarya durumu, buton logu, not ekleme, CSV dışa aktarma
- Kalıcı kalibrasyon ofseti (NVS)
- Tarayıcı üzerinden OTA firmware güncellemesi
- LiPo batarya gerilim ölçümü ve şarj çıkarımı
- Fiziksel buton durumunun okunması ve yayınlanması
- Cihaz içi durum/hata log halka tamponu
- Python scripti ile buton basışında ölçümün aktif pencereye klavye girişi olarak yazılması

Kapsam dışında:

- Kumpasa veri gönderme (kumpas kontrolü, sıfırlama, birim değiştirme)
- Bulut bağlantısı, uzak veritabanı, kullanıcı hesapları
- Kimlik doğrulama ve şifreli iletişim (bkz. [ADR-022](adr/ADR-022_KIMLIK_DOGRULAMASIZ_YEREL_AG_GUVENLIK_MODELI.md))
- Bluetooth/BLE klavye emülasyonu (kütüphane iskeleti mevcut ama kullanılmıyor, bkz. [ADR-021](adr/ADR-021_PYTHON_ILE_KLAVYE_EMULASYONU.md))
- Gerçek zaman saati / NTP ile cihaz tarafı zaman damgası

### 1.3 Tanımlar ve Kısaltmalar

| Terim | Açıklama |
|---|---|
| Çerçeve (frame) | Kumpasın bir veri patlamasında gönderdiği 24 bitlik paket |
| Count | Kumpasın ham büyüklük birimi; mm modunda 0,005 mm, inç modunda 0,00025 in |
| Ham anahtar (rawKey / key) | Çerçevenin değer + işaret + birim bitleri ile maskelenmiş hali |
| ISR | Interrupt Service Routine — kesme servis rutini |
| Patlama boşluğu (frame gap) | İki çerçeve arasında CLOCK hattının sessiz kaldığı süre |
| Stabil değer | Filtreden geçip yayınlanan ölçüm |
| Ofset | Kullanıcının girdiği, mm değerine eklenen kalibrasyon düzeltmesi |
| NVS | Non-Volatile Storage — ESP32 flash üzerindeki anahtar/değer deposu |
| OTA | Over-The-Air — kablosuz firmware güncellemesi |
| mDNS | Multicast DNS — `kumpas.local` adının yerel ağda çözülmesi |
| WS | WebSocket |
| STA | WiFi istasyon (istemci) modu |
| PROGMEM | Verinin flash'ta tutulmasını belirten işaret |
| UI | Kullanıcı arayüzü (tarayıcıda çalışan HTML/JS) |
| KS-nn | [Bilinen Sorunlar](BILINEN_SORUNLAR.md) listesindeki kayıt |

### 1.4 Gereksinim Kimlik Şeması

| Önek | Anlam |
|---|---|
| `FG-<ALAN>-nn` | Fonksiyonel gereksinim |
| `FOG-<ALAN>-nn` | Fonksiyonel olmayan gereksinim |
| `AG-<ALAN>-nn` | Dış arayüz gereksinimi |
| `KIS-nn` | Tasarım/uygulama kısıtı |

Alan kodları: `OKU` okuma, `COZ` çözümleme, `FLT` filtre, `BRM` birim, `NET` ağ, `HTTP` HTTP API, `WS` WebSocket, `UI` arayüz, `KAL` kalibrasyon, `OTA` güncelleme, `BAT` batarya, `BTN` buton, `LOG` log, `PY` Python istemcisi, `PRF` performans, `GVN` güvenilirlik, `KAY` kaynak, `SEC` güvenlik, `BAK` bakım, `KUL` kullanılabilirlik, `TAS` taşınabilirlik.

Öncelik: **Z** = Zorunlu, **Ö** = Önemli, **İ** = İsteğe bağlı.  
Doğrulama: **T** = Test, **D** = Gösterim, **İ** = İnceleme, **A** = Analiz.  
Durum: ✅ Karşılanıyor · ⚠️ Kısmen · ❌ Karşılanmıyor.

### 1.5 Referanslar

- [README.md](../README.md) — kullanıcı odaklı proje kılavuzu
- [src/main.cpp](../src/main.cpp), [src/web_ui.cpp](../src/web_ui.cpp), [include/web_ui.h](../include/web_ui.h)
- [scripts/measure_ws.py](../scripts/measure_ws.py)
- [partitions_ota_4mb.csv](../partitions_ota_4mb.csv), [platformio.ini](../platformio.ini)
- Espressif ESP32-S3 Teknik Referans Kılavuzu (GPIO, ADC, flash bölümleme)
- RFC 6455 (WebSocket), RFC 6762 (mDNS)

---

## 2. Genel Tanım

### 2.1 Ürün Perspektifi

Sistem bağımsız, gömülü bir üründür. Ticari kumpasın kendi ekranına ve iç devresine dokunulmaz; yalnızca kumpasın servis/veri konektöründen yayınlanan seri veri pasif olarak dinlenir.

```
 ┌────────────┐  DATA/CLOCK (1,5 V)  ┌─────────────┐  3,3 V   ┌─────────────────────┐
 │  Dijital   │ ───────────────────► │   Seviye    │ ───────► │  ESP32-S3 Super Mini │
 │  Kumpas    │ ◄──── GND ortak ───► │ dönüştürücü │          │  (bu firmware)       │
 └────────────┘                      └─────────────┘          │                     │
 ┌────────────┐                                               │  GPIO8  DATA        │
 │ LiPo pil   │──► R1/R2 bölücü ─────────────────────────────►│  GPIO9  CLOCK (IRQ) │
 └────────────┘                                               │  GPIO5  ADC         │
 ┌────────────┐                                               │  GPIO10 Buton       │
 │ Buton      │──────────────────────────────────────────────►│                     │
 └────────────┘                                               └──────────┬──────────┘
                                                                         │ WiFi 2,4 GHz
                                          ┌──────────────────────────────┼─────────────────┐
                                          ▼                              ▼                 ▼
                                   Tarayıcı (HTTP:80)          Python scripti (WS:81)   Diğer WS istemcileri
```

### 2.2 Ürün İşlevleri (Özet)

1. Kumpas ölçümünü sürekli ve kayıpsız okumak
2. Ölçümü mm ve inç olarak hesaplamak, gürültüyü filtrelemek
3. Ölçümü tarayıcıda canlı göstermek ve WebSocket ile anında yayınlamak
4. Butona basıldığında ölçümün kaydedilmesini (UI logu veya Python ile hedef uygulamaya yazma) sağlamak
5. Batarya seviyesini ve şarj eğilimini göstermek
6. Kalibrasyon ofsetini kalıcı saklamak
7. Firmware'i kablosuz güncellemek
8. Teşhis amaçlı cihaz içi log tutmak

### 2.3 Kullanıcı Sınıfları

| Sınıf | Tanım | Teknik seviye | Ana etkileşim |
|---|---|---|---|
| Operatör | Atölyede ölçüm alan kişi | Düşük | Kumpas, buton, tarayıcı sayfası, Excel |
| Kurulumcu | Cihazı monte eden, WiFi ayarlayan, kalibre eden kişi | Orta | Lehim, PlatformIO, `/calibrate`, `/update` |
| Geliştirici | Firmware'i değiştiren, yeni kumpasa uyarlayan kişi | Yüksek | Kaynak kod, seri monitör, logic analyzer |
| Entegrasyon istemcisi | Ölçümü programatik olarak tüketen yazılım | — | `GET /api/measure`, `ws://…:81/` |

### 2.4 İşletim Ortamı

| Öğe | Değer |
|---|---|
| Mikrodenetleyici | ESP32-S3 (çift çekirdek Xtensa LX7, 240 MHz) — "Super Mini" kartı |
| Flash | 4 MB (`board_build.flash_size = 4MB`) |
| Kart tanımı | `esp32-s3-devkitc-1` (PlatformIO) |
| Çatı | Arduino-ESP32 (PlatformIO `espressif32` platformu) |
| Harici kütüphane | `links2004/WebSockets` |
| Ağ | 2,4 GHz WiFi, WPA/WPA2 kişisel, DHCP |
| İstemci tarayıcı | Güncel Chrome/Edge/Firefox/Safari (ES2017 `async/await`, `fetch`) |
| Python istemcisi | Python 3.8+, `websocket-client`, `pyautogui`; masaüstü oturumu (Windows/macOS/Linux X11) |
| Seri konsol | USB CDC, 115200 baud |

### 2.5 Varsayımlar ve Bağımlılıklar

- **VAR-01** Kumpas, CLOCK düşen kenarında geçerli DATA biti sunan, LSB-önce, 24-bit çerçeveli protokolü kullanır.
- **VAR-02** Çerçeveler arasında en az `FRAME_GAP_US` (1500 µs) sessizlik vardır; çerçeve içindeki ardışık düşen kenarlar arasındaki süre `MIN_EDGE_US` (5 µs) ile `FRAME_GAP_US` arasındadır.
- **VAR-03** Kumpas sinyalleri uygun seviye dönüştürme ile ESP32'nin 3,3 V mantık eşiklerine taşınmıştır ve toprak ortaktır.
- **VAR-04** Kullanılan kumpas varyantında büyüklük bit 0–19, işaret bit 21'dir. Farklı varyantlar için [GUIDE-010](guide/GUIDE-010_YENI_KUMPAS_MODELINE_UYARLAMA.md) uygulanır.
- **VAR-05** Cihaz ile istemciler aynı, güvenilir yerel ağdadır.
- **VAR-06** Kullanıcı derleme öncesi `include/wifi_secrets.h` dosyasını oluşturmuştur.
- **VAR-07** Batarya ölçümü kullanılıyorsa 100 kΩ / 100 kΩ bölücü GPIO5'e bağlıdır; kullanılmıyorsa batarya değerleri anlamsızdır ama sistem çalışır.

---

## 3. Fonksiyonel Gereksinimler

### 3.1 Kumpas Verisinin Edinimi (OKU)

| ID | Gereksinim | Öncelik | Doğrulama | Durum |
|---|---|---|---|---|
| FG-OKU-01 | Sistem, kumpas CLOCK hattının (GPIO9) her **düşen kenarında** DATA hattından (GPIO8) bir bit örneklemelidir. | Z | T, İ | ✅ |
| FG-OKU-02 | Bitler **LSB-önce** sırasıyla bir kaydırma yazmacında birleştirilmelidir (n. bit → `bit << n`). | Z | İ | ✅ |
| FG-OKU-03 | Ardışık iki düşen kenar arası süre `FRAME_GAP_US`'ten büyükse yeni çerçeve başlatılmalı (bit sayacı ve yazmaç sıfırlanmalı). | Z | T, İ | ✅ |
| FG-OKU-04 | Ardışık iki düşen kenar arası süre `MIN_EDGE_US`'ten küçükse kenar gürültü sayılmalı ve bit örneklenmemelidir. | Ö | İ | ✅ |
| FG-OKU-05 | 24 bit toplandığında çerçeve ana döngüye teslim edilmeli ve toplama yeniden başlatılmalıdır. | Z | T | ✅ |
| FG-OKU-06 | ISR ile ana döngü arasındaki çerçeve teslimi, yarış durumuna karşı kritik bölge ile korunmalıdır. | Z | İ, A | ✅ |
| FG-OKU-07 | Ana döngü çerçeveyi zamanında almazsa, yeni çerçeve eskisinin üzerine yazmalıdır (en güncel değer kazanır; kuyruk tutulmaz). | Ö | İ | ✅ |
| FG-OKU-08 | ISR, flash erişimi gerektiren Arduino yardımcılarını (`digitalRead`, `micros`) kullanmamalı; IRAM'de çalışmalıdır. | Z | İ | ✅ |

### 3.2 Çözümleme (COZ)

| ID | Gereksinim | Öncelik | Doğrulama | Durum |
|---|---|---|---|---|
| FG-COZ-01 | Çerçevenin alt `CALIPER_VALUE_BITS` (20) biti işaretsiz büyüklük (count) olarak alınmalıdır. | Z | T | ✅ |
| FG-COZ-02 | `CALIPER_SIGN_BIT` (21) biti 1 ise değer negatif kabul edilmelidir (polarite çalışma zamanı bayrağıyla ters çevrilebilir). | Z | T | ✅ |
| FG-COZ-03 | `CALIPER_UNIT_BIT` (23) biti başlangıç birimi ipucu olarak okunmalı; birim biti işaret bitiyle aynı konumda ise yok sayılmalıdır. | Ö | İ | ✅ |
| FG-COZ-04 | mm modunda `mm = count / COUNTS_PER_MM` (200), inç modunda `inch = count / COUNTS_PER_INCH` (4000) hesaplanmalıdır. | Z | T | ✅ |
| FG-COZ-05 | Her ölçüm hem mm hem inç olarak, aralarında 25,4 katsayısıyla dönüştürülerek üretilmelidir. | Z | T | ✅ |
| FG-COZ-06 | Mutlak değeri `MAX_ABS_MM` (500 mm) üzerinde veya sonlu olmayan ölçümler yayınlanmamalıdır. | Z | T | ✅ |
| FG-COZ-07 | `REJECT_IF_TOP_BITS_SET` etkin ise bit 22–23'ten herhangi biri 1 olan çerçeveler atılmalıdır (varsayılan: kapalı). | İ | İ | ✅ |

### 3.3 Filtreleme (FLT)

| ID | Gereksinim | Öncelik | Doğrulama | Durum |
|---|---|---|---|---|
| FG-FLT-01 | Ham çerçeve 24 bite maskelendikten sonra yalnızca değer + işaret + birim bitlerinden oluşan "anahtar" ile karşılaştırılmalıdır. | Z | İ | ✅ |
| FG-FLT-02 | Bir anahtar en az `STABLE_REPEATS_NORMAL` (2) kez **ardışık** görülmeden yayınlanmamalıdır. | Z | T | ✅ |
| FG-FLT-03 | Yeni değer son yayınlanandan `JUMP_MM_THRESHOLD` (0,50 mm) fazla farklıysa `STABLE_REPEATS_ON_JUMP` (4) ardışık tekrar gerekmelidir. | Z | T | ✅ |
| FG-FLT-04 | Son yayınlanan anahtarla aynı olan anahtar tekrar yayınlanmamalıdır (yalnızca değişim yayınlanır). | Ö | T | ✅ |

### 3.4 Birim Çıkarımı (BRM)

| ID | Gereksinim | Öncelik | Doğrulama | Durum |
|---|---|---|---|---|
| FG-BRM-01 | İlk geçerli çerçevede birim durumu, birim bitinden başlatılmalıdır. | Z | İ | ✅ |
| FG-BRM-02 | `g_autoUnitFix` etkin iken, mevcut birim yorumuna göre değer son yayınlanandan ≥ 4,0 mm sapıyor **ve** alternatif yorum ≤ 0,40 mm içinde kalıyorsa bu "güçlü birim değişim ipucu" sayılmalıdır. | Z | T, A | ✅ |
| FG-BRM-03 | Birim durumu yalnızca **3 ardışık** güçlü ipucundan sonra değiştirilmelidir; arada ipucu kesilirse sayaç sıfırlanmalıdır. | Z | T | ✅ |
| FG-BRM-04 | Yayınlanan `isInch` alanı, çıkarılmış birim durumunu yansıtmalıdır. | Z | T | ✅ |

### 3.5 Ağ Bağlantısı (NET)

| ID | Gereksinim | Öncelik | Doğrulama | Durum |
|---|---|---|---|---|
| FG-NET-01 | Cihaz açılışta `wifi_secrets.h` içindeki SSID/şifre ile STA modunda bağlanmayı denemelidir. | Z | D | ✅ |
| FG-NET-02 | Bağlantı 15 s içinde kurulamazsa cihaz ağ olmadan çalışmaya devam etmelidir (kumpas okuma durmamalıdır). | Z | T | ✅ |
| FG-NET-03 | Bağlantı kurulursa `kumpas.local` adıyla mDNS yayını yapılmalı ve `_http._tcp` servisi 80 portunda ilan edilmelidir. | Ö | D | ✅ |
| FG-NET-04 | WiFi güç tasarrufu kapatılmalıdır (gecikmeyi azaltmak için). | Ö | İ | ✅ |
| FG-NET-05 | Bağlantı sonradan koparsa/kurulursa sistem ağ hizmetlerini (mDNS dahil) yeniden sağlamalıdır. | Ö | T | ⚠️ KS-09 |
| FG-NET-06 | IP adresi ve mDNS adresi seri konsola yazdırılmalıdır. | Ö | D | ✅ |

### 3.6 HTTP API (HTTP)

| ID | Gereksinim | Öncelik | Doğrulama | Durum |
|---|---|---|---|---|
| FG-HTTP-01 | `GET /` ana arayüz HTML'ini döndürmelidir. | Z | T | ✅ |
| FG-HTTP-02 | `GET /api/measure` son durumu [SDD §6.3](SDD_YAZILIM_TASARIM_DOKUMANI.md#63-ölçüm-json-sözleşmesi) şemasındaki JSON olarak döndürmelidir. | Z | T | ✅ |
| FG-HTTP-03 | `GET /api/log?since=N` sıra numarası N'den büyük log satırlarını ve güncel `seq` değerini döndürmelidir. | Ö | T | ✅ |
| FG-HTTP-04 | `GET /log`, `GET /calibrate`, `GET /update` ilgili sayfaları döndürmelidir. | Z | T | ✅ |
| FG-HTTP-05 | Tanımsız yollar için `404 Not Found` döndürülmelidir. | Ö | T | ✅ |
| FG-HTTP-06 | `ageMs` alanı son **ölçüm** güncellemesinden bu yana geçen süreyi göstermelidir. | Ö | T | ⚠️ KS-06 |

### 3.7 WebSocket Yayını (WS)

| ID | Gereksinim | Öncelik | Doğrulama | Durum |
|---|---|---|---|---|
| FG-WS-01 | Port 81'de WebSocket sunucusu çalışmalıdır. | Z | T | ✅ |
| FG-WS-02 | Yeni stabil ölçüm, buton durum değişimi veya batarya raporu olduğunda, bağlı en az bir istemci varsa ölçüm JSON'u tüm istemcilere yayınlanmalıdır (`ageMs = 0`). | Z | T | ✅ |
| FG-WS-03 | Bağlı istemci yoksa JSON üretilmemeli (gereksiz yük oluşturulmamalı). | Ö | İ | ✅ |
| FG-WS-04 | İstemciden gelen mesajlar yok sayılmalıdır (tek yönlü yayın). | Ö | İ | ✅ |

### 3.8 Tarayıcı Arayüzü (UI)

| ID | Gereksinim | Öncelik | Doğrulama | Durum |
|---|---|---|---|---|
| FG-UI-01 | Ana sayfa mm (2 ondalık) ve inç (4 ondalık) değerlerini büyük puntoyla göstermelidir. | Z | D | ✅ |
| FG-UI-02 | Ana sayfa bağlantı durumu (online/offline), yaş (ms), ham anahtar (hex), birim, işaret, buton, kalibrasyon ofseti göstergelerini içermelidir. | Ö | D | ✅ |
| FG-UI-03 | Ana sayfa `/api/measure`'ı 200 ms aralıkla sorgulamalıdır. | Z | İ | ✅ |
| FG-UI-04 | Batarya kartı yüzde, gerilim, dolum çubuğu ve şarj göstergesi içermeli; yüzde ≤ 20 iken çubuk kırmızı/turuncu olmalıdır. | Ö | D | ✅ |
| FG-UI-05 | Buton `released → pressed` geçişinde ölçüm, tarayıcı saatiyle zaman damgalanarak buton loguna eklenmelidir. | Z | D | ⚠️ KS-05, KS-07 |
| FG-UI-06 | Kullanıcı, kayıt öncesi "Not" alanına metin girebilmeli; bu not bir sonraki kayda eklenip alan temizlenmelidir. | Ö | D | ✅ |
| FG-UI-07 | Her log satırının notu sonradan düzenlenebilmelidir. | İ | D | ✅ |
| FG-UI-08 | Buton logu UTF-8 BOM'lu, tüm hücreleri tırnaklı CSV olarak `buton-logu-YYYYMMDD-HHMMSS.csv` adıyla indirilebilmelidir. | Ö | D | ✅ |
| FG-UI-09 | Buton logu en fazla 200 satır tutmalı, eski satırlar atılmalıdır. | Ö | İ | ✅ |
| FG-UI-10 | Arayüz 640 px altı ekranlarda (telefon) kullanılabilir olmalı ve açık/koyu temaya uymalıdır. | Ö | D | ✅ |
| FG-UI-11 | `/log` sayfası cihaz logunu 250 ms aralıkla artımlı çekip göstermelidir. | Ö | D | ✅ |
| FG-UI-12 | `/log` sayfasındaki "Temizle" görünümü temizlemeli ve temizlenen satırlar geri gelmemelidir. | İ | T | ❌ KS-08 |

### 3.9 Kalibrasyon (KAL)

| ID | Gereksinim | Öncelik | Doğrulama | Durum |
|---|---|---|---|---|
| FG-KAL-01 | Kullanıcı `/calibrate` formundan mm cinsinden ofset (0,001 çözünürlük) girebilmelidir. | Z | D | ✅ |
| FG-KAL-02 | Ofset NVS'de `kumpas` ad alanında `offset_mm` anahtarıyla saklanmalı ve açılışta yüklenmelidir. | Z | T | ✅ |
| FG-KAL-03 | Ofset, yayınlanan mm değerine eklenmelidir. | Z | T | ✅ |
| FG-KAL-04 | "Offset Sıfırla" ofseti 0 yapmalıdır. | Z | T | ✅ |
| FG-KAL-05 | `offset` parametresi yoksa 400 döndürülmeli; başarılı kayıt sonrası `303 → /calibrate` yönlendirmesi yapılmalıdır. | Ö | T | ✅ |
| FG-KAL-06 | Geçersiz veya makul aralık dışı ofset girişi reddedilmelidir. | Ö | T | ❌ KS-13 |
| FG-KAL-07 | Ofset inç değerine de tutarlı biçimde uygulanmalıdır. | İ | T | ❌ KS-12 |

### 3.10 OTA Güncelleme (OTA)

| ID | Gereksinim | Öncelik | Doğrulama | Durum |
|---|---|---|---|---|
| FG-OTA-01 | `POST /update` (multipart, alan adı `firmware`) ile `.bin` dosyası yüklenebilmelidir. | Z | D | ✅ |
| FG-OTA-02 | Yükleme pasif OTA bölümüne yazılmalı; başarıyla bitmeden aktif firmware değişmemelidir. | Z | A | ✅ |
| FG-OTA-03 | Başarılı yükleme sonrası kullanıcıya başarı sayfası gösterilip ~400 ms sonra cihaz yeniden başlatılmalıdır. | Z | D | ✅ |
| FG-OTA-04 | Başarısız yüklemede 400 ve hata nedeni gösterilmeli, cihaz eski firmware ile çalışmaya devam etmelidir. | Z | T | ⚠️ KS-02 |
| FG-OTA-05 | OTA bölümünü aşan dosyalar, yazma başlamadan reddedilmelidir. | Ö | T | ⚠️ KS-01 |
| FG-OTA-06 | Tüm OTA olayları cihaz loguna yazılmalıdır. | Ö | İ | ✅ |

### 3.11 Batarya İzleme (BAT)

| ID | Gereksinim | Öncelik | Doğrulama | Durum |
|---|---|---|---|---|
| FG-BAT-01 | GPIO5 ADC, 12-bit çözünürlük ve 11 dB zayıflatma ile okunmalı; her örneklemede 8 okuma ortalaması (mV) alınmalıdır. | Z | İ | ✅ |
| FG-BAT-02 | Batarya gerilimi `Vadc × (R1+R2)/R2` ile hesaplanmalıdır. | Z | T | ✅ |
| FG-BAT-03 | Gerilim, α = 0,15 üstel (IIR) filtre ile yumuşatılmalıdır. | Ö | İ | ✅ |
| FG-BAT-04 | Yüzde, 3,0 V → %0 ile 4,2 V → %100 arasında doğrusal ve sınırlandırılmış hesaplanmalıdır. | Ö | T | ✅ |
| FG-BAT-05 | Şarj durumu; 30 s pencerede ≥ 10 mV yükselme **veya** ≥ 120 s kararlılık sonrası ≥ 10 mV yükselme ile, gerilim 4,15 V altındayken çıkarılmalıdır. | Ö | A | ✅ |
| FG-BAT-06 | Batarya raporu yalnızca gerilim ≥ 10 mV değiştiğinde, yüzde veya şarj durumu değiştiğinde ya da son rapordan 5 s geçtiğinde yayınlanmalıdır. | Ö | İ | ✅ |
| FG-BAT-07 | Örnekleme periyodu şarj sırasında ve boşta farklı ayarlanabilmelidir. | İ | İ | ⚠️ KS-10 |

### 3.12 Buton (BTN)

| ID | Gereksinim | Öncelik | Doğrulama | Durum |
|---|---|---|---|---|
| FG-BTN-01 | GPIO10 her döngüde okunmalı ve 20 ms kararlılık (debounce) sonrası değişim kabul edilmelidir. | Z | İ | ✅ |
| FG-BTN-02 | Kabul edilen her değişim `buttonPressed` olarak HTTP/WS'e yansıtılmalıdır. | Z | T | ✅ |
| FG-BTN-03 | Belgelenen donanım bağlantısıyla basılı = `true`, bırakılmış = `false` olmalıdır. | Z | T | ❌ KS-05 |

### 3.13 Cihaz Logu (LOG)

| ID | Gereksinim | Öncelik | Doğrulama | Durum |
|---|---|---|---|---|
| FG-LOG-01 | `webLogf(fmt, …)` printf biçimli satırları RAM'de 100 satırlık halka tampona yazmalıdır (satır başına en fazla 95 karakter). | Ö | İ | ✅ |
| FG-LOG-02 | Her satıra monoton artan sıra numarası (1'den başlayan) verilmelidir. | Ö | T | ✅ |
| FG-LOG-03 | Açılış, WiFi, kalibrasyon ve OTA olayları loglanmalıdır. | Ö | İ | ✅ |
| FG-LOG-04 | JSON çıktısında özel karakterler (`"`, `\`, `\n`, `\r`, `\t`) kaçışlanmalı, diğer kontrol karakterleri atılmalıdır. | Z | T | ✅ |

### 3.14 Python İstemcisi (PY)

| ID | Gereksinim | Öncelik | Doğrulama | Durum |
|---|---|---|---|---|
| FG-PY-01 | Script `ws://kumpas.local:81/` adresine bağlanmalı, kopma halinde 2 s bekleyip yeniden bağlanmalıdır. | Z | D | ✅ |
| FG-PY-02 | Her mesajda ölçüm konsola yazdırılmalıdır. | İ | D | ⚠️ KS-16 |
| FG-PY-03 | `buttonPressed` `false → true` geçişinde mm değeri `DECIMALS` ondalıkla (isteğe bağlı virgüllü) aktif pencereye yazılıp Enter'a basılmalıdır. | Z | D | ✅ |
| FG-PY-04 | Ardışık iki tetikleme arası 0,2 s'den kısa ise ikincisi yok sayılmalıdır. | Ö | T | ✅ |
| FG-PY-05 | İlk mesaj yalnızca durum başlangıcı olarak kullanılmalı, tetikleme yapmamalıdır. | Ö | İ | ✅ |
| FG-PY-06 | Bağımlılıklar eksikse kurulum talimatıyla anlaşılır biçimde çıkılmalıdır. | Ö | D | ✅ |

---

## 4. Dış Arayüz Gereksinimleri

### 4.1 Donanım Arayüzleri (DON)

| ID | Gereksinim | Değer |
|---|---|---|
| AG-DON-01 | Kumpas DATA girişi | GPIO8, `INPUT` (dahili direnç yok) |
| AG-DON-02 | Kumpas CLOCK girişi | GPIO9, `INPUT`, düşen kenar kesmesi |
| AG-DON-03 | Buton girişi | GPIO10, `INPUT_PULLUP`, "basılı = HIGH" yorumu (bkz. KS-05) |
| AG-DON-04 | Batarya ADC girişi | GPIO5 (ADC1), 12 bit, 11 dB |
| AG-DON-05 | Kumpas sinyal seviyesi | ~1,5 V → 3,3 V dönüştürülmeli; GND ortak |
| AG-DON-06 | Batarya bölücü | R1 = R2 = 100 kΩ (oran 2,0) — sabitlerle değiştirilebilir |
| AG-DON-07 | Seri/USB | Yerleşik USB-CDC (`ARDUINO_USB_CDC_ON_BOOT=1`) |

### 4.2 Yazılım Arayüzleri (YAZ)

| ID | Gereksinim |
|---|---|
| AG-YAZ-01 | `web_ui.h` modüller arası tek sözleşmedir: `webBegin`, `webLoop`, `webUpdateMeasurement`, `webUpdateButton`, `webUpdateBattery`, `webLogf`. Ayrıntı: [SDD §8](SDD_YAZILIM_TASARIM_DOKUMANI.md#8-modül-arayüz-sözleşmesi). |
| AG-YAZ-02 | NVS ad alanı `kumpas`, anahtar `offset_mm` (float). |
| AG-YAZ-03 | Flash bölüm tablosu `partitions_ota_4mb.csv` ile uyumlu olmalıdır. |

### 4.3 İletişim Arayüzleri (ILT)

| ID | Gereksinim |
|---|---|
| AG-ILT-01 | HTTP/1.1, port 80, şifresiz. Yollar: `/`, `/api/measure`, `/api/log`, `/log`, `/calibrate` (GET/POST), `/update` (GET/POST). |
| AG-ILT-02 | WebSocket, port 81, yol `/`, metin çerçeveleri, JSON yükü. |
| AG-ILT-03 | mDNS, ana makine adı `kumpas`, servis `_http._tcp:80`. |
| AG-ILT-04 | JSON alan adları ve tipleri geriye dönük uyumlu tutulmalıdır (alan eklenebilir, silinemez/yeniden adlandırılamaz). |

### 4.4 Kullanıcı Arayüzü (KUA)

| ID | Gereksinim |
|---|---|
| AG-KUA-01 | Arayüz dili Türkçe'dir. |
| AG-KUA-02 | Harici CDN/font/kütüphane kullanılmaz; sayfa internet olmadan tam çalışır. |
| AG-KUA-03 | HTML elemanlarının `id`'leri (`mm`, `inch`, `status`, `age`, `raw`, `unit`, `sign`, `btn`, `cal`, `batPct`, `batV`, `batCharge`, `batChargeText`, `batFill`, `logBody`, `noteInput`, `exportCsv`, `clearLog`, `noteBackdrop`) sabit sözleşme kabul edilir. |

---

## 5. Fonksiyonel Olmayan Gereksinimler

### 5.1 Performans (PRF)

| ID | Gereksinim | Doğrulama | Durum |
|---|---|---|---|
| FOG-PRF-01 | ISR yürütme süresi tek kenar için birkaç mikrosaniyeyi aşmamalı, kumpas saat periyodundan belirgin şekilde kısa olmalıdır. | A | ✅ |
| FOG-PRF-02 | Kumpas değeri durduktan sonra yayın gecikmesi, normal durumda ≤ 2 kumpas çerçeve periyodu + 200 ms (UI sorgu aralığı) olmalıdır. | T | ✅ |
| FOG-PRF-03 | WebSocket yayın gecikmesi, stabil değerin üretilmesinden itibaren yerel ağda tipik < 50 ms olmalıdır. | T | ✅ (beklenen) |
| FOG-PRF-04 | Ana döngü periyodu ~2 ms + ağ işleme süresi olmalıdır; ağ işlemleri döngüyü 100 ms'den uzun bloklamamalıdır (OTA hariç). | A | ✅ |
| FOG-PRF-05 | Açılıştan ölçüm okumaya geçiş süresi, WiFi bağlantı süresiyle (en fazla ~15 s) sınırlıdır; ISR açılışta hemen etkinleşir. | T | ✅ |

### 5.2 Güvenilirlik (GVN)

| ID | Gereksinim | Doğrulama | Durum |
|---|---|---|---|
| FOG-GVN-01 | Tek bozuk çerçeve hiçbir zaman yayınlanmamalıdır. | A, T | ✅ |
| FOG-GVN-02 | Birim biti gürültüsü, tek başına birim değişimine yol açmamalıdır. | A, T | ✅ |
| FOG-GVN-03 | WiFi yokluğu ölçüm okumayı engellememelidir. | T | ✅ |
| FOG-GVN-04 | Yarıda kalan OTA, cihazı açılamaz hale getirmemelidir. | A | ✅ |
| FOG-GVN-05 | Boot edip hatalı çalışan yeni firmware'den otomatik geri dönüş sağlanmalıdır. | A | ❌ KS-21 |
| FOG-GVN-06 | Kalibrasyon ofseti güç kesintisinde korunmalıdır. | T | ✅ |

### 5.3 Kaynak Kullanımı (KAY)

| ID | Gereksinim | Doğrulama | Durum |
|---|---|---|---|
| FOG-KAY-01 | Firmware ikili dosyası OTA bölüm boyutunu (0x140000 = 1 310 720 bayt) aşmamalıdır. | A | ✅ (derleme çıktısıyla izlenir) |
| FOG-KAY-02 | Statik HTML/CSS/JS flash'ta (PROGMEM) tutulmalıdır. | İ | ✅ |
| FOG-KAY-03 | Cihaz logu için ayrılan statik RAM 100 × 96 = 9 600 baytı aşmamalıdır. | İ | ✅ |
| FOG-KAY-04 | Dinamik bellek tahsisleri sınırlı ve kısa ömürlü olmalıdır (JSON `String`'leri önceden `reserve` edilir). | İ | ✅ |

### 5.4 Güvenlik (SEC)

| ID | Gereksinim | Doğrulama | Durum |
|---|---|---|---|
| FOG-SEC-01 | WiFi kimlik bilgileri sürüm kontrolüne girmemelidir (`.gitignore`). | İ | ✅ |
| FOG-SEC-02 | Derlenmiş `firmware.bin` paylaşılırken kimlik bilgilerinin içinde düz metin bulunduğu dokümante edilmelidir. | İ | ✅ (bu doküman) — KS-04 |
| FOG-SEC-03 | OTA ve kalibrasyon uç noktaları yetkisiz değişikliğe karşı korunmalıdır. | T | ❌ KS-03 (bilinçli kabul: ADR-022) |
| FOG-SEC-04 | JSON çıktısı enjeksiyona karşı kaçışlanmalı; UI kullanıcı notlarını `textContent` ile basmalıdır (XSS önlemi). | İ | ✅ |

### 5.5 Bakım Kolaylığı (BAK)

| ID | Gereksinim | Doğrulama | Durum |
|---|---|---|---|
| FOG-BAK-01 | Tüm donanım/protokol ayarları `src/main.cpp` başında `constexpr` sabitler olarak toplanmalıdır. | İ | ✅ |
| FOG-BAK-02 | Ölçüm mantığı ile ağ/sunum katmanı ayrı derleme birimlerinde olmalı ve yalnızca `web_ui.h` üzerinden konuşmalıdır. | İ | ✅ |
| FOG-BAK-03 | Mimari kararlar ADR olarak, işletim/geliştirme adımları GUIDE olarak `doc/` altında tutulmalıdır. | İ | ✅ |
| FOG-BAK-04 | Otomatik birim/entegrasyon testleri bulunmalıdır. | İ | ❌ KS-19 |

### 5.6 Kullanılabilirlik (KUL)

| ID | Gereksinim | Doğrulama | Durum |
|---|---|---|---|
| FOG-KUL-01 | Operatör, cihaz adresini bildikten sonra eğitim gerektirmeden ölçüm görebilmeli ve CSV indirebilmelidir. | D | ✅ |
| FOG-KUL-02 | Cihaz IP'si değişse bile `kumpas.local` ile erişilebilmelidir (mDNS destekleyen istemcilerde). | D | ✅ |
| FOG-KUL-03 | Hata durumları (offline, OTA hatası) kullanıcıya görünür olmalıdır. | D | ✅ |

### 5.7 Taşınabilirlik (TAS)

| ID | Gereksinim | Doğrulama | Durum |
|---|---|---|---|
| FOG-TAS-01 | Pin atamaları tek yerden değiştirilerek başka ESP32-S3 kartlarına taşınabilmelidir. | İ | ✅ |
| FOG-TAS-02 | Farklı bit düzenine sahip kumpaslar yalnızca sabit değişikliğiyle desteklenebilmelidir. | İ | ✅ |
| FOG-TAS-03 | 8 MB/16 MB flash'lı kartlar için bölüm tablosu ve `flash_size` değiştirilerek derlenebilmelidir. | İ | ✅ |

---

## 6. Tasarım ve Uygulama Kısıtları

| ID | Kısıt | Gerekçe / ADR |
|---|---|---|
| KIS-01 | Donanım: ESP32-S3 Super Mini, 4 MB flash | [ADR-001](adr/ADR-001_DONANIM_PLATFORMU_ESP32_S3_SUPER_MINI.md) |
| KIS-02 | Derleme sistemi: PlatformIO; çatı: Arduino-ESP32 | [ADR-002](adr/ADR-002_PLATFORMIO_VE_ARDUINO_CATISI.md) |
| KIS-03 | Tek "süper döngü" — ek FreeRTOS görevi oluşturulmaz | [ADR-009](adr/ADR-009_TEK_SUPER_DONGU_MIMARISI.md) |
| KIS-04 | Harici web kaynağı kullanılmaz (çevrimdışı LAN) | [ADR-012](adr/ADR-012_GOMULU_HTML_PROGMEM.md) |
| KIS-05 | Kimlik bilgileri derleme zamanında gömülür | [ADR-016](adr/ADR-016_WIFI_STA_MDNS_VE_GIZLI_BILGI_YONETIMI.md) |
| KIS-06 | Bölüm tablosu: 2 × 1,25 MB uygulama slotu | [ADR-015](adr/ADR-015_TARAYICI_UZERINDEN_OTA_VE_CIFT_SLOT.md) |
| KIS-07 | WiFi açıkken ADC2 kullanılamaz; batarya ADC'si ADC1 kanalında olmalıdır | [ADR-017](adr/ADR-017_BATARYA_OLCUMU_VE_SARJ_CIKARIMI.md) |

---

## 7. Doğrulama Yaklaşımı

- **Test (T):** [GUIDE-015](guide/GUIDE-015_TEST_VE_DOGRULAMA.md) içindeki `TC-nn` manuel test senaryoları ile.
- **Gösterim (D):** Gerçek donanımda, tarayıcı ve seri monitör üzerinden gözlemle.
- **İnceleme (İ):** Kaynak kod incelemesiyle (satır referansları SDD'de).
- **Analiz (A):** Zamanlama/olasılık/bölüm boyutu hesaplarıyla (SDD §7).

Bir sürüm yayınlanmadan önce öncelik **Z** olan tüm gereksinimlerin ilgili test senaryoları geçmelidir. ⚠️/❌ durumundaki gereksinimler [BILINEN_SORUNLAR.md](BILINEN_SORUNLAR.md) içinde izlenir.

---

## 8. İzlenebilirlik Matrisi

| Gereksinim grubu | SDD bölümü | Kaynak kod | ADR | Test |
|---|---|---|---|---|
| FG-OKU-01..08 | §4.1, §7.1 | [main.cpp:175-236](../src/main.cpp#L175-L236) | ADR-003, ADR-004, ADR-005 | TC-01, TC-02 |
| FG-COZ-01..07 | §4.2 | [main.cpp:238-283](../src/main.cpp#L238-L283), [main.cpp:50-67](../src/main.cpp#L50-L67) | ADR-006 | TC-03, TC-04, TC-05 |
| FG-FLT-01..04 | §4.3 | [main.cpp:422-470](../src/main.cpp#L422-L470) | ADR-007 | TC-06, TC-07 |
| FG-BRM-01..04 | §4.4 | [main.cpp:133-173](../src/main.cpp#L133-L173) | ADR-008 | TC-08 |
| FG-NET-01..06 | §4.7.1 | [web_ui.cpp:767-801](../src/web_ui.cpp#L767-L801) | ADR-016 | TC-09, TC-10 |
| FG-HTTP-01..06 | §4.7.2, §6.3 | [web_ui.cpp:654-740](../src/web_ui.cpp#L654-L740) | ADR-011, ADR-013 | TC-11, TC-12 |
| FG-WS-01..04 | §4.7.3 | [web_ui.cpp:743-752](../src/web_ui.cpp#L743-L752), [web_ui.cpp:884-919](../src/web_ui.cpp#L884-L919) | ADR-011 | TC-13 |
| FG-UI-01..12 | §4.8 | [web_ui.cpp:89-561](../src/web_ui.cpp#L89-L561) | ADR-012, ADR-020 | TC-14..TC-17 |
| FG-KAL-01..07 | §4.7.4 | [web_ui.cpp:754-765](../src/web_ui.cpp#L754-L765), [web_ui.cpp:809-820](../src/web_ui.cpp#L809-L820) | ADR-014 | TC-18, TC-19 |
| FG-OTA-01..06 | §4.7.5 | [web_ui.cpp:821-870](../src/web_ui.cpp#L821-L870) | ADR-015 | TC-20, TC-21 |
| FG-BAT-01..07 | §4.5 | [main.cpp:345-420](../src/main.cpp#L345-L420) | ADR-017 | TC-22, TC-23 |
| FG-BTN-01..03 | §4.6 | [main.cpp:332-343](../src/main.cpp#L332-L343) | ADR-018 | TC-24 |
| FG-LOG-01..04 | §4.7.6 | [web_ui.cpp:39-87](../src/web_ui.cpp#L39-L87), [web_ui.cpp:711-740](../src/web_ui.cpp#L711-L740) | ADR-019 | TC-25 |
| FG-PY-01..06 | §4.9 | [measure_ws.py](../scripts/measure_ws.py) | ADR-021 | TC-26 |
| FOG-SEC-* | §10 | [.gitignore](../.gitignore), web_ui.cpp | ADR-016, ADR-022 | TC-27 |

---

## Ek A — Gereksinim Durum Özeti

| Durum | Sayı | Gereksinimler |
|---|---|---|
| ❌ Karşılanmıyor | 7 | FG-UI-12, FG-KAL-06, FG-KAL-07, FG-BTN-03, FOG-GVN-05, FOG-SEC-03, FOG-BAK-04 |
| ⚠️ Kısmen | 7 | FG-NET-05, FG-HTTP-06, FG-UI-05, FG-OTA-04, FG-OTA-05, FG-BAT-07, FG-PY-02 |
| ✅ Karşılanıyor | Diğerleri | — |

## Ek B — Değişiklik Geçmişi

| Sürüm | Tarih | Açıklama |
|---|---|---|
| 1.0 | 2026-10-10 | İlk sürüm; `2e2619b` commit'indeki koddan çıkarıldı. |
