# ADR-002: PlatformIO Derleme Sistemi ve Arduino-ESP32 Çatısı

| Alan | Değer |
|---|---|
| **Durum** | Kabul edildi |
| **Karar tarihi** | 2026-04-26 (commit `0be7619`) |
| **Dokümante edilme** | 2026-10-10 |
| **İlgili** | [ADR-001](ADR-001_DONANIM_PLATFORMU_ESP32_S3_SUPER_MINI.md), [GUIDE-001](../guide/GUIDE-001_GELISTIRME_ORTAMI_KURULUMU.md), [KS-26](../BILINEN_SORUNLAR.md#ks-26--kütüphane-ve-platform-sürümleri-sabitlenmemiş-) |

## Bağlam

Firmware'in hem yeni başlayanlar tarafından derlenebilmesi hem de düşük seviyeli ESP-IDF API'lerine (`esp_timer`, `gpio_get_level`, `portMUX`) erişebilmesi gerekiyordu. Projenin VS Code içinde, bağımlılıkları otomatik çözülerek ve tekrarlanabilir biçimde derlenmesi isteniyordu.

## Karar

- Derleme sistemi: **PlatformIO** (`platformio.ini`, VS Code eklentisi veya CLI).
- Çatı: **Arduino-ESP32** (`framework = arduino`). Gerekli yerlerde ESP-IDF API'leri doğrudan çağrılır.
- Harici kütüphane yalnızca `links2004/WebSockets`; geri kalan her şey çekirdekle gelir (`WiFi`, `WebServer`, `ESPmDNS`, `Update`, `Preferences`).
- Windows'ta PlatformIO'nun kendi Python ortamını (penv) zorlamak için [pio.ps1](../../pio.ps1) sarmalayıcısı.

## Değerlendirilen Alternatifler

| Seçenek | Artı | Eksi |
|---|---|---|
| Arduino IDE | En kolay kurulum | Bölüm tablosu/bayrak yönetimi zahmetli, tekrarlanabilirlik zayıf |
| Saf ESP-IDF (CMake) | Tam kontrol, rollback, güvenli önyükleme | Öğrenme eğrisi yüksek, HTTP/WS/mDNS için çok daha fazla kod |
| **PlatformIO + Arduino** | Tek dosyada yapılandırma, otomatik bağımlılık, VS Code entegrasyonu, ESP-IDF API'lerine erişim | Sürüm sabitlenmezse kayma; Windows'ta penv sorunları |

## Sonuçlar

**Olumlu**
- `pio run -t upload` tek komutla derleme + yükleme.
- `build_flags` ile USB-CDC gibi çekirdek ayarları kod dışında yönetilir.
- Arduino'nun hazır `WebServer`, `Update`, `Preferences` sınıfları ağ/OTA/NVS kodunu ~300 satırda tutar.

**Olumsuz / Riskler**
- `platform` ve `lib_deps` sürümleri sabitlenmemiştir (KS-26): Arduino-ESP32 3.x'e geçişte API değişiklikleri derlemeyi bozabilir.
- Arduino çekirdeğinin önceden derlenmiş `sdkconfig`'i değiştirilemez (ör. uygulama rollback'i, ISR IRAM yerleşimi seçenekleri).
- Windows'ta proje `.venv`'i ile PlatformIO penv çakışabilir → [GUIDE-001 §5](../guide/GUIDE-001_GELISTIRME_ORTAMI_KURULUMU.md).

## Uygulama Notları

```ini
[env:esp32s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
lib_deps = links2004/WebSockets
board_build.flash_size = 4MB
board_upload.flash_size = 4MB
board_build.partitions = partitions_ota_4mb.csv
build_flags = -D ARDUINO_USB_CDC_ON_BOOT=1 -D ARDUINO_USB_MODE=1
```

## Gelecek

Sürüm sabitleme (KS-26) yapıldığında bu ADR'ye "Sürüm politikası" bölümü eklenmelidir.
