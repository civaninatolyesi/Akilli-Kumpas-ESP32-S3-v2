# ADR-001: Donanım Platformu Olarak ESP32-S3 Super Mini

| Alan | Değer |
|---|---|
| **Durum** | Kabul edildi |
| **Karar tarihi** | 2026-04-26 (commit `0be7619`) |
| **Dokümante edilme** | 2026-10-10 |
| **Karar verici** | Proje sahibi |
| **İlgili** | [ADR-002](ADR-002_PLATFORMIO_VE_ARDUINO_CATISI.md), [ADR-015](ADR-015_TARAYICI_UZERINDEN_OTA_VE_CIFT_SLOT.md), [ADR-017](ADR-017_BATARYA_OLCUMU_VE_SARJ_CIKARIMI.md), SRS KIS-01 |

## Bağlam

Ucuz bir dijital kumpasın ölçümlerini bilgisayara/tarayıcıya aktaracak bir okuyucu gerekiyordu. Gereksinimler:

- Mikrosaniye düzeyinde kenar yakalayabilen kesme desteği
- Kablosuz bağlantı (kumpas atölyede serbestçe kullanılabilmeli; kablo istenmiyor)
- Kumpasın üzerine/yanına takılabilecek kadar küçük boyut
- LiPo batarya ile çalışabilme
- Düşük maliyet ve kolay bulunabilirlik
- USB üzerinden programlama ve seri konsol

## Karar

**ESP32-S3 Super Mini** kartı kullanılır (4 MB flash, yerleşik USB-CDC, 2,4 GHz WiFi + BLE, ~22 × 18 mm).

Pin ataması:

| İşlev | GPIO | Gerekçe |
|---|---|---|
| Kumpas DATA | 8 | Kartın kenarında, strapping pini değil |
| Kumpas CLOCK | 9 | Kesme destekli, DATA'ya komşu (kısa kablo) |
| Buton | 10 | Serbest dijital giriş |
| Batarya ADC | 5 | **ADC1** kanalı — WiFi açıkken ADC2 kullanılamaz |

## Değerlendirilen Alternatifler

| Seçenek | Artı | Eksi | Sonuç |
|---|---|---|---|
| Arduino Nano/Uno + HC-05 | Basit, bol örnek | 5 V mantık, ayrı kablosuz modül, büyük | Reddedildi |
| ESP8266 (D1 mini) | Ucuz, WiFi | Tek çekirdek, zayıf ADC, WiFi yığını ISR zamanlamasını bozabilir, yerel USB yok | Reddedildi |
| ESP32 (klasik) DevKit | Güçlü, yaygın | Büyük kart, USB-UART köprüsü gerekli | Reddedildi |
| ESP32-C3 Super Mini | Çok küçük, ucuz, RISC-V | Tek çekirdek; WiFi ve uygulama aynı çekirdeği paylaşır | Değerlendirildi, S3 tercih edildi |
| **ESP32-S3 Super Mini** | Çift çekirdek (WiFi çekirdek 0'da, uygulama çekirdek 1'de), yerel USB, küçük | Biraz daha pahalı, 4 MB flash varyantı OTA alanını sınırlıyor | **Seçildi** |
| Raspberry Pi Pico W | Ucuz, PIO ile mükemmel kenar yakalama | Arduino/WiFi ekosistemi daha az olgun (karar anında) | Reddedildi |

## Sonuçlar

**Olumlu**
- WiFi yığını çekirdek 0'da, Arduino `loopTask` çekirdek 1'de çalıştığı için ölçüm döngüsü ağ trafiğinden büyük ölçüde yalıtılır.
- Yerel USB-CDC ile ek sürücü gerekmeden yükleme ve seri konsol (`ARDUINO_USB_CDC_ON_BOOT=1`).
- OTA için iki uygulama slotu sığar.

**Olumsuz / Riskler**
- Bazı Super Mini kartlar 4 MB flash ile gelir; 8 MB varsayan bölüm tablosu "partition invalid … exceeds flash chip size" hatasıyla açılmaz. Bu nedenle `platformio.ini`'de 4 MB açıkça belirtilir.
- Her OTA slotu 1,25 MB ile sınırlı (bkz. ADR-015).
- Kart klonları arasında yerleşik LED/pin farkları olabilir; pinler `main.cpp` başında tek yerden değiştirilebilir tutulur.
- GPIO'lar 3,3 V'tur ve kumpas 1,5 V ile çalışır → seviye dönüştürme zorunlu ([GUIDE-002](../guide/GUIDE-002_DONANIM_BAGLANTISI.md)).

## Uygulama Notları

- [platformio.ini](../../platformio.ini): `board = esp32-s3-devkitc-1`, `board_build.flash_size = 4MB`, `board_upload.flash_size = 4MB`.
- [main.cpp:9-12](../../src/main.cpp#L9-L12): pin sabitleri.

## Doğrulama

- Seri açılış logunda flash boyutunun 0x400000 olduğu görülür.
- Batarya ADC'si WiFi bağlıyken anlamlı değer döndürür (ADC1 doğrulaması) — [GUIDE-015](../guide/GUIDE-015_TEST_VE_DOGRULAMA.md) TC-22.
